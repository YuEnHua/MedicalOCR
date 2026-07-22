# PaddleOCR Integration Guide

## Overview

This guide walks through installing Paddle Inference SDK and PP-OCRv4 models so the Medical OCR DLL can perform **real offline Chinese/English OCR**. Without this setup, only the Mock engine is available (returns sample data).

---

## Prerequisites

| Component | Minimum Version | Where |
|-----------|----------------|-------|
| Paddle Inference C++ SDK | 2.5+ | [Official download](https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html) |
| PP-OCRv4 Detection Model | — | [PaddleOCR models](https://github.com/PaddlePaddle/PaddleOCR) |
| PP-OCRv4 Recognition Model | — | 同上 |
| Character Dictionary | ppocr_keys_v1.txt | 同上 |

---

## Step 1: Install Paddle Inference SDK

### Option A — Official binary (recommended)

1. Visit: https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html
2. Select: **Windows** → **CPU** (or GPU) → **C++** → **2.5+**
3. Download the ZIP (~200 MB)
4. Extract to `C:\paddle_inference`

Expected structure:
```
C:\paddle_inference\
├── include\
│   └── paddle_inference_api.h
├── lib\
│   ├── paddle_inference.lib          (MSVC)
│   └── libpaddle_inference.dll.a     (MinGW)
├── third_party\
│   └── install\
│       ├── mklml\lib\*.dll
│       ├── onnxruntime\lib\*.dll
│       └── ...
├── paddle_inference.dll
└── version.txt
```

### Option B — Build from source

```bash
git clone https://github.com/PaddlePaddle/Paddle.git
cd Paddle
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_GPU=OFF \
         -DWITH_MKL=ON -DWITH_ONNXRUNTIME=ON -DWITH_TESTING=OFF
cmake --build . --config Release -j 8
```

> Building from source requires ~30 GB disk space and takes 1–2 hours.

### Option C — pip (Python only, for model download)

```bash
pip install paddlepaddle  # CPU version
pip install paddleocr      # For downloading models only
```

---

## Step 2: Download PP-OCRv4 Models

### Automatic (PowerShell)

```powershell
.\scripts\download_models.ps1 -OutputDir .\models
```

Downloads (~17 MB total):
- `ch_PP-OCRv4_det_infer.tar` → detection model (text box locations)
- `ch_PP-OCRv4_rec_infer.tar` → recognition model (text content)
- `ppocr_keys_v1.txt` → 6,623-character Chinese dictionary

### Manual download

| File | URL | Size |
|------|-----|------|
| Detection model | https://paddleocr.bj.bcebos.com/PP-OCRv4/chinese/ch_PP-OCRv4_det_infer.tar | ~5 MB |
| Recognition model | https://paddleocr.bj.bcebos.com/PP-OCRv4/chinese/ch_PP-OCRv4_rec_infer.tar | ~12 MB |
| Dictionary | https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/release/2.7/ppocr/utils/ppocr_keys_v1.txt | ~90 KB |

Extract `.tar` files:
```bash
tar -xf ch_PP-OCRv4_det_infer.tar -C models/
tar -xf ch_PP-OCRv4_rec_infer.tar -C models/
```

Result:
```
models/
├── ch_PP-OCRv4_det_infer/
│   ├── inference.pdmodel       (~5 MB)
│   └── inference.pdiparams     (~50 KB)
├── ch_PP-OCRv4_rec_infer/
│   ├── inference.pdmodel       (~12 MB)
│   └── inference.pdiparams     (~90 KB)
└── ppocr_keys_v1.txt           (~90 KB, 6623 lines)
```

---

## Step 3: Build with PaddleOCR Enabled

### Visual Studio 2022

```powershell
cmake -S . -B build -A x64 `
    -DMEDICAL_OCR_ENABLE_PADDLE=ON `
    -DPADDLE_INFERENCE_DIR="C:/paddle_inference"

cmake --build build --config Release
```

### MinGW GCC

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DMEDICAL_OCR_ENABLE_PADDLE=ON \
    -DPADDLE_INFERENCE_DIR=/c/paddle_inference \
    -DOpenCV_DIR="/ucrt64/lib/cmake/opencv4"

cmake --build build --config Release
```

Verify: CMake output should show:
```
-- Paddle Inference found:
--   Include: C:/paddle_inference/include
--   Library: C:/paddle_inference/lib/paddle_inference.lib
--   PaddleOCR:    ENABLED
```

If it shows `stub (Paddle SDK not available)`, check `PADDLE_INFERENCE_DIR`.

---

## Step 4: Runtime DLL Setup

Copy these DLLs next to `MedicalOCR.dll` or add to PATH:

| DLL | Source |
|-----|--------|
| `paddle_inference.dll` | `C:\paddle_inference\` |
| `libiomp5md.dll` | `C:\paddle_inference\third_party\install\mklml\lib\` |
| `mklml.dll` | 同上 |
| `onnxruntime.dll` | `C:\paddle_inference\third_party\install\onnxruntime\lib\` |
| `paddle_*.dll` | `C:\paddle_inference\` (if any additional) |

Or in PowerShell:
```powershell
$src = "C:\paddle_inference"
Copy-Item "$src\paddle_inference.dll" ".\build\bin\Release\"
Copy-Item "$src\third_party\install\mklml\lib\*.dll" ".\build\bin\Release\"
Copy-Item "$src\third_party\install\onnxruntime\lib\*.dll" ".\build\bin\Release\"
```

---

## Step 5: Configuration

Set `ocr.engine` to `"paddle"` in `config/medical_ocr.json`:

```json
{
  "ocr": {
    "engine": "paddle",
    "model_directory": "./models",
    "use_gpu": false,
    "cpu_threads": 4,
    "minimum_confidence": 0.65
  }
}
```

---

## Step 6: Verify

```powershell
.\build\bin\Release\medical_ocr_cli.exe test_report.jpg config\medical_ocr.json
```

With PaddleOCR active, the output JSON will contain real recognized text from the image.

---

## GPU Setup (Optional)

### Requirements

- NVIDIA GPU with CUDA Compute Capability 6.0+
- CUDA Toolkit 11.2+ (or cuDNN-compatible version)
- cuDNN 8.2+

### Download GPU SDK

Select **GPU** (not CPU) when downloading Paddle Inference SDK. The GPU package includes CUDA/cuDNN-dependent DLLs.

### Configuration

```json
{
  "ocr": {
    "engine": "paddle",
    "model_directory": "./models",
    "use_gpu": true,
    "gpu_id": 0,
    "cpu_threads": 1
  }
}
```

GPU memory: ~500 MB for models + ~100 MB per concurrent inference.

---

## Model Version Compatibility

| PaddleOCR Version | Detection Model | Recognition Model | Paddle Inference |
|-------------------|----------------|-------------------|-----------------|
| PP-OCRv4 | `ch_PP-OCRv4_det_infer` | `ch_PP-OCRv4_rec_infer` | ≥ 2.5 |
| PP-OCRv3 | `ch_PP-OCRv3_det_infer` | `ch_PP-OCRv3_rec_infer` | ≥ 2.4 |
| PP-OCRv2 | `ch_PP-OCRv2_det_infer` | `ch_PP-OCRv2_rec_infer` | ≥ 2.3 |

> The Medical OCR DLL targets PP-OCRv4 by default. Older model versions may work but are not tested.

---

## Troubleshooting

### "PaddleOcrEngine: NOT compiled with Paddle Inference support"
The DLL was built without `-DMEDICAL_OCR_ENABLE_PADDLE=ON`. Rebuild with the flag and a valid `PADDLE_INFERENCE_DIR`.

### "detection model not found: .../ch_PP-OCRv4_det_infer/inference.pdmodel"
Run `.\scripts\download_models.ps1` or manually place model files in `models/`. Check the directory structure matches.

### "paddle_inference.dll not found" (0xc0000135)
Copy `paddle_inference.dll` and its dependencies to the executable directory or add to PATH.

### "Cannot open include file: 'paddle_inference_api.h'"
Set `PADDLE_INFERENCE_DIR` to the root of the extracted SDK (the directory containing `include/` and `lib/`).

### "MKL/DNNL libraries not found"
Copy ALL DLLs from `third_party/install/*/lib/` to the executable directory.

### Memory usage > 2 GB
- Reduce `cpu_threads` to 1 or 2
- Disable MKL multi-threading: set env `MKL_NUM_THREADS=1`
- Process images one at a time instead of concurrently
- Use GPU mode if available (frees CPU memory)

### Recognition is slow (> 5 seconds per image)
- CPU mode on large images is slow. Expected: 1–3s for A4 at 300 DPI.
- Reduce input image resolution before calling Recognize
- Enable GPU mode
- Reduce `minimum_confidence` to skip more detections early

### Detection finds no text boxes
- Image may be too dark/blurry — check `quality.is_blurry` in output
- Adjust `det_threshold` (lower to 0.2) and `det_box_threshold` (lower to 0.3)
- Ensure image has visible text at reasonable size
