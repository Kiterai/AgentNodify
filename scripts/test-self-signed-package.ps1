[CmdletBinding(SupportsShouldProcess = $true, DefaultParameterSetName = 'Build')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Build')][string]$QtPath,
    [Parameter(Mandatory = $true, ParameterSetName = 'Prepared')][string]$PreparedPackageDirectory,
    [Parameter(Mandatory = $true)][string]$IfwPath,
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$SignToolPath,
    [string]$DependencySourcesDirectory,
    [switch]$AllowTemporaryUserTrust,
    [ValidateSet('Host', 'WindowsSandbox')][string]$EnvironmentKind = 'Host',
    [string]$ProgressReportPath
)
$ErrorActionPreference = 'Stop'
if (!$AllowTemporaryUserTrust) {
    throw 'Explicit -AllowTemporaryUserTrust consent is required: temporarily trusts one disposable test certificate in CurrentUser/Root.'
}
if (Test-Path -LiteralPath $BuildDirectory) { throw 'Use a new dedicated build directory; test packages must not replace release packages.' }
if (!$PSCmdlet.ShouldProcess($BuildDirectory, 'Create a disposable signing key, temporarily trust its certificate in CurrentUser/Root, build/sign/test, then remove that exact certificate and key')) { return }
$certificate = $null
$thumbprint = $null
$report = [ordered]@{ schema = 1; testOnly = $true; passed = $false; trustCleanupPassed = $false; environment = $EnvironmentKind; packagingMode = $PSCmdlet.ParameterSetName; phase = 'preflight' }
$cmakeArguments = @()
if ($DependencySourcesDirectory) {
    foreach ($dependency in @(@('JSON', 'json-3.11.3'), @('JSON_VALIDATOR', 'json-schema-validator-2.3.0'), @('QTNODES', 'nodeeditor-3.0.12'))) {
        $source = (Resolve-Path -LiteralPath (Join-Path $DependencySourcesDirectory $dependency[1])).Path
        $cmakeArguments += "-DFETCHCONTENT_SOURCE_DIR_$($dependency[0])=$source"
    }
}
function Save-TestReport {
    $json = $report | ConvertTo-Json
    [IO.File]::WriteAllText((Join-Path (Resolve-Path $BuildDirectory).Path 'self-signing-test.json'), $json, (New-Object Text.UTF8Encoding $false))
    if ($ProgressReportPath) { [IO.File]::WriteAllText($ProgressReportPath, $json, (New-Object Text.UTF8Encoding $false)) }
}
try {
    New-Item -ItemType Directory -Path $BuildDirectory | Out-Null
    [IO.File]::WriteAllText((Join-Path (Resolve-Path $BuildDirectory).Path 'TEST-ONLY.txt'), 'SELF-SIGNED TEST PACKAGE. DO NOT DISTRIBUTE.')
    Save-TestReport
    # Fail before creating a key/trust entry if the portable SDK runtime is incomplete.
    $preflight = New-Object Diagnostics.Process
    $preflight.StartInfo = New-Object Diagnostics.ProcessStartInfo
    $preflight.StartInfo.FileName = (Resolve-Path -LiteralPath $SignToolPath).Path
    $preflight.StartInfo.Arguments = '/?'
    $preflight.StartInfo.UseShellExecute = $false
    $preflight.StartInfo.RedirectStandardOutput = $true
    $preflight.StartInfo.RedirectStandardError = $true
    try {
        if (!$preflight.Start()) { throw 'Cannot start SignTool preflight.' }
        $stdout = $preflight.StandardOutput.ReadToEndAsync()
        $stderr = $preflight.StandardError.ReadToEndAsync()
        if (!$preflight.WaitForExit(10000)) { $preflight.Kill(); $preflight.WaitForExit(); throw 'SignTool preflight timed out.' }
        [IO.File]::WriteAllText((Join-Path (Resolve-Path $BuildDirectory).Path 'signtool-preflight.log'), ($stdout.Result + $stderr.Result))
        if ($preflight.ExitCode) { throw 'SignTool preflight failed; no test certificate was created.' }
    } finally { $preflight.Dispose() }
    $report.phase = 'creating-certificate'; Save-TestReport
    $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject ('CN=AgentNodify TEST ONLY ' + [guid]::NewGuid().ToString('N')) `
        -CertStoreLocation Cert:/CurrentUser/My -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 `
        -KeyExportPolicy NonExportable -NotAfter (Get-Date).AddDays(7)
    $thumbprint = $certificate.Thumbprint
    $report.thumbprint = $thumbprint
    Save-TestReport
    # Only the public certificate is exported. No PFX/password/private key files.
    $publicCertificate = Join-Path (Resolve-Path $BuildDirectory).Path 'test-signing.cer'
    Export-Certificate -Cert $certificate -FilePath $publicCertificate | Out-Null
    $report.phase = 'trusting-guest-certificate'; Save-TestReport
    Import-Certificate -FilePath $publicCertificate -CertStoreLocation Cert:/CurrentUser/Root | Out-Null
    $report.phase = 'signing-and-packaging'; Save-TestReport
    if ($PSCmdlet.ParameterSetName -eq 'Prepared') {
        # Repackage CPack's deployed payload: no compiler or full SDK in the VM.
        $stage = Join-Path $BuildDirectory 'stage'
        New-Item -ItemType Directory -Path $stage | Out-Null
        foreach ($name in @('config', 'packages')) { Copy-Item -LiteralPath (Join-Path $PreparedPackageDirectory $name) -Destination $stage -Recurse }
        $data = Join-Path $stage 'packages/org.agentnodify.application/data'
        $maintenanceDirectory = Join-Path $data 'maintenance'
        New-Item -ItemType Directory -Path $maintenanceDirectory -Force | Out-Null
        $base = Join-Path $maintenanceDirectory 'installerbase.exe'
        Copy-Item -LiteralPath (Join-Path $IfwPath 'bin/installerbase.exe') -Destination $base
        & "$PSScriptRoot/sign-windows.ps1" -Files @($base, (Join-Path $data 'bin/AgentNodify.exe')) `
            -CertificateThumbprint $thumbprint -TestOnlyNoTimestamp -SignToolPath $SignToolPath
        $controller = Join-Path $stage 'config/controlscript.qs'
        [IO.File]::WriteAllText($controller, ("var signedInstallerBaseEnabled = true;`n" + [IO.File]::ReadAllText($controller)), (New-Object Text.UTF8Encoding $false))
        $installerDirectory = Join-Path $BuildDirectory 'installer'
        New-Item -ItemType Directory -Path $installerDirectory | Out-Null
        $setup = Join-Path $installerDirectory 'AgentNodify-TEST-ONLY-Setup.exe'
        & (Join-Path $IfwPath 'bin/binarycreator.exe') --offline-only -t (Join-Path $IfwPath 'bin/installerbase.exe') `
            -c (Join-Path $stage 'config/config.xml') -p (Join-Path $stage 'packages') $setup
        if ($LASTEXITCODE) { throw 'Test-only IFW binary creation failed.' }
        & "$PSScriptRoot/sign-windows.ps1" -Files $setup -CertificateThumbprint $thumbprint -TestOnlyNoTimestamp -SignToolPath $SignToolPath
    } else {
        & "$PSScriptRoot/package.ps1" -QtPath $QtPath -IfwPath $IfwPath -BuildDirectory $BuildDirectory `
            -SigningCertificateThumbprint $thumbprint -TestOnlyNoTimestamp -SignToolPath $SignToolPath -CMakeArguments $cmakeArguments
    }
    $installers = @(Get-ChildItem (Join-Path $BuildDirectory 'installer') -Filter '*-Setup.exe')
    if ($installers.Count -ne 1) { throw 'Expected exactly one self-signed installer.' }
    $report.phase = 'installation-roundtrip'; Save-TestReport
    & "$PSScriptRoot/test-installer.ps1" -Installer $installers[0].FullName `
        -WorkDirectory (Join-Path $BuildDirectory 'self-signed-roundtrip') -RequireSigned -ExpectedSignerThumbprint $thumbprint -EnvironmentKind $EnvironmentKind
    $report.passed = $true
} catch {
    $report.error = $_.Exception.Message
    throw
} finally {
    $cleanupErrors = @()
    $report.phase = 'cleaning-up-certificates'
    if (Test-Path -LiteralPath $BuildDirectory) { Save-TestReport }
    if ($thumbprint) {
        # Never remove pre-existing certificates: this thumbprint was just created.
        try { if (Test-Path "Cert:/CurrentUser/Root/$thumbprint") { Remove-Item "Cert:/CurrentUser/Root/$thumbprint" -ErrorAction Stop } } catch { $cleanupErrors += $_.Exception.Message }
        try { if (Test-Path "Cert:/CurrentUser/My/$thumbprint") { Remove-Item "Cert:/CurrentUser/My/$thumbprint" -DeleteKey -ErrorAction Stop } } catch { $cleanupErrors += $_.Exception.Message }
        $report.trustCleanupPassed = !(Test-Path "Cert:/CurrentUser/Root/$thumbprint") -and !(Test-Path "Cert:/CurrentUser/My/$thumbprint")
    }
    if ($cleanupErrors.Count) { $report.passed = $false; $report.cleanupErrors = $cleanupErrors }
    $report.phase = if ($report.passed -and $report.trustCleanupPassed) { 'completed' } else { 'failed' }
    if (Test-Path -LiteralPath $BuildDirectory) {
        Save-TestReport
    }
    if ($cleanupErrors.Count) { throw ('Test certificate cleanup failed: ' + ($cleanupErrors -join '; ')) }
}
