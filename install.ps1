param (
    [switch]$InstallDeps
)

Write-Host "⚡ CM Language Installer for Windows" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan

$hasGcc = Get-Command gcc -ErrorAction SilentlyContinue
$hasCMake = Get-Command cmake -ErrorAction SilentlyContinue

if (-not $hasCMake) {
    Write-Host "❌ CMake is not installed or not in PATH." -ForegroundColor Red
    Write-Host "Please install CMake from https://cmake.org/download/ or run: winget install CMake" -ForegroundColor Yellow
    exit 1
}

Write-Host "✅ CMake found. Building CM..." -ForegroundColor Green
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "✨ CM Compiler successfully built!" -ForegroundColor Green
    $exePath = ".\build\cm.exe"
    if (Test-Path ".\build\Release\cm.exe") {
        $exePath = ".\build\Release\cm.exe"
    } elseif (Test-Path ".\build\Debug\cm.exe") {
        $exePath = ".\build\Debug\cm.exe"
    }
    Write-Host "Executable is located at: $exePath" -ForegroundColor Green
    Write-Host "Copying cm.exe to root directory..." -ForegroundColor Cyan
    Copy-Item $exePath -Destination ".\cm.exe" -Force
    Write-Host "✅ Done! You can now use: .\cm.exe run tests\oop_test.cm" -ForegroundColor Green
} else {
    Write-Host "❌ Build failed!" -ForegroundColor Red
    exit 1
}
