param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Codex', 'Claude Code', 'OpenCode')][string]$Agent,
    [ValidateSet('text', 'json', 'loop', 'and', 'file', 'cwd')][string]$Case = 'text',
    [string]$ProjectDirectory,
    [string]$Program,
    [string]$QtPath = $env:Qt6_ROOT,
    [string]$Model,
    [string]$CodexCaCertificate,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Run,
    [switch]$Login,
    [switch]$Models,
    [switch]$WorkspaceWrite
)
# No host-execution fallback. Without -Run/-Login/-Models, only check the version.
$ErrorActionPreference = 'Stop'
if (([int]$Run.IsPresent + [int]$Login.IsPresent + [int]$Models.IsPresent) -gt 1) { throw 'Choose only one of -Run, -Login, or -Models.' }
$root = Split-Path $PSScriptRoot -Parent
$profile = Join-Path $root 'build/live-profile'
if (!$Program) {
    $command = switch ($Agent) {
        'Codex' { 'codex.exe' }
        'Claude Code' { 'claude.exe' }
        'OpenCode' { 'opencode-cli.exe' }
    }
    $resolved = Get-Command $command -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$resolved) { throw 'CLI not found on PATH. Specify a native CLI .exe with -Program.' }
    $Program = $resolved.Source
}
$Program = [IO.Path]::GetFullPath($Program)
if ([IO.Path]::GetExtension($Program) -ne '.exe' -or !(Test-Path -LiteralPath $Program -PathType Leaf)) {
    throw 'Specify an existing native CLI .exe with -Program.'
}
$runner = Join-Path $root "build/$Configuration/agentnodify_live_test.exe"
if ($Run -and !(Test-Path -LiteralPath $runner -PathType Leaf)) { throw "Build first: $runner" }
$catalogProbe = Join-Path $root "build/$Configuration/agentnodify_catalog_probe.exe"
if ($Models -and !(Test-Path -LiteralPath $catalogProbe -PathType Leaf)) { throw "Build first: $catalogProbe" }
if ($Run -and !$Model) { throw 'Specify an authorized model ID with -Model for paid tests.' }
if ($QtPath) {
    $QtPath = (Resolve-Path -LiteralPath $QtPath).Path
    if (!(Test-Path -LiteralPath "$QtPath/bin" -PathType Container)) { throw 'QtPath must point to a Qt kit directory.' }
}
$variables = @{
    HOME = $profile
    USERPROFILE = $profile
    APPDATA = "$profile/AppData/Roaming"
    LOCALAPPDATA = "$profile/AppData/Local"
    CODEX_HOME = "$profile/.codex"
    CLAUDE_CONFIG_DIR = "$profile/.claude"
    XDG_CONFIG_HOME = "$profile/.config"
    XDG_DATA_HOME = "$profile/.local/share"
    XDG_CACHE_HOME = "$profile/.cache"
    XDG_STATE_HOME = "$profile/.local/state"
}
if ($QtPath) { $variables['PATH'] = "$QtPath/bin;$env:PATH" }
if ($Agent -eq 'Codex') {
    # Only use an explicitly supplied CA bundle; never disable verification.
    if ($CodexCaCertificate) {
        if (!(Test-Path -LiteralPath $CodexCaCertificate -PathType Leaf)) { throw 'Codex CA bundle does not exist.' }
        $variables['CODEX_CA_CERTIFICATE'] = [IO.Path]::GetFullPath($CodexCaCertificate)
    }
}
$saved = @{}
try {
    foreach ($name in $variables.Keys) {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        if ($name -notin @('PATH', 'CODEX_CA_CERTIFICATE')) { New-Item -ItemType Directory -Force -Path $variables[$name] | Out-Null }
        [Environment]::SetEnvironmentVariable($name, $variables[$name], 'Process')
    }
    Write-Host "CLI: $Program"
    Write-Host "Isolated profile: $profile"
    if ($Login) {
        # Run interactively in a user terminal, not via safe-shell (stdin is closed).
        switch ($Agent) {
            'Codex' { & $Program login }
            'Claude Code' { & $Program auth login }
            'OpenCode' { & $Program auth login }
        }
    } elseif ($Models) {
        $slug = switch ($Agent) { 'Codex' { 'codex' }; 'Claude Code' { 'claude' }; 'OpenCode' { 'opencode' } }
        & $catalogProbe --agent $Agent --program $Program --report "$root/build/$slug-models-sandbox.json"
    } elseif ($Run) {
        $slug = switch ($Agent) { 'Codex' { 'codex' }; 'Claude Code' { 'claude' }; 'OpenCode' { 'opencode' } }
        $extra = @(); if ($WorkspaceWrite) { $extra += '--workspace-write' }
        if ($ProjectDirectory) { $extra += @('--project', [IO.Path]::GetFullPath($ProjectDirectory)) }
        & $runner --agent $Agent --program $Program --model $Model --case $Case --report "$root/build/$slug-$Case-sandbox-live.json" @extra
    } else {
        & $Program --version
    }
    if ($LASTEXITCODE -ne 0) { throw "CLI/test failed (exit $LASTEXITCODE). No outside-sandbox retry was attempted." }
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
