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
    $payload = Join-Path $WorkDirectory 'payload'
    $ifw = Join-Path $WorkDirectory 'ifw'
    foreach ($relative in @('config/config.xml', 'config/controlscript.qs', 'packages/org.agentnodify.application/data/bin/AgentNodify.exe', 'packages/org.agentnodify.application/meta/installscript.qs')) {
        $path = Join-Path $payload $relative
        New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
        [IO.File]::WriteAllText($path, 'fixture only')
    }
    New-Item -ItemType Directory -Path (Join-Path $ifw 'bin') | Out-Null
    foreach ($name in @('binarycreator.exe', 'installerbase.exe')) { [IO.File]::WriteAllText((Join-Path $ifw "bin/$name"), 'fixture only') }
    $prepare = Join-Path $SourceDirectory 'scripts/prepare-signing-sandbox.ps1'
    $parameters = @{ PreparedPackageDirectory = $payload; IfwPath = $ifw; SignToolPath = $tool }
    $planned = Join-Path $WorkDirectory 'planned'
    & $prepare @parameters -OutputDirectory $planned -WhatIf
    if (Test-Path $planned) { throw 'WhatIf must not create sandbox output.' }
    $sandbox = Join-Path $WorkDirectory 'sandbox'
    & $prepare @parameters -OutputDirectory $sandbox
    $refused = $false
    try { & $prepare @parameters -OutputDirectory $sandbox -WhatIf } catch { $refused = $true }
    if (!$refused) { throw 'Signing Sandbox must reject an existing output directory.' }
    [xml]$wsb = [IO.File]::ReadAllText((Join-Path $sandbox 'validate-signing.wsb'))
    $mappings = @($wsb.Configuration.MappedFolders.MappedFolder)
    if ($wsb.Configuration.Networking -ne 'Disable' -or $wsb.Configuration.ClipboardRedirection -ne 'Disable' -or $mappings.Count -ne 2 -or $mappings[0].ReadOnly -ne 'true' -or $mappings[1].ReadOnly -ne 'false') { throw 'Signing Sandbox isolation is incomplete.' }
    $manifest = Get-Content -LiteralPath (Join-Path $sandbox 'input/manifest.json') -Raw | ConvertFrom-Json
    foreach ($entry in $manifest.files) {
        if ((Get-FileHash -LiteralPath (Join-Path $sandbox "input/$($entry.path)") -Algorithm SHA256).Hash -ne $entry.sha256) { throw 'Signing manifest hash mismatch.' }
    }
    if (Test-Path (Join-Path $sandbox 'input/scripts/package.ps1')) { throw 'Guest must not need compiler/build tooling.' }
    $guest = [IO.File]::ReadAllText((Join-Path $sandbox 'input/scripts/run-signing-sandbox-guest.ps1'))
    if ($guest -notmatch '-RequireSigned -ExpectedSignerThumbprint' -or $guest -notmatch '-DeleteKey' -or $guest -notmatch 'finally\s*\{\s*\$cleanupErrors') { throw 'Guest signing and certificate cleanup guards are missing.' }
    if ($guest -match 'LocalMachine|test-self-signed-package|package\.ps1') { throw 'Self-signing must stay inside the standalone guest runner.' }
    # Missing transitive SDK assemblies must fail during preparation, before VM trust changes.
    [IO.File]::WriteAllText((Join-Path $WorkDirectory 'signtool.exe.manifest'), '<assembly><dependency><dependentAssembly><assemblyIdentity name="Missing.Runtime"/></dependentAssembly></dependency></assembly>')
    $refused = $false
    try { & $prepare @parameters -OutputDirectory (Join-Path $WorkDirectory 'missing-runtime') } catch { $refused = $_.Exception.Message -like '*Missing SDK assembly manifest*' }
    if (!$refused) { throw 'Missing SDK assembly dependency was accepted.' }
} finally {
    Remove-Variable signingTestCalls -Scope Global -ErrorAction SilentlyContinue
    Remove-Variable signingTestCertificate -Scope Global -ErrorAction SilentlyContinue
}
