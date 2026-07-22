# Windows Build Guide

## Prerequisites

### Required

| Tool | Version | Where to get |
|------|---------|-------------|
| **CMake** | 3.16+ | https://cmake.org/download/ |
| **Git** | 2.0+ | https://git-scm.com/download/win |
| **C++ compiler** | C++17 | Visual Studio 2022 or MinGW GCC 13+ |

### Compiler Options

**Option A — Visual Studio 2022**
- Install workload: "Desktop development with C++"
- Includes CMake, MSBuild, Windows SDK

**Option B — MinGW GCC (MSYS2)**
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-opencv \
          mingw-w64-ucrt-x86_64-gtest
```

### Optional Dependencies

| Library | Required for | Install |
|---------|-------------|---------|
| OpenCV 4.x | Image preprocessing (Phase 2+) | MSYS2: `pacman -S mingw-w64-ucrt-x86_64-opencv`<br>vcpkg: `vcpkg install opencv:x64-windows` |
| Paddle Inference 2.5+ | Real OCR (Phase 4) | [Download SDK](https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html) |

> **Note**: `nlohmann/json` and `GoogleTest` are **automatically downloaded** by CMake (FetchContent). Git + internet required on first configure.

---

## Quick Build

### Visual Studio 2022

```powershell
# Open "Developer PowerShell for VS 2022" or "x64 Native Tools Command Prompt"
cd MedicalOCR

# Configure
cmake -S . -B build -A x64

# Build Release
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release --output-on-failure
```

### MinGW GCC (Ninja)

```bash
cd MedicalOCR

# Configure (set OpenCV_DIR for MSYS2)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DOpenCV_DIR="/D/Environmen/MSYS2/ucrt64/lib/cmake/opencv4"

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release --output-on-failure
```

---

## Build Options

```powershell
cmake -S . -B build -A x64 `
    -DMEDICAL_OCR_BUILD_TESTS=ON `
    -DMEDICAL_OCR_BUILD_CLI=ON `
    -DMEDICAL_OCR_USE_OPENCV=ON `
    -DMEDICAL_OCR_ENABLE_PADDLE=OFF `
    -DMEDICAL_OCR_ENABLE_ONNX=OFF `
    -DMEDICAL_OCR_STATIC_RUNTIME=OFF
```

| Option | Default | Description |
|--------|---------|-------------|
| `MEDICAL_OCR_BUILD_TESTS` | ON | Build 118 GoogleTest unit tests |
| `MEDICAL_OCR_BUILD_CLI` | ON | Build `medical_ocr_cli.exe` |
| `MEDICAL_OCR_USE_OPENCV` | ON | Use OpenCV for image processing (required for Phase 2+) |
| `MEDICAL_OCR_ENABLE_PADDLE` | OFF | Enable real PaddleOCR. Requires `PADDLE_INFERENCE_DIR`. |
| `MEDICAL_OCR_ENABLE_ONNX` | OFF | Enable ONNX Runtime backend (future) |
| `MEDICAL_OCR_STATIC_RUNTIME` | OFF | Link MSVC runtime statically (/MT instead of /MD) |

### Building without OpenCV

If OpenCV is not available, disable it:

```powershell
cmake -S . -B build -DMEDICAL_OCR_USE_OPENCV=OFF
```

The DLL will still compile, but image preprocessing (quality check, perspective correction, CLAHE) will be unavailable. Calls to `OCR_RecognizeFile` with real images will fail.

### Building with PaddleOCR (requires SDK)

```powershell
# 1. Download Paddle Inference C++ SDK from paddlepaddle.org.cn
# 2. Extract to C:\paddle_inference
# 3. Download models: .\scripts\download_models.ps1

cmake -S . -B build -A x64 `
    -DMEDICAL_OCR_ENABLE_PADDLE=ON `
    -DPADDLE_INFERENCE_DIR="C:/paddle_inference"

cmake --build build --config Release
```

See `docs/PADDLE_SETUP.md` for detailed instructions.

---

## Build Outputs

```
build/
├── bin/
│   └── Release/                         (MSVC)
│   └── (or just bin/ for Ninja)
│       ├── MedicalOCR.dll                Main DLL
│       ├── MedicalOCR.lib                MSVC import library
│       ├── libMedicalOCR.dll.a           MinGW import library
│       ├── medical_ocr_cli.exe           CLI demo
│       ├── medical_ocr_tests.exe         Unit test runner
│       ├── config/                       Configuration (copied automatically)
│       │   ├── medical_ocr.json
│       │   └── templates/
│       ├── models/                       Models directory (empty)
│       └── samples/                      Sample mock data
└── lib/
    ├── libgtest.a                        Google Test (static)
    └── ...
```

---

## Testing

### All tests

```powershell
# CTest (recommended)
ctest --test-dir build -C Release --output-on-failure

# Direct execution (all 118 tests)
.\build\bin\medical_ocr_tests.exe
```

### Filter tests

```powershell
# Run specific suite
ctest --test-dir build -C Release -R "DllApiTest"
ctest --test-dir build -C Release -R "IdCardValidator"

# Run specific test
.\build\bin\medical_ocr_tests.exe --gtest_filter="*ValidIdNumber*"

# List all tests
.\build\bin\medical_ocr_tests.exe --gtest_list_tests
```

### Test suites (118 total)

| Suite | Count | What it tests |
|-------|-------|---------------|
| `DllApiTest` | 18 | C API lifecycle, error paths, E2E pipeline |
| `ImageQualityTest` | 17 | Empty, resolution, blur, exposure, glare |
| `PreprocessorTest` | 18 | Grayscale, CLAHE, contours, perspective, orientation |
| `IdCardValidatorTest` | 16 | GB 11643 checksum, OCR correction, birth/gender extract |
| `DateValidatorTest` | 13 | ISO/Chinese/Slash/8-digit date normalization, leap year |
| `FieldNormalizerTest` | 15 | Gender mapping, age extraction, string strip |
| `AnchorFieldExtractorTest` | 10 | Anchor search, right/below box finding, region check |
| `ParagraphExtractorTest` | 3 | Start→end anchor paragraph extraction |
| `FieldExtractorTest` | 2 | Full extraction orchestration, empty template |
| `ConcurrencyTest` | 6 | Multi-thread recognize, error isolation, stress test |

---

## Using DLL in Another Project

### CMake integration

```cmake
# Find the MedicalOCR package
set(MEDICAL_OCR_DIR "path/to/MedicalOCR")
find_library(MEDICAL_OCR_LIB MedicalOCR
    PATHS "${MEDICAL_OCR_DIR}/build/bin/Release"
    REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE ${MEDICAL_OCR_LIB})
target_include_directories(myapp PRIVATE "${MEDICAL_OCR_DIR}/include")
```

### Manual linking (MSVC)

```powershell
cl /std:c++17 main.cpp /I"MedicalOCR\include" /link "MedicalOCR\build\bin\Release\MedicalOCR.lib"
```

### Manual linking (MinGW)

```bash
g++ -std=c++17 main.cpp -I"MedicalOCR/include" -L"MedicalOCR/build/bin" -lMedicalOCR -o myapp.exe
```

---

## Build Troubleshooting

### `CMake cannot find nlohmann/json`
First configure downloads dependencies via `FetchContent`. Requires internet + Git in PATH. Delete `build/` and retry.

### `fatal error: opencv2/core.hpp: No such file or directory`
OpenCV not found. Either install it or build with `-DMEDICAL_OCR_USE_OPENCV=OFF`.

### `paddle_inference_api.h: No such file or directory`
Paddle SDK not installed. Build with `-DMEDICAL_OCR_ENABLE_PADDLE=OFF` (default). See `docs/PADDLE_SETUP.md`.

### `undefined reference to 'WinMain'` (MinGW)
CMake detected GUI subsystem. Add `-DCMAKE_CXX_FLAGS="-mconsole"` or pass `-DCMAKE_BUILD_TYPE=Release`.

### Tests fail with `exit code 0xc0000135` (DLL not found)
Copy `MedicalOCR.dll` and OpenCV DLLs to the test executable directory, or add `build/bin/` to PATH. The CMake config does this automatically via `POST_BUILD` copy commands.

### Chinese path cannot be opened
All API functions accept UTF-8 paths. On Windows, the DLL uses `MultiByteToWideChar(CP_UTF8, ...)` → `_wfopen()` internally. Ensure your calling code passes valid UTF-8.

### Release build is slower than Debug (MinGW)
Add `-DCMAKE_CXX_FLAGS="-O3 -flto"` for link-time optimization. For MSVC, `/O2` is default in Release.

---

## CI/CD Integration

### GitHub Actions (example)

```yaml
- name: Configure
  run: cmake -S . -B build -A x64 -DMEDICAL_OCR_BUILD_CLI=OFF

- name: Build
  run: cmake --build build --config Release

- name: Test
  run: ctest --test-dir build -C Release --output-on-failure
```

### Offline Build (no internet)

Pre-download dependencies into `build/_deps/` on a machine with internet, then copy `build/_deps/` to the offline machine before running CMake.
