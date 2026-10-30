param(
    [switch]$RequireSigned,
    [ValidatePattern('^[a-fA-F0-9]{40}$')][string]$ExpectedSignerThumbprint,
    [string]$InputDirectory = 'C:\AgentNodifyValidation\Input',
    [string]$OutputDirectory = 'C:\AgentNodifyValidation\Output\run'
)
$ErrorActionPreference = 'Stop'
foreach ($name in @('Setup.exe', 'test-installer.ps1')) {
    if (!(Test-Path -LiteralPath (Join-Path $inputDirectory $name) -PathType Leaf)) { throw "Missing sandbox input: $name" }
}
if (Test-Path -LiteralPath $outputDirectory) { throw 'Sandbox output already exists; prepare a fresh sandbox run.' }
New-Item -ItemType Directory -Path $outputDirectory | Out-Null
$localRoot = Join-Path $env:LOCALAPPDATA ('AgentNodifyValidation-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $localRoot 'source'
$work = Join-Path $localRoot 'work'
$state = [ordered]@{ schema = 1; phase = 'starting'; workStorage = 'GuestLocalAppData'; passed = $false }
$encoding = New-Object Text.UTF8Encoding $false
function Save-State { [IO.File]::WriteAllText((Join-Path $outputDirectory 'guest-status.json'), ($state | ConvertTo-Json), $encoding) }
Save-State
try {
    # Install and self-uninstall on the guest's local filesystem, not a mapped share.
    # Only reports cross the writable host mapping. No host project/home is shared.
    New-Item -ItemType Directory -Path $source | Out-Null
    foreach ($name in @('Setup.exe', 'test-installer.ps1')) {
        Copy-Item -LiteralPath (Join-Path $inputDirectory $name) -Destination (Join-Path $source $name)
        if ((Get-FileHash -LiteralPath (Join-Path $inputDirectory $name) -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $source $name) -Algorithm SHA256).Hash) { throw "Sandbox input copy mismatch: $name" }
    }
    $arguments = @{ Installer = (Join-Path $source 'Setup.exe'); EnvironmentKind = 'WindowsSandbox' }
    if ($RequireSigned) { $arguments.RequireSigned = $true }
    if ($ExpectedSignerThumbprint) { $arguments.ExpectedSignerThumbprint = $ExpectedSignerThumbprint }
    $state.phase = 'testing'; Save-State
    foreach ($mode in @('Retain', 'Delete')) {
        $arguments.WorkDirectory = Join-Path $work $mode
        $arguments.AppDataMode = $mode
        & (Join-Path $source 'test-installer.ps1') @arguments
        $result = Get-Content -LiteralPath (Join-Path $arguments.WorkDirectory 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        if (!$result.passed) { throw "Sandbox installation roundtrip failed: $mode" }
    }
    $state.passed = $true
    $state.phase = 'completed'
} catch {
    $state.phase = 'failed'
    $state.error = $_.Exception.Message
    throw
} finally {
    foreach ($mode in @('Retain', 'Delete')) {
        $destination = Join-Path $outputDirectory $mode
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        foreach ($name in @('result.json', 'install.log', 'application-smoke.log', 'uninstall.log')) {
            $file = Join-Path (Join-Path $work $mode) $name
            if (Test-Path -LiteralPath $file -PathType Leaf) { Copy-Item -LiteralPath $file -Destination (Join-Path $destination $name) }
        }
    }
    Save-State
}
