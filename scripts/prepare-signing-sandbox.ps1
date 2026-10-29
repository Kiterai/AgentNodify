[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)][string]$PreparedPackageDirectory,
    [Parameter(Mandatory = $true)][string]$IfwPath,
    [Parameter(Mandatory = $true)][string]$SignToolPath,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$PreparedPackageDirectory = (Resolve-Path -LiteralPath $PreparedPackageDirectory).Path
$IfwPath = (Resolve-Path -LiteralPath $IfwPath).Path
$SignToolPath = (Resolve-Path -LiteralPath $SignToolPath).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must not exist.' }
foreach ($file in @('config/config.xml', 'config/controlscript.qs', 'packages/org.agentnodify.application/data/bin/AgentNodify.exe', 'packages/org.agentnodify.application/meta/installscript.qs')) {
    if (!(Test-Path -LiteralPath (Join-Path $PreparedPackageDirectory $file) -PathType Leaf)) { throw "Missing prepared package file: $file" }
}
if (!$PSCmdlet.ShouldProcess($OutputDirectory, 'Prepare isolated offline signing Sandbox; no host certificates or trust settings are changed')) { return }
$inputDirectory = Join-Path $OutputDirectory 'input'
$output = Join-Path $OutputDirectory 'output'
$payload = Join-Path $inputDirectory 'payload'
$tools = Join-Path $inputDirectory 'ifw/bin'
$sdk = Join-Path $inputDirectory 'sdk'
$scripts = Join-Path $inputDirectory 'scripts'
New-Item -ItemType Directory -Path $payload, $output, $tools, $sdk, $scripts | Out-Null
foreach ($name in @('config', 'packages')) { Copy-Item -LiteralPath (Join-Path $PreparedPackageDirectory $name) -Destination $payload -Recurse }
foreach ($name in @('binarycreator.exe', 'installerbase.exe')) { Copy-Item -LiteralPath (Join-Path $IfwPath "bin/$name") -Destination $tools }
Copy-Item -LiteralPath $SignToolPath -Destination (Join-Path $sdk 'signtool.exe')
# Explicit SDK runtime allowlist, never copy a whole SDK or key material.
foreach ($name in @('signtool.exe.manifest', 'mssign32.dll', 'wintrust.dll', 'appxsip.dll', 'AppxPackaging.dll', 'OpcServices.dll',
    'Microsoft.Windows.Build.Signing.mssign32.dll.manifest', 'Microsoft.Windows.Build.Signing.wintrust.dll.manifest',
    'Microsoft.Windows.Build.Appx.AppxSip.dll.manifest', 'Microsoft.Windows.Build.Appx.AppxPackaging.dll.manifest', 'Microsoft.Windows.Build.Appx.OpcServices.dll.manifest')) {
    $file = Join-Path (Split-Path $SignToolPath -Parent) $name
    if (Test-Path -LiteralPath $file -PathType Leaf) { Copy-Item -LiteralPath $file -Destination $sdk }
}
foreach ($name in @('test-self-signed-package.ps1', 'sign-windows.ps1', 'test-installer.ps1', 'run-signing-sandbox-guest.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $scripts
}
# Check the transitive side-by-side manifest closure before asking for guest trust.
foreach ($manifestFile in @(Get-ChildItem -LiteralPath $sdk -Filter '*.manifest' -File)) {
    [xml]$document = [IO.File]::ReadAllText($manifestFile.FullName)
    foreach ($dependency in $document.SelectNodes("//*[local-name()='dependentAssembly']/*[local-name()='assemblyIdentity']")) {
        $name = $dependency.GetAttribute('name') + '.manifest'
        if ([IO.Path]::GetFileName($name) -ne $name -or !(Test-Path -LiteralPath (Join-Path $sdk $name) -PathType Leaf)) { throw "Missing SDK assembly manifest: $name" }
    }
    foreach ($dependency in $document.SelectNodes("//*[local-name()='file']")) {
        $name = $dependency.GetAttribute('name')
        if ([IO.Path]::GetFileName($name) -ne $name -or !(Test-Path -LiteralPath (Join-Path $sdk $name) -PathType Leaf)) { throw "Missing SDK assembly runtime: $name" }
    }
}
$files = @(Get-ChildItem -LiteralPath $inputDirectory -File -Recurse | ForEach-Object {
    @{ path = $_.FullName.Substring($inputDirectory.Length + 1).Replace('\', '/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
[IO.File]::WriteAllText((Join-Path $inputDirectory 'manifest.json'), (@{ schema = 1; testOnly = $true; files = $files } | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding $false))
$inputXml = [Security.SecurityElement]::Escape($inputDirectory)
$outputXml = [Security.SecurityElement]::Escape($output)
$xml = @"
<Configuration>
  <VGpu>Disable</VGpu><Networking>Disable</Networking><ClipboardRedirection>Disable</ClipboardRedirection>
  <MappedFolders>
    <MappedFolder><HostFolder>$inputXml</HostFolder><SandboxFolder>C:\AgentNodifySigning\Input</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$outputXml</HostFolder><SandboxFolder>C:\AgentNodifySigning\Output</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File C:\AgentNodifySigning\Input\scripts\run-signing-sandbox-guest.ps1</Command></LogonCommand>
</Configuration>
"@
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'validate-signing.wsb'), $xml, (New-Object Text.UTF8Encoding $false))
Write-Output "Prepared signing Sandbox: $OutputDirectory/validate-signing.wsb"
