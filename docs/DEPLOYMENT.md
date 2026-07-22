# Deployment Guide

## 1. Build Release DLL

```powershell
# Visual Studio 2022
cmake -S . -B build -A x64
cmake --build build --config Release

# MinGW GCC
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 2. Runtime Dependencies

### Always required

| File | Purpose |
|------|---------|
| `MedicalOCR.dll` | Main library |
| `libopencv_core-413.dll` | OpenCV core (exact name varies by version) |
| `libopencv_imgproc-413.dll` | OpenCV image processing |
| `libopencv_imgcodecs-413.dll` | OpenCV image codec |
| `config/medical_ocr.json` | Configuration |

### Optional

| File | Purpose |
|------|---------|
| `config/templates/*.json` | Hospital report templates |
| `libopencv_*.dll` | Other OpenCV modules (if linked) |
| `paddle_inference.dll` | Paddle Inference (only with PaddleOCR) |
| `libiomp5md.dll` | OpenMP runtime (only with PaddleOCR + MKL) |
| `mklml.dll` | Intel MKL (only with PaddleOCR) |
| `onnxruntime.dll` | ONNX Runtime (only with PaddleOCR) |
| `models/` | OCR model files (only with PaddleOCR) |

> **Finding OpenCV DLLs**: MSYS2 puts them in `/ucrt64/bin/`. vcpkg puts them in `vcpkg_installed/x64-windows/bin/`. VS build may statically link or put them next to the executable.

---

## 3. Directory Layout (Recommended)

### Minimal deployment (Mock engine)

```
MyApp/
├── MyApp.exe
├── MedicalOCR.dll
├── libopencv_core-413.dll
├── libopencv_imgproc-413.dll
├── libopencv_imgcodecs-413.dll
└── config/
    ├── medical_ocr.json
    └── templates/
        └── my_hospital_template.json
```

### Full deployment (PaddleOCR)

```
MyApp/
├── MyApp.exe
├── MedicalOCR.dll
├── libopencv_core-413.dll
├── libopencv_imgproc-413.dll
├── libopencv_imgcodecs-413.dll
├── paddle_inference.dll
├── libiomp5md.dll
├── mklml.dll
├── onnxruntime.dll
├── config/
│   ├── medical_ocr.json
│   └── templates/
│       └── my_hospital_template.json
└── models/
    ├── ch_PP-OCRv4_det_infer/
    │   ├── inference.pdmodel
    │   └── inference.pdiparams
    ├── ch_PP-OCRv4_rec_infer/
    │   ├── inference.pdmodel
    │   └── inference.pdiparams
    └── ppocr_keys_v1.txt
```

---

## 4. Production Configuration

Edit `config/medical_ocr.json` for production:

```jsonc
{
  "ocr": {
    "engine": "paddle",           // "mock" for testing, "paddle" for production
    "model_directory": "./models",
    "use_gpu": false,
    "cpu_threads": 4,             // Match your CPU cores (1–16)
    "minimum_confidence": 0.65    // Raise to 0.8 for higher precision
  },
  "preprocessing": {
    "enable_document_detection": true,
    "enable_perspective_correction": true,
    "enable_grayscale": false,
    "enable_contrast_enhancement": true,
    "enable_adaptive_threshold": false,
    "minimum_width": 800,
    "minimum_height": 800,
    "blur_threshold": 80.0
  },
  "privacy": {
    "mask_id_number": true,          // **Keep TRUE in production**
    "enable_debug_image_output": false,  // **Keep FALSE in production**
    "enable_raw_text_log": false        // **Keep FALSE in production**
  },
  "templates_directory": "./config/templates"
}
```

---

## 5. Thread Safety Guidelines

| Scenario | Recommendation |
|----------|---------------|
| Single-threaded app | Call `OCR_Init` once at startup, `OCR_Shutdown` at exit |
| Multi-threaded app | Init once; call `Recognize` from any thread |
| Max concurrency | Mock engine: unlimited. Paddle engine: `cpu_threads × 2` |
| Thread pool | Pre-init the library; workers call `Recognize` directly |
| Web server | Init on startup; call `RecognizeMemory` per request; `OCR_FreeResult` after response sent |

**Important**: Do NOT call `OCR_Init`/`OCR_Shutdown` concurrently. Serialize them or call once at app start/end.

---

## 6. Memory Management

| Pattern | Code |
|---------|------|
| Basic | `char* j; OCR_RecognizeFile(p, &j); ... OCR_FreeResult(j);` |
| NULL safety | `OCR_FreeResult(NULL)` — OK, no-op |
| Don't mix | Never `free()` or `delete[]` a result — always `OCR_FreeResult` |
| Leak check | Every `OCR_Recognize*` that returns `MEDOCR_OK` allocates memory |

**Estimated memory per call:**
- Mock engine: ~5 MB
- Paddle engine (CPU): ~50 MB + model weights (~500 MB shared)
- Large images (600 DPI A4): add ~100 MB

---

## 7. Performance Tuning

| Option | Effect | When to change |
|--------|--------|---------------|
| `cpu_threads: 1–16` | OCR inference threads | Match physical cores |
| `minimum_confidence: 0.8` | Filter low-confidence text | Reduce noise in output |
| `enable_document_detection: false` | Skip contour detection | Pre-cropped flatbed scans |
| `enable_perspective_correction: false` | Skip deskew | Already aligned documents |
| `enable_contrast_enhancement: false` | Skip CLAHE | High-quality scans > 200 DPI |
| `enable_denoise: false` | Skip Gaussian blur | Clean digital images |
| `enable_adaptive_threshold: false` | Skip binarization | Keep for photos of paper reports |
| `blur_threshold: 50` | Lower blur rejection | Accept slightly blurry images |

---

## 8. Error Handling Patterns

### C

```c
int rc = OCR_RecognizeFile(path, &json);
switch (rc) {
case 0:     // MEDOCR_OK
    ProcessResult(json);
    OCR_FreeResult(json);
    break;
case 1001:  // Not initialized
    OCR_Init(NULL, config);
    break;
case 2001:  // File not found
    LogError("File missing: %s", path);
    break;
case 2004:  // Quality failed
    LogWarning("Image quality insufficient: %s", OCR_GetLastError());
    break;
default:
    LogError("Error %d: %s", rc, OCR_GetLastError());
}
```

### C++ (RAII)

```cpp
std::unique_ptr<char, decltype(&OCR_FreeResult)> result(nullptr, OCR_FreeResult);
char* raw = nullptr;
if (OCR_RecognizeFile(path, &raw) == 0) {
    result.reset(raw);
    std::cout << result.get() << std::endl;
}
// Auto-freed by unique_ptr.
```

---

## 9. Security Checklist

| # | Item | Status |
|---|------|--------|
| 1 | `mask_id_number: true` in production config | ☐ |
| 2 | `enable_debug_image_output: false` | ☐ |
| 3 | `enable_raw_text_log: false` | ☐ |
| 4 | No network connectivity in deployment environment | ☐ |
| 5 | Medical data handled per local regulations (PIPL, HIPAA, etc.) | ☐ |
| 6 | Temporary files cleaned after processing | ☐ |
| 7 | DLL signature verification (optional, via Authenticode) | ☐ |
| 8 | Access control on `config/` and `models/` directories | ☐ |
| 9 | Audit log of all `Recognize` calls (caller's responsibility) | ☐ |
| 10 | Input image retention policy (caller's responsibility) | ☐ |

---

## 10. Pre-Production Validation

| Check | How to verify |
|-------|---------------|
| Output structure | Run 100+ test images, diff JSON against expected schema |
| ID masking | Verify ID numbers show `4403**********1234` format |
| Extraction accuracy | Compare extracted fields against ground-truth annotations |
| Poor image handling | Test with blurry, skewed, low-contrast, and overexposed images |
| Concurrent load | Run 10+ threads × 100 iterations, check for crashes |
| Memory stability | Process 1000 images, monitor RAM (should not grow unboundedly) |
| Temp file cleanup | Check %TEMP% after processing — no leftover files |
| Error recovery | Simulate: missing file, corrupt image, wrong config, model not found |

---

## 11. Packaging for Distribution

```powershell
# Create a distributable ZIP
.\scripts\package_release.ps1 -BuildDir .\build -Version 1.0.0

# Output:
#   release\MedicalOCR-1.0.0-windows-x64\   (directory)
#   release\MedicalOCR-1.0.0-windows-x64.zip (archive)
```

The package includes: DLL, headers, config, templates, docs, examples, model download script.

### Manual packaging

1. Copy `MedicalOCR.dll` + `MedicalOCR.lib` from `build/bin/`
2. Copy OpenCV DLLs from their install location
3. Copy `include/medical_ocr/*.h`
4. Copy `config/medical_ocr.json` + `config/templates/`
5. Copy `docs/` for reference
6. Optionally copy `scripts/download_models.ps1`

---

## 12. Common Deployment Issues

| Problem | Solution |
|---------|----------|
| **DLL not found (0xc0000135)** | Place MedicalOCR.dll + OpenCV DLLs next to .exe or in PATH |
| **"Not initialized" (1001)** | Call `OCR_Init` before `OCR_Recognize*` |
| **High memory** | Reduce `cpu_threads`; process images one at a time |
| **Slow processing** | Disable `perspective_correction` for flatbed scans; reduce image DPI |
| **Chinese text garbled** | Ensure UTF-8 encoding in your code (not GBK/ANSI) |
| **Template not matching** | Enable `enable_raw_text_log` to see OCR output; adjust keywords |
| **Paddle DLL dependencies** | Ensure `paddle_inference.dll`, `libiomp5md.dll`, `mklml.dll`, `onnxruntime.dll` are findable |
| **Model files not found** | Check `model_directory` path; models must be at `{model_directory}/ch_PP-OCRv4_det_infer/inference.pdmodel` |
| **OpenCV DLL version mismatch** | Match OpenCV DLL version to the version used during build |
| **CRT mismatch (/MT vs /MD)** | Use `MEDICAL_OCR_STATIC_RUNTIME=ON` to avoid MSVC runtime DLL dependency |

---

## 13. Compliance Notes

This library processes **medical data** and **personally identifiable information** (Chinese ID numbers). Callers are responsible for:

- **PIPL (个人信息保护法)** compliance for patient data processing
- **HIPAA** (if applicable) for medical record handling
- Data retention policies — the library does not store results after `OCR_FreeResult`
- Input image disposal — caller must delete source images per their retention policy
- Access logging — caller should log who accessed which report
- Encryption — caller should encrypt data at rest and in transit (the DLL itself has no network capability)
