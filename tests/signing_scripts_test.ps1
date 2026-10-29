param([string]$SourceDirectory, [string]$WorkDirectory)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$file = Join-Path $WorkDirectory 'app.exe'
$tool = Join-Path $WorkDirectory 'fake-signtool.ps1'
[IO.File]::WriteAllText($file, 'fixture; never executed')
[IO.File]::WriteAllText($tool, '$global:signingTestCalls += ,@($args); $global:LASTEXITCODE = 0')
$global:signingTestCertificate = [pscustomobject]@{ HasPrivateKey = $true; Subject = 'CN=Production Publisher'; Issuer = 'CN=CA'; NotBefore = (Get-Date).AddDays(-1); NotAfter = (Get-Date).AddDays(1) }
function Get-Item { param([string]$LiteralPath) if ($LiteralPath -like 'Cert:*') { return $global:signingTestCertificate }; Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath }
function Get-AuthenticodeSignature { param([string]$LiteralPath) return [pscustomobject]@{ SignerCertificate = $null } }
$global:signingTestCalls = @()
try {
    $parameters = @{ Files = $file; CertificateThumbprint = ('a' * 40); SignToolPath = $tool }
    $signer = Join-Path $SourceDirectory 'scripts/sign-windows.ps1'
    $refused = $false
    try { & $signer @parameters } catch { $refused = $true }
    if (!$refused -or $global:signingTestCalls.Count) { throw 'Production signing must require a timestamp.' }
    & $signer @parameters -TimestampUrl https://timestamp.example.invalid
    if ($global:signingTestCalls.Count -ne 2 -or $global:signingTestCalls[0] -notcontains '/tr' -or $global:signingTestCalls[1] -notcontains '/pa') { throw 'Production signing/verification arguments are incomplete.' }
    $global:signingTestCalls = @()
    $refused = $false
    try { & $signer @parameters -TestOnlyNoTimestamp } catch { $refused = $true }
    if (!$refused) { throw 'A production certificate cannot use test-only signing.' }
    $global:signingTestCertificate.Subject = 'CN=AgentNodify TEST ONLY fixture'
    $global:signingTestCertificate.Issuer = $global:signingTestCertificate.Subject
    $refused = $false
    try { & $signer @parameters -TimestampUrl https://timestamp.example.invalid } catch { $refused = $true }
    if (!$refused) { throw 'Self-signed production signing must fail.' }
    & $signer @parameters -TestOnlyNoTimestamp
    if ($global:signingTestCalls.Count -ne 2 -or $global:signingTestCalls[0] -contains '/tr' -or $global:signingTestCalls[1] -notcontains '/pa') { throw 'Test-only signing must skip timestamping, not verification.' }
    & (Join-Path $SourceDirectory 'scripts/test-self-signed-package.ps1') -QtPath fixture -IfwPath fixture `
        -BuildDirectory (Join-Path $WorkDirectory 'planned') -SignToolPath $tool -AllowTemporaryUserTrust -WhatIf
    if (Test-Path (Join-Path $WorkDirectory 'planned')) { throw 'WhatIf must not create certificates or build output.' }
    $payload = Join-Path $WorkDirectory 'payload'
    $ifw = Join-Path $WorkDirectory 'ifw'
    foreach ($relative in @('config/config.xml', 'config/controlscript.qs', 'packages/org.agentnodify.application/data/bin/AgentNodify.exe', 'packages/org.agentnodify.application/meta/installscript.qs')) {
        $path = Join-Path $payload $relative
        New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
        [IO.File]::WriteAllText($path, 'fixture only')
    }
    New-Item -ItemType Directory -Path (Join-Path $ifw 'bin') | Out-Null
    foreach ($name in @('binarycreator.exe', 'installerbase.exe')) { [IO.File]::WriteAllText((Join-Path $ifw "bin/$name"), 'fixture only') }
    $sandbox = Join-Path $WorkDirectory 'sandbox'
    & (Join-Path $SourceDirectory 'scripts/prepare-signing-sandbox.ps1') -PreparedPackageDirectory $payload -IfwPath $ifw -SignToolPath $tool -OutputDirectory $sandbox
    [xml]$wsb = [IO.File]::ReadAllText((Join-Path $sandbox 'validate-signing.wsb'))
    $mappings = @($wsb.Configuration.MappedFolders.MappedFolder)
    if ($wsb.Configuration.Networking -ne 'Disable' -or $wsb.Configuration.ClipboardRedirection -ne 'Disable' -or $mappings.Count -ne 2 -or $mappings[0].ReadOnly -ne 'true' -or $mappings[1].ReadOnly -ne 'false') { throw 'Signing Sandbox isolation is incomplete.' }
    $manifest = Get-Content -LiteralPath (Join-Path $sandbox 'input/manifest.json') -Raw | ConvertFrom-Json
    foreach ($entry in $manifest.files) {
        if ((Get-FileHash -LiteralPath (Join-Path $sandbox "input/$($entry.path)") -Algorithm SHA256).Hash -ne $entry.sha256) { throw 'Signing manifest hash mismatch.' }
    }
    if (Test-Path (Join-Path $sandbox 'input/scripts/package.ps1')) { throw 'Guest must not need compiler/build tooling.' }
} finally {
    Remove-Variable signingTestCalls -Scope Global -ErrorAction SilentlyContinue
    Remove-Variable signingTestCertificate -Scope Global -ErrorAction SilentlyContinue
}
