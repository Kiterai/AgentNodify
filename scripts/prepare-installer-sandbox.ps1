[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [string]$OutputDirectory = (Join-Path 'build' ('installer-sandbox-' + [guid]::NewGuid().ToString('N'))),
    [switch]$RequireSigned,
    [ValidatePattern('^[a-fA-F0-9]{40}$')][string]$ExpectedSignerThumbprint
)
$ErrorActionPreference = 'Stop'
$Installer = (Resolve-Path -LiteralPath $Installer).Path
if ([IO.Path]::GetExtension($Installer) -ne '.exe') { throw 'Installer must be a Windows executable.' }
if (![IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path (Get-Location) $OutputDirectory }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must not exist; existing files are never overwritten.' }
if (!$PSCmdlet.ShouldProcess($OutputDirectory, 'Prepare read-only installer input and a network-disabled Windows Sandbox configuration')) { return }
$inputDirectory = Join-Path $OutputDirectory 'input'
$resultDirectory = Join-Path $OutputDirectory 'output'
New-Item -ItemType Directory -Path $inputDirectory, $resultDirectory | Out-Null
$hash = (Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash.ToLowerInvariant()
$copy = Join-Path $inputDirectory 'Setup.exe'
Copy-Item -LiteralPath $Installer -Destination $copy
if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash) { throw 'Installer changed during sandbox preparation.' }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test-installer.ps1') -Destination (Join-Path $inputDirectory 'test-installer.ps1')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'run-installer-sandbox-guest.ps1') -Destination (Join-Path $inputDirectory 'run-installer-sandbox-guest.ps1')
$command = 'powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File "C:\AgentNodifyValidation\Input\run-installer-sandbox-guest.ps1"'
if ($RequireSigned) { $command += ' -RequireSigned' }
if ($ExpectedSignerThumbprint) { $command += ' -ExpectedSignerThumbprint ' + $ExpectedSignerThumbprint }
$inputXml = [Security.SecurityElement]::Escape($inputDirectory)
$outputXml = [Security.SecurityElement]::Escape($resultDirectory)
$commandXml = [Security.SecurityElement]::Escape($command)
$xml = @"
<Configuration>
  <VGpu>Disable</VGpu>
  <Networking>Disable</Networking>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <MappedFolders>
    <MappedFolder><HostFolder>$inputXml</HostFolder><SandboxFolder>C:\AgentNodifyValidation\Input</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder>$outputXml</HostFolder><SandboxFolder>C:\AgentNodifyValidation\Output</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>$commandXml</Command></LogonCommand>
</Configuration>
"@
$configuration = Join-Path $OutputDirectory 'validate-installer.wsb'
[IO.File]::WriteAllText($configuration, $xml, (New-Object Text.UTF8Encoding $false))
$manifest = @{ schema = 1; installerSha256 = $hash; requireSigned = [bool]($RequireSigned -or $ExpectedSignerThumbprint);
    expectedSignerThumbprint = $ExpectedSignerThumbprint; testScriptSha256 = (Get-FileHash -LiteralPath (Join-Path $inputDirectory 'test-installer.ps1') -Algorithm SHA256).Hash.ToLowerInvariant();
    guestRunnerSha256 = (Get-FileHash -LiteralPath (Join-Path $inputDirectory 'run-installer-sandbox-guest.ps1') -Algorithm SHA256).Hash.ToLowerInvariant() }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'input-manifest.json'), ($manifest | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))
Write-Output "Sandbox configuration: $configuration"
Write-Output 'Open the .wsb file manually on a Windows Sandbox-capable machine. No Windows features were enabled and no VM was launched.'
Write-Output "Result after the guest finishes: $resultDirectory/run/result.json"
