# ============================================================================
# FindPaddleInference.cmake — Paddle Inference C++ SDK (3.3.x layout)
# ============================================================================
#
# Usage:
#   set(PADDLE_INFERENCE_DIR "D:/Environment/paddle_inference_3.3.0")
#   find_package(PaddleInference REQUIRED)
#
# Defines:
#   PADDLE_INFERENCE_FOUND
#   PADDLE_INFERENCE_INCLUDE_DIR
#   PADDLE_INFERENCE_LIBRARY
#   PADDLE_INFERENCE_LIBRARIES
# ============================================================================

set(PADDLE_INFERENCE_DIR "" CACHE PATH
    "Path to Paddle Inference C++ SDK install directory")

set(_PADDLE_SEARCH_PATHS
    ${PADDLE_INFERENCE_DIR}
    $ENV{PADDLE_INFERENCE_DIR}
    D:/Environment/paddle_inference_3.3.0
    C:/paddle_inference
    $ENV{HOME}/paddle_inference
    /usr/local/paddle_inference
    /opt/paddle_inference
    ${CMAKE_SOURCE_DIR}/third_party/paddle_inference_macos
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/paddle_inference_macos
)

# 3.3.x install layout: paddle/include + paddle/lib
find_path(PADDLE_INFERENCE_INCLUDE_DIR
    NAMES paddle_inference_api.h
    HINTS ${PADDLE_INFERENCE_DIR}
    PATHS ${_PADDLE_SEARCH_PATHS}
    PATH_SUFFIXES paddle/include include
    DOC "Paddle Inference include directory"
)

find_library(PADDLE_INFERENCE_LIBRARY
    NAMES paddle_inference libpaddle_inference
    HINTS ${PADDLE_INFERENCE_DIR}
    PATHS ${_PADDLE_SEARCH_PATHS}
    PATH_SUFFIXES paddle/lib lib
    DOC "Paddle Inference main library"
)

set(PADDLE_EXTRA_LIBRARIES "")

# Shared companions (3.3.0 CPU shared build)
foreach(_lib common phi)
    find_library(PADDLE_${_lib}_LIBRARY
        NAMES ${_lib}
        HINTS ${PADDLE_INFERENCE_DIR}
        PATHS ${_PADDLE_SEARCH_PATHS}
        PATH_SUFFIXES paddle/lib lib
        NO_DEFAULT_PATH
    )
    if(PADDLE_${_lib}_LIBRARY)
        list(APPEND PADDLE_EXTRA_LIBRARIES ${PADDLE_${_lib}_LIBRARY})
    endif()
endforeach()

# Third-party libs under third_party/install (Windows CPU+MKL)
set(_PADDLE_TP_ROOT "${PADDLE_INFERENCE_DIR}/third_party/install")
if(NOT EXISTS "${_PADDLE_TP_ROOT}" AND PADDLE_INFERENCE_INCLUDE_DIR)
    get_filename_component(_paddle_root "${PADDLE_INFERENCE_INCLUDE_DIR}/../.." ABSOLUTE)
    set(_PADDLE_TP_ROOT "${_paddle_root}/third_party/install")
endif()

macro(_paddle_find_tp _name)
    find_library(PADDLE_TP_${_name}_LIBRARY
        NAMES ${_name} lib${_name}
        HINTS ${PADDLE_INFERENCE_DIR}
        PATHS ${_PADDLE_SEARCH_PATHS}
        PATH_SUFFIXES
            third_party/install/mklml/lib
            third_party/install/glog/lib
            third_party/install/gflags/lib
            third_party/install/protobuf/lib
            third_party/install/xxhash/lib
            third_party/install/yaml-cpp/lib
            third_party/install/utf8proc/lib
            third_party/install/cryptopp/lib
        NO_DEFAULT_PATH
    )
    if(PADDLE_TP_${_name}_LIBRARY)
        list(APPEND PADDLE_EXTRA_LIBRARIES ${PADDLE_TP_${_name}_LIBRARY})
    endif()
endmacro()

_paddle_find_tp(mklml)
_paddle_find_tp(libiomp5md)
_paddle_find_tp(glog)
_paddle_find_tp(gflags_static)
_paddle_find_tp(gflags)
_paddle_find_tp(libprotobuf)
_paddle_find_tp(protobuf)
_paddle_find_tp(xxhash)
_paddle_find_tp(yaml-cpp)
_paddle_find_tp(utf8proc_static)
_paddle_find_tp(utf8proc)
_paddle_find_tp(cryptopp-static)
_paddle_find_tp(cryptopp)

# Include dirs for third_party headers used by cpp_infer sources
set(PADDLE_INFERENCE_THIRD_PARTY_INCLUDE_DIRS "")
foreach(_tp glog gflags protobuf xxhash yaml-cpp utf8proc cryptopp mklml)
    if(EXISTS "${_PADDLE_TP_ROOT}/${_tp}/include")
        list(APPEND PADDLE_INFERENCE_THIRD_PARTY_INCLUDE_DIRS
            "${_PADDLE_TP_ROOT}/${_tp}/include")
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PaddleInference
    REQUIRED_VARS
        PADDLE_INFERENCE_INCLUDE_DIR
        PADDLE_INFERENCE_LIBRARY
)

# Normalize FOUND flag (FPHSA sets PaddleInference_FOUND; consumers use PADDLE_INFERENCE_FOUND).
if(PaddleInference_FOUND)
    set(PADDLE_INFERENCE_FOUND TRUE)
else()
    set(PADDLE_INFERENCE_FOUND FALSE)
endif()

if(PADDLE_INFERENCE_FOUND)
    set(PADDLE_INFERENCE_LIBRARIES
        ${PADDLE_INFERENCE_LIBRARY}
        ${PADDLE_EXTRA_LIBRARIES}
    )

    if(WIN32)
        list(APPEND PADDLE_INFERENCE_LIBRARIES shlwapi)
    endif()
    if(APPLE)
        list(APPEND PADDLE_INFERENCE_LIBRARIES
            "-framework Accelerate" "-framework Foundation")
    endif()

    if(NOT TARGET PaddleInference::PaddleInference)
        add_library(PaddleInference::PaddleInference UNKNOWN IMPORTED)
        set_target_properties(PaddleInference::PaddleInference PROPERTIES
            IMPORTED_LOCATION "${PADDLE_INFERENCE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES
                "${PADDLE_INFERENCE_INCLUDE_DIR};${PADDLE_INFERENCE_THIRD_PARTY_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${PADDLE_EXTRA_LIBRARIES}"
        )
        if(WIN32)
            target_link_libraries(PaddleInference::PaddleInference
                INTERFACE shlwapi)
        endif()
        if(APPLE)
            target_link_libraries(PaddleInference::PaddleInference
                INTERFACE "-framework Accelerate" "-framework Foundation")
        endif()
    endif()

    # Resolve install root for runtime DLL copy
    get_filename_component(PADDLE_INFERENCE_ROOT
        "${PADDLE_INFERENCE_INCLUDE_DIR}/../.." ABSOLUTE)

    message(STATUS "Paddle Inference found:")
    message(STATUS "  Root:    ${PADDLE_INFERENCE_ROOT}")
    message(STATUS "  Include: ${PADDLE_INFERENCE_INCLUDE_DIR}")
    message(STATUS "  Library: ${PADDLE_INFERENCE_LIBRARY}")
else()
    message(STATUS "Paddle Inference NOT found. "
                   "Set PADDLE_INFERENCE_DIR (e.g. D:/Environment/paddle_inference_3.3.0).")
endif()
