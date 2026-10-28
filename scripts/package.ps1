param(
    [string]$QtPath = $env:Qt6_ROOT,
    [string]$IfwPath = $env:QTIFWDIR,
    [string]$BuildDirectory = 'build',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Architecture = 'x64',
    [string]$LegalDirectory
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
$savedPath = $env:PATH
Push-Location $root
try {
    $configure = @('-S', '.', '-B', $BuildDirectory, '-G', $Generator,
        "-DCMAKE_PREFIX_PATH=$QtPath", '-DAGENTNODIFY_BUILD_INSTALLER=ON',
        "-DCPACK_IFW_ROOT=$IfwPath", "-DAGENTNODIFY_LEGAL_DIRECTORY=$LegalDirectory")
    if ($Generator -like 'Visual Studio*') { $configure += @('-A', $Architecture) }
    else { $configure += '-DCMAKE_BUILD_TYPE=Release' }
    & cmake @configure
    if ($LASTEXITCODE) { throw 'Installer configure failed' }
    cmake --build $BuildDirectory --config Release --parallel
    if ($LASTEXITCODE) { throw 'Release build failed' }
    $env:PATH = "$QtPath/bin;$savedPath"
    ctest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed; installer was not generated' }
    & cpack --config "$BuildDirectory/CPackConfig.cmake" -C Release -G IFW
    if ($LASTEXITCODE) { throw 'IFW packaging failed' }
    Write-Host "Installer: $BuildDirectory/installer/"
} finally {
    $env:PATH = $savedPath
    Pop-Location
}
