[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [string]$WorkDirectory = (Join-Path 'build' ('installer-roundtrip-' + [guid]::NewGuid().ToString('N'))),
    [switch]$RequireSigned,
    [ValidatePattern('^[a-fA-F0-9]{40}$')][string]$ExpectedSignerThumbprint,
    [ValidateSet('Host', 'CIRunner', 'WindowsSandbox')][string]$EnvironmentKind = 'Host'
)
$ErrorActionPreference = 'Stop'
$Installer = (Resolve-Path -LiteralPath $Installer).Path
if ([IO.Path]::GetExtension($Installer) -ne '.exe') { throw 'Installer must be a Windows executable.' }
if (![IO.Path]::IsPathRooted($WorkDirectory)) { $WorkDirectory = Join-Path (Get-Location) $WorkDirectory }
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $WorkDirectory) { throw 'WorkDirectory must not exist; existing data is never reused or deleted.' }
if (!$PSCmdlet.ShouldProcess($WorkDirectory, 'Install, smoke-test and uninstall with an isolated product UUID and shortcuts')) { return }
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
# Windows PowerShell 5.1 can decode BOM-less scripts using the system ANSI page.
# Construct the Japanese path independently of that page (Japanese + space + path).
$testDirectoryLeaf = -join @([char]0x65e5, [char]0x672c, [char]0x8a9e, [char]0x0020, [char]0x30d1, [char]0x30b9)
$target = Join-Path (Join-Path $WorkDirectory $testDirectoryLeaf) 'AgentNodify'
$cache = Join-Path $WorkDirectory 'cache'
$shortcuts = Join-Path $WorkDirectory 'shortcuts'
$productId = '{' + [guid]::NewGuid().ToString() + '}'
$productName = 'AgentNodify Validation ' + $productId
$menuName = 'Validation'
$maintenance = Join-Path $target 'MaintenanceTool.exe'
$reportPath = Join-Path $WorkDirectory 'result.json'
$report = [ordered]@{
    schema = 1; passed = $false; installerSha256 = (Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash.ToLowerInvariant()
    productId = $productId; checks = @(); signatures = @(); cleanupAttempted = $false
    environment = $EnvironmentKind; signatureRequired = [bool]($RequireSigned -or $ExpectedSignerThumbprint)
    expectedSignerThumbprint = $ExpectedSignerThumbprint
    testDirectoryLeaf = $testDirectoryLeaf; ansiCodePage = [Text.Encoding]::Default.CodePage
}
$uninstalled = $false

function Save-Report {
    [IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 8), (New-Object Text.UTF8Encoding $false))
}
function Assert-Check([bool]$Condition, [string]$Name) {
    if (!$Condition) { throw "Installer check failed: $Name" }
    $report.checks += $Name
}
function Quote-Argument([string]$Value) {
    $value = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $value = [regex]::Replace($value, '(\\+)$', '$1$1')
    return '"' + $value + '"'
}
function Invoke-TestProcess([string]$File, [string[]]$Arguments, [string]$LogName) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $File
    $info.Arguments = (($Arguments | ForEach-Object { Quote-Argument $_ }) -join ' ')
    $info.WorkingDirectory = $WorkDirectory
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.EnvironmentVariables['PATH'] = "$env:SystemRoot/System32;$env:SystemRoot"
    $info.EnvironmentVariables.Remove('QT_PLUGIN_PATH')
    $info.EnvironmentVariables.Remove('QT_QPA_PLATFORM_PLUGIN_PATH')
    $info.EnvironmentVariables['QT_QPA_PLATFORM'] = 'windows'
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    try {
        if (!$process.Start()) { throw "Cannot start test process: $LogName" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(180000)) {
            # Only the exact process started by this test, never other app instances.
            $process.Kill()
            $process.WaitForExit()
            throw "Test process timed out: $LogName"
        }
        $process.WaitForExit()
        $log = $stdout.Result + "`n" + $stderr.Result
        [IO.File]::WriteAllText((Join-Path $WorkDirectory "$LogName.log"), $log, (New-Object Text.UTF8Encoding $false))
        if ($process.ExitCode -ne 0) { throw "$LogName failed ($($process.ExitCode)); see $LogName.log" }
    } finally {
        $process.Dispose()
    }
}
function Get-Registration {
    foreach ($view in @([Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32)) {
        $hive = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, $view)
        try {
            $key = $hive.OpenSubKey("Software\Microsoft\Windows\CurrentVersion\Uninstall\$productId")
            if ($key) {
                try {
                    return @{ name = $key.GetValue('DisplayName'); location = $key.GetValue('InstallLocation');
                        command = $key.GetValue('UninstallString') }
                } finally { $key.Dispose() }
            }
        } finally { $hive.Dispose() }
    }
    return $null
}
function Check-Signature([string]$File, [string]$Name) {
    $signature = Get-AuthenticodeSignature -LiteralPath $File
    $thumbprint = if ($signature.SignerCertificate) { $signature.SignerCertificate.Thumbprint } else { $null }
    $selfSigned = [bool]($signature.SignerCertificate -and $signature.SignerCertificate.Subject -eq $signature.SignerCertificate.Issuer)
    $report.signatures += @{ name = $Name; status = $signature.Status.ToString(); thumbprint = $thumbprint; selfSigned = $selfSigned }
    if ($RequireSigned -or $ExpectedSignerThumbprint) {
        Assert-Check ($signature.Status -eq 'Valid') "$Name signature valid"
        if ($ExpectedSignerThumbprint) {
            Assert-Check ($thumbprint -eq $ExpectedSignerThumbprint) "$Name signer matches requested identity"
        }
    }
}
function Remove-TestInstallation {
    Invoke-TestProcess $maintenance @('--no-proxy', '--cache-path', $cache,
        '--confirm-command', '--default-answer', 'purge') 'uninstall'
    $script:uninstalled = $true
}

try {
    Assert-Check ($null -eq (Get-Registration)) 'validation UUID is unused'
    Check-Signature $Installer 'installer'
    Invoke-TestProcess $Installer @('--no-proxy', '--cache-path', $cache, '--root', $target,
        '--confirm-command', '--accept-licenses', '--default-answer', 'install',
        "ProductUUID=$productId", "ProductName=$productName", "UserStartMenuProgramsPath=$shortcuts",
        "StartMenuDir=$menuName", 'AllUsers=false') 'install'
    foreach ($file in @('bin/AgentNodify.exe', 'bin/Qt6Core.dll', 'bin/Qt6Gui.dll', 'bin/Qt6Widgets.dll',
        'plugins/platforms/qwindows.dll', 'LICENSE', 'licenses/Qt/LGPL-3.0-only.txt',
        'licenses/IFW/LICENSE.GPL3-EXCEPT', 'MaintenanceTool.exe')) {
        Assert-Check (Test-Path -LiteralPath (Join-Path $target $file) -PathType Leaf) "installed $file"
    }
    $registration = Get-Registration
    Assert-Check ($null -ne $registration) 'uninstall registration exists'
    Assert-Check ($registration.name -eq $productName) 'uninstall registration has isolated name'
    Assert-Check ([IO.Path]::GetFullPath($registration.location) -eq [IO.Path]::GetFullPath($target)) 'uninstall location matches test directory'
    Assert-Check ($registration.command -eq ('"' + [IO.Path]::GetFullPath($maintenance) + '"')) 'uninstall command points to test MaintenanceTool'
    foreach ($link in @('AgentNodify.lnk', 'AgentNodify Maintenance.lnk')) {
        Assert-Check (Test-Path -LiteralPath (Join-Path $shortcuts "$menuName/$link")) "isolated shortcut $link"
    }
    Check-Signature (Join-Path $target 'bin/AgentNodify.exe') 'application'
    Check-Signature $maintenance 'maintenance'
    Invoke-TestProcess (Join-Path $target 'bin/AgentNodify.exe') @('--smoke-test') 'application-smoke'
    $report.checks += 'installed application starts without developer PATH or external CLI calls'
    $sentinels = @((Join-Path $target 'user-owned.txt'), (Join-Path $target 'bin/user-owned.keep'),
        (Join-Path $WorkDirectory 'external-project/flow.flow.json'), (Join-Path $WorkDirectory 'user-data/workspace.json'))
    foreach ($file in $sentinels) {
        New-Item -ItemType Directory -Path (Split-Path $file -Parent) -Force | Out-Null
        [IO.File]::WriteAllText($file, 'AgentNodify installer validation: preserve this fixture')
    }
    $hashes = @{}
    foreach ($file in $sentinels) { $hashes[$file] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash }
    Remove-TestInstallation
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while ((Test-Path -LiteralPath $maintenance) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    foreach ($file in @('bin/AgentNodify.exe', 'bin/Qt6Core.dll', 'MaintenanceTool.exe', 'maintenance/installerbase.exe')) {
        Assert-Check (!(Test-Path -LiteralPath (Join-Path $target $file))) "removed $file"
    }
    Assert-Check ($null -eq (Get-Registration)) 'uninstall registration removed'
    foreach ($link in @('AgentNodify.lnk', 'AgentNodify Maintenance.lnk')) {
        Assert-Check (!(Test-Path -LiteralPath (Join-Path $shortcuts "$menuName/$link"))) "removed shortcut $link"
    }
    foreach ($file in $sentinels) {
        Assert-Check ((Test-Path -LiteralPath $file) -and (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -eq $hashes[$file]) 'user-owned fixture preserved'
    }
    $report.passed = $true
} catch {
    $report.error = $_.Exception.Message
    throw
} finally {
    # Only this test's known installation is eligible for cleanup. No recursive deletes.
    if (!$uninstalled -and (Test-Path -LiteralPath $maintenance)) {
        $report.cleanupAttempted = $true
        try { Remove-TestInstallation } catch { $report.cleanupError = $_.Exception.Message }
    }
    Save-Report
    Write-Output "Installer validation report: $reportPath"
}
