param(
    [string]$QtPath = "C:/Qt/6.12.0/msvc2022_64",
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$QtPath"
    if ($LASTEXITCODE) { throw "CMake configure failed" }
    cmake --build build --config $Configuration --parallel 8
    if ($LASTEXITCODE) { throw "Build failed" }
    $env:PATH = "$QtPath/bin;$env:PATH"
    ctest --test-dir build -C $Configuration --output-on-failure
    if ($LASTEXITCODE) { throw "Tests failed" }
    & "$QtPath/bin/windeployqt.exe" "build/$Configuration/AgentNodify.exe" --no-translations --no-compiler-runtime
    if ($LASTEXITCODE) { throw "Qt deployment failed" }
    Write-Host "Ready: $root/build/$Configuration/AgentNodify.exe"
} finally {
    Pop-Location
}
