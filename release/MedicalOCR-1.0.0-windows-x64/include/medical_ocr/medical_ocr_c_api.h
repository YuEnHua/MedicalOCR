#ifndef MEDICAL_OCR_C_API_H_
#define MEDICAL_OCR_C_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Platform-specific export/import macros
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    // MinGW GCC supports __declspec(dllexport/dllimport) as an extension.
    #ifdef MEDICAL_OCR_DLL_EXPORTS
        #define OCR_API __declspec(dllexport)
    #elif defined(__GNUC__)
        // MinGW GCC: dllimport is optional for functions; omit to avoid
        // warnings when linking against the static import library.
        #define OCR_API
    #else
        #define OCR_API __declspec(dllimport)
    #endif
#else
    #define OCR_API __attribute__((visibility("default")))
#endif

// ============================================================================
// Lifecycle
// ============================================================================

/**
 * Initialize the OCR library.
 *
 * This function must be called once before any other API function.
 * It loads configuration, initializes the OCR engine, and prepares
 * internal resources. Calling it again without a prior OCR_Shutdown
 * returns MEDOCR_ERR_ALREADY_INITIALIZED.
 *
 * @param model_directory  Path to the directory containing OCR model files
 *                         (UTF-8). May be NULL when using the Mock engine.
 * @param config_path      Path to the medical_ocr.json configuration file
 *                         (UTF-8). If NULL, default settings are used.
 * @return                 MEDOCR_OK on success, or an error code.
 */
OCR_API int OCR_Init(
    const char* model_directory,
    const char* config_path
);

/**
 * Recognize text from an image file and return structured JSON.
 *
 * @param image_path_utf8  Path to the image file (UTF-8). Supports common
 *                         formats: JPEG, PNG, BMP, TIFF.
 * @param output_json_utf8 Output parameter. On success, receives a pointer
 *                         to a UTF-8 JSON string allocated by the DLL.
 *                         This string MUST be freed with OCR_FreeResult.
 * @return                 MEDOCR_OK on success, or an error code.
 */
OCR_API int OCR_RecognizeFile(
    const char* image_path_utf8,
    char** output_json_utf8
);

/**
 * Recognize text from in-memory image data and return structured JSON.
 *
 * @param image_data       Pointer to the raw image bytes (JPEG, PNG, BMP, TIFF).
 * @param image_size       Size of the image data in bytes.
 * @param output_json_utf8 Output parameter. On success, receives a pointer
 *                         to a UTF-8 JSON string allocated by the DLL.
 *                         This string MUST be freed with OCR_FreeResult.
 * @return                 MEDOCR_OK on success, or an error code.
 */
OCR_API int OCR_RecognizeMemory(
    const unsigned char* image_data,
    int image_size,
    char** output_json_utf8
);

/**
 * Free a result string previously returned by OCR_RecognizeFile or
 * OCR_RecognizeMemory.
 *
 * Safe to call with NULL (no-op).
 *
 * @param output_json_utf8  The string to free.
 */
OCR_API void OCR_FreeResult(
    char* output_json_utf8
);

/**
 * Get the library version string.
 *
 * The returned pointer is valid for the lifetime of the library.
 * Do not free it.
 *
 * @return  Version string, e.g. "1.0.0".
 */
OCR_API const char* OCR_GetVersion(void);

/**
 * Get a human-readable description of the last error.
 *
 * The returned pointer is valid until the next API call.
 * Do not free it.
 *
 * @return  Error description string (UTF-8).
 */
OCR_API const char* OCR_GetLastError(void);

/**
 * Shut down the OCR library and release all resources.
 *
 * After this call, OCR_Init must be called again before any
 * other API function. Safe to call without a prior OCR_Init.
 */
OCR_API void OCR_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif  // MEDICAL_OCR_C_API_H_
