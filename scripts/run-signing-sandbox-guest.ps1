$ErrorActionPreference = 'Stop'
$inputDirectory = 'C:\AgentNodifySigning\Input'
$output = 'C:\AgentNodifySigning\Output'
if (Test-Path -LiteralPath (Join-Path $output 'guest-status.json')) { throw 'Use a fresh signing Sandbox output directory.' }
$local = Join-Path $env:LOCALAPPDATA ('AgentNodifySigning-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $local 'source'
$build = Join-Path $local 'test'
$thumbprint = $null
$state = [ordered]@{ schema = 1; testOnly = $true; environment = 'WindowsSandbox'; phase = 'starting'; passed = $false; hostTrustModified = $false }
$report = [ordered]@{ schema = 1; testOnly = $true; environment = 'WindowsSandbox'; passed = $false; trustCleanupPassed = $false; phase = 'preflight' }
function Save-State { [IO.File]::WriteAllText((Join-Path $output 'guest-status.json'), ($state | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding $false)) }
function Save-SigningReport { [IO.File]::WriteAllText((Join-Path $output 'self-signing-test.json'), ($report | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding $false)) }
Save-State
Start-Transcript -LiteralPath (Join-Path $output 'signing.log') | Out-Null
try {
    New-Item -ItemType Directory -Path $source | Out-Null
    Copy-Item -Path "$inputDirectory/*" -Destination $source -Recurse
    $manifest = Get-Content -LiteralPath (Join-Path $source 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($file in $manifest.files) {
        $path = [IO.Path]::GetFullPath((Join-Path $source $file.path))
        if (!$path.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid manifest path.' }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) { throw "Input hash mismatch: $($file.path)" }
    }
    $state.inputsVerified = $true; $state.phase = 'testing'; Save-State
    New-Item -ItemType Directory -Path $build | Out-Null
    $signTool = Join-Path $source 'sdk/signtool.exe'
    $ifw = Join-Path $source 'ifw/bin'
    Save-SigningReport
    # Validate the portable SDK runtime before creating a certificate or trust entry.
    $preflight = New-Object Diagnostics.Process
    $preflight.StartInfo = New-Object Diagnostics.ProcessStartInfo
    $preflight.StartInfo.FileName = $signTool
    $preflight.StartInfo.Arguments = '/?'
    $preflight.StartInfo.UseShellExecute = $false
    $preflight.StartInfo.RedirectStandardOutput = $true
    $preflight.StartInfo.RedirectStandardError = $true
    try {
        if (!$preflight.Start()) { throw 'Cannot start SignTool preflight.' }
        $stdout = $preflight.StandardOutput.ReadToEndAsync()
        $stderr = $preflight.StandardError.ReadToEndAsync()
        if (!$preflight.WaitForExit(10000)) { $preflight.Kill(); $preflight.WaitForExit(); throw 'SignTool preflight timed out.' }
        [IO.File]::WriteAllText((Join-Path $output 'signtool-preflight.log'), ($stdout.Result + $stderr.Result))
        if ($preflight.ExitCode) { throw 'SignTool preflight failed; no test certificate was created.' }
    } finally { $preflight.Dispose() }

    $report.phase = 'creating-certificate'; Save-SigningReport
    $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject ('CN=AgentNodify TEST ONLY ' + [guid]::NewGuid().ToString('N')) `
        -CertStoreLocation Cert:/CurrentUser/My -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 `
        -KeyExportPolicy NonExportable -NotAfter (Get-Date).AddDays(7)
    $thumbprint = $certificate.Thumbprint
    $report.thumbprint = $thumbprint
    $publicCertificate = Join-Path $build 'test-signing.cer'
    Export-Certificate -Cert $certificate -FilePath $publicCertificate | Out-Null
    $report.phase = 'trusting-guest-certificate'; Save-SigningReport
    Import-Certificate -FilePath $publicCertificate -CertStoreLocation Cert:/CurrentUser/Root | Out-Null

    $report.phase = 'signing-and-packaging'; Save-SigningReport
    $stage = Join-Path $source 'payload'
    $data = Join-Path $stage 'packages/org.agentnodify.application/data'
    $maintenanceDirectory = Join-Path $data 'maintenance'
    New-Item -ItemType Directory -Path $maintenanceDirectory -Force | Out-Null
    $base = Join-Path $maintenanceDirectory 'installerbase.exe'
    Copy-Item -LiteralPath (Join-Path $ifw 'installerbase.exe') -Destination $base
    $signing = @{ CertificateThumbprint = $thumbprint; TestOnlyNoTimestamp = $true; SignToolPath = $signTool }
    & (Join-Path $source 'scripts/sign-windows.ps1') -Files @($base, (Join-Path $data 'bin/AgentNodify.exe')) @signing
    $controller = Join-Path $stage 'config/controlscript.qs'
    [IO.File]::WriteAllText($controller, ("var signedInstallerBaseEnabled = true;`n" + [IO.File]::ReadAllText($controller)), (New-Object Text.UTF8Encoding $false))
    $setup = Join-Path $build 'AgentNodify-TEST-ONLY-Setup.exe'
    & (Join-Path $ifw 'binarycreator.exe') --offline-only -t (Join-Path $ifw 'installerbase.exe') `
        -c (Join-Path $stage 'config/config.xml') -p (Join-Path $stage 'packages') $setup
    if ($LASTEXITCODE) { throw 'Test-only IFW binary creation failed.' }
    & (Join-Path $source 'scripts/sign-windows.ps1') -Files $setup @signing
    $report.phase = 'installation-roundtrip'; Save-SigningReport
    & (Join-Path $source 'scripts/test-installer.ps1') -Installer $setup -WorkDirectory (Join-Path $build 'self-signed-roundtrip') `
        -RequireSigned -ExpectedSignerThumbprint $thumbprint -EnvironmentKind WindowsSandbox
    $report.passed = $true
} catch {
    $report.error = $_.Exception.Message
    throw
} finally {
    $cleanupErrors = @()
    $report.phase = 'cleaning-up-certificates'
    try { Save-SigningReport } catch { $cleanupErrors += $_.Exception.Message }
    if ($thumbprint) {
        # Delete only the certificate and private key created by this guest run.
        try { if (Test-Path "Cert:/CurrentUser/Root/$thumbprint") { Remove-Item "Cert:/CurrentUser/Root/$thumbprint" -ErrorAction Stop } } catch { $cleanupErrors += $_.Exception.Message }
        try { if (Test-Path "Cert:/CurrentUser/My/$thumbprint") { Remove-Item "Cert:/CurrentUser/My/$thumbprint" -DeleteKey -ErrorAction Stop } } catch { $cleanupErrors += $_.Exception.Message }
        $report.trustCleanupPassed = !(Test-Path "Cert:/CurrentUser/Root/$thumbprint") -and !(Test-Path "Cert:/CurrentUser/My/$thumbprint")
    }
    if ($cleanupErrors.Count) { $report.passed = $false; $report.cleanupErrors = $cleanupErrors }
    $state.passed = $report.passed -and $report.trustCleanupPassed
    $state.phase = if ($state.passed) { 'completed' } else { 'failed' }
    $report.phase = $state.phase
    if ($report.error) { $state.error = $report.error }
    Save-SigningReport
    # Export evidence only: no private key, certificate store or signed binaries.
    foreach ($name in @('result.json', 'install.log', 'uninstall.log', 'application-smoke.log')) {
        $file = Join-Path $build "self-signed-roundtrip/$name"
        if (Test-Path -LiteralPath $file -PathType Leaf) { Copy-Item -LiteralPath $file -Destination (Join-Path $output $name) }
    }
    Save-State
    Stop-Transcript | Out-Null
    if ($cleanupErrors.Count) { throw ('Test certificate cleanup failed: ' + ($cleanupErrors -join '; ')) }
}
