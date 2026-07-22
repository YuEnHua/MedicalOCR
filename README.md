# Medical OCR — Local Offline Medical Report Recognition

A Windows 10/11 x64 native C++ DLL for offline OCR of medical examination
reports (A4/A5). Supports Chinese, English, and numeric text recognition
with structured JSON output including patient and examination information.

## Features

- **Fully offline** — No cloud APIs, no HTTP requests, no telemetry
- **C ABI DLL** — Can be called from C, C++, C#, Python, and other languages
- **Multiple OCR backends** — Pluggable engine architecture (IOcrEngine)
- **Structured output** — Extracted patient info, exam details, quality metrics
- **Privacy-first** — Default ID masking, configurable debug output, no data exfiltration
- **Template-based extraction** — Configurable hospital report templates

## Current Status — Phase 1

✅ Project structure and build system
✅ Type definitions and error codes
✅ C ABI DLL interface (7 exported functions)
✅ IOcrEngine abstraction layer
✅ MockOcrEngine with sample data
✅ MedicalOcrService singleton
✅ JSON result builder
✅ CLI demo application
✅ Unit tests (DLL API lifecycle)
✅ Configuration file support
✅ Sample template definition

⏳ Phase 2: Image preprocessing
⏳ Phase 3: Field extraction pipeline
⏳ Phase 4: PaddleOCR integration
⏳ Phase 5: Optimization and packaging

## Quick Start

### Build

```powershell
# Configure (generates Visual Studio 2022 solution)
cmake -S . -B build -A x64

# Build Release
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release
```

### Run CLI Demo

```powershell
.\build\bin\Release\medical_ocr_cli.exe sample.jpg config\medical_ocr.json
```

### Use the DLL

```c
#include "medical_ocr/medical_ocr_c_api.h"

OCR_Init(NULL, "config/medical_ocr.json");

char* json = NULL;
int rc = OCR_RecognizeFile("report.jpg", &json);
if (rc == 0) {
    printf("%s\n", json);
    OCR_FreeResult(json);
}

OCR_Shutdown();
```

## Project Structure

```
MedicalOCR/
├── CMakeLists.txt              # Root build configuration
├── cmake/Dependencies.cmake    # Third-party dependency management
├── include/medical_ocr/        # Public C API headers
│   ├── medical_ocr_c_api.h     # DLL exported functions
│   ├── types.h                 # C++ data types
│   └── error_codes.h           # Error code definitions
├── src/
│   ├── api/                    # C API implementation
│   ├── core/                   # Core service layer
│   ├── ocr/                    # OCR engine interface + implementations
│   │   ├── i_ocr_engine.h      # Abstract OCR engine interface
│   │   └── mock_ocr_engine.*   # Mock engine for testing
│   ├── image/                  # Image preprocessing (Phase 2)
│   ├── document/               # Document classification (Phase 3)
│   ├── extraction/             # Field extraction (Phase 3)
│   ├── validation/             # Field validation (Phase 3)
│   └── common/                 # Shared utilities
├── apps/cli_demo/              # CLI demo application
├── tests/                      # Unit tests
├── config/                     # Configuration files
│   ├── medical_ocr.json        # Main configuration
│   └── templates/              # Hospital report templates
├── samples/                    # Sample data for testing
├── models/                     # OCR model files directory
└── docs/                       # Documentation
```

## Configuration

See `config/medical_ocr.json` for all configuration options.

Key settings:
- `ocr.engine` — "mock" (default) or "paddle" (Phase 4)
- `ocr.minimum_confidence` — Minimum confidence threshold
- `privacy.mask_id_number` — Mask ID numbers in output
- `privacy.enable_debug_image_output` — Save debug images (disabled by default)

## Error Codes

| Code | Description |
|------|-------------|
| 0    | Success |
| 1001 | Not initialized |
| 1002 | Initialization failed |
| 1003 | Configuration file not found |
| 1004 | Model loading failed |
| 2001 | Image file not found |
| 3001 | OCR recognition failed |
| 9000 | Unknown error |

See `include/medical_ocr/error_codes.h` for the complete list.

## Privacy & Security

- No network requests — fully offline
- No telemetry, crash reports, or auto-updates
- Patient ID numbers masked by default
- Debug output requires explicit configuration
- No raw patient data in logs by default
- Caller is responsible for medical data compliance

## Documentation

- [API Reference](docs/API.md)
- [Windows Build Guide](docs/BUILD_WINDOWS.md)
- [Template Format](docs/TEMPLATE_FORMAT.md)
- [Architecture Overview](docs/ARCHITECTURE.md)

## License

This project is provided for educational and reference purposes.
Ensure compliance with local medical data regulations before use.
