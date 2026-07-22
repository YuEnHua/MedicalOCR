# Windows Build Guide

## Prerequisites

### Required
- **Windows 10/11 x64**
- **Visual Studio 2022** (Community, Professional, or Enterprise)
  - Workload: "Desktop development with C++"
  - Windows 10/11 SDK
- **CMake** 3.16+ (included with Visual Studio, or download from cmake.org)
- **Git** (for FetchContent to download dependencies)

### Optional (Future Phases)
- **OpenCV** 4.x — for image preprocessing (Phase 2+)
- **Paddle Inference** — for real OCR (Phase 4+)

## Quick Build (Phase 1 — Mock Engine Only)

Open a **Developer PowerShell for VS 2022** or **x64 Native Tools Command Prompt**:

```powershell
# Navigate to the project root
cd MedicalOCR

# Configure
cmake -S . -B build -A x64

# Build Release
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release
```

### Build Options

```powershell
cmake -S . -B build -A x64 `
    -DMEDICAL_OCR_BUILD_TESTS=ON `
    -DMEDICAL_OCR_BUILD_CLI=ON `
    -DMEDICAL_OCR_ENABLE_PADDLE=OFF `
    -DMEDICAL_OCR_ENABLE_ONNX=OFF `
    -DMEDICAL_OCR_STATIC_RUNTIME=OFF
```

| Option | Default | Description |
|--------|---------|-------------|
| MEDICAL_OCR_BUILD_TESTS | ON | Build GoogleTest-based unit tests |
| MEDICAL_OCR_BUILD_CLI | ON | Build CLI demo application |
| MEDICAL_OCR_ENABLE_PADDLE | OFF | Enable PaddleOCR integration |
| MEDICAL_OCR_ENABLE_ONNX | OFF | Enable ONNX Runtime integration |
| MEDICAL_OCR_STATIC_RUNTIME | OFF | Link MSVC runtime statically (/MT) |

## Build Outputs

After a successful Release build:

```
build/
├── bin/
│   └── Release/
│       ├── MedicalOCR.dll          # Main DLL
│       ├── MedicalOCR.lib          # Import library
│       ├── medical_ocr_cli.exe     # CLI demo
│       ├── config/                 # Configuration files
│       └── samples/                # Sample data
└── lib/
    └── Release/
        └── MedicalOCR.lib          # Import library (copy)
```

Debug build outputs to `build/bin/Debug/`.

## Testing

```powershell
# Run all tests
ctest --test-dir build -C Release --output-on-failure

# Run a specific test
ctest --test-dir build -C Release -R "DllApiTest"
```

## Using the DLL in Another Project

### CMake

```cmake
find_library(MEDICAL_OCR_LIB MedicalOCR PATHS "path/to/build/bin/Release")
add_executable(myapp main.cpp)
target_link_libraries(myapp ${MEDICAL_OCR_LIB})
target_include_directories(myapp PRIVATE "path/to/MedicalOCR/include")
```

### Manual

1. Include `medical_ocr_c_api.h`, `types.h`, `error_codes.h` in your project
2. Link against `MedicalOCR.lib`
3. Place `MedicalOCR.dll` next to your executable or in PATH
4. Place `config/medical_ocr.json` in your working directory

## Future: Building with PaddleOCR (Phase 4)

When PaddleOCR integration is complete:

```powershell
# 1. Download Paddle Inference C++ library from:
#    https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html
# 2. Extract to C:\paddle_inference

cmake -S . -B build -A x64 `
    -DMEDICAL_OCR_ENABLE_PADDLE=ON `
    -DPADDLE_INFERENCE_DIR="C:/paddle_inference"

# Copy Paddle DLLs to output directory
# paddle_inference.dll, paddle_*.dll
```

## Troubleshooting

### CMake cannot find nlohmann/json
The first configure will download nlohmann/json via FetchContent. Ensure you
have an internet connection for the initial CMake configure.

### "Cannot open include file: 'nlohmann/json.hpp'"
This means FetchContent didn't complete. Delete the build directory and
reconfigure with a working internet connection.

### Test failures with file not found
Tests expect to run with the project root as the working directory.
The CMake configuration copies config files to the build output automatically.

### Chinese path issues
All API functions accept UTF-8 paths. On Windows, the DLL internally converts
to wide strings via `MultiByteToWideChar(CP_UTF8, ...)`.
