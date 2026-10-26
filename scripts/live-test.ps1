param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Codex', 'Claude Code', 'OpenCode')][string]$Agent,
    [ValidateSet('text', 'json', 'loop', 'and')][string]$Case = 'text',
    [string]$Program,
    [string]$QtPath = 'C:/Qt/6.12.0/msvc2022_64',
    [string]$CodexCaCertificate,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Run,
    [switch]$Login
)
# No host-execution fallback. Without -Run/-Login this only checks local setup.
$ErrorActionPreference = 'Stop'
if ($Run -and $Login) { throw 'Choose either -Run or -Login.' }
$root = Split-Path $PSScriptRoot -Parent
$profile = Join-Path $root 'build/live-profile'
$homePath = [Environment]::GetEnvironmentVariable('USERPROFILE')
if (!$Program) {
    $Program = switch ($Agent) {
        'Codex' { "$homePath/AppData/Local/Programs/OpenAI/Codex/bin/codex.exe" }
        'Claude Code' { "$homePath/.local/bin/claude.exe" }
        'OpenCode' { "$homePath/AppData/Local/Programs/@opencode-aidesktop/resources/opencode-cli.exe" }
    }
}
$Program = [IO.Path]::GetFullPath($Program)
if ([IO.Path]::GetExtension($Program) -ne '.exe' -or !(Test-Path -LiteralPath $Program -PathType Leaf)) {
    throw 'Specify an existing native CLI .exe with -Program.'
}
$runner = Join-Path $root "build/$Configuration/agentnodify_live_test.exe"
if ($Run -and !(Test-Path -LiteralPath $runner -PathType Leaf)) { throw "Build first: $runner" }
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
    PATH = "$QtPath/bin;$env:PATH"
}
if ($Agent -eq 'Codex') {
    # A CA override makes Codex use rustls instead of Windows native TLS.
    # Keep certificate verification enabled and respect existing CA overrides.
    if (!$CodexCaCertificate -and !$env:CODEX_CA_CERTIFICATE -and !$env:SSL_CERT_FILE) {
        foreach ($candidate in @(
            "$homePath/AppData/Local/Programs/Git/mingw64/ssl/certs/ca-bundle.crt",
            "$env:ProgramFiles/Git/mingw64/ssl/certs/ca-bundle.crt"
        )) {
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $CodexCaCertificate = $candidate; break }
        }
    }
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
    } elseif ($Run) {
        $model = switch ($Agent) {
            'Codex' { 'gpt-6-luna' }
            'Claude Code' { 'claude-haiku-5-5' }
            'OpenCode' { 'openai/gpt-6-luna' }
        }
        $slug = switch ($Agent) { 'Codex' { 'codex' }; 'Claude Code' { 'claude' }; 'OpenCode' { 'opencode' } }
        & $runner --agent $Agent --program $Program --model $model --case $Case --report "$root/build/$slug-$Case-sandbox-live.json"
    } else {
        & $Program --version
    }
    if ($LASTEXITCODE -ne 0) { throw "CLI/test failed (exit $LASTEXITCODE). No outside-sandbox retry was attempted." }
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
