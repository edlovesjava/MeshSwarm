<#
.SYNOPSIS
    Compile test for MeshSwarm library examples.

.DESCRIPTION
    Compiles all example sketches using Arduino CLI to verify the library builds correctly.

.PARAMETER Example
    Specific example to compile. If not specified, compiles all examples.

.PARAMETER Board
    Board FQBN. Defaults to esp32:esp32:esp32

.EXAMPLE
    .\compile-test.ps1
    .\compile-test.ps1 -Example BasicNode
    .\compile-test.ps1 -Board esp32:esp32:esp32s3
#>

param(
    [string]$Example = "",
    [string]$Board = "esp32:esp32:esp32"
)

$ErrorActionPreference = "Stop"

# Get script and library paths
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$libraryDir = Split-Path -Parent $scriptDir
$examplesDir = Join-Path $libraryDir "examples"

# Check arduino-cli is available
try {
    $null = Get-Command arduino-cli -ErrorAction Stop
} catch {
    Write-Host "ERROR: arduino-cli not found. Please install it first." -ForegroundColor Red
    Write-Host "  winget install ArduinoSA.ArduinoCLI" -ForegroundColor Yellow
    exit 1
}

# Verify library structure
$srcFile = Join-Path $libraryDir "src\MeshSwarm.cpp"
if (-not (Test-Path $srcFile)) {
    Write-Host "ERROR: MeshSwarm.cpp not found at $srcFile" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MeshSwarm Compile Test" -ForegroundColor White
Write-Host "  Board: $Board" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

# Find examples
$examples = @()
if ($Example) {
    $examplePath = Join-Path $examplesDir $Example
    if (-not (Test-Path $examplePath)) {
        Write-Host "ERROR: Example '$Example' not found" -ForegroundColor Red
        exit 1
    }
    $examples = @($examplePath)
} else {
    $examples = Get-ChildItem -Path $examplesDir -Directory | Select-Object -ExpandProperty FullName
}

if ($examples.Count -eq 0) {
    Write-Host "ERROR: No examples found in $examplesDir" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Found $($examples.Count) example(s) to compile" -ForegroundColor Yellow
Write-Host ""

$passed = 0
$failed = 0
$results = @()

foreach ($exampleDir in $examples) {
    $exampleName = Split-Path -Leaf $exampleDir
    $sketchFile = Join-Path $exampleDir "$exampleName.ino"

    if (-not (Test-Path $sketchFile)) {
        Write-Host "  SKIP: $exampleName (no .ino file)" -ForegroundColor DarkGray
        continue
    }

    Write-Host "  Compiling: $exampleName ... " -NoNewline

    # Compile using cmd to avoid PowerShell stderr issues
    $null = cmd /c "arduino-cli compile --fqbn $Board `"$sketchFile`" 2>&1"
    $exitCode = $LASTEXITCODE

    if ($exitCode -eq 0) {
        Write-Host "PASS" -ForegroundColor Green
        $passed++
        $results += @{ Name = $exampleName; Status = "PASS" }
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        $failed++
        $results += @{ Name = $exampleName; Status = "FAIL" }
    }
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Results" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Passed: $passed" -ForegroundColor Green
if ($failed -gt 0) {
    Write-Host "  Failed: $failed" -ForegroundColor Red
}
Write-Host ""

# Exit with error if any failed
if ($failed -gt 0) {
    exit 1
}
