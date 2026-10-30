param(
    [string]$QtPath = $env:Qt6_ROOT,
    [string]$IfwPath = $env:QTIFWDIR,
    [string]$BuildDirectory = 'build',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Architecture = 'x64',
    [string]$LegalDirectory,
    [string]$Version,
    [string]$SigningCertificateThumbprint,
    [uri]$TimestampUrl,
    [string]$SignToolPath = $env:SIGNTOOL_PATH
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$QtPath) { throw 'Specify -QtPath or Qt6_ROOT.' }
if (!$IfwPath) { throw 'Specify -IfwPath or QTIFWDIR (Qt Installer Framework root).' }
$QtPath = (Resolve-Path -LiteralPath $QtPath).Path
$IfwPath = (Resolve-Path -LiteralPath $IfwPath).Path
foreach ($tool in @('binarycreator.exe', 'installerbase.exe', 'repogen.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $IfwPath "bin/$tool") -PathType Leaf)) {
        throw "Missing Qt Installer Framework tool: $tool"
    }
}
if ($LegalDirectory) { $LegalDirectory = (Resolve-Path -LiteralPath $LegalDirectory).Path }
if (($SigningCertificateThumbprint -and !$TimestampUrl) -or ($TimestampUrl -and !$SigningCertificateThumbprint)) {
    throw 'Signing requires both -SigningCertificateThumbprint and -TimestampUrl.'
}
$savedPath = $env:PATH
Push-Location $root
try {
    $signedBase = ''
    $signing = @{ CertificateThumbprint = $SigningCertificateThumbprint; TimestampUrl = $TimestampUrl }
    if ($SignToolPath) { $signing.SignToolPath = $SignToolPath }
    if ($SigningCertificateThumbprint) {
        $signingDirectory = Join-Path $BuildDirectory 'signing'
        New-Item -ItemType Directory -Path $signingDirectory -Force | Out-Null
        $signedBase = Join-Path $signingDirectory 'installerbase.exe'
        Copy-Item -LiteralPath (Join-Path $IfwPath 'bin/installerbase.exe') -Destination $signedBase
        & "$PSScriptRoot/sign-windows.ps1" -Files $signedBase @signing
        $signedBase = (Resolve-Path -LiteralPath $signedBase).Path
    }
    $configure = @('-S', '.', '-B', $BuildDirectory, '-G', $Generator,
        "-DCMAKE_PREFIX_PATH=$QtPath", '-DAGENTNODIFY_BUILD_INSTALLER=ON',
        "-DCPACK_IFW_ROOT=$IfwPath", "-DAGENTNODIFY_LEGAL_DIRECTORY=$LegalDirectory",
        "-DAGENTNODIFY_SIGNED_INSTALLERBASE=$signedBase")
    if ($Version) { $configure += "-DAGENTNODIFY_VERSION=$Version" }
    if ($Generator -like 'Visual Studio*') { $configure += @('-A', $Architecture) }
    else { $configure += '-DCMAKE_BUILD_TYPE=Release' }
    & cmake @configure
    if ($LASTEXITCODE) { throw 'Installer configure failed' }
    cmake --build $BuildDirectory --config Release --parallel
    if ($LASTEXITCODE) { throw 'Release build failed' }
    if ($SigningCertificateThumbprint) {
        $app = Join-Path $BuildDirectory 'Release/AgentNodify.exe'
        if (!(Test-Path -LiteralPath $app)) { $app = Join-Path $BuildDirectory 'AgentNodify.exe' }
        & "$PSScriptRoot/sign-windows.ps1" -Files $app @signing
    }
    $env:PATH = "$QtPath/bin;$savedPath"
    ctest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed; installer was not generated' }
    & cpack --config "$BuildDirectory/CPackConfig.cmake" -C Release -G IFW
    if ($LASTEXITCODE) { throw 'IFW packaging failed' }
    if ($SigningCertificateThumbprint) {
        $packageConfig = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CPackConfig.cmake') -Raw
        $name = [regex]::Match($packageConfig, 'set\(CPACK_PACKAGE_FILE_NAME "([^"]+)"\)').Groups[1].Value
        if (!$name) { throw 'Installer filename was not found in CPackConfig.cmake' }
        & "$PSScriptRoot/sign-windows.ps1" -Files (Join-Path $BuildDirectory "installer/$name.exe") @signing
    }
    Write-Host "Installer: $BuildDirectory/installer/"
} finally {
    $env:PATH = $savedPath
    Pop-Location
}
