#ifndef MEDICAL_OCR_PADDLE_OCR_ENGINE_H_
#define MEDICAL_OCR_PADDLE_OCR_ENGINE_H_

#include <memory>
#include <string>

#include "i_ocr_engine.h"
#include "medical_ocr/types.h"

#ifdef MEDICAL_OCR_HAS_PADDLE
#include <opencv2/core.hpp>
class TextDetPredictor;
class TextRecPredictor;
class CropByPolys;
#endif

namespace medical_ocr {

/**
 * OCR engine using Paddle Inference 3.3.x + official PP-OCRv6 det/rec
 * preprocessing from PaddleOCR deploy/cpp_infer.
 *
 * Requires MEDICAL_OCR_HAS_PADDLE (MEDICAL_OCR_ENABLE_PADDLE=ON + SDK found).
 * Runtime: CPU, enable_mkldnn=false, NewIR on (inference.json).
 */
class PaddleOcrEngine : public IOcrEngine {
public:
    PaddleOcrEngine();
    ~PaddleOcrEngine() override;

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

#ifdef MEDICAL_OCR_HAS_PADDLE
    /// Recognize text from an already-decoded BGR image (avoids encode/decode).
    bool RecognizeMat(const cv::Mat& image, OcrResult& result);
#endif

private:
    std::string last_error_;
    bool initialized_ = false;

    std::string model_directory_;
    std::string det_model_dir_;
    std::string rec_model_dir_;
    std::string det_model_name_ = "PP-OCRv6_small_det";
    std::string rec_model_name_ = "PP-OCRv6_small_rec";

    int cpu_threads_ = 8;
    float minimum_confidence_ = 0.0f;
    bool enable_mkldnn_ = false;

#ifdef MEDICAL_OCR_HAS_PADDLE
    cv::Mat DecodeImageFile(const std::string& path_utf8);
    cv::Mat DecodeImageBuffer(const unsigned char* data, int size);

    std::unique_ptr<TextDetPredictor> detector_;
    std::unique_ptr<TextRecPredictor> recognizer_;
    std::unique_ptr<CropByPolys> crop_by_polys_;
#endif
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_PADDLE_OCR_ENGINE_H_
