# ============================================================================
# FindPaddleInference.cmake
# ============================================================================
#
# Locates the Paddle Inference C++ SDK.
#
# Usage:
#   find_package(PaddleInference REQUIRED)
#
# Or set the install directory manually:
#   cmake -DPADDLE_INFERENCE_DIR=C:/paddle_inference ...
#
# Defines:
#   PADDLE_INFERENCE_FOUND      - TRUE if found
#   PADDLE_INFERENCE_INCLUDE_DIR- Include directory
#   PADDLE_INFERENCE_LIBRARY    - Main library (paddle_inference)
#   PADDLE_INFERENCE_LIBRARIES  - All required libraries
#   PADDLE_INFERENCE_VERSION    - Version string
# ============================================================================

# Allow users to specify the install directory.
set(PADDLE_INFERENCE_DIR "" CACHE PATH
    "Path to Paddle Inference C++ SDK install directory")

# Common install paths.
set(_PADDLE_SEARCH_PATHS
    ${PADDLE_INFERENCE_DIR}
    $ENV{PADDLE_INFERENCE_DIR}
    C:/paddle_inference
    C:/Paddle/Paddle Inference
    D:/paddle_inference
    $ENV{HOME}/paddle_inference
    /usr/local/paddle_inference
    /opt/paddle_inference
)

# ---- Find include directory ----
find_path(PADDLE_INFERENCE_INCLUDE_DIR
    NAMES paddle_inference_api.h
    HINTS ${PADDLE_INFERENCE_DIR}
    PATHS ${_PADDLE_SEARCH_PATHS}
    PATH_SUFFIXES include
    DOC "Paddle Inference include directory"
)

# ---- Find library ----
# On Windows, the library is typically paddle_inference.lib or libpaddle_inference.dll.a.
# The SDK also ships several dependency DLLs.
find_library(PADDLE_INFERENCE_LIBRARY
    NAMES paddle_inference libpaddle_inference
    HINTS ${PADDLE_INFERENCE_DIR}
    PATHS ${_PADDLE_SEARCH_PATHS}
    PATH_SUFFIXES lib
    DOC "Paddle Inference main library"
)

# ---- Find all required third-party libraries bundled with Paddle ----
# Paddle Inference depends on: libiomp5md, mklml, onnxruntime, etc.
# We look for them in the same directory.
set(_PADDLE_EXTRA_LIBS
    paddle_fluid
    libiomp5md
    mklml
    mklml_intel
    onnxruntime
    pthreadpool
    XNNPACK
    cpuinfo
)

foreach(_lib ${_PADDLE_EXTRA_LIBS})
    find_library(PADDLE_${_lib}_LIBRARY
        NAMES ${_lib}
        HINTS ${PADDLE_INFERENCE_DIR}
        PATHS ${_PADDLE_SEARCH_PATHS}
        PATH_SUFFIXES lib third_party/install/mklml/lib
                       third_party/install/onnxruntime/lib
                       third_party/install/xnnpack/lib
        NO_DEFAULT_PATH
    )
    if(PADDLE_${_lib}_LIBRARY)
        list(APPEND PADDLE_EXTRA_LIBRARIES ${PADDLE_${_lib}_LIBRARY})
    endif()
endforeach()

# ---- Set standard variables ----
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PaddleInference
    REQUIRED_VARS
        PADDLE_INFERENCE_INCLUDE_DIR
        PADDLE_INFERENCE_LIBRARY
    VERSION_VAR PADDLE_INFERENCE_VERSION
)

if(PADDLE_INFERENCE_FOUND)
    set(PADDLE_INFERENCE_LIBRARIES
        ${PADDLE_INFERENCE_LIBRARY}
        ${PADDLE_EXTRA_LIBRARIES}
    )

    if(NOT TARGET PaddleInference::PaddleInference)
        add_library(PaddleInference::PaddleInference UNKNOWN IMPORTED)
        set_target_properties(PaddleInference::PaddleInference PROPERTIES
            IMPORTED_LOCATION "${PADDLE_INFERENCE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PADDLE_INFERENCE_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${PADDLE_EXTRA_LIBRARIES}"
        )
    endif()

    message(STATUS "Paddle Inference found:")
    message(STATUS "  Include: ${PADDLE_INFERENCE_INCLUDE_DIR}")
    message(STATUS "  Library: ${PADDLE_INFERENCE_LIBRARY}")
else()
    message(STATUS "Paddle Inference NOT found. "
                   "Set PADDLE_INFERENCE_DIR to enable PaddleOCR.")
    message(STATUS "  Download: https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html")
endif()
