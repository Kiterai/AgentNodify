param(
    [Parameter(Mandatory = $true)][string[]]$Files,
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-fA-F0-9]{40}$')][string]$CertificateThumbprint,
    [uri]$TimestampUrl,
    [switch]$TestOnlyNoTimestamp,
    [string]$SignToolPath = $env:SIGNTOOL_PATH
)
$ErrorActionPreference = 'Stop'
if ($TestOnlyNoTimestamp -and $TimestampUrl) { throw 'Test-only signing must not use a timestamp service.' }
if (!$TestOnlyNoTimestamp -and (!$TimestampUrl -or $TimestampUrl.Scheme -ne 'https' -or !$TimestampUrl.Host -or $TimestampUrl.UserInfo)) {
    throw 'TimestampUrl must be an HTTPS timestamp service without credentials.'
}
$certificate = Get-Item -LiteralPath "Cert:/CurrentUser/My/$CertificateThumbprint" -ErrorAction Stop
if (!$certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)) {
    throw 'Signing requires a currently valid certificate with an accessible private key.'
}
if ($TestOnlyNoTimestamp) {
    if ($certificate.Subject -notlike 'CN=AgentNodify TEST ONLY *' -or $certificate.Subject -ne $certificate.Issuer) {
        throw 'Test-only signing requires a dedicated AgentNodify TEST ONLY self-signed certificate.'
    }
} elseif ($certificate.Subject -eq $certificate.Issuer) {
    throw 'Self-signed certificates are allowed only with -TestOnlyNoTimestamp, never for production signing.'
}
if (!$SignToolPath) {
    $SignToolPath = (Get-Command signtool.exe -CommandType Application -ErrorAction Stop).Source
}
$SignToolPath = (Resolve-Path -LiteralPath $SignToolPath).Path
$targets = @($Files | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
# Validate every target before signing any of them. Never re-sign vendor DLLs.
foreach ($file in $targets) {
    if ([IO.Path]::GetExtension($file) -notin @('.exe', '.dll')) { throw "Not a PE signing target: $file" }
    $signature = Get-AuthenticodeSignature -LiteralPath $file
    if ($signature.SignerCertificate -and $signature.SignerCertificate.Thumbprint -ne $CertificateThumbprint) {
        throw "Refusing to replace another publisher's signature: $file"
    }
}
foreach ($file in $targets) {
    $arguments = @('sign', '/s', 'My', '/sha1', $CertificateThumbprint, '/fd', 'SHA256')
    if (!$TestOnlyNoTimestamp) { $arguments += @('/tr', $TimestampUrl.AbsoluteUri, '/td', 'SHA256') }
    & $SignToolPath @arguments $file
    if ($LASTEXITCODE) { throw "Authenticode signing failed: $file" }
    & $SignToolPath verify /pa /all /v $file
    if ($LASTEXITCODE) { throw "Authenticode verification failed: $file" }
}
