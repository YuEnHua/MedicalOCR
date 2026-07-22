#ifndef MEDICAL_OCR_ERROR_CODES_H_
#define MEDICAL_OCR_ERROR_CODES_H_

#ifdef __cplusplus
extern "C" {
#endif

/// Error codes returned by the Medical OCR DLL API.
typedef enum {
    /// Operation completed successfully.
    MEDOCR_OK = 0,

    // ---- Initialization errors (1000–1999) ----
    /// The library has not been initialized. Call OCR_Init first.
    MEDOCR_ERR_NOT_INITIALIZED = 1001,
    /// Initialization failed (generic).
    MEDOCR_ERR_INIT_FAILED = 1002,
    /// Configuration file not found or unreadable.
    MEDOCR_ERR_CONFIG_NOT_FOUND = 1003,
    /// Model files could not be loaded.
    MEDOCR_ERR_MODEL_LOAD_FAILED = 1004,
    /// Already initialized. Call OCR_Shutdown first to re-initialize.
    MEDOCR_ERR_ALREADY_INITIALIZED = 1005,

    // ---- Input errors (2000–2999) ----
    /// The specified image file does not exist.
    MEDOCR_ERR_FILE_NOT_FOUND = 2001,
    /// Unsupported image format.
    MEDOCR_ERR_UNSUPPORTED_FORMAT = 2002,
    /// Failed to decode the image.
    MEDOCR_ERR_IMAGE_DECODE_FAILED = 2003,
    /// Image does not meet quality requirements.
    MEDOCR_ERR_IMAGE_QUALITY_FAILED = 2004,

    // ---- OCR errors (3000–3999) ----
    /// OCR inference failed.
    MEDOCR_ERR_OCR_FAILED = 3001,
    /// OCR engine returned no results.
    MEDOCR_ERR_OCR_NO_RESULT = 3002,

    // ---- Extraction errors (4000–4999) ----
    /// Field extraction failed.
    MEDOCR_ERR_EXTRACTION_FAILED = 4001,
    /// No matching template found for this document.
    MEDOCR_ERR_NO_TEMPLATE_MATCH = 4002,

    // ---- Output errors (5000–5999) ----
    /// Failed to allocate memory for the output.
    MEDOCR_ERR_OUTPUT_ALLOC_FAILED = 5001,

    // ---- General errors (9000–9999) ----
    /// An unknown internal error occurred.
    MEDOCR_ERR_UNKNOWN = 9000,
    /// Invalid argument passed to the API function.
    MEDOCR_ERR_INVALID_ARGUMENT = 9001,
    /// An internal C++ exception was caught.
    MEDOCR_ERR_INTERNAL_EXCEPTION = 9002

} MedOcrErrorCode;

/// Return a human-readable description for an error code.
const char* MedOcrErrorCodeToString(int error_code);

#ifdef __cplusplus
}
#endif

#endif  // MEDICAL_OCR_ERROR_CODES_H_
