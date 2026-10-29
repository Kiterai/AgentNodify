$ErrorActionPreference = 'Stop'
$inputDirectory = 'C:\AgentNodifySigning\Input'
$output = 'C:\AgentNodifySigning\Output'
if (Test-Path -LiteralPath (Join-Path $output 'guest-status.json')) { throw 'Use a fresh signing Sandbox output directory.' }
$local = Join-Path $env:LOCALAPPDATA ('AgentNodifySigning-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $local 'source'
$build = Join-Path $local 'test'
$state = [ordered]@{ schema = 1; testOnly = $true; environment = 'WindowsSandbox'; phase = 'starting'; passed = $false; hostTrustModified = $false }
function Save-State { [IO.File]::WriteAllText((Join-Path $output 'guest-status.json'), ($state | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding $false)) }
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
    & (Join-Path $source 'scripts/test-self-signed-package.ps1') -PreparedPackageDirectory (Join-Path $source 'payload') `
        -IfwPath (Join-Path $source 'ifw') -BuildDirectory $build -SignToolPath (Join-Path $source 'sdk/signtool.exe') `
        -AllowTemporaryUserTrust -EnvironmentKind WindowsSandbox -ProgressReportPath (Join-Path $output 'self-signing-test.json')
    $report = Get-Content -LiteralPath (Join-Path $build 'self-signing-test.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if (!$report.passed -or !$report.trustCleanupPassed) { throw 'Signing or temporary guest certificate cleanup failed.' }
    $state.passed = $true; $state.phase = 'completed'
} catch {
    $state.phase = 'failed'; $state.error = $_.Exception.Message
    throw
} finally {
    # Export evidence only: no private key, certificate store or signed binaries.
    foreach ($relative in @('self-signing-test.json', 'self-signed-roundtrip/result.json', 'self-signed-roundtrip/install.log', 'self-signed-roundtrip/uninstall.log', 'self-signed-roundtrip/application-smoke.log')) {
        $file = Join-Path $build $relative
        if (Test-Path -LiteralPath $file -PathType Leaf) { Copy-Item -LiteralPath $file -Destination (Join-Path $output (Split-Path $relative -Leaf)) }
    }
    Save-State
    Stop-Transcript | Out-Null
}
