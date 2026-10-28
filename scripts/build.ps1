param(
    [string]$QtPath = $env:Qt6_ROOT,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Architecture = 'x64'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$QtPath) { throw 'Specify the Qt kit directory with -QtPath or Qt6_ROOT.' }
$QtPath = (Resolve-Path -LiteralPath $QtPath).Path
if (!(Test-Path -LiteralPath "$QtPath/lib/cmake/Qt6/Qt6Config.cmake" -PathType Leaf)) { throw 'QtPath must point to a Qt 6 kit directory.' }
foreach ($tool in @('cmake', 'ctest')) { Get-Command $tool -CommandType Application -ErrorAction Stop | Out-Null }
$savedPath = $env:PATH
Push-Location $root
try {
    $configure = @('-S', '.', '-B', $BuildDirectory, '-G', $Generator, "-DCMAKE_PREFIX_PATH=$QtPath")
    if ($Generator -like 'Visual Studio*') { $configure += @('-A', $Architecture) }
    else { $configure += "-DCMAKE_BUILD_TYPE=$Configuration" }
    & cmake @configure
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    cmake --build $BuildDirectory --config $Configuration --parallel
    if ($LASTEXITCODE) { throw 'Build failed' }
    $env:PATH = "$QtPath/bin;$savedPath"
    ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed' }
    $executable = Join-Path $BuildDirectory "$Configuration/AgentNodify.exe"
    if (!(Test-Path -LiteralPath $executable -PathType Leaf)) { $executable = Join-Path $BuildDirectory 'AgentNodify.exe' }
    & "$QtPath/bin/windeployqt.exe" $executable --no-translations --no-compiler-runtime
    if ($LASTEXITCODE) { throw 'Qt deployment failed' }
    Write-Host "Ready: $executable"
} finally {
    $env:PATH = $savedPath
    Pop-Location
}
