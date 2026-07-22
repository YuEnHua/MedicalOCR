# PaddleOCR Integration Guide

## Overview

This guide covers setting up Paddle Inference SDK and PP-OCRv4 models for
the Medical OCR DLL. Once configured, the DLL can perform real offline OCR
using PaddleOCR's Chinese text recognition models.

## Prerequisites

- **Paddle Inference C++ SDK** (>= 2.5)
- **PP-OCRv4 Chinese models** (detection + recognition)
- **Character dictionary** (ppocr_keys_v1.txt)
- **Visual Studio 2022** or **MinGW GCC 13+** (with C++17)

## Step 1: Download Paddle Inference SDK

### Option A: Official Download (Recommended)

1. Visit: https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html
2. Select: **Windows** → **CPU** (or GPU) → **C++** → **2.5+**
3. Download the zip package (~200 MB)
4. Extract to `C:\paddle_inference`

Expected directory structure:
```
C:\paddle_inference\
├── include\
│   └── paddle_inference_api.h
├── lib\
│   ├── paddle_inference.lib        (MSVC)
│   └── libpaddle_inference.dll.a   (MinGW)
├── third_party\
│   └── install\
│       ├── mklml\
│       ├── onnxruntime\
│       └── ...
└── paddle_inference.dll            (runtime)
```

### Option B: Build from Source

```bash
git clone https://github.com/PaddlePaddle/Paddle.git
cd Paddle
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_GPU=OFF -DWITH_MKL=ON -DWITH_ONNXRUNTIME=ON
cmake --build . --config Release -j 8
```

## Step 2: Download PP-OCRv4 Models

### Automatic (PowerShell)

```powershell
.\scripts\download_models.ps1 -OutputDir .\models
```

### Manual

Download the following files:

| File | URL | Size |
|------|-----|------|
| ch_PP-OCRv4_det_infer.tar | https://paddleocr.bj.bcebos.com/PP-OCRv4/chinese/ch_PP-OCRv4_det_infer.tar | ~5 MB |
| ch_PP-OCRv4_rec_infer.tar | https://paddleocr.bj.bcebos.com/PP-OCRv4/chinese/ch_PP-OCRv4_rec_infer.tar | ~12 MB |
| ppocr_keys_v1.txt | https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/release/2.7/ppocr/utils/ppocr_keys_v1.txt | ~90 KB |

Extract `.tar` files into `models/`:
```
models/
├── ch_PP-OCRv4_det_infer/
│   ├── inference.pdmodel
│   └── inference.pdiparams
├── ch_PP-OCRv4_rec_infer/
│   ├── inference.pdmodel
│   └── inference.pdiparams
└── ppocr_keys_v1.txt
```

## Step 3: Build with PaddleOCR

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
    -DPADDLE_INFERENCE_DIR=/c/paddle_inference

cmake --build build --config Release
```

## Step 4: Runtime DLL Dependencies

When using PaddleOCR, the following DLLs must be in the system PATH or next
to `MedicalOCR.dll`:

```
paddle_inference.dll
libiomp5md.dll
mklml.dll
onnxruntime.dll
```

Copy them from:
- `C:\paddle_inference\paddle_inference.dll`
- `C:\paddle_inference\third_party\install\mklml\lib\libiomp5md.dll`
- `C:\paddle_inference\third_party\install\mklml\lib\mklml.dll`
- `C:\paddle_inference\third_party\install\onnxruntime\lib\onnxruntime.dll`

Or add `C:\paddle_inference` and its `third_party` lib directories to PATH.

## Step 5: Configuration

Set `ocr.engine` to `"paddle"` in `medical_ocr.json`:

```json
{
  "ocr": {
    "engine": "paddle",
    "model_directory": "./models",
    "use_gpu": false,
    "cpu_threads": 4,
    "minimum_confidence": 0.65,
    "det_threshold": 0.3,
    "det_box_threshold": 0.5
  }
}
```

## Step 6: Verify

```powershell
.\build\bin\Release\medical_ocr_cli.exe test_report.jpg config\medical_ocr.json
```

With PaddleOCR enabled, the output JSON will contain real recognized text
instead of mock data.

## Troubleshooting

### "PaddleOcrEngine: NOT compiled with Paddle Inference support"
The DLL was built without `-DMEDICAL_OCR_ENABLE_PADDLE=ON`. Rebuild.

### "detection model not found"
Check that `models/ch_PP-OCRv4_det_infer/` contains `inference.pdmodel` and
`inference.pdiparams`.

### "paddle_inference.dll not found"
Add the Paddle Inference `bin` directory to PATH, or copy the DLLs next to
`MedicalOCR.dll`.

### "Cannot open include file: 'paddle_inference_api.h'"
Set `PADDLE_INFERENCE_DIR` correctly — it should point to the root of the
extracted Paddle Inference SDK (containing `include/` and `lib/`).

### Memory usage too high
Reduce `cpu_threads` in the config, or use `"use_gpu": true` if a GPU is
available.
