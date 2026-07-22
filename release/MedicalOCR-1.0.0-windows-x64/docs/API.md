# Medical OCR — API Reference

## Overview

The Medical OCR DLL provides a C ABI for offline medical report recognition.
All functions use UTF-8 encoding. Strings allocated by the DLL must be freed
with `OCR_FreeResult`.

## Lifecycle Functions

### OCR_Init

```c
int OCR_Init(const char* model_directory, const char* config_path);
```

Initialize the library. Must be called once before any recognition function.

- `model_directory`: Path to OCR model files (may be NULL for mock engine).
- `config_path`: Path to `medical_ocr.json` (may be NULL for defaults).
- Returns: `MEDOCR_OK` (0) on success, error code otherwise.

Calling `OCR_Init` again without a prior `OCR_Shutdown` returns
`MEDOCR_ERR_ALREADY_INITIALIZED` (1005).

### OCR_Shutdown

```c
void OCR_Shutdown(void);
```

Release all resources. Safe to call without a prior `OCR_Init`.
After this call, `OCR_Init` must be called again before recognition.

### OCR_GetVersion

```c
const char* OCR_GetVersion(void);
```

Returns the library version string (e.g., "1.0.0"). The pointer is valid
for the lifetime of the library. Do not free.

### OCR_GetLastError

```c
const char* OCR_GetLastError(void);
```

Returns a human-readable description of the last error. The pointer is valid
until the next API call on the same thread. Do not free.

## Recognition Functions

### OCR_RecognizeFile

```c
int OCR_RecognizeFile(const char* image_path_utf8, char** output_json_utf8);
```

Recognize text from an image file.

- `image_path_utf8`: Path to the image file (UTF-8). Supports JPEG, PNG, BMP, TIFF.
- `output_json_utf8`: Output parameter. On success, receives a UTF-8 JSON string
  allocated by the DLL. **Must be freed with `OCR_FreeResult`.**
- Returns: `MEDOCR_OK` on success.

### OCR_RecognizeMemory

```c
int OCR_RecognizeMemory(const unsigned char* image_data, int image_size, char** output_json_utf8);
```

Recognize text from in-memory image data.

- `image_data`: Pointer to raw image bytes.
- `image_size`: Size of data in bytes.
- `output_json_utf8`: Same as `OCR_RecognizeFile`.
- Returns: `MEDOCR_OK` on success.

### OCR_FreeResult

```c
void OCR_FreeResult(char* output_json_utf8);
```

Free a result string from `OCR_RecognizeFile` or `OCR_RecognizeMemory`.
Safe to call with NULL.

## Output JSON Format

```json
{
  "success": true,
  "error_code": 0,
  "message": "ok",
  "document": {
    "type": "ultrasound_report",
    "template_id": "sample_hospital_ultrasound_v1",
    "page_type": "A4",
    "image_width": 2480,
    "image_height": 3508
  },
  "patient": {
    "name": {
      "value": "张三",
      "confidence": 0.98,
      "validation_status": "valid"
    }
  },
  "examination": {
    "hospital_name": { "value": "..." },
    "findings": { "value": "..." }
  },
  "quality": {
    "blur_score": 125.6,
    "is_blurry": false
  },
  "warnings": []
}
```

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | MEDOCR_OK | Success |
| 1001 | MEDOCR_ERR_NOT_INITIALIZED | Library not initialized |
| 1002 | MEDOCR_ERR_INIT_FAILED | Initialization failed |
| 1003 | MEDOCR_ERR_CONFIG_NOT_FOUND | Config file not found |
| 1004 | MEDOCR_ERR_MODEL_LOAD_FAILED | Model loading failed |
| 1005 | MEDOCR_ERR_ALREADY_INITIALIZED | Already initialized |
| 2001 | MEDOCR_ERR_FILE_NOT_FOUND | Image file not found |
| 2002 | MEDOCR_ERR_UNSUPPORTED_FORMAT | Unsupported format |
| 2003 | MEDOCR_ERR_IMAGE_DECODE_FAILED | Image decode failed |
| 2004 | MEDOCR_ERR_IMAGE_QUALITY_FAILED | Quality check failed |
| 3001 | MEDOCR_ERR_OCR_FAILED | OCR recognition failed |
| 3002 | MEDOCR_ERR_OCR_NO_RESULT | No OCR results |
| 4001 | MEDOCR_ERR_EXTRACTION_FAILED | Extraction failed |
| 4002 | MEDOCR_ERR_NO_TEMPLATE_MATCH | No template matched |
| 5001 | MEDOCR_ERR_OUTPUT_ALLOC_FAILED | Output allocation failed |
| 9000 | MEDOCR_ERR_UNKNOWN | Unknown error |
| 9001 | MEDOCR_ERR_INVALID_ARGUMENT | Invalid argument |
| 9002 | MEDOCR_ERR_INTERNAL_EXCEPTION | Internal exception |

## Thread Safety

- `OCR_Init` and `OCR_Shutdown` are serialized (mutex protected).
- `OCR_RecognizeFile` and `OCR_RecognizeMemory` are reentrant.
- Concurrent calls to recognition functions from multiple threads are supported
  (subject to the underlying OCR engine's thread safety).
- `OCR_GetLastError` returns thread-local error information.
- Global state does not contain patient data.

## Usage Example

```c
#include "medical_ocr/medical_ocr_c_api.h"
#include <stdio.h>

int main() {
    // Initialize with default mock engine
    int rc = OCR_Init(NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "Init failed: %s\n", OCR_GetLastError());
        return rc;
    }

    printf("Version: %s\n", OCR_GetVersion());

    // Recognize
    char* json = NULL;
    rc = OCR_RecognizeFile("report.jpg", &json);
    if (rc == 0) {
        printf("Result:\n%s\n", json);
        OCR_FreeResult(json);
    } else {
        fprintf(stderr, "Error: %s\n", OCR_GetLastError());
    }

    OCR_Shutdown();
    return rc;
}
```
