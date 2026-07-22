#ifndef MEDICAL_OCR_PADDLE_OCR_ENGINE_H_
#define MEDICAL_OCR_PADDLE_OCR_ENGINE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "i_ocr_engine.h"
#include "medical_ocr/types.h"

// Forward declarations for Paddle Inference types (avoid leaking headers).
// Real types are in "paddle_inference_api.h".
namespace paddle_infer {
class Predictor;
class Config;
}  // namespace paddle_infer

namespace cv {
class Mat;
}  // namespace cv

namespace medical_ocr {

/**
 * OCR engine using Paddle Inference + PP-OCRv4 models.
 *
 * Dependencies:
 *   - Paddle Inference C++ SDK (>= 2.5)
 *   - ch_PP-OCRv4_det_infer (text detection model)
 *   - ch_PP-OCRv4_rec_infer (text recognition model)
 *   - ppocr_keys_v1.txt (character dictionary)
 *
 * Pipeline:
 *   1. Detection:  input image → text box coordinates
 *   2. Crop:       for each detected box, crop and rotate the image region
 *   3. Recognition: each cropped region → text string
 *   4. (Optional) Direction classification for inverted text
 *
 * GPU support is controlled via configuration.
 */
class PaddleOcrEngine : public IOcrEngine {
public:
    PaddleOcrEngine();
    ~PaddleOcrEngine() override;

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
    // ---- Model Loading ----

    /// Initialize the detection model predictor.
    bool InitDetector(const std::string& model_dir);

    /// Initialize the recognition model predictor.
    bool InitRecognizer(const std::string& model_dir);

    /// Load the character dictionary (ppocr_keys_v1.txt).
    bool LoadDictionary(const std::string& dict_path);

    // ---- Inference ----

    /// Run detection model on a preprocessed image.
    /// Returns detected text boxes as quadrilateral coordinates.
    bool RunDetection(const cv::Mat& image,
                      std::vector<std::array<PointF, 4>>& boxes);

    /// Run recognition model on a single cropped text region.
    /// Returns the recognized text and confidence.
    bool RunRecognition(const cv::Mat& crop,
                        std::string& text,
                        float& confidence);

    // ---- Image Preprocessing for PaddleOCR ----

    /// Preprocess an image for the detection model.
    /// Resizes, normalizes, and converts to CHW format expected by Paddle.
    void PreprocessForDetection(const cv::Mat& image,
                                std::vector<float>& input_data,
                                float& scale_x, float& scale_y);

    /// Preprocess a cropped text region for the recognition model.
    /// Resizes to 3x32x320, normalizes with mean=[0.5,0.5,0.5] std=[0.5,0.5,0.5].
    void PreprocessForRecognition(const cv::Mat& crop,
                                  std::vector<float>& input_data);

    // ---- Postprocessing ----

    /// Convert detection model output to quadrilateral boxes.
    void PostprocessDetection(const std::vector<float>& output,
                              int image_width, int image_height,
                              float scale_x, float scale_y,
                              std::vector<std::array<PointF, 4>>& boxes);

    /// Convert recognition model output to text using the dictionary.
    std::string DecodeRecognitionOutput(const std::vector<float>& output,
                                        float& confidence);

    // ---- Image Loading ----

    cv::Mat LoadImage(const std::string& path_utf8);
    cv::Mat LoadImage(const unsigned char* data, int size);

    // ---- State ----
    std::string last_error_;
    bool initialized_ = false;

    // Paddle Inference predictors.
    // Use shared_ptr instead of unique_ptr because shared_ptr's deleter is
    // type-erased — the complete type is not needed in the header.
    // This allows the header to compile without including Paddle SDK headers.
    std::shared_ptr<paddle_infer::Predictor> detector_;
    std::shared_ptr<paddle_infer::Predictor> recognizer_;

    // Character dictionary. Maps class index → character.
    std::vector<std::string> dictionary_;

    // Configuration.
    std::string model_directory_;
    bool use_gpu_ = false;
    int gpu_id_ = 0;
    int cpu_threads_ = 4;
    float minimum_confidence_ = 0.65f;

    // Detection model parameters.
    int det_input_height_ = 640;
    int det_input_width_ = 640;
    float det_threshold_ = 0.3f;
    float det_box_threshold_ = 0.5f;

    // Recognition model parameters.
    int rec_input_height_ = 32;
    int rec_input_width_ = 320;
    std::vector<int> rec_image_shape_ = {3, 32, 320};
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_PADDLE_OCR_ENGINE_H_
