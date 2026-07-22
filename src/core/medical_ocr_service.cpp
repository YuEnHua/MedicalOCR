#include "medical_ocr_service.h"

#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "medical_ocr/error_codes.h"
#include "nlohmann/json.hpp"
#include "src/common/geometry_utils.h"
#include "src/document/document_classifier.h"
#include "src/document/template_matcher.h"
#include "src/extraction/field_extractor.h"
#include "src/ocr/mock_ocr_engine.h"
#include "src/ocr/paddle_ocr_engine.h"

namespace medical_ocr {

// ============================================================================
// Singleton
// ============================================================================

MedicalOcrService& MedicalOcrService::Instance() {
    static MedicalOcrService instance;
    return instance;
}

MedicalOcrService::MedicalOcrService() = default;
MedicalOcrService::~MedicalOcrService() { Shutdown(); }

// ============================================================================
// Lifecycle
// ============================================================================

int MedicalOcrService::Initialize(const std::string& model_directory,
                                  const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        last_error_ = "Service is already initialized. Call OCR_Shutdown first.";
        return MEDOCR_ERR_ALREADY_INITIALIZED;
    }

    model_directory_ = model_directory;

    // ---- Load configuration ----
    if (!config_path.empty()) {
        std::ifstream ifs(config_path);
        if (!ifs.is_open()) {
            last_error_ = "Configuration file not found: " + config_path;
            return MEDOCR_ERR_CONFIG_NOT_FOUND;
        }
        try {
            auto cfg = nlohmann::json::parse(ifs);

            // Parse OCR engine config.
            if (cfg.contains("ocr")) {
                const auto& ocr = cfg["ocr"];
                ocr_engine_name_ = ocr.value("engine", "mock");

                nlohmann::json engine_cfg;
                engine_cfg["model_directory"] = model_directory_;
                engine_cfg["use_gpu"] = ocr.value("use_gpu", false);
                engine_cfg["cpu_threads"] = ocr.value("cpu_threads", 4);
                engine_cfg["minimum_confidence"] = ocr.value("minimum_confidence", 0.65);
                config_json_ = engine_cfg.dump();
            }

            // Parse preprocessing config.
            if (cfg.contains("preprocessing")) {
                preproc_config_ = LoadPreprocessConfig(cfg);
                quality_config_ = LoadQualityConfig(cfg);
            }

            // Parse privacy config.
            if (cfg.contains("privacy")) {
                const auto& pr = cfg["privacy"];
                mask_id_number_ = pr.value("mask_id_number", true);
                enable_debug_image_ = pr.value("enable_debug_image_output", false);
                enable_raw_text_log_ = pr.value("enable_raw_text_log", false);
            }

            templates_directory_ = cfg.value("templates_directory", "./config/templates");
        } catch (const nlohmann::json::exception& e) {
            last_error_ = std::string("Failed to parse configuration: ") + e.what();
            return MEDOCR_ERR_CONFIG_NOT_FOUND;
        }
    } else {
        config_json_ = R"({"model_directory":"","use_gpu":false,"cpu_threads":4,"minimum_confidence":0.65})";
    }

    // ---- Create OCR engine ----
    ocr_engine_ = CreateEngine(ocr_engine_name_);
    if (!ocr_engine_) {
        last_error_ = "Failed to create OCR engine: " + ocr_engine_name_;
        return MEDOCR_ERR_INIT_FAILED;
    }

    if (!ocr_engine_->Initialize(model_directory_, config_json_)) {
        last_error_ = "OCR engine initialization failed: " + ocr_engine_->GetLastError();
        ocr_engine_.reset();
        return MEDOCR_ERR_MODEL_LOAD_FAILED;
    }

    // ---- Load templates ----
    if (!templates_directory_.empty()) {
        int count = template_repo_.LoadFromDirectory(templates_directory_);
        // Templates are optional — OK if none loaded.
        (void)count;
    }

    initialized_ = true;
    last_error_.clear();
    return MEDOCR_OK;
}

void MedicalOcrService::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (ocr_engine_) {
        ocr_engine_->Shutdown();
        ocr_engine_.reset();
    }
    initialized_ = false;
    last_error_.clear();
}

bool MedicalOcrService::IsInitialized() const {
    return initialized_;
}

// ============================================================================
// Image Decoding
// ============================================================================

bool MedicalOcrService::DecodeImage(const unsigned char* data, int size,
                                    cv::Mat& image) {
    if (!data || size <= 0) return false;

    // Wrap raw bytes in a cv::Mat for imdecode.
    std::vector<unsigned char> buffer(data, data + size);
    image = cv::imdecode(buffer, cv::IMREAD_COLOR);
    return !image.empty();
}

bool MedicalOcrService::DecodeImage(const std::string& path,
                                    cv::Mat& image) {
    // Use imread with the path. On Windows, OpenCV's imread accepts
    // UTF-8 paths when built with the right options.
    // For MSYS2/MinGW builds, we need to convert to wide path.
#ifdef _WIN32
    // Convert UTF-8 to wide string for Windows file API.
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len <= 0) return false;
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);

    // Use imdecode with file bytes for reliable Unicode path support.
    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long fsize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { std::fclose(f); return false; }
    std::vector<unsigned char> buf(fsize);
    std::fread(buf.data(), 1, fsize, f);
    std::fclose(f);

    image = cv::imdecode(buf, cv::IMREAD_COLOR);
#else
    image = cv::imread(path, cv::IMREAD_COLOR);
#endif
    return !image.empty();
}

// ============================================================================
// Pipeline
// ============================================================================

int MedicalOcrService::RunPipeline(const cv::Mat& image,
                                   std::string& out_json) {
    RecognitionResult recog_result;

    // ---- Stage 1: Quality assessment ----
    ImageQualityChecker quality_checker(quality_config_);
    QualityInfo quality_info;
    bool quality_ok = quality_checker.Check(image, quality_info);
    recog_result.quality = quality_info;

    if (!quality_ok) {
        recog_result.success = false;
        recog_result.error_code = MEDOCR_ERR_IMAGE_QUALITY_FAILED;
        recog_result.message = "Image quality check failed";
        if (quality_info.is_blurry) {
            recog_result.warnings.push_back(
                "Image is blurry (Laplacian variance: " +
                std::to_string(static_cast<int>(quality_info.blur_score)) +
                ", threshold: " +
                std::to_string(static_cast<int>(quality_config_.blur_threshold)) +
                ")");
        }
        if (!quality_info.document_detected) {
            recog_result.warnings.push_back(
                "No document detected — image may be empty or too small");
        }
        out_json = BuildResultJson(recog_result);
        return MEDOCR_ERR_IMAGE_QUALITY_FAILED;
    }

    // ---- Stage 2: Preprocessing ----
    DocumentPreprocessor preprocessor(preproc_config_);
    cv::Mat processed;
    DocumentDetectionResult doc_detection;

    bool pp_ok = preprocessor.Process(image, processed, &doc_detection);

    // Use the preprocessed image if available; otherwise fall back to original.
    const cv::Mat& ocr_input = pp_ok ? processed : image;

    // Fill document info.
    recog_result.document.image_width = ocr_input.cols;
    recog_result.document.image_height = ocr_input.rows;
    recog_result.document.page_type = doc_detection.found
                                          ? doc_detection.page_type
                                          : "unknown";

    if (!doc_detection.found && preproc_config_.enable_document_detection) {
        recog_result.warnings.push_back(
            "Document contour not detected — using original image");
    }

    // ---- Stage 3: OCR ----
    OcrResult ocr_result;
    // For file-based recognition with the mock engine, we pass through
    // RecognizeMemory since we already decoded the image.
    if (!ocr_engine_->Recognize(
            reinterpret_cast<const unsigned char*>(ocr_input.data),
            static_cast<int>(ocr_input.total() * ocr_input.elemSize()),
            ocr_result)) {
        last_error_ = "OCR recognition failed: " + ocr_engine_->GetLastError();
        recog_result.success = false;
        recog_result.error_code = MEDOCR_ERR_OCR_FAILED;
        recog_result.message = "OCR failed";
        out_json = BuildResultJson(recog_result);
        return MEDOCR_ERR_OCR_FAILED;
    }

    // ---- Stage 4: Document classification + template matching + field extraction ----
    RunExtraction(ocr_result, recog_result);

    // ---- Stage 5: Build result ----
    recog_result.success = true;
    recog_result.error_code = MEDOCR_OK;
    recog_result.message = "ok";

    // If blur was borderline, add a note.
    if (quality_info.is_blurry) {
        recog_result.warnings.push_back(
            "Image may be slightly blurry (score: " +
            std::to_string(static_cast<int>(quality_info.blur_score)) + ")");
    }

    try {
        out_json = BuildResultJson(recog_result);
    } catch (const std::exception& e) {
        last_error_ = std::string("JSON build failed: ") + e.what();
        return MEDOCR_ERR_OUTPUT_ALLOC_FAILED;
    }

    return MEDOCR_OK;
}

// ============================================================================
// Recognition (Public)
// ============================================================================

int MedicalOcrService::RecognizeFile(const std::string& image_path_utf8,
                                     std::string& out_json) {
    if (!initialized_) {
        last_error_ = "Service not initialized. Call OCR_Init first.";
        return MEDOCR_ERR_NOT_INITIALIZED;
    }

    if (image_path_utf8.empty()) {
        last_error_ = "Image path is empty.";
        return MEDOCR_ERR_INVALID_ARGUMENT;
    }

    // Decode image.
    cv::Mat image;
    if (!DecodeImage(image_path_utf8, image)) {
        // Check if file exists at all.
        std::ifstream test(image_path_utf8, std::ios::binary);
        if (!test.is_open()) {
            last_error_ = "Image file not found: " + image_path_utf8;
            return MEDOCR_ERR_FILE_NOT_FOUND;
        }
        last_error_ = "Failed to decode image: " + image_path_utf8;
        return MEDOCR_ERR_IMAGE_DECODE_FAILED;
    }

    return RunPipeline(image, out_json);
}

int MedicalOcrService::RecognizeMemory(const unsigned char* image_data,
                                       int image_size,
                                       std::string& out_json) {
    if (!initialized_) {
        last_error_ = "Service not initialized. Call OCR_Init first.";
        return MEDOCR_ERR_NOT_INITIALIZED;
    }

    if (!image_data || image_size <= 0) {
        last_error_ = "Invalid image data pointer or size.";
        return MEDOCR_ERR_INVALID_ARGUMENT;
    }

    // Decode image from memory.
    cv::Mat image;
    if (!DecodeImage(image_data, image_size, image)) {
        last_error_ = "Failed to decode image from memory buffer";
        return MEDOCR_ERR_IMAGE_DECODE_FAILED;
    }

    return RunPipeline(image, out_json);
}

// ============================================================================
// Information
// ============================================================================

const char* MedicalOcrService::GetVersion() const {
    return "1.0.0";
}

std::string MedicalOcrService::GetLastError() const {
    return last_error_;
}

// ============================================================================
// Factory
// ============================================================================

IOcrEnginePtr MedicalOcrService::CreateEngine(const std::string& engine_name) {
    if (engine_name == "mock") {
        return std::make_unique<MockOcrEngine>();
    }
    if (engine_name == "paddle") {
        return std::make_unique<PaddleOcrEngine>();
    }
    last_error_ = "Unknown OCR engine: " + engine_name;
    return nullptr;
}

// ============================================================================
// Configuration Helpers
// ============================================================================

PreprocessConfig MedicalOcrService::LoadPreprocessConfig(
    const nlohmann::json& cfg) const {
    PreprocessConfig pc;
    const auto& pp = cfg["preprocessing"];

    pc.enable_document_detection =
        pp.value("enable_document_detection", true);
    pc.enable_perspective_correction =
        pp.value("enable_perspective_correction", true);
    pc.enable_grayscale =
        pp.value("enable_grayscale", false);
    pc.enable_contrast_enhancement =
        pp.value("enable_contrast_enhancement", true);
    pc.enable_adaptive_threshold =
        pp.value("enable_adaptive_threshold", false);
    pc.target_width = pp.value("target_width", 2480);
    pc.target_height = pp.value("target_height", 3508);

    return pc;
}

ImageQualityConfig MedicalOcrService::LoadQualityConfig(
    const nlohmann::json& cfg) const {
    ImageQualityConfig qc;
    const auto& pp = cfg["preprocessing"];

    qc.minimum_width = pp.value("minimum_width", 800);
    qc.minimum_height = pp.value("minimum_height", 800);
    qc.blur_threshold = pp.value("blur_threshold", 80.0);

    return qc;
}

// ============================================================================
// Extraction Pipeline
// ============================================================================

void MedicalOcrService::RunExtraction(const OcrResult& ocr_result,
                                      RecognitionResult& recog_result) {
    // Step 1: Normalize coordinates to 0–1000 for template matching.
    OcrResult normalized = ocr_result;
    geometry::NormalizeCoordinates(normalized, 1000);

    // Step 2: Concatenate all OCR text for classification.
    std::string all_text = TemplateMatcher::ConcatenateText(normalized);

    // Step 3: Classify document type.
    recog_result.document.type = DocumentClassifier::Classify(all_text);

    // Step 4: Match against templates.
    TemplateMatcher matcher;
    TemplateMatchResult match = matcher.Match(normalized, template_repo_);

    if (!match.matched_template.template_id.empty()) {
        recog_result.document.template_id = match.matched_template.template_id;
        recog_result.document.type =
            StringToDocumentType(match.matched_template.document_type);

        // Step 5: Extract fields using the matched template.
        try {
            FieldExtractor::ExtractAll(
                normalized.boxes,
                match.matched_template,
                recog_result.patient,
                recog_result.examination,
                recog_result.warnings);
        } catch (const std::exception& e) {
            recog_result.warnings.push_back(
                std::string("Field extraction error: ") + e.what());
        }

        recog_result.warnings.push_back(
            "Matched template: " + match.matched_template.template_id +
            " (score: " + std::to_string(match.score) + ")");
    } else {
        // No template matched — still try basic keyword-based extraction.
        recog_result.warnings.push_back(
            "No template matched. Document type: " +
            std::string(DocumentTypeToString(recog_result.document.type)));

        // Attempt extraction using a generic template if available.
        // For now, report the template count.
        recog_result.warnings.push_back(
            "Templates loaded: " + std::to_string(template_repo_.Count()) +
            ", none matched the report");
    }
}

// ============================================================================
// ID Masking
// ============================================================================

std::string MedicalOcrService::MaskIdNumber(const std::string& id) {
    if (id.size() < 8) return std::string(id.size(), '*');

    // Keep first 4 and last 4 characters; mask the middle.
    // "440305198809211234" → "4403**********1234"
    return id.substr(0, 4) + std::string(id.size() - 8, '*') +
           id.substr(id.size() - 4);
}

// ============================================================================
// JSON Result Builder
// ============================================================================

std::string MedicalOcrService::BuildResultJson(
    const RecognitionResult& result) const {
    nlohmann::json j;

    j["success"] = result.success;
    j["error_code"] = result.error_code;
    j["message"] = result.message;

    // Document classification
    j["document"]["type"] = DocumentTypeToString(result.document.type);
    j["document"]["template_id"] = result.document.template_id;
    j["document"]["page_type"] = result.document.page_type;
    j["document"]["image_width"] = result.document.image_width;
    j["document"]["image_height"] = result.document.image_height;

    // Patient info
    auto serialize_field = [](const ExtractedField& f) -> nlohmann::json {
        if (f.value.empty() && f.raw_value.empty()) return nullptr;
        nlohmann::json fj;
        fj["value"] = f.value;
        if (!f.raw_value.empty() && f.raw_value != f.value) {
            fj["raw_value"] = f.raw_value;
        }
        fj["confidence"] = f.confidence;
        fj["validation_status"] = ValidationStatusToString(f.validation_status);
        if (f.extraction_method != ExtractionMethod::Default) {
            fj["extraction_method"] = ExtractionMethodToString(f.extraction_method);
        }
        return fj;
    };

    auto& p = result.patient;
    if (auto fj = serialize_field(p.name); !fj.is_null())
        j["patient"]["name"] = fj;
    if (auto fj = serialize_field(p.gender); !fj.is_null())
        j["patient"]["gender"] = fj;
    if (auto fj = serialize_field(p.birth_date); !fj.is_null())
        j["patient"]["birth_date"] = fj;
    if (auto fj = serialize_field(p.age); !fj.is_null())
        j["patient"]["age"] = fj;
    if (auto fj = serialize_field(p.id_number); !fj.is_null()) {
        // Apply ID masking if configured.
        if (mask_id_number_ && !p.id_number.value.empty()) {
            std::string masked_value = MaskIdNumber(p.id_number.value);
            fj["value"] = masked_value;
            if (!p.id_number.raw_value.empty()) {
                fj["raw_value"] = MaskIdNumber(p.id_number.raw_value);
            }
            fj["checksum_valid"] =
                (p.id_number.validation_status == ValidationStatus::Valid);
        }
        j["patient"]["id_number"] = fj;
    }

    // Examination info
    auto& e = result.examination;
    if (auto fj = serialize_field(e.hospital_name); !fj.is_null())
        j["examination"]["hospital_name"] = fj;
    if (auto fj = serialize_field(e.report_type); !fj.is_null())
        j["examination"]["report_type"] = fj;
    if (auto fj = serialize_field(e.exam_name); !fj.is_null())
        j["examination"]["exam_name"] = fj;
    if (auto fj = serialize_field(e.exam_date); !fj.is_null())
        j["examination"]["exam_date"] = fj;
    if (auto fj = serialize_field(e.department); !fj.is_null())
        j["examination"]["department"] = fj;
    if (auto fj = serialize_field(e.findings); !fj.is_null())
        j["examination"]["findings"] = fj;
    if (auto fj = serialize_field(e.impression); !fj.is_null())
        j["examination"]["impression"] = fj;
    if (auto fj = serialize_field(e.report_doctor); !fj.is_null())
        j["examination"]["report_doctor"] = fj;
    if (auto fj = serialize_field(e.review_doctor); !fj.is_null())
        j["examination"]["review_doctor"] = fj;

    // Quality
    j["quality"]["blur_score"] = result.quality.blur_score;
    j["quality"]["is_blurry"] = result.quality.is_blurry;
    j["quality"]["is_overexposed"] = result.quality.is_overexposed;
    j["quality"]["document_detected"] = result.quality.document_detected;

    // Warnings
    j["warnings"] = result.warnings;

    return j.dump(2);
}

}  // namespace medical_ocr
