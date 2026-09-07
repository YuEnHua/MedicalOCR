/**
 * PaddleOcrEngine — PP-OCRv6 via vendored official cpp_infer det+rec.
 *
 * Compile with: MEDICAL_OCR_ENABLE_PADDLE=ON and PADDLE_INFERENCE_DIR set.
 */

#include "paddle_ocr_engine.h"

#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "nlohmann/json.hpp"

#ifdef MEDICAL_OCR_HAS_PADDLE

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "src/common/processors.h"
#include "src/modules/text_detection/predictor.h"
#include "src/modules/text_recognition/predictor.h"
#include "src/utils/ilogger.h"

#endif  // MEDICAL_OCR_HAS_PADDLE

namespace medical_ocr {

PaddleOcrEngine::PaddleOcrEngine() = default;
PaddleOcrEngine::~PaddleOcrEngine() { Shutdown(); }

const char* PaddleOcrEngine::EngineName() const {
#ifdef MEDICAL_OCR_HAS_PADDLE
    return "paddle";
#else
    return "paddle_stub";
#endif
}

std::string PaddleOcrEngine::GetLastError() const { return last_error_; }

void PaddleOcrEngine::Shutdown() {
#ifdef MEDICAL_OCR_HAS_PADDLE
    detector_.reset();
    recognizer_.reset();
    crop_by_polys_.reset();
#endif
    initialized_ = false;
}

#ifndef MEDICAL_OCR_HAS_PADDLE

bool PaddleOcrEngine::Initialize(const std::string&, const std::string&) {
    last_error_ =
        "PaddleOcrEngine: NOT compiled with Paddle Inference support. "
        "Rebuild with -DMEDICAL_OCR_ENABLE_PADDLE=ON and "
        "-DPADDLE_INFERENCE_DIR=...";
    return false;
}

bool PaddleOcrEngine::Recognize(const std::string&, OcrResult&) {
    last_error_ = "PaddleOcrEngine: stub — not compiled with Paddle";
    return false;
}

bool PaddleOcrEngine::Recognize(const unsigned char*, int, OcrResult&) {
    last_error_ = "PaddleOcrEngine: stub — not compiled with Paddle";
    return false;
}

#else  // MEDICAL_OCR_HAS_PADDLE

namespace {

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + "/" + b;
}

bool DirHasInference(const std::string& dir) {
    std::ifstream j(JoinPath(dir, "inference.json"), std::ios::binary);
    std::ifstream p(JoinPath(dir, "inference.pdiparams"), std::ios::binary);
    std::ifstream y(JoinPath(dir, "inference.yml"), std::ios::binary);
    return j.is_open() && p.is_open() && y.is_open();
}

}  // namespace

bool PaddleOcrEngine::Initialize(const std::string& model_directory,
                                 const std::string& config_json) {
    if (initialized_) {
        last_error_ = "PaddleOcrEngine: already initialized";
        return false;
    }

    model_directory_ = model_directory;
    det_model_name_ = "PP-OCRv6_small_det";
    rec_model_name_ = "PP-OCRv6_small_rec";
    det_model_dir_ = JoinPath(model_directory_, det_model_name_);
    rec_model_dir_ = JoinPath(model_directory_, rec_model_name_);
    enable_mkldnn_ = false;
    cpu_threads_ = 8;
    minimum_confidence_ = 0.0f;

    if (!config_json.empty()) {
        try {
            auto cfg = nlohmann::json::parse(config_json);
            cpu_threads_ = cfg.value("cpu_threads", cpu_threads_);
            minimum_confidence_ =
                cfg.value("minimum_confidence", minimum_confidence_);
            enable_mkldnn_ = cfg.value("enable_mkldnn", false);
            if (cfg.contains("det_model_name")) {
                det_model_name_ = cfg["det_model_name"].get<std::string>();
            }
            if (cfg.contains("rec_model_name")) {
                rec_model_name_ = cfg["rec_model_name"].get<std::string>();
            }
            if (cfg.contains("det_model_dir")) {
                det_model_dir_ = cfg["det_model_dir"].get<std::string>();
            } else {
                det_model_dir_ = JoinPath(model_directory_, det_model_name_);
            }
            if (cfg.contains("rec_model_dir")) {
                rec_model_dir_ = cfg["rec_model_dir"].get<std::string>();
            } else {
                rec_model_dir_ = JoinPath(model_directory_, rec_model_name_);
            }
        } catch (const nlohmann::json::exception& e) {
            last_error_ =
                std::string("PaddleOcrEngine: config parse error: ") + e.what();
            return false;
        }
    }

    if (!DirHasInference(det_model_dir_)) {
        last_error_ =
            "PaddleOcrEngine: detection model missing inference.json/"
            "pdiparams/yml under: " +
            det_model_dir_;
        return false;
    }
    if (!DirHasInference(rec_model_dir_)) {
        last_error_ =
            "PaddleOcrEngine: recognition model missing inference.json/"
            "pdiparams/yml under: " +
            rec_model_dir_;
        return false;
    }

    try {
        TextDetPredictorParams det_params;
        det_params.model_name = det_model_name_;
        det_params.model_dir = det_model_dir_;
        det_params.device = std::string("cpu");
        det_params.precision = "fp32";
        det_params.enable_mkldnn = enable_mkldnn_;
        det_params.cpu_threads = cpu_threads_;
        det_params.batch_size = 1;
        det_params.limit_side_len = 64;
        det_params.limit_type = std::string("min");
        det_params.max_side_limit = 4000;
        det_params.thresh = 0.3f;
        det_params.box_thresh = 0.6f;
        det_params.unclip_ratio = 1.5f;

        detector_ = std::make_unique<TextDetPredictor>(det_params);

        TextRecPredictorParams rec_params;
        rec_params.model_name = rec_model_name_;
        rec_params.model_dir = rec_model_dir_;
        rec_params.device = std::string("cpu");
        rec_params.precision = "fp32";
        rec_params.enable_mkldnn = enable_mkldnn_;
        rec_params.cpu_threads = cpu_threads_;
        rec_params.batch_size = 6;

        recognizer_ = std::make_unique<TextRecPredictor>(rec_params);
        crop_by_polys_ = std::make_unique<CropByPolys>("quad");
    } catch (const std::exception& e) {
        last_error_ =
            std::string("PaddleOcrEngine: predictor init failed: ") + e.what();
        Shutdown();
        return false;
    }

    initialized_ = true;
    last_error_.clear();
    return true;
}

cv::Mat PaddleOcrEngine::DecodeImageFile(const std::string& path_utf8) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen > 0 ? wlen - 1 : 0, L'\0');
    if (wlen > 0) {
        MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, &wpath[0], wlen);
    }
    std::ifstream ifs(wpath, std::ios::binary);
#else
    std::ifstream ifs(path_utf8, std::ios::binary);
#endif
    if (!ifs) return {};
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(ifs)),
                                   std::istreambuf_iterator<char>());
    if (buf.empty()) return {};
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}

cv::Mat PaddleOcrEngine::DecodeImageBuffer(const unsigned char* data, int size) {
    if (!data || size <= 0) return {};
    std::vector<unsigned char> buf(data, data + size);
    return cv::imdecode(buf, cv::IMREAD_COLOR);
}

bool PaddleOcrEngine::RecognizeMat(const cv::Mat& image, OcrResult& result) {
    result = OcrResult{};
    if (image.empty()) {
        last_error_ = "PaddleOcrEngine: empty image";
        return false;
    }

    result.imageWidth = image.cols;
    result.imageHeight = image.rows;

    try {
        std::vector<cv::Mat> batch = {image.clone()};
        detector_->Predict(batch);
        auto det_results = detector_->PredictorResult();
        if (det_results.empty()) {
            last_error_.clear();
            return true;
        }

        auto& det = det_results[0];
        if (det.dt_polys.empty()) {
            last_error_.clear();
            return true;
        }

        auto sorted = ComponentsProcessor::SortQuadBoxes(det.dt_polys);
        auto crops_or = (*crop_by_polys_)(image, sorted);
        if (!crops_or.ok()) {
            last_error_ = "PaddleOcrEngine: crop failed: " +
                          std::string(crops_or.status().message());
            return false;
        }
        auto crops = crops_or.value();
        if (crops.empty()) {
            last_error_.clear();
            return true;
        }

        std::vector<cv::Mat> crop_batch;
        crop_batch.reserve(crops.size());
        for (auto& c : crops) {
            crop_batch.push_back(c.clone());
        }

        recognizer_->Predict(crop_batch);
        auto rec_results = recognizer_->PredictorResult();

        const size_t n = (std::min)(sorted.size(), rec_results.size());
        result.boxes.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (rec_results[i].rec_score < minimum_confidence_) {
                continue;
            }
            OcrTextBox box;
            box.text = rec_results[i].rec_text;
            box.confidence = rec_results[i].rec_score;
            const auto& poly = sorted[i];
            for (size_t k = 0; k < 4 && k < poly.size(); ++k) {
                box.points[k].x = poly[k].x;
                box.points[k].y = poly[k].y;
            }
            result.boxes.push_back(std::move(box));
        }
    } catch (const std::exception& e) {
        last_error_ =
            std::string("PaddleOcrEngine: recognize failed: ") + e.what();
        return false;
    }

    last_error_.clear();
    return true;
}

bool PaddleOcrEngine::Recognize(const std::string& image_path_utf8,
                                OcrResult& result) {
    if (!initialized_) {
        last_error_ = "PaddleOcrEngine: not initialized";
        return false;
    }
    cv::Mat image = DecodeImageFile(image_path_utf8);
    if (image.empty()) {
        last_error_ = "PaddleOcrEngine: failed to load image: " + image_path_utf8;
        return false;
    }
    return RecognizeMat(image, result);
}

bool PaddleOcrEngine::Recognize(const unsigned char* image_data,
                                int image_size,
                                OcrResult& result) {
    if (!initialized_) {
        last_error_ = "PaddleOcrEngine: not initialized";
        return false;
    }
    cv::Mat image = DecodeImageBuffer(image_data, image_size);
    if (image.empty()) {
        last_error_ = "PaddleOcrEngine: failed to decode image buffer";
        return false;
    }
    return RecognizeMat(image, result);
}

#endif  // MEDICAL_OCR_HAS_PADDLE

}  // namespace medical_ocr
