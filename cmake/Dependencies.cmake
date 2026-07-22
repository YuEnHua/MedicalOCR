# ============================================================================
# Medical OCR — Dependency Management
# ============================================================================
#
# This file fetches and configures third-party dependencies.
#
# Dependencies:
#   - nlohmann/json  (header-only, fetched via FetchContent)
#   - GoogleTest     (fetched via FetchContent, only when tests are enabled)
#   - OpenCV         (find_package, enabled by default)
#   - Paddle Inference (optional, find_package, disabled by default)
# ============================================================================

include(FetchContent)

# ----------------------------------------------------------------------------
# nlohmann/json (header-only JSON library)
# ----------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY  https://github.com/nlohmann/json.git
    GIT_TAG         v3.11.3
    GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

message(STATUS "MedicalOCR: nlohmann/json configured")

# ----------------------------------------------------------------------------
# OpenCV (image processing — enabled by default for Phase 2+)
# ----------------------------------------------------------------------------
if(MEDICAL_OCR_USE_OPENCV)
    find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
    if(OpenCV_FOUND)
        message(STATUS "MedicalOCR: OpenCV ${OpenCV_VERSION} found")
        message(STATUS "  Include: ${OpenCV_INCLUDE_DIRS}")
    else()
        message(WARNING "MedicalOCR: OpenCV not found. "
                        "Image preprocessing will be unavailable. "
                        "Install OpenCV or set MEDICAL_OCR_USE_OPENCV=OFF.")
        message(STATUS "  To install with MSYS2: pacman -S mingw-w64-ucrt-x86_64-opencv")
        message(STATUS "  To install with vcpkg:  vcpkg install opencv:x64-windows")
    endif()
endif()

# ----------------------------------------------------------------------------
# GoogleTest (only when tests are enabled)
# ----------------------------------------------------------------------------
if(MEDICAL_OCR_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY  https://github.com/google/googletest.git
        GIT_TAG         v1.15.2
        GIT_SHALLOW     TRUE
    )
    # Prevent gtest from installing itself.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    message(STATUS "MedicalOCR: GoogleTest configured")
endif()

# ----------------------------------------------------------------------------
# Paddle Inference (Phase 4)
# ----------------------------------------------------------------------------
if(MEDICAL_OCR_ENABLE_PADDLE)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(PaddleInference QUIET)

    if(PADDLE_INFERENCE_FOUND)
        message(STATUS "MedicalOCR: Paddle Inference found — PaddleOcrEngine enabled")
        message(STATUS "  Include: ${PADDLE_INFERENCE_INCLUDE_DIR}")
        message(STATUS "  Library: ${PADDLE_INFERENCE_LIBRARY}")
    else()
        message(WARNING "MedicalOCR: MEDICAL_OCR_ENABLE_PADDLE=ON but Paddle Inference not found.")
        message(STATUS "  Set PADDLE_INFERENCE_DIR to the SDK install directory.")
        message(STATUS "  Download from: https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html")
        message(STATUS "  The PaddleOcrEngine stub will be compiled (returns errors at runtime).")
    endif()
endif()

# ----------------------------------------------------------------------------
# ONNX Runtime (future)
# ----------------------------------------------------------------------------
# TODO: When MEDICAL_OCR_ENABLE_ONNX is ON:
#   find_package(ONNXRuntime REQUIRED)
