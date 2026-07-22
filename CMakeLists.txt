# ============================================================================
# Medical OCR — CMake Build Configuration
# ============================================================================
#
# Minimum requirements:
#   cmake -S . -B build -A x64
#   cmake --build build --config Release
#   ctest --test-dir build -C Release
#
# Options:
#   MEDICAL_OCR_BUILD_TESTS      Build unit tests (default: ON)
#   MEDICAL_OCR_BUILD_CLI        Build CLI demo (default: ON)
#   MEDICAL_OCR_ENABLE_PADDLE    Enable PaddleOCR integration (default: OFF)
#   MEDICAL_OCR_ENABLE_ONNX      Enable ONNX Runtime integration (default: OFF)
#   MEDICAL_OCR_USE_OPENCV       Use OpenCV for image processing (default: ON)
#   MEDICAL_OCR_STATIC_RUNTIME   Link MSVC runtime statically (default: OFF)
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(MedicalOCR
    VERSION 1.0.0
    DESCRIPTION "Local offline medical examination report OCR recognition library"
    LANGUAGES CXX
)

# ---- Options ----
option(MEDICAL_OCR_BUILD_TESTS      "Build unit tests"                  ON)
option(MEDICAL_OCR_BUILD_CLI        "Build CLI demo application"        ON)
option(MEDICAL_OCR_ENABLE_PADDLE    "Enable PaddleOCR integration"      OFF)
option(MEDICAL_OCR_ENABLE_ONNX      "Enable ONNX Runtime integration"   OFF)
option(MEDICAL_OCR_USE_OPENCV       "Use OpenCV for image processing"   ON)
option(MEDICAL_OCR_STATIC_RUNTIME   "Link MSVC runtime statically (/MT)" OFF)

# ---- C++ Standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- MSVC Runtime Library ----
if(MEDICAL_OCR_STATIC_RUNTIME AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- Output directories ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---- Dependencies ----
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

# ============================================================================
# MedicalOCR DLL Library
# ============================================================================

set(MEDICAL_OCR_SOURCES
    # C API layer
    src/api/medical_ocr_c_api.cpp

    # Core service
    src/core/medical_ocr_service.cpp

    # Image processing (Phase 2)
    src/image/image_quality_checker.cpp
    src/image/document_preprocessor.cpp

    # Document analysis (Phase 3)
    src/document/template_repository.cpp
    src/document/document_classifier.cpp
    src/document/template_matcher.cpp

    # Field extraction (Phase 3)
    src/extraction/anchor_field_extractor.cpp
    src/extraction/paragraph_extractor.cpp
    src/extraction/field_extractor.cpp

    # Validation (Phase 3)
    src/validation/id_card_validator.cpp
    src/validation/date_validator.cpp
    src/validation/field_normalizer.cpp

    # OCR engines
    src/ocr/mock_ocr_engine.cpp
    src/ocr/paddle_ocr_engine.cpp

    # Common utilities
    src/common/utf8_utils.cpp
    src/common/geometry_utils.cpp
)

set(MEDICAL_OCR_HEADERS
    include/medical_ocr/medical_ocr_c_api.h
    include/medical_ocr/types.h
    include/medical_ocr/error_codes.h
    src/core/medical_ocr_service.h
    src/image/image_quality_checker.h
    src/image/document_preprocessor.h
    src/document/template_repository.h
    src/document/document_classifier.h
    src/document/template_matcher.h
    src/extraction/anchor_field_extractor.h
    src/extraction/paragraph_extractor.h
    src/extraction/field_extractor.h
    src/validation/id_card_validator.h
    src/validation/date_validator.h
    src/validation/field_normalizer.h
    src/ocr/i_ocr_engine.h
    src/ocr/mock_ocr_engine.h
    src/ocr/paddle_ocr_engine.h
    src/common/utf8_utils.h
    src/common/geometry_utils.h
)

add_library(MedicalOCR SHARED ${MEDICAL_OCR_SOURCES} ${MEDICAL_OCR_HEADERS})

target_include_directories(MedicalOCR
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---- Link libraries ----
set(MEDICAL_OCR_LIBS nlohmann_json::nlohmann_json)

if(MEDICAL_OCR_USE_OPENCV AND OpenCV_FOUND)
    list(APPEND MEDICAL_OCR_LIBS ${OpenCV_LIBS})
    target_compile_definitions(MedicalOCR PRIVATE MEDICAL_OCR_HAS_OPENCV)
endif()

if(MEDICAL_OCR_ENABLE_PADDLE AND PADDLE_INFERENCE_FOUND)
    list(APPEND MEDICAL_OCR_LIBS PaddleInference::PaddleInference)
    target_include_directories(MedicalOCR
        PRIVATE ${PADDLE_INFERENCE_INCLUDE_DIR}
    )
    target_compile_definitions(MedicalOCR PRIVATE MEDICAL_OCR_HAS_PADDLE)
    message(STATUS "  PaddleOCR:    ENABLED")
else()
    message(STATUS "  PaddleOCR:    stub (Paddle SDK not available)")
endif()

target_link_libraries(MedicalOCR
    PRIVATE ${MEDICAL_OCR_LIBS}
)

# DLL export macro
target_compile_definitions(MedicalOCR
    PRIVATE MEDICAL_OCR_DLL_EXPORTS
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(MedicalOCR PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# Set DLL output name
set_target_properties(MedicalOCR PROPERTIES
    OUTPUT_NAME MedicalOCR
    PREFIX ""
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ============================================================================
# Install rules
# ============================================================================

include(GNUInstallDirs)

install(TARGETS MedicalOCR
    EXPORT MedicalOCRTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/medical_ocr
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY config/ templates/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/medical_ocr
    OPTIONAL
)

# ============================================================================
# CLI Demo Application
# ============================================================================

if(MEDICAL_OCR_BUILD_CLI)
    add_executable(medical_ocr_cli
        apps/cli_demo/main.cpp
    )
    target_link_libraries(medical_ocr_cli
        PRIVATE MedicalOCR
    )
    target_include_directories(medical_ocr_cli
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(WIN32)
        target_compile_definitions(medical_ocr_cli PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# ============================================================================
# Tests
# ============================================================================

if(MEDICAL_OCR_BUILD_TESTS)
    set(MEDICAL_OCR_TEST_SOURCES
        tests/test_dll_api.cpp
        tests/test_image_quality.cpp
        tests/test_preprocessing.cpp
        tests/test_id_card_validator.cpp
        tests/test_date_validator.cpp
        tests/test_field_normalizer.cpp
        tests/test_extraction.cpp
        tests/test_concurrency.cpp
    )

    add_executable(medical_ocr_tests ${MEDICAL_OCR_TEST_SOURCES})

    # Also compile internal sources directly into tests
    # since they are not exported from the DLL.
    target_sources(medical_ocr_tests PRIVATE
        src/image/image_quality_checker.cpp
        src/image/document_preprocessor.cpp
        src/document/template_repository.cpp
        src/document/document_classifier.cpp
        src/document/template_matcher.cpp
        src/extraction/anchor_field_extractor.cpp
        src/extraction/paragraph_extractor.cpp
        src/extraction/field_extractor.cpp
        src/validation/id_card_validator.cpp
        src/validation/date_validator.cpp
        src/validation/field_normalizer.cpp
        src/ocr/paddle_ocr_engine.cpp
        src/common/utf8_utils.cpp
        src/common/geometry_utils.cpp
    )

    target_link_libraries(medical_ocr_tests
        PRIVATE
            MedicalOCR
            GTest::gtest
            GTest::gtest_main
            ${OpenCV_LIBS}
    )

    # Link pthread for std::thread on MinGW.
    if(MINGW)
        target_link_libraries(medical_ocr_tests PRIVATE pthread)
    endif()
    target_include_directories(medical_ocr_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include
            ${OpenCV_INCLUDE_DIRS}
    )
    if(WIN32)
        target_compile_definitions(medical_ocr_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    # Copy DLL to test executable directory so tests find it.
    add_custom_command(TARGET medical_ocr_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:MedicalOCR>"
            "$<TARGET_FILE_DIR:medical_ocr_tests>"
    )

    # Register tests with CTest.
    include(GoogleTest)
    gtest_discover_tests(medical_ocr_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )

    # Also copy the config directory to the build directory for tests.
    add_custom_command(TARGET medical_ocr_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "${CMAKE_BINARY_DIR}/bin/config"
    )
endif()

# ============================================================================
# Copy config to build output
# ============================================================================

add_custom_command(TARGET MedicalOCR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/config"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/models"
)

add_custom_command(TARGET MedicalOCR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/samples"
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/samples"
)

message(STATUS "MedicalOCR v${PROJECT_VERSION} configured successfully")
message(STATUS "  Build tests:  ${MEDICAL_OCR_BUILD_TESTS}")
message(STATUS "  Build CLI:    ${MEDICAL_OCR_BUILD_CLI}")
message(STATUS "  OpenCV:       ${OpenCV_FOUND} (${OpenCV_VERSION})")
message(STATUS "  PaddleOCR:    ${MEDICAL_OCR_ENABLE_PADDLE}")
message(STATUS "  ONNX:         ${MEDICAL_OCR_ENABLE_ONNX}")
message(STATUS "  Static CRT:   ${MEDICAL_OCR_STATIC_RUNTIME}")
