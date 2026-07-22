# Medical OCR — API Reference

## Overview

| Property | Value |
|----------|-------|
| ABI | C (pure C calling convention) |
| Encoding | UTF-8 for all string parameters |
| Library | `MedicalOCR.dll` + `MedicalOCR.lib` |
| Threads | Init/Shutdown serialized; Recognize reentrant |
| Errors | Integer error codes + thread-local `OCR_GetLastError()` |

## Lifecycle Functions

### OCR_Init

```c
int OCR_Init(const char* model_directory, const char* config_path);
```

Initializes the library. Must be called **once** before any `Recognize` call.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `model_directory` | `const char*` (UTF-8) | No | Path to OCR model files. `NULL` for Mock engine (uses built-in sample data). |
| `config_path` | `const char*` (UTF-8) | No | Path to `medical_ocr.json`. `NULL` for defaults (mock engine, no templates). |

**Returns**: `MEDOCR_OK` (0) on success, or error code.

**Errors**:
- `1003` — config file not found
- `1004` — model files missing/corrupt
- `1005` — already initialized (call `OCR_Shutdown` first)

---

### OCR_Shutdown

```c
void OCR_Shutdown(void);
```

Releases the OCR engine, loaded templates, and all internal resources. **Safe to call without a prior `OCR_Init`** (no-op). **Safe to call multiple times**.

After `OCR_Shutdown`, you must call `OCR_Init` again before any `Recognize`.

---

### OCR_GetVersion

```c
const char* OCR_GetVersion(void);
```

Returns the library version (e.g. `"1.0.0"`). The pointer is valid for the lifetime of the DLL. **Do not free**. Always available (even before `OCR_Init`).

---

### OCR_GetLastError

```c
const char* OCR_GetLastError(void);
```

Returns a human-readable UTF-8 description of the **last error on the calling thread**. The pointer is valid until the **next API call from the same thread**. **Do not free**.

---

## Recognition Functions

### OCR_RecognizeFile

```c
int OCR_RecognizeFile(const char* image_path_utf8, char** output_json_utf8);
```

Recognizes a medical report from an image file on disk.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `image_path_utf8` | in | Absolute or relative path (UTF-8). Supports JPEG, PNG, BMP, TIFF. On Windows, wide-character paths are handled internally. |
| `output_json_utf8` | out | Receives a pointer to a UTF-8 JSON string **allocated by the DLL**. Caller **must** free via `OCR_FreeResult`. Set to `NULL` on failure. |

**Returns**: `MEDOCR_OK` on success.

**Pipeline** (transparent to caller):
1. Decode image (OpenCV `imdecode`)
2. Quality check (blur, exposure, resolution)
3. Preprocessing (grayscale, denoise, CLAHE, perspective correction)
4. OCR recognition
5. Document classification + template matching
6. Field extraction + validation
7. JSON build (with ID masking if configured)

---

### OCR_RecognizeMemory

```c
int OCR_RecognizeMemory(const unsigned char* image_data, int image_size,
                        char** output_json_utf8);
```

Recognizes a medical report from **in-memory image bytes** (e.g., camera capture, network stream, embedded resource).

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `image_data` | in | Raw image bytes (JPEG, PNG, BMP, TIFF). Not modified. |
| `image_size` | in | Byte count. Must be > 0. |
| `output_json_utf8` | out | Same as `OCR_RecognizeFile`. |

Same pipeline as `OCR_RecognizeFile`, but skips file I/O.

**Example — from memory:**
```c
// Read file into memory, then recognize.
FILE* f = fopen("report.jpg", "rb");
fseek(f, 0, SEEK_END);
long sz = ftell(f);
fseek(f, 0, SEEK_SET);
unsigned char* buf = malloc(sz);
fread(buf, 1, sz, f);
fclose(f);

char* json = NULL;
int rc = OCR_RecognizeMemory(buf, (int)sz, &json);
free(buf);
// ... use json ...
OCR_FreeResult(json);
```

---

### OCR_FreeResult

```c
void OCR_FreeResult(char* output_json_utf8);
```

Frees a string returned by `OCR_RecognizeFile` or `OCR_RecognizeMemory`. **Safe with `NULL`** (no-op).

> **Never** use `free()` or `delete` on result strings. Always use `OCR_FreeResult`.

---

## Output JSON Reference

### Top-level

| Key | Type | Always present | Description |
|-----|------|---------------|-------------|
| `success` | bool | Yes | `true` if the pipeline completed |
| `error_code` | int | Yes | 0 = success; see error codes |
| `message` | string | Yes | Short status message |
| `document` | object | Yes | Document classification |
| `patient` | object | On success | Extracted patient info |
| `examination` | object | On success | Extracted examination info |
| `quality` | object | Yes | Image quality metrics |
| `warnings` | string[] | Yes | Diagnostic messages (may be empty) |

### `document` object

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | One of: `ultrasound_report`, `ct_report`, `mri_report`, `xray_report`, `laboratory_report`, `unknown` |
| `template_id` | string | Matched template ID (empty if none) |
| `page_type` | string | `"A4"`, `"A5"`, or `"unknown"` |
| `image_width` | int | Pixels |
| `image_height` | int | Pixels |

### `patient` object — per-field structure

Each patient field has the same sub-structure:

```json
{
  "value": "张三",
  "raw_value": "张三",
  "confidence": 0.98,
  "validation_status": "valid",
  "extraction_method": "anchor_right"
}
```

| Sub-key | Type | Always | Description |
|---------|------|--------|-------------|
| `value` | string/number | Yes | Normalized/cleaned value |
| `raw_value` | string | On demand | Original OCR text (only if differs from `value`) |
| `confidence` | float | Yes | Extraction confidence [0.0, 1.0] |
| `validation_status` | string | Yes | `valid`, `invalid`, `uncertain`, `not_validated` |
| `extraction_method` | string | Optional | `anchor_right`, `anchor_below`, `paragraph_between_anchors`, `regex`, `default` |
| `checksum_valid` | bool | ID only | Whether the 18-digit checksum passed |

### Patient fields

| Field | `data_type` | Validation |
|-------|-------------|------------|
| `name` | string | None |
| `gender` | gender | Normalized to `"男"` / `"女"` |
| `birth_date` | date | Normalized to `YYYY-MM-DD` |
| `age` | age | Integer, range 0–150 |
| `id_number` | id_number | GB 11643-1999 checksum, masked by default |

### Examination fields

| Field | `data_type` | Extraction method |
|-------|-------------|-------------------|
| `hospital_name` | string | anchor_right |
| `report_type` | string | anchor_right |
| `exam_name` | string | anchor_right |
| `exam_date` | date | anchor_right |
| `department` | string | anchor_right |
| `findings` | text | paragraph_between_anchors |
| `impression` | text | paragraph_between_anchors |
| `report_doctor` | string | anchor_right |
| `review_doctor` | string | anchor_right |

### `quality` object

| Key | Type | Description |
|-----|------|-------------|
| `blur_score` | float | Laplacian variance (higher = sharper). Typical threshold: 80. |
| `is_blurry` | bool | `true` if below blur threshold |
| `is_overexposed` | bool | `true` if > 15% pixels near 255 |
| `document_detected` | bool | `false` for empty/all-white images |

### `warnings` array

Contains human-readable diagnostic strings. Examples:

- `"Image is blurry (Laplacian variance: 45, threshold: 80)"`
- `"Matched template: sample_hospital_ultrasound_v1 (score: 45)"`
- `"Birth date conflict: ID number implies '1988-09-21' but extracted value is '1988-03-15'"`
- `"No template matched. Document type: unknown"`

---

## Complete Error Codes

| Code | Symbol | Meaning | Recovery |
|------|--------|---------|----------|
| 0 | `MEDOCR_OK` | Success | — |
| **Init (1000s)** | | | |
| 1001 | `MEDOCR_ERR_NOT_INITIALIZED` | `OCR_Init` not called | Call `OCR_Init` first |
| 1002 | `MEDOCR_ERR_INIT_FAILED` | Generic init failure | Check `OCR_GetLastError()` |
| 1003 | `MEDOCR_ERR_CONFIG_NOT_FOUND` | Config file missing | Verify path, check permissions |
| 1004 | `MEDOCR_ERR_MODEL_LOAD_FAILED` | Model files missing | Check `model_directory`, download models |
| 1005 | `MEDOCR_ERR_ALREADY_INITIALIZED` | Double init | Call `OCR_Shutdown` first |
| **Input (2000s)** | | | |
| 2001 | `MEDOCR_ERR_FILE_NOT_FOUND` | Image file missing | Verify path exists |
| 2002 | `MEDOCR_ERR_UNSUPPORTED_FORMAT` | Format not JPEG/PNG/BMP/TIFF | Convert image |
| 2003 | `MEDOCR_ERR_IMAGE_DECODE_FAILED` | OpenCV cannot decode | Check file integrity |
| 2004 | `MEDOCR_ERR_IMAGE_QUALITY_FAILED` | Too blurry/small/empty | Rescan at higher resolution |
| **OCR (3000s)** | | | |
| 3001 | `MEDOCR_ERR_OCR_FAILED` | Engine error | Check `OCR_GetLastError()` |
| 3002 | `MEDOCR_ERR_OCR_NO_RESULT` | No text detected | Check image has visible text |
| **Extraction (4000s)** | | | |
| 4001 | `MEDOCR_ERR_EXTRACTION_FAILED` | Field extraction error | Template may need adjustment |
| 4002 | `MEDOCR_ERR_NO_TEMPLATE_MATCH` | No template matched | Add hospital-specific template |
| **Output (5000s)** | | | |
| 5001 | `MEDOCR_ERR_OUTPUT_ALLOC_FAILED` | Memory allocation failed | Free memory, reduce image size |
| **General (9000s)** | | | |
| 9000 | `MEDOCR_ERR_UNKNOWN` | Unknown internal error | Report bug |
| 9001 | `MEDOCR_ERR_INVALID_ARGUMENT` | NULL/invalid parameter | Fix caller code |
| 9002 | `MEDOCR_ERR_INTERNAL_EXCEPTION` | C++ exception caught | Check `OCR_GetLastError()` |

---

## Thread Safety

| Function | Safety | Notes |
|----------|--------|-------|
| `OCR_Init` | Serialized (mutex) | One thread at a time |
| `OCR_Shutdown` | Serialized (mutex) | One thread at a time |
| `OCR_RecognizeFile` | Reentrant | Multiple threads OK (reads config, delegates to engine) |
| `OCR_RecognizeMemory` | Reentrant | Multiple threads OK |
| `OCR_GetLastError` | Thread-local | Each thread sees its own error |
| `OCR_GetVersion` | Immutable | Always safe |
| `OCR_FreeResult` | Always safe | Thread-safe even with NULL |

**Concurrent workload**: Tested with 4 threads × 3 iterations (12 concurrent `Recognize` calls). See `tests/test_concurrency.cpp`.

---

## Integration Examples

### C

```c
#include "medical_ocr/medical_ocr_c_api.h"
#include <stdio.h>

int main() {
    int rc = OCR_Init(NULL, "config/medical_ocr.json");
    if (rc) { fprintf(stderr, "Init: %s\n", OCR_GetLastError()); return rc; }

    char* json = NULL;
    rc = OCR_RecognizeFile("report.jpg", &json);
    if (rc == 0) {
        printf("%s\n", json);
        OCR_FreeResult(json);
    } else {
        fprintf(stderr, "Error: %s\n", OCR_GetLastError());
    }

    OCR_Shutdown();
    return rc;
}
```

### C++ (RAII)

```cpp
#include "medical_ocr/medical_ocr_c_api.h"
#include <memory>
#include <stdexcept>

struct OcrGuard {
    OcrGuard(const char* cfg) { if (int rc = OCR_Init(nullptr, cfg)) throw std::runtime_error(OCR_GetLastError()); }
    ~OcrGuard() { OCR_Shutdown(); }
};

int main() {
    OcrGuard guard("config/medical_ocr.json");
    char* json = nullptr;
    if (OCR_RecognizeFile("report.jpg", &json) == 0) {
        std::cout << json << std::endl;
        OCR_FreeResult(json);
    }
}
```

### C# (P/Invoke)

```csharp
[DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
static extern int OCR_Init(IntPtr modelDir, string configPath);

[DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
static extern int OCR_RecognizeFile(string path, out IntPtr json);

[DllImport("MedicalOCR.dll", CallingConvention = CallingConvention.Cdecl)]
static extern void OCR_FreeResult(IntPtr json);

// Usage:
OCR_Init(IntPtr.Zero, "config/medical_ocr.json");
IntPtr ptr;
OCR_RecognizeFile("report.jpg", out ptr);
string result = Marshal.PtrToStringAnsi(ptr);
OCR_FreeResult(ptr);
```

### Python (ctypes)

```python
import ctypes
ocr = ctypes.CDLL("MedicalOCR.dll")
ocr.OCR_Init.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
ocr.OCR_Init.restype = ctypes.c_int
ocr.OCR_RecognizeFile.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
ocr.OCR_RecognizeFile.restype = ctypes.c_int
ocr.OCR_FreeResult.argtypes = [ctypes.c_char_p]

ocr.OCR_Init(None, b"config/medical_ocr.json")
out = ctypes.c_char_p()
ocr.OCR_RecognizeFile(b"report.jpg", ctypes.byref(out))
print(out.value.decode("utf-8"))
ocr.OCR_FreeResult(out)
```

See `examples/` directory for complete, runnable examples.
