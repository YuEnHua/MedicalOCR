# Deployment Guide

## Production Deployment Checklist

### 1. Build Release DLL

```powershell
# Visual Studio 2022
cmake -S . -B build -A x64
cmake --build build --config Release

# MinGW
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 2. Runtime Dependencies

The following files must be available at runtime:

| File | Required | Notes |
|------|----------|-------|
| `MedicalOCR.dll` | Yes | Main library |
| `config/medical_ocr.json` | Yes | Configuration |
| `config/templates/*.json` | Optional | Hospital report templates |
| OpenCV DLLs | Yes | `libopencv_core*.dll`, `libopencv_imgproc*.dll`, `libopencv_imgcodecs*.dll` |
| Paddle Inference DLLs | Only with Paddle | `paddle_inference.dll`, `libiomp5md.dll`, `mklml.dll`, `onnxruntime.dll` |
| OCR models | Only with Paddle | `models/ch_PP-OCRv4_det_infer/`, `models/ch_PP-OCRv4_rec_infer/`, `models/ppocr_keys_v1.txt` |

### 3. Directory Layout (Recommended)

```
YourApp/
├── YourApp.exe
├── MedicalOCR.dll
├── libopencv_core-413.dll
├── libopencv_imgproc-413.dll
├── libopencv_imgcodecs-413.dll
├── config/
│   ├── medical_ocr.json
│   └── templates/
│       └── sample_template.json
└── models/                         (only for PaddleOCR)
    ├── ch_PP-OCRv4_det_infer/
    ├── ch_PP-OCRv4_rec_infer/
    └── ppocr_keys_v1.txt
```

### 4. Configuration for Production

Edit `config/medical_ocr.json`:

```jsonc
{
  "ocr": {
    "engine": "mock",         // Change to "paddle" for real OCR
    "model_directory": "./models",
    "cpu_threads": 4,         // Match your CPU core count
    "minimum_confidence": 0.65
  },
  "preprocessing": {
    "enable_perspective_correction": true,
    "target_width": 2480,     // A4 at 300 DPI
    "target_height": 3508
  },
  "privacy": {
    "mask_id_number": true,   // Mask patient ID numbers
    "enable_debug_image_output": false,
    "enable_raw_text_log": false
  }
}
```

### 5. Thread Safety Guidelines

- `OCR_Init` / `OCR_Shutdown`: Call once at application start/end
- `OCR_RecognizeFile` / `OCR_RecognizeMemory`: Can be called from multiple threads concurrently
- Each recognition call processes one image independently
- For high-throughput scenarios, limit concurrent calls to `cpu_threads` × 2
- The Mock engine works with unlimited concurrency
- The Paddle engine shares model weights (read-only), so concurrent calls are safe

### 6. Memory Management

- **Always free results**: Every successful `OCR_Recognize*` call allocates memory that must be freed with `OCR_FreeResult`
- **Don't mix allocators**: Never `delete` or `free()` a result pointer — always use `OCR_FreeResult`
- **NULL is safe**: `OCR_FreeResult(NULL)` is a no-op
- **Memory per call**: ~1–10 MB depending on image size and OCR engine
- **Peak memory with Paddle**: ~500 MB (model weights) + ~50 MB per concurrent call

### 7. Performance Tuning

| Parameter | Effect |
|-----------|--------|
| `cpu_threads` | Number of Paddle Inference threads (1–16) |
| `minimum_confidence` | Raise to 0.8 to filter low-confidence text |
| `enable_document_detection` | Disable if documents are already aligned |
| `enable_perspective_correction` | Disable for flatbed scans |
| `enable_contrast_enhancement` | Disable for high-quality scans |
| `enable_denoise` | Disable for clean digital images |

### 8. Error Handling

```c
int rc = OCR_RecognizeFile(path, &json);
switch (rc) {
    case MEDOCR_OK:                // Success
        ProcessJson(json);
        OCR_FreeResult(json);
        break;
    case MEDOCR_ERR_NOT_INITIALIZED: // Call OCR_Init first
        OCR_Init(NULL, config);
        break;
    case MEDOCR_ERR_FILE_NOT_FOUND:  // Check file path
        LogError("File not found: %s", path);
        break;
    case MEDOCR_ERR_IMAGE_QUALITY_FAILED: // Image too blurry/dark
        LogWarning("Image quality insufficient");
        break;
    default:
        LogError("Error %d: %s", rc, OCR_GetLastError());
}
```

### 9. Security Checklist

- [ ] `mask_id_number: true` in production config
- [ ] `enable_debug_image_output: false`
- [ ] `enable_raw_text_log: false`
- [ ] No network access in deployment environment
- [ ] DLL signature verification (optional)
- [ ] Medical data handled per local regulations (HIPAA, PIPL, etc.)
- [ ] Temporary files cleaned after processing
- [ ] Access control on model/config directories

### 10. Validation Before Go-Live

1. Run 100+ test images through the pipeline
2. Verify JSON output structure matches expectations
3. Check ID number masking is active
4. Validate extraction accuracy for your report types
5. Test with poor-quality images (blurry, skewed, low contrast)
6. Load test: process 10+ images concurrently
7. Memory leak check: process 1000 images, monitor memory
8. Verify no temp files left after processing

### 11. Packaging for Distribution

```powershell
# Create a release package
.\scripts\package_release.ps1 -BuildDir .\build -Version 1.0.0

# Output: release\MedicalOCR-1.0.0-windows-x64.zip
```

### 12. Common Issues

| Problem | Solution |
|---------|----------|
| DLL not found | Place MedicalOCR.dll + OpenCV DLLs next to your exe |
| "Not initialized" | Call OCR_Init before any Recognize call |
| High memory usage | Reduce cpu_threads or process images sequentially |
| Slow on large images | Enable perspective correction to crop to document area |
| Chinese text garbled | Ensure UTF-8 encoding for all string parameters |
| Template not matching | Add hospital-specific keywords to template JSON |
