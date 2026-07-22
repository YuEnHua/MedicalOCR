# ============================================================================
# Medical OCR — PP-OCRv4 Model Download Script
# ============================================================================
#
# Downloads the required PaddleOCR inference models for offline use.
#
# Usage:
#   .\scripts\download_models.ps1 [-OutputDir .\models]
#
# Downloads:
#   - ch_PP-OCRv4_det_infer  (text detection, ~5 MB)
#   - ch_PP-OCRv4_rec_infer  (text recognition, ~12 MB)
#   - ppocr_keys_v1.txt      (character dictionary, ~90 KB)
#
# Requires: PowerShell 5.1+, internet connection.
# ============================================================================

param(
    [string]$OutputDir = ".\models"
)

$ErrorActionPreference = "Stop"

# PaddleOCR model URLs (official PaddleOCR GitHub releases).
$BaseUrl = "https://paddleocr.bj.bcebos.com/PP-OCRv4/chinese"

$Models = @(
    @{
        Name = "ch_PP-OCRv4_det_infer"
        Url  = "$BaseUrl/ch_PP-OCRv4_det_infer.tar"
        File = "ch_PP-OCRv4_det_infer.tar"
    },
    @{
        Name = "ch_PP-OCRv4_rec_infer"
        Url  = "$BaseUrl/ch_PP-OCRv4_rec_infer.tar"
        File = "ch_PP-OCRv4_rec_infer.tar"
    }
)

$DictUrl = "https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/release/2.7/ppocr/utils/ppocr_keys_v1.txt"

# Create output directory.
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

Write-Host "Medical OCR — PP-OCRv4 Model Download" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Output: $OutputDir"
Write-Host ""

# ---- Download models ----
foreach ($model in $Models) {
    $tarPath = Join-Path $OutputDir $model.File
    $extractPath = Join-Path $OutputDir $model.Name

    if (Test-Path $extractPath) {
        Write-Host "[SKIP] $($model.Name) already exists" -ForegroundColor Yellow
        continue
    }

    Write-Host "[DOWNLOAD] $($model.Name) ..." -ForegroundColor Green
    try {
        # Download .tar file.
        if (-not (Test-Path $tarPath)) {
            Invoke-WebRequest -Uri $model.Url -OutFile $tarPath -ErrorAction Stop
        }

        # Extract.
        Write-Host "[EXTRACT] $($model.Name) ..."
        tar -xf $tarPath -C $OutputDir

        # Clean up tar file.
        Remove-Item $tarPath -Force

        Write-Host "[OK] $($model.Name) installed" -ForegroundColor Green
    }
    catch {
        Write-Host "[FAIL] $($model.Name): $_" -ForegroundColor Red
    }
}

# ---- Download dictionary ----
$dictPath = Join-Path $OutputDir "ppocr_keys_v1.txt"
if (-not (Test-Path $dictPath)) {
    Write-Host "[DOWNLOAD] ppocr_keys_v1.txt ..." -ForegroundColor Green
    try {
        Invoke-WebRequest -Uri $DictUrl -OutFile $dictPath -ErrorAction Stop
        Write-Host "[OK] ppocr_keys_v1.txt installed" -ForegroundColor Green
    }
    catch {
        Write-Host "[FAIL] ppocr_keys_v1.txt: $_" -ForegroundColor Red
    }
}
else {
    Write-Host "[SKIP] ppocr_keys_v1.txt already exists" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Download complete." -ForegroundColor Cyan
Write-Host ""
Write-Host "Model directory structure:" -ForegroundColor White
Write-Host "  $OutputDir/"
Write-Host "    ch_PP-OCRv4_det_infer/"
Write-Host "      inference.pdmodel"
Write-Host "      inference.pdiparams"
Write-Host "    ch_PP-OCRv4_rec_infer/"
Write-Host "      inference.pdmodel"
Write-Host "      inference.pdiparams"
Write-Host "    ppocr_keys_v1.txt"
Write-Host ""
Write-Host "Next: Build with PaddleOCR enabled:"
Write-Host '  cmake -S . -B build -DMEDICAL_OCR_ENABLE_PADDLE=ON -DPADDLE_INFERENCE_DIR=C:/paddle_inference'
