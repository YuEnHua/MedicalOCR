/**
 * C ABI implementation for the Medical OCR DLL.
 *
 * All functions catch C++ exceptions at the DLL boundary and translate
 * them into error codes. Internal strings are allocated via the DLL's
 * allocator and must be freed with OCR_FreeResult.
 */

#include "medical_ocr/medical_ocr_c_api.h"
#include "medical_ocr/error_codes.h"

#include <cstring>
#include <functional>
#include <mutex>
#include <new>
#include <string>

#include "src/core/medical_ocr_service.h"

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/// Thread-safe storage for the last error string.
class LastErrorStore {
public:
    static LastErrorStore& Instance() {
        static LastErrorStore s;
        return s;
    }

    void Set(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_ = msg;
    }

    std::string Get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_;
    }

private:
    mutable std::mutex mutex_;
    std::string buffer_;
};

/// Allocate and copy a string for return to the caller.
/// The caller must free via OCR_FreeResult.
char* AllocResultString(const std::string& s) {
    if (s.empty()) {
        // Return an empty JSON object rather than NULL.
        char* buf = static_cast<char*>(std::malloc(3));
        if (buf) {
            buf[0] = '{';
            buf[1] = '}';
            buf[2] = '\0';
        }
        return buf;
    }
    size_t len = s.size() + 1;
    char* buf = static_cast<char*>(std::malloc(len));
    if (!buf) return nullptr;
    std::memcpy(buf, s.c_str(), len);
    return buf;
}

/// Catch-all wrapper for API calls.
/// Returns the error code from the inner function; sets last error on
/// exceptions.
int SafeCall(const char* api_name, std::function<int()> fn) {
    try {
        return fn();
    } catch (const std::bad_alloc& e) {
        LastErrorStore::Instance().Set(
            std::string(api_name) + ": memory allocation failed: " + e.what());
        return MEDOCR_ERR_OUTPUT_ALLOC_FAILED;
    } catch (const std::exception& e) {
        LastErrorStore::Instance().Set(
            std::string(api_name) + ": internal exception: " + e.what());
        return MEDOCR_ERR_INTERNAL_EXCEPTION;
    } catch (...) {
        LastErrorStore::Instance().Set(
            std::string(api_name) + ": unknown internal exception");
        return MEDOCR_ERR_UNKNOWN;
    }
}

}  // anonymous namespace

// ============================================================================
// Error code string conversion
// ============================================================================

const char* MedOcrErrorCodeToString(int error_code) {
    switch (error_code) {
        case MEDOCR_OK:                      return "Success";
        case MEDOCR_ERR_NOT_INITIALIZED:     return "Not initialized";
        case MEDOCR_ERR_INIT_FAILED:         return "Initialization failed";
        case MEDOCR_ERR_CONFIG_NOT_FOUND:    return "Configuration file not found";
        case MEDOCR_ERR_MODEL_LOAD_FAILED:   return "Model loading failed";
        case MEDOCR_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case MEDOCR_ERR_FILE_NOT_FOUND:      return "Image file not found";
        case MEDOCR_ERR_UNSUPPORTED_FORMAT:  return "Unsupported image format";
        case MEDOCR_ERR_IMAGE_DECODE_FAILED: return "Image decode failed";
        case MEDOCR_ERR_IMAGE_QUALITY_FAILED:return "Image quality check failed";
        case MEDOCR_ERR_OCR_FAILED:          return "OCR recognition failed";
        case MEDOCR_ERR_OCR_NO_RESULT:       return "OCR returned no results";
        case MEDOCR_ERR_EXTRACTION_FAILED:   return "Field extraction failed";
        case MEDOCR_ERR_NO_TEMPLATE_MATCH:   return "No template matched";
        case MEDOCR_ERR_OUTPUT_ALLOC_FAILED: return "Output memory allocation failed";
        case MEDOCR_ERR_UNKNOWN:             return "Unknown error";
        case MEDOCR_ERR_INVALID_ARGUMENT:    return "Invalid argument";
        case MEDOCR_ERR_INTERNAL_EXCEPTION:  return "Internal exception";
        default:                             return "Unrecognized error code";
    }
}

// ============================================================================
// Public C API
// ============================================================================

OCR_API int OCR_Init(const char* model_directory, const char* config_path) {
    return SafeCall("OCR_Init", [&]() -> int {
        std::string md = model_directory ? std::string(model_directory) : "";
        std::string cp = config_path ? std::string(config_path) : "";
        int rc = medical_ocr::MedicalOcrService::Instance().Initialize(md, cp);
        if (rc != MEDOCR_OK) {
            LastErrorStore::Instance().Set(
                medical_ocr::MedicalOcrService::Instance().GetLastError());
        }
        return rc;
    });
}

OCR_API int OCR_RecognizeFile(const char* image_path_utf8,
                               char** output_json_utf8) {
    return SafeCall("OCR_RecognizeFile", [&]() -> int {
        if (!image_path_utf8 || !output_json_utf8) {
            LastErrorStore::Instance().Set(
                "OCR_RecognizeFile: null pointer argument");
            return MEDOCR_ERR_INVALID_ARGUMENT;
        }
        *output_json_utf8 = nullptr;

        std::string out_json;
        int rc = medical_ocr::MedicalOcrService::Instance().RecognizeFile(
            std::string(image_path_utf8), out_json);
        if (rc != MEDOCR_OK) {
            LastErrorStore::Instance().Set(
                medical_ocr::MedicalOcrService::Instance().GetLastError());
            return rc;
        }

        char* buf = AllocResultString(out_json);
        if (!buf) {
            LastErrorStore::Instance().Set(
                "OCR_RecognizeFile: failed to allocate output buffer");
            return MEDOCR_ERR_OUTPUT_ALLOC_FAILED;
        }
        *output_json_utf8 = buf;
        return MEDOCR_OK;
    });
}

OCR_API int OCR_RecognizeMemory(const unsigned char* image_data,
                                 int image_size,
                                 char** output_json_utf8) {
    return SafeCall("OCR_RecognizeMemory", [&]() -> int {
        if (!image_data || !output_json_utf8 || image_size <= 0) {
            LastErrorStore::Instance().Set(
                "OCR_RecognizeMemory: invalid arguments");
            return MEDOCR_ERR_INVALID_ARGUMENT;
        }
        *output_json_utf8 = nullptr;

        std::string out_json;
        int rc = medical_ocr::MedicalOcrService::Instance().RecognizeMemory(
            image_data, image_size, out_json);
        if (rc != MEDOCR_OK) {
            LastErrorStore::Instance().Set(
                medical_ocr::MedicalOcrService::Instance().GetLastError());
            return rc;
        }

        char* buf = AllocResultString(out_json);
        if (!buf) {
            LastErrorStore::Instance().Set(
                "OCR_RecognizeMemory: failed to allocate output buffer");
            return MEDOCR_ERR_OUTPUT_ALLOC_FAILED;
        }
        *output_json_utf8 = buf;
        return MEDOCR_OK;
    });
}

OCR_API void OCR_FreeResult(char* output_json_utf8) {
    // Safe to call with NULL.
    std::free(output_json_utf8);
}

OCR_API const char* OCR_GetVersion(void) {
    return medical_ocr::MedicalOcrService::Instance().GetVersion();
}

OCR_API const char* OCR_GetLastError(void) {
    // Return a pointer to a thread-local or mutex-protected buffer.
    // The pointer is valid until the next API call.
    static thread_local std::string buffer;
    buffer = LastErrorStore::Instance().Get();
    return buffer.c_str();
}

OCR_API void OCR_Shutdown(void) {
    SafeCall("OCR_Shutdown", [&]() -> int {
        medical_ocr::MedicalOcrService::Instance().Shutdown();
        return MEDOCR_OK;
    });
}
