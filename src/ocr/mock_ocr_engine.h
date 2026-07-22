#ifndef MEDICAL_OCR_MOCK_OCR_ENGINE_H_
#define MEDICAL_OCR_MOCK_OCR_ENGINE_H_

#include <string>
#include <vector>

#include "i_ocr_engine.h"
#include "medical_ocr/types.h"

namespace medical_ocr {

/**
 * A mock OCR engine for testing and development.
 *
 * This engine does NOT perform real OCR. Instead, it can:
 * - Return hard-coded sample text boxes for basic pipeline testing.
 * - Load OCR results from a JSON file (see samples/mock_ocr_result.json).
 *
 * This allows testing the full pipeline (extraction, validation, JSON output)
 * without installing PaddleOCR or any heavy dependencies.
 *
 * When the "mock_data_path" config key points to a valid JSON file, the engine
 * reads pre-recorded OCR results from that file. Otherwise it returns a small
 * set of built-in sample results that represent a typical ultrasound report.
 */
class MockOcrEngine : public IOcrEngine {
public:
    MockOcrEngine();
    ~MockOcrEngine() override;

    // ---- IOcrEngine interface ----
    bool Initialize(const std::string& model_directory,
                    const std::string& config_json) override;
    bool Recognize(const std::string& image_path_utf8,
                   OcrResult& result) override;
    bool Recognize(const unsigned char* image_data,
                   int image_size,
                   OcrResult& result) override;
    const char* EngineName() const override;
    std::string GetLastError() const override;
    void Shutdown() override;

private:
    /// Load OCR results from a JSON file.
    bool LoadMockData(const std::string& json_path);

    /// Generate built-in sample OCR results for a typical ultrasound report.
    OcrResult GenerateSampleResult() const;

    /// OCR results loaded from file, or generated at init time.
    OcrResult mock_result_;
    std::string last_error_;
    bool initialized_ = false;
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_MOCK_OCR_ENGINE_H_
