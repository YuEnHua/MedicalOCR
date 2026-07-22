#ifndef MEDICAL_OCR_I_OCR_ENGINE_H_
#define MEDICAL_OCR_I_OCR_ENGINE_H_

#include <memory>
#include <string>

#include "medical_ocr/types.h"

namespace medical_ocr {

/**
 * Abstract interface for OCR engines.
 *
 * All OCR backends (Mock, PaddleOCR, ONNX, RapidOCR, etc.) must implement
 * this interface. The factory or service layer selects the concrete
 * implementation based on configuration.
 *
 * Thread safety: implementations must document whether they support
 * concurrent calls to Recognize().
 */
class IOcrEngine {
public:
    virtual ~IOcrEngine() = default;

    /**
     * Initialize the OCR engine.
     *
     * @param model_directory  Path to model files (may be empty for mock).
     * @param config_json      Engine-specific JSON configuration.
     * @return                 true on success, false on failure.
     */
    virtual bool Initialize(const std::string& model_directory,
                            const std::string& config_json) = 0;

    /**
     * Recognize text from an image file.
     *
     * @param image_path_utf8  Path to the image file (UTF-8).
     * @param result           Output OCR result.
     * @return                 true on success, false on failure.
     */
    virtual bool Recognize(const std::string& image_path_utf8,
                           OcrResult& result) = 0;

    /**
     * Recognize text from in-memory image data.
     *
     * @param image_data  Raw image bytes.
     * @param image_size  Size of image_data in bytes.
     * @param result      Output OCR result.
     * @return            true on success, false on failure.
     */
    virtual bool Recognize(const unsigned char* image_data,
                           int image_size,
                           OcrResult& result) = 0;

    /**
     * Get the engine name for logging and diagnostics.
     */
    virtual const char* EngineName() const = 0;

    /**
     * Get the last error message from this engine.
     */
    virtual std::string GetLastError() const = 0;

    /**
     * Shut down the engine and release resources.
     */
    virtual void Shutdown() = 0;
};

using IOcrEnginePtr = std::unique_ptr<IOcrEngine>;

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_I_OCR_ENGINE_H_
