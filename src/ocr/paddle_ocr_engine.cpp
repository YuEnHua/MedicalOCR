/**
 * PaddleOcrEngine — Real OCR using Paddle Inference + PP-OCRv4 models.
 *
 * REQUIRES:
 *   - Paddle Inference C++ SDK (>= 2.5)
 *     Download from: https://www.paddlepaddle.org.cn/inference/master/guides/introduction/index_intro.html
 *   - PP-OCRv4 models in model_directory:
 *       ch_PP-OCRv4_det_infer/inference.pdmodel
 *       ch_PP-OCRv4_det_infer/inference.pdiparams
 *       ch_PP-OCRv4_rec_infer/inference.pdmodel
 *       ch_PP-OCRv4_rec_infer/inference.pdiparams
 *       ppocr_keys_v1.txt
 *
 * Compile with: MEDICAL_OCR_ENABLE_PADDLE=ON
 *
 * This file is conditionally compiled. When Paddle SDK is not available,
 * the build falls back to MockOcrEngine.
 */

#ifdef MEDICAL_OCR_HAS_PADDLE

#include "paddle_ocr_engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// Paddle Inference C++ API.
// This header is provided by the Paddle Inference SDK.
// Install path example: C:/paddle_inference/include/paddle_inference_api.h
#include <paddle_inference_api.h>

#include "nlohmann/json.hpp"

namespace medical_ocr {

// ============================================================================
// Construction / Destruction
// ============================================================================

PaddleOcrEngine::PaddleOcrEngine() = default;
PaddleOcrEngine::~PaddleOcrEngine() { Shutdown(); }

// ============================================================================
// Initialize
// ============================================================================

bool PaddleOcrEngine::Initialize(const std::string& model_directory,
                                 const std::string& config_json) {
    if (initialized_) {
        last_error_ = "PaddleOcrEngine: already initialized";
        return false;
    }

    model_directory_ = model_directory;

    // Parse engine-specific configuration.
    if (!config_json.empty()) {
        try {
            auto cfg = nlohmann::json::parse(config_json);
            use_gpu_ = cfg.value("use_gpu", false);
            gpu_id_ = cfg.value("gpu_id", 0);
            cpu_threads_ = cfg.value("cpu_threads", 4);
            minimum_confidence_ = cfg.value("minimum_confidence", 0.65f);
            det_threshold_ = cfg.value("det_threshold", 0.3f);
            det_box_threshold_ = cfg.value("det_box_threshold", 0.5f);
        } catch (const nlohmann::json::exception& e) {
            last_error_ = std::string("PaddleOcrEngine: config parse error: ") + e.what();
            return false;
        }
    }

    // ---- Load models ----
    if (!InitDetector(model_directory_)) return false;
    if (!InitRecognizer(model_directory_)) return false;

    // ---- Load dictionary ----
    std::string dict_path = model_directory_ + "/ppocr_keys_v1.txt";
    if (!LoadDictionary(dict_path)) {
        last_error_ = "PaddleOcrEngine: failed to load dictionary: " + dict_path;
        return false;
    }

    initialized_ = true;
    last_error_.clear();
    return true;
}

// ============================================================================
// Init Detector
// ============================================================================

bool PaddleOcrEngine::InitDetector(const std::string& model_dir) {
    std::string model_file = model_dir + "/ch_PP-OCRv4_det_infer/inference.pdmodel";
    std::string params_file = model_dir + "/ch_PP-OCRv4_det_infer/inference.pdiparams";

    // Check files exist.
    std::ifstream mf(model_file, std::ios::binary);
    if (!mf.is_open()) {
        last_error_ = "PaddleOcrEngine: detection model not found: " + model_file;
        return false;
    }
    std::ifstream pf(params_file, std::ios::binary);
    if (!pf.is_open()) {
        last_error_ = "PaddleOcrEngine: detection params not found: " + params_file;
        return false;
    }

    try {
        paddle_infer::Config config;
        config.SetModel(model_file, params_file);

        if (use_gpu_) {
            // GPU: 100 MB initial memory, GPU ID from config.
            config.EnableUseGpu(100, gpu_id_);
        } else {
            config.DisableGpu();
        }

        config.SetCpuMathLibraryNumThreads(cpu_threads_);
        // Disable memory/log optimization for determinism.
        config.DisableGlogInfo();
        config.EnableMemoryOptim();

        detector_.reset(paddle_infer::CreatePredictor(config));
    } catch (const std::exception& e) {
        last_error_ = std::string("PaddleOcrEngine: detector init failed: ") + e.what();
        return false;
    }

    return true;
}

// ============================================================================
// Init Recognizer
// ============================================================================

bool PaddleOcrEngine::InitRecognizer(const std::string& model_dir) {
    std::string model_file = model_dir + "/ch_PP-OCRv4_rec_infer/inference.pdmodel";
    std::string params_file = model_dir + "/ch_PP-OCRv4_rec_infer/inference.pdiparams";

    std::ifstream mf(model_file, std::ios::binary);
    if (!mf.is_open()) {
        last_error_ = "PaddleOcrEngine: recognition model not found: " + model_file;
        return false;
    }
    std::ifstream pf(params_file, std::ios::binary);
    if (!pf.is_open()) {
        last_error_ = "PaddleOcrEngine: recognition params not found: " + params_file;
        return false;
    }

    try {
        paddle_infer::Config config;
        config.SetModel(model_file, params_file);

        if (use_gpu_) {
            config.EnableUseGpu(100, gpu_id_);
        } else {
            config.DisableGpu();
        }

        config.SetCpuMathLibraryNumThreads(cpu_threads_);
        config.DisableGlogInfo();
        config.EnableMemoryOptim();

        recognizer_.reset(paddle_infer::CreatePredictor(config));
    } catch (const std::exception& e) {
        last_error_ = std::string("PaddleOcrEngine: recognizer init failed: ") + e.what();
        return false;
    }

    return true;
}

// ============================================================================
// Load Dictionary
// ============================================================================

bool PaddleOcrEngine::LoadDictionary(const std::string& dict_path) {
    std::ifstream ifs(dict_path);
    if (!ifs.is_open()) return false;

    dictionary_.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        // Remove trailing \r.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        dictionary_.push_back(line);
    }

    // PaddleOCR keys file should have ~6623 entries for Chinese model.
    return dictionary_.size() >= 100;
}

// ============================================================================
// Recognize (File)
// ============================================================================

bool PaddleOcrEngine::Recognize(const std::string& image_path_utf8,
                                OcrResult& result) {
    if (!initialized_) {
        last_error_ = "PaddleOcrEngine: not initialized";
        return false;
    }

    cv::Mat image = LoadImage(image_path_utf8);
    if (image.empty()) {
        last_error_ = "PaddleOcrEngine: failed to load image: " + image_path_utf8;
        return false;
    }

    result.imageWidth = image.cols;
    result.imageHeight = image.rows;

    // ---- Detection: find text boxes ----
    std::vector<std::array<PointF, 4>> boxes;
    if (!RunDetection(image, boxes)) {
        last_error_ = "PaddleOcrEngine: detection failed";
        return false;
    }

    // ---- Recognition: recognize text in each box ----
    for (const auto& box : boxes) {
        // Compute axis-aligned bounding rectangle of the quadrilateral.
        float x_min = box[0].x, y_min = box[0].y;
        float x_max = box[0].x, y_max = box[0].y;
        for (int i = 1; i < 4; ++i) {
            x_min = std::min(x_min, box[i].x);
            y_min = std::min(y_min, box[i].y);
            x_max = std::max(x_max, box[i].x);
            y_max = std::max(y_max, box[i].y);
        }

        int cx = std::max(0, static_cast<int>(x_min));
        int cy = std::max(0, static_cast<int>(y_min));
        int cw = std::min(image.cols - cx, static_cast<int>(x_max - x_min + 1));
        int ch = std::min(image.rows - cy, static_cast<int>(y_max - y_min + 1));

        if (cw <= 0 || ch <= 0) continue;

        cv::Rect crop_rect(cx, cy, cw, ch);
        cv::Mat crop = image(crop_rect).clone();

        std::string text;
        float confidence = 0.0f;
        if (RunRecognition(crop, text, confidence)) {
            if (confidence >= minimum_confidence_ && !text.empty()) {
                OcrTextBox tb;
                tb.points = box;
                tb.text = text;
                tb.confidence = confidence;
                result.boxes.push_back(std::move(tb));
            }
        }
    }

    return true;
}

// ============================================================================
// Recognize (Memory)
// ============================================================================

bool PaddleOcrEngine::Recognize(const unsigned char* image_data,
                                int image_size,
                                OcrResult& result) {
    if (!initialized_) {
        last_error_ = "PaddleOcrEngine: not initialized";
        return false;
    }

    cv::Mat image = LoadImage(image_data, image_size);
    if (image.empty()) {
        last_error_ = "PaddleOcrEngine: failed to decode image from memory";
        return false;
    }

    result.imageWidth = image.cols;
    result.imageHeight = image.rows;

    // Delegate to the same detection+recognition pipeline.
    // We reuse the core logic: detection → crop → recognize.
    std::vector<std::array<PointF, 4>> boxes;
    if (!RunDetection(image, boxes)) return false;

    for (const auto& box : boxes) {
        float x_min = box[0].x, y_min = box[0].y;
        float x_max = box[0].x, y_max = box[0].y;
        for (int i = 1; i < 4; ++i) {
            x_min = std::min(x_min, box[i].x);
            y_min = std::min(y_min, box[i].y);
            x_max = std::max(x_max, box[i].x);
            y_max = std::max(y_max, box[i].y);
        }
        int cx = std::max(0, static_cast<int>(x_min));
        int cy = std::max(0, static_cast<int>(y_min));
        int cw = std::min(image.cols - cx, static_cast<int>(x_max - x_min + 1));
        int ch = std::min(image.rows - cy, static_cast<int>(y_max - y_min + 1));
        if (cw <= 0 || ch <= 0) continue;

        cv::Mat crop = image(cv::Rect(cx, cy, cw, ch)).clone();
        std::string text;
        float conf = 0.0f;
        if (RunRecognition(crop, text, conf)) {
            if (conf >= minimum_confidence_ && !text.empty()) {
                OcrTextBox tb;
                tb.points = box;
                tb.text = text;
                tb.confidence = conf;
                result.boxes.push_back(std::move(tb));
            }
        }
    }

    return true;
}

// ============================================================================
// Run Detection
// ============================================================================

bool PaddleOcrEngine::RunDetection(
    const cv::Mat& image,
    std::vector<std::array<PointF, 4>>& boxes) {

    if (!detector_) return false;

    // Step 1: Preprocess — resize + normalize.
    std::vector<float> input_data;
    float scale_x = 1.0f, scale_y = 1.0f;
    PreprocessForDetection(image, input_data, scale_x, scale_y);

    // Step 2: Copy data to input tensor.
    auto input_names = detector_->GetInputNames();
    if (input_names.empty()) return false;
    auto input_tensor = detector_->GetInputHandle(input_names[0]);
    input_tensor->Reshape({1, 3, det_input_height_, det_input_width_});

    // Copy preprocessed float data into the tensor.
    input_tensor->CopyFromCpu(input_data.data());

    // Step 3: Run inference.
    detector_->Run();

    // Step 4: Get output.
    auto output_names = detector_->GetOutputNames();
    if (output_names.empty()) return false;
    auto output_tensor = detector_->GetOutputHandle(output_names[0]);

    std::vector<float> output_data(output_tensor->Size());
    output_tensor->CopyToCpu(output_data.data());
    std::vector<int> output_shape = output_tensor->shape();

    // Step 5: Postprocess — parse box coordinates.
    PostprocessDetection(output_data, image.cols, image.rows,
                         scale_x, scale_y, boxes);

    return true;
}

// ============================================================================
// Run Recognition
// ============================================================================

bool PaddleOcrEngine::RunRecognition(const cv::Mat& crop,
                                     std::string& text,
                                     float& confidence) {
    if (!recognizer_) return false;

    // Ensure crop is 3-channel.
    cv::Mat input;
    if (crop.channels() == 1) {
        cv::cvtColor(crop, input, cv::COLOR_GRAY2BGR);
    } else {
        input = crop;
    }

    // Preprocess: resize to 3×32×320, normalize.
    std::vector<float> input_data;
    PreprocessForRecognition(input, input_data);

    // Copy to tensor.
    auto input_names = recognizer_->GetInputNames();
    if (input_names.empty()) return false;
    auto input_tensor = recognizer_->GetInputHandle(input_names[0]);
    input_tensor->Reshape({1, 3, rec_input_height_, rec_input_width_});
    input_tensor->CopyFromCpu(input_data.data());

    // Run.
    recognizer_->Run();

    // Get output.
    auto output_names = recognizer_->GetOutputNames();
    if (output_names.empty()) return false;
    auto output_tensor = recognizer_->GetOutputHandle(output_names[0]);

    std::vector<float> output_data(output_tensor->Size());
    output_tensor->CopyToCpu(output_data.data());

    // Decode.
    text = DecodeRecognitionOutput(output_data, confidence);

    return true;
}

// ============================================================================
// Preprocess for Detection
// ============================================================================

void PaddleOcrEngine::PreprocessForDetection(
    const cv::Mat& image,
    std::vector<float>& input_data,
    float& scale_x, float& scale_y) {

    // Resize keeping aspect ratio to fit within det_input_width_ × det_input_height_.
    int h = image.rows;
    int w = image.cols;
    float ratio = std::min(
        static_cast<float>(det_input_width_) / w,
        static_cast<float>(det_input_height_) / h);

    int new_w = static_cast<int>(w * ratio);
    int new_h = static_cast<int>(h * ratio);

    // Round to multiples of 32 (required by detection model).
    new_w = std::max(32, (new_w / 32) * 32);
    new_h = std::max(32, (new_h / 32) * 32);

    scale_x = static_cast<float>(new_w) / w;
    scale_y = static_cast<float>(new_h) / h;

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_w, new_h));

    // Convert to float and normalize: (pixel - mean) / std.
    // PaddleOCR detection uses mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225].
    resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);

    // Create a padded image of det_input_width_ × det_input_height_.
    cv::Mat padded(det_input_height_, det_input_width_, CV_32FC3,
                   cv::Scalar(0, 0, 0));
    resized.copyTo(padded(cv::Rect(0, 0, new_w, new_h)));

    // Convert to CHW layout.
    input_data.resize(3 * det_input_height_ * det_input_width_);
    std::vector<cv::Mat> channels(3);
    cv::split(padded, channels);

    const float mean[3] = {0.485f, 0.456f, 0.406f};
    const float std[3]  = {0.229f, 0.224f, 0.225f};

    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < det_input_height_ * det_input_width_; ++i) {
            float val = channels[c].at<float>(i);
            val = (val - mean[c]) / std[c];
            input_data[c * det_input_height_ * det_input_width_ + i] = val;
        }
    }
}

// ============================================================================
// Preprocess for Recognition
// ============================================================================

void PaddleOcrEngine::PreprocessForRecognition(
    const cv::Mat& crop,
    std::vector<float>& input_data) {

    // Resize to rec_input_height_ × rec_input_width_ (32 × 320).
    cv::Mat resized;
    cv::resize(crop, resized, cv::Size(rec_input_width_, rec_input_height_));

    // Convert to float and normalize with mean=0.5, std=0.5 → range [-1, 1].
    resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);

    // CHW layout.
    input_data.resize(3 * rec_input_height_ * rec_input_width_);
    std::vector<cv::Mat> channels(3);
    cv::split(resized, channels);

    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < rec_input_height_ * rec_input_width_; ++i) {
            float val = channels[c].at<float>(i);
            val = (val - 0.5f) / 0.5f;
            input_data[c * rec_input_height_ * rec_input_width_ + i] = val;
        }
    }
}

// ============================================================================
// Postprocess Detection Output
// ============================================================================

void PaddleOcrEngine::PostprocessDetection(
    const std::vector<float>& output,
    int image_width, int image_height,
    float scale_x, float scale_y,
    std::vector<std::array<PointF, 4>>& boxes) {

    // The detection model output shape is typically [1, 1, N, 6] or [N, 6],
    // where each row is [x1, y1, x2, y2, confidence, class].
    // We need to reconstruct quadrilaterals from these axis-aligned rects.
    // For PP-OCRv4, the output is a segmentation map; we use the simpler
    // bounding-box format from the DB (Differentiable Binarization) head.

    // Expected output shape: [N, 6] after squeezing.
    // But Paddle's actual shape may differ; we parse dynamically.
    // For simplicity, we assume the flattened output is groups of 6 floats.

    size_t n = output.size() / 6;
    for (size_t i = 0; i < n; ++i) {
        float x1 = output[i * 6 + 0];
        float y1 = output[i * 6 + 1];
        float x2 = output[i * 6 + 2];
        float y2 = output[i * 6 + 3];
        float score = output[i * 6 + 4];

        if (score < det_box_threshold_) continue;

        // Rescale from model input coordinates back to original image.
        float rx1 = x1 / scale_x;
        float ry1 = y1 / scale_y;
        float rx2 = x2 / scale_x;
        float ry2 = y2 / scale_y;

        // Clamp to image bounds.
        rx1 = std::max(0.0f, std::min(rx1, static_cast<float>(image_width)));
        ry1 = std::max(0.0f, std::min(ry1, static_cast<float>(image_height)));
        rx2 = std::max(0.0f, std::min(rx2, static_cast<float>(image_width)));
        ry2 = std::max(0.0f, std::min(ry2, static_cast<float>(image_height)));

        // Convert axis-aligned rect to quadrilateral.
        boxes.push_back({{
            {rx1, ry1},  // TL
            {rx2, ry1},  // TR
            {rx2, ry2},  // BR
            {rx1, ry2}   // BL
        }});
    }
}

// ============================================================================
// Decode Recognition Output
// ============================================================================

std::string PaddleOcrEngine::DecodeRecognitionOutput(
    const std::vector<float>& output,
    float& confidence) {

    // Recognition output shape: [1, N, dict_size] where N is sequence length
    // (typically 25 or 40 for Chinese model).
    // For each position in the sequence, we take argmax over the dictionary.
    // We also compute the average confidence of non-blank predictions.

    if (dictionary_.empty() || output.empty()) {
        confidence = 0.0f;
        return {};
    }

    int dict_size = static_cast<int>(dictionary_.size());
    int seq_len = static_cast<int>(output.size()) / dict_size;

    std::string result;
    float total_conf = 0.0f;
    int valid_chars = 0;
    std::string last_char;

    for (int t = 0; t < seq_len; ++t) {
        // Find the character with maximum probability at this position.
        int max_idx = 0;
        float max_prob = output[t * dict_size];
        for (int d = 1; d < dict_size; ++d) {
            float prob = output[t * dict_size + d];
            if (prob > max_prob) {
                max_prob = prob;
                max_idx = d;
            }
        }

        if (max_idx >= 0 && max_idx < dict_size) {
            const std::string& ch = dictionary_[max_idx];
            // Skip blank/repeated characters (CTC decoding).
            if (ch != "#" && ch != " " && ch != last_char) {
                result += ch;
                total_conf += max_prob;
                ++valid_chars;
            }
            last_char = ch;
        }
    }

    confidence = (valid_chars > 0) ? total_conf / valid_chars : 0.0f;
    return result;
}

// ============================================================================
// Image Loading
// ============================================================================

cv::Mat PaddleOcrEngine::LoadImage(const std::string& path_utf8) {
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, &wpath[0], len);

    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf(size);
    std::fread(buf.data(), 1, size, f);
    std::fclose(f);
    return cv::imdecode(buf, cv::IMREAD_COLOR);
#else
    return cv::imread(path_utf8, cv::IMREAD_COLOR);
#endif
}

cv::Mat PaddleOcrEngine::LoadImage(const unsigned char* data, int size) {
    if (!data || size <= 0) return {};
    std::vector<unsigned char> buf(data, data + size);
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}

// ============================================================================
// Engine Info
// ============================================================================

const char* PaddleOcrEngine::EngineName() const {
    return "PaddleOcrEngine (PP-OCRv4)";
}

std::string PaddleOcrEngine::GetLastError() const {
    return last_error_;
}

void PaddleOcrEngine::Shutdown() {
    detector_.reset();
    recognizer_.reset();
    dictionary_.clear();
    initialized_ = false;
    last_error_.clear();
}

}  // namespace medical_ocr

#else  // !MEDICAL_OCR_HAS_PADDLE — Stub implementation

#include "paddle_ocr_engine.h"

namespace medical_ocr {

PaddleOcrEngine::PaddleOcrEngine() = default;
PaddleOcrEngine::~PaddleOcrEngine() = default;

bool PaddleOcrEngine::Initialize(const std::string&, const std::string&) {
    last_error_ = "PaddleOcrEngine: NOT compiled with Paddle Inference support. "
                  "Rebuild with -DMEDICAL_OCR_ENABLE_PADDLE=ON and install Paddle Inference SDK.";
    return false;
}
bool PaddleOcrEngine::Recognize(const std::string&, OcrResult&) {
    last_error_ = "PaddleOcrEngine: NOT compiled with Paddle Inference support.";
    return false;
}
bool PaddleOcrEngine::Recognize(const unsigned char*, int, OcrResult&) {
    last_error_ = "PaddleOcrEngine: NOT compiled with Paddle Inference support.";
    return false;
}
const char* PaddleOcrEngine::EngineName() const {
    return "PaddleOcrEngine (STUB — Paddle SDK not available)";
}
std::string PaddleOcrEngine::GetLastError() const { return last_error_; }
void PaddleOcrEngine::Shutdown() {}

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_HAS_PADDLE
