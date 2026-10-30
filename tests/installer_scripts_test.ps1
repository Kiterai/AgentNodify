param([Parameter(Mandatory = $true)][string]$SourceDirectory, [Parameter(Mandatory = $true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$root = Join-Path $BuildDirectory ('installer-script-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
# All files in this fixture are new, disposable and under the build directory.
$installer = Join-Path $root 'fixture.exe'
[IO.File]::WriteAllText($installer, 'not an executable; WhatIf must never start this file')
foreach ($name in @('test-installer.ps1', 'prepare-installer-sandbox.ps1', 'run-installer-sandbox-guest.ps1', 'sign-windows.ps1', 'package.ps1', 'prepare-signing-sandbox.ps1', 'run-signing-sandbox-guest.ps1')) {
    $tokens = $null; $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $SourceDirectory "scripts/$name"), [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors) { throw "PowerShell syntax errors in $name" }
}
& (Join-Path $SourceDirectory 'tests/signing_scripts_test.ps1') -SourceDirectory $SourceDirectory -WorkDirectory (Join-Path $root 'signing')
$planned = Join-Path $root 'planned-installation'
& (Join-Path $SourceDirectory 'scripts/test-installer.ps1') -Installer $installer -WorkDirectory $planned -WhatIf
if (Test-Path -LiteralPath $planned) { throw 'WhatIf created an installation directory.' }
$plannedSandbox = Join-Path $root 'planned-sandbox'
& (Join-Path $SourceDirectory 'scripts/prepare-installer-sandbox.ps1') -Installer $installer -OutputDirectory $plannedSandbox -WhatIf
if (Test-Path -LiteralPath $plannedSandbox) { throw 'WhatIf created a sandbox directory.' }
$existing = Join-Path $root 'existing'
New-Item -ItemType Directory -Path $existing | Out-Null
$sentinel = Join-Path $existing 'keep.txt'
[IO.File]::WriteAllText($sentinel, 'keep this fixture')
$refused = $false
try { & (Join-Path $SourceDirectory 'scripts/test-installer.ps1') -Installer $installer -WorkDirectory $existing -WhatIf } catch { $refused = $true }
if (!$refused -or [IO.File]::ReadAllText($sentinel) -ne 'keep this fixture') { throw 'Existing-directory guard failed.' }
$sandbox = Join-Path $root 'generated-sandbox'
& (Join-Path $SourceDirectory 'scripts/prepare-installer-sandbox.ps1') -Installer $installer -OutputDirectory $sandbox
[xml]$configuration = [IO.File]::ReadAllText((Join-Path $sandbox 'validate-installer.wsb'))
if ($configuration.Configuration.Networking -ne 'Disable' -or $configuration.Configuration.ClipboardRedirection -ne 'Disable') {
    throw 'Sandbox network/clipboard isolation missing.'
}
$mappings = @($configuration.Configuration.MappedFolders.MappedFolder)
if ($mappings.Count -ne 2 -or $mappings[0].ReadOnly -ne 'true' -or $mappings[1].ReadOnly -ne 'false') {
    throw 'Sandbox folder mapping policy incorrect.'
}
if ($configuration.Configuration.LogonCommand.Command -notlike '*run-installer-sandbox-guest.ps1*') { throw 'Sandbox guest runner command is missing.' }
$guestScript = [IO.File]::ReadAllText((Join-Path $sandbox 'input/run-installer-sandbox-guest.ps1'))
if ($guestScript -notlike '*GuestLocalAppData*' -or $guestScript -notlike '*EnvironmentKind = ''WindowsSandbox''*') { throw 'Sandbox must test on the guest local filesystem.' }
$testBytes = [IO.File]::ReadAllBytes((Join-Path $sandbox 'input/test-installer.ps1'))
if (@($testBytes | Where-Object { $_ -gt 127 }).Count -ne 0) { throw 'Test driver must not depend on an ANSI script codepage.' }
if ((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $sandbox 'input/Setup.exe') -Algorithm SHA256).Hash) {
    throw 'Sandbox installer copy differs from input.'
}
Write-Output 'Installer script guards and sandbox configuration validated; no executable was started.'
