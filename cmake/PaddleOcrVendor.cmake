# ============================================================================
# Vendored PaddleOCR cpp_infer (det+rec subset) for MEDICAL_OCR_ENABLE_PADDLE
# ============================================================================

# Prefer MEDICAL_OCR_ROOT when PdfToMarkdown (or others) reuse this vendor
# from outside the MedicalOCR source tree.
if(DEFINED MEDICAL_OCR_ROOT AND EXISTS
   "${MEDICAL_OCR_ROOT}/third_party/paddleocr_cpp_infer/src/modules/text_detection/predictor.cc")
    set(PADDLEOCR_CPP_INFER_ROOT
        "${MEDICAL_OCR_ROOT}/third_party/paddleocr_cpp_infer")
else()
    set(PADDLEOCR_CPP_INFER_ROOT
        "${CMAKE_SOURCE_DIR}/third_party/paddleocr_cpp_infer")
endif()

if(NOT EXISTS "${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_detection/predictor.cc")
    message(FATAL_ERROR
        "PaddleOCR vendor missing at ${PADDLEOCR_CPP_INFER_ROOT}. "
        "Expected official cpp_infer det+rec sources.")
endif()

# Abseil must stay static; clipper's CMakeLists caches BUILD_SHARED_LIBS=ON.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
set(ABSL_BUILD_DLL OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${PADDLEOCR_CPP_INFER_ROOT}/third_party/abseil-cpp"
    "${CMAKE_BINARY_DIR}/_deps/abseil-cpp"
    EXCLUDE_FROM_ALL
)

# Clipper as shared DLL (needed at runtime). Avoid add_subdirectory CACHE pollution.
set(_clipper_src
    "${PADDLEOCR_CPP_INFER_ROOT}/third_party/clipper_ver6.4.2/cpp/clipper.cpp")
set(_clipper_hdr
    "${PADDLEOCR_CPP_INFER_ROOT}/third_party/clipper_ver6.4.2/cpp/clipper.hpp")
set(POLYCLIPPING_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/polyclipping_inc")
file(MAKE_DIRECTORY "${POLYCLIPPING_INCLUDE_DIR}/polyclipping")
configure_file("${_clipper_hdr}"
    "${POLYCLIPPING_INCLUDE_DIR}/polyclipping/clipper.hpp" COPYONLY)
    if(NOT TARGET polyclipping)
        add_library(polyclipping SHARED "${_clipper_src}")
        target_include_directories(polyclipping PUBLIC "${POLYCLIPPING_INCLUDE_DIR}")
        set_target_properties(polyclipping PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        )
        if(WIN32)
            set_target_properties(polyclipping PROPERTIES
                WINDOWS_EXPORT_ALL_SYMBOLS ON)
        endif()
        if(APPLE)
            set_target_properties(polyclipping PROPERTIES
                MACOSX_RPATH ON
                INSTALL_NAME_DIR "@rpath"
                BUILD_WITH_INSTALL_RPATH ON)
        endif()
    endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)

set(PADDLEOCR_VENDOR_SOURCES
    ${PADDLEOCR_CPP_INFER_ROOT}/src/base/base_batch_sampler.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/base/base_predictor.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/common/image_batch_sampler.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/common/processors.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/common/static_infer.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/common/thread_pool.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_detection/predictor.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_detection/processors.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_detection/result.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_recognition/predictor.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_recognition/processors.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/modules/text_recognition/result.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/utils/ilogger.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/utils/mkldnn_blocklist.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/utils/pp_option.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/utils/utility.cc
    ${PADDLEOCR_CPP_INFER_ROOT}/src/utils/yaml_config.cc
)

function(medical_ocr_link_paddle_vendor target_name)
    target_sources(${target_name} PRIVATE ${PADDLEOCR_VENDOR_SOURCES})

    target_include_directories(${target_name} PRIVATE
        ${PADDLEOCR_CPP_INFER_ROOT}
        ${POLYCLIPPING_INCLUDE_DIR}
        ${PADDLE_INFERENCE_INCLUDE_DIR}
        ${PADDLE_INFERENCE_THIRD_PARTY_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
    )
    # Windows-only POSIX dirent shim. On macOS/Linux the vendor copy of
    # dirent.h sits at the cpp_infer root and would shadow libc <dirent.h>,
    # then fail with "windows.h file not found".
    if(WIN32)
        target_include_directories(${target_name} PRIVATE
            ${PADDLEOCR_CPP_INFER_ROOT}/include_win
        )
    endif()

    target_compile_definitions(${target_name} PRIVATE
        MEDICAL_OCR_HAS_PADDLE
        GOOGLE_GLOG_DLL_DECL=
        YAML_CPP_STATIC_DEFINE
        NOMINMAX
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /bigobj /utf-8)
    endif()

    target_link_libraries(${target_name} PRIVATE
        PaddleInference::PaddleInference
        absl::statusor
        absl::status
        absl::optional
        polyclipping
        ${OpenCV_LIBS}
    )
endfunction()

function(medical_ocr_copy_paddle_runtime target_name)
    set(_paddle_root "${PADDLE_INFERENCE_ROOT}")
    if(NOT _paddle_root AND PADDLE_INFERENCE_DIR)
        set(_paddle_root "${PADDLE_INFERENCE_DIR}")
    endif()

    if(WIN32)
        set(_runtime_dlls
            "${_paddle_root}/paddle/lib/paddle_inference.dll"
            "${_paddle_root}/paddle/lib/common.dll"
            "${_paddle_root}/paddle/lib/phi.dll"
            "${_paddle_root}/third_party/install/mklml/lib/mklml.dll"
            "${_paddle_root}/third_party/install/mklml/lib/libiomp5md.dll"
        )
        foreach(_dll ${_runtime_dlls})
            if(EXISTS "${_dll}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" $<TARGET_FILE_DIR:${target_name}>
                    COMMENT "Copy Paddle runtime DLL ${_dll}"
                )
            endif()
        endforeach()
        if(OpenCV_DIR)
            file(GLOB _opencv_world
                "${OpenCV_DIR}/../bin/opencv_world*.dll"
                "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll"
                "${OpenCV_DIR}/../../x64/vc16/bin/opencv_world*.dll"
            )
            foreach(_dll ${_opencv_world})
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" $<TARGET_FILE_DIR:${target_name}>
                    COMMENT "Copy OpenCV DLL ${_dll}"
                )
            endforeach()
        endif()
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:polyclipping> $<TARGET_FILE_DIR:${target_name}>
            COMMENT "Copy polyclipping.dll"
        )
        return()
    endif()

    if(APPLE)
        file(GLOB _paddle_dylibs
            "${_paddle_root}/paddle/lib/*.dylib"
            "${_paddle_root}/third_party/install/*/lib/*.dylib")
        foreach(_lib ${_paddle_dylibs})
            if(NOT EXISTS "${_lib}")
                continue()
            endif()
            get_filename_component(_bn "${_lib}" NAME)
            if(_bn MATCHES "gfortran|quadmath|lapack-netlib")
                continue()
            endif()
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_lib}" $<TARGET_FILE_DIR:${target_name}>
                COMMENT "Copy Paddle dylib ${_lib}"
            )
        endforeach()
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:polyclipping> $<TARGET_FILE_DIR:${target_name}>
            COMMENT "Copy polyclipping dylib"
        )
    endif()
endfunction()
