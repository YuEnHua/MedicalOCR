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
    # OpenCV Windows packs (e.g. 4.7.0) ship vc16 binaries; newer MSVC (19.4x)
    # is not listed in OpenCVConfig.cmake — force a compatible runtime.
    if(MSVC AND NOT DEFINED OpenCV_RUNTIME)
        set(OpenCV_RUNTIME "vc16" CACHE STRING "OpenCV MSVC runtime folder (vc16/vc17)")
    endif()
    if(MSVC AND NOT DEFINED OpenCV_ARCH)
        set(OpenCV_ARCH "x64" CACHE STRING "OpenCV arch folder")
    endif()
    if(APPLE)
        find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
    else()
        find_package(OpenCV QUIET)
        if(NOT OpenCV_FOUND)
            find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
        endif()
    endif()
    if(OpenCV_FOUND)
        message(STATUS "MedicalOCR: OpenCV ${OpenCV_VERSION} found")
        message(STATUS "  Include: ${OpenCV_INCLUDE_DIRS}")
        message(STATUS "  Libs:    ${OpenCV_LIBS}")
    else()
        message(WARNING "MedicalOCR: OpenCV not found. "
                        "Image preprocessing will be unavailable. "
                        "Install OpenCV or set MEDICAL_OCR_USE_OPENCV=OFF.")
        message(STATUS "  Set OpenCV_DIR to e.g. D:/Environment/opencv/build")
        message(STATUS "  Or OpenCV_DIR=D:/Environment/opencv/build/x64/vc16/lib")
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
# Paddle Inference 3.3.x (PP-OCRv6)
# ----------------------------------------------------------------------------
# Recommended:
#   -DMEDICAL_OCR_ENABLE_PADDLE=ON
#   -DPADDLE_INFERENCE_DIR=D:/Environment/paddle_inference_3.3.0
#   -DMEDICAL_OCR_STATIC_RUNTIME=ON
#   -DOpenCV_DIR=D:/Environment/opencv/build
if(MEDICAL_OCR_ENABLE_PADDLE)
    if(DEFINED MEDICAL_OCR_ROOT)
        list(APPEND CMAKE_MODULE_PATH "${MEDICAL_OCR_ROOT}/cmake")
    else()
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    endif()
    find_package(PaddleInference QUIET)

    if(PADDLE_INFERENCE_FOUND)
        message(STATUS "MedicalOCR: Paddle Inference found — PaddleOcrEngine enabled")
        message(STATUS "  Include: ${PADDLE_INFERENCE_INCLUDE_DIR}")
        message(STATUS "  Library: ${PADDLE_INFERENCE_LIBRARY}")
        message(STATUS "  Use /MT (MEDICAL_OCR_STATIC_RUNTIME=ON) with paddle_inference_3.3.0")
    else()
        message(WARNING "MedicalOCR: MEDICAL_OCR_ENABLE_PADDLE=ON but Paddle Inference not found.")
        message(STATUS "  Set PADDLE_INFERENCE_DIR to D:/Environment/paddle_inference_3.3.0")
        message(STATUS "  The PaddleOcrEngine stub will be compiled (returns errors at runtime).")
    endif()
endif()

# ----------------------------------------------------------------------------
# ONNX Runtime (future)
# ----------------------------------------------------------------------------
# TODO: When MEDICAL_OCR_ENABLE_ONNX is ON:
#   find_package(ONNXRuntime REQUIRED)
