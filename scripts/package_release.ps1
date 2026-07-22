# ============================================================================
# Medical OCR — Release Packaging Script
# ============================================================================
#
# Creates a distributable release package containing:
#   - MedicalOCR.dll + import library
#   - Public C API headers
#   - Default configuration files
#   - Sample templates
#   - CLI demo executable
#   - Documentation
#   - Integration examples
#
# Usage:
#   .\scripts\package_release.ps1 -BuildDir .\build -OutputDir .\release -Version 1.0.0
#
# Requires: CMake build completed in Release configuration.
# ============================================================================

param(
    [string]$BuildDir = ".\build",
    [string]$OutputDir = ".\release",
    [string]$Version = "1.0.0",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$PackageName = "MedicalOCR-$Version-windows-x64"
$PackageDir = Join-Path $OutputDir $PackageName

Write-Host "Medical OCR — Release Packaging" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host "Version:  $Version"
Write-Host "Build:    $BuildDir"
Write-Host "Output:   $PackageDir"
Write-Host ""

# ---- Clean ----
if (Test-Path $PackageDir) {
    Remove-Item -Recurse -Force $PackageDir
}
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

# ---- Find build outputs ----
$BinDir = Join-Path $BuildDir "bin"
if (Test-Path (Join-Path $BinDir $Config)) {
    $BinDir = Join-Path $BinDir $Config
}

$DllPath = Get-ChildItem -Path $BinDir -Name "MedicalOCR.dll" -ErrorAction SilentlyContinue
if (-not $DllPath) {
    # Try without Config subdir.
    $DllPath = Get-ChildItem -Path (Join-Path $BuildDir "bin") -Name "MedicalOCR.dll" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
}

if (-not $DllPath) {
    Write-Host "ERROR: MedicalOCR.dll not found in $BinDir" -ForegroundColor Red
    Write-Host "Make sure to build first: cmake --build build --config $Config" -ForegroundColor Red
    exit 1
}

# ---- Copy binaries ----
Write-Host "[1/6] Copying binaries..." -ForegroundColor Green

$DestBin = Join-Path $PackageDir "bin"
New-Item -ItemType Directory -Force -Path $DestBin | Out-Null

# DLL and LIB.
Copy-Item (Join-Path $BinDir "MedicalOCR.dll") $DestBin -ErrorAction SilentlyContinue
Copy-Item (Join-Path $BinDir "MedicalOCR.lib") $DestBin -ErrorAction SilentlyContinue
Copy-Item (Join-Path $BinDir "libMedicalOCR.dll.a") $DestBin -ErrorAction SilentlyContinue

# CLI demo.
Copy-Item (Join-Path $BinDir "medical_ocr_cli.exe") $DestBin -ErrorAction SilentlyContinue

# OpenCV DLLs (if used).
Get-ChildItem -Path $BinDir -Name "libopencv_*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item (Join-Path $BinDir $_) $DestBin
}

Write-Host "       Binaries copied."

# ---- Copy headers ----
Write-Host "[2/6] Copying headers..." -ForegroundColor Green

$DestInclude = Join-Path $PackageDir "include\medical_ocr"
New-Item -ItemType Directory -Force -Path $DestInclude | Out-Null

Copy-Item "include\medical_ocr\*.h" $DestInclude

Write-Host "       Headers copied."

# ---- Copy config ----
Write-Host "[3/6] Copying configuration..." -ForegroundColor Green

$DestConfig = Join-Path $PackageDir "config"
New-Item -ItemType Directory -Force -Path $DestConfig | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $DestConfig "templates") | Out-Null

Copy-Item "config\medical_ocr.json" $DestConfig
Copy-Item "config\templates\sample_template.json" (Join-Path $DestConfig "templates")

Write-Host "       Configuration copied."

# ---- Copy docs ----
Write-Host "[4/6] Copying documentation..." -ForegroundColor Green

$DestDocs = Join-Path $PackageDir "docs"
New-Item -ItemType Directory -Force -Path $DestDocs | Out-Null

Copy-Item "README.md" $PackageDir
Get-ChildItem -Path "docs\*.md" | ForEach-Object {
    Copy-Item $_.FullName $DestDocs
}

Write-Host "       Documentation copied."

# ---- Copy examples ----
Write-Host "[5/6] Creating examples..." -ForegroundColor Green

$DestExamples = Join-Path $PackageDir "examples"
New-Item -ItemType Directory -Force -Path $DestExamples | Out-Null

# Create models directory.
$DestModels = Join-Path $PackageDir "models"
New-Item -ItemType Directory -Force -Path $DestModels | Out-Null
Copy-Item "models\README.md" $DestModels -ErrorAction SilentlyContinue

# Create scripts directory.
$DestScripts = Join-Path $PackageDir "scripts"
New-Item -ItemType Directory -Force -Path $DestScripts | Out-Null
Copy-Item "scripts\download_models.ps1" $DestScripts -ErrorAction SilentlyContinue

Write-Host "       Examples directory created."

# ---- Write version info ----
Write-Host "[6/6] Writing metadata..." -ForegroundColor Green

$VersionInfo = @"
# Medical OCR v$Version

Build Date: $(Get-Date -Format "yyyy-MM-dd")
Platform:   Windows 10/11 x64
Compiler:   Visual Studio 2022 / MinGW GCC

## Quick Start

1. Place MedicalOCR.dll in your application directory
2. Include the headers from include/medical_ocr/
3. Link against MedicalOCR.lib (MSVC) or libMedicalOCR.dll.a (MinGW)
4. Copy config/medical_ocr.json and customize as needed
5. For real OCR, download models: .\scripts\download_models.ps1

## Contents

- bin/           DLL, import library, CLI demo
- include/       C API headers
- config/        Configuration files and templates
- docs/          Documentation
- examples/      Integration examples
- models/        OCR model directory (download separately)
- scripts/       Utility scripts
- README.md      This file
- VERSION.txt    Version information
"@

Set-Content -Path (Join-Path $PackageDir "VERSION.txt") -Value $VersionInfo

# ---- Create ZIP ----
Write-Host ""
Write-Host "Creating archive..." -ForegroundColor Green

$ZipPath = Join-Path $OutputDir "$PackageName.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }

# Use Compress-Archive (PowerShell 5+).
Compress-Archive -Path $PackageDir -DestinationPath $ZipPath -Force

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Package created successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "  Directory: $PackageDir"
Write-Host "  Archive:   $ZipPath"
Write-Host ""
Write-Host "Contents:" -ForegroundColor White
Get-ChildItem -Recurse $PackageDir | Where-Object { -not $_.PSIsContainer } | ForEach-Object {
    $relPath = $_.FullName.Substring($PackageDir.Length + 1)
    $size = "{0,8:N0} KB" -f ($_.Length / 1KB)
    Write-Host "  $relPath  $size"
}
