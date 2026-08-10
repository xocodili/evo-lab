# Phase 0 headless smoke test
# Usage: .\tests\smoke\run_headless.ps1 [-BuildDir build]

param(
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

$Exe = Join-Path $Root "$BuildDir\src\evo-lab.exe"
if (-not (Test-Path $Exe)) {
  Write-Error "evo-lab binary not found at $Exe. Build first: cmake -B build -G MinGW Makefiles && cmake --build build"
}

Write-Host "Running smoke: $Exe --headless --frames 120 --seed 42 --exit"
& $Exe --headless --frames 120 --seed 42 --exit
if ($LASTEXITCODE -ne 0) {
  Write-Error "Smoke test failed with exit code $LASTEXITCODE"
}
Write-Host "Smoke test passed."
