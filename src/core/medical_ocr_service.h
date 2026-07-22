#ifndef MEDICAL_OCR_SERVICE_H_
#define MEDICAL_OCR_SERVICE_H_

#include <memory>
#include <mutex>
#include <string>

#include "nlohmann/json.hpp"
#include "medical_ocr/types.h"
#include "src/document/template_repository.h"
#include "src/image/document_preprocessor.h"
#include "src/image/image_quality_checker.h"
#include "src/ocr/i_ocr_engine.h"

namespace medical_ocr {

/**
 * Central service class that orchestrates the OCR pipeline.
 *
 * This is a singleton that manages:
 * - Configuration loading
 * - Image quality assessment
 * - Image preprocessing and perspective correction
 * - OCR engine lifecycle
 * - Pipeline orchestration
 *
 * NOT exposed through the C API boundary — the C API functions
 * delegate to this class internally.
 *
 * Thread safety: Initialize/Shutdown are serialized via mutex.
 * Recognize is reentrant (reads only state, delegates to engine).
 */
class MedicalOcrService {
public:
    static MedicalOcrService& Instance();

    // ---- Lifecycle ----

    /**
     * Initialize the service.
     *
     * @param model_directory  OCR model directory path.
     * @param config_path      medical_ocr.json path (may be empty for defaults).
     * @return                 Error code (0 = success).
     */
    int Initialize(const std::string& model_directory,
                   const std::string& config_path);

    /**
     * Shut down and release all resources.
     */
    void Shutdown();

    /**
     * Check whether the service has been initialized.
     */
    bool IsInitialized() const;

    // ---- Recognition ----

    /**
     * Recognize a medical report from a file.
     *
     * @param image_path_utf8  Path to the image (UTF-8).
     * @param out_json         Output JSON string (allocated by this method,
     *                         caller must free with FreeResult).
     * @return                 Error code (0 = success).
     */
    int RecognizeFile(const std::string& image_path_utf8,
                      std::string& out_json);

    /**
     * Recognize a medical report from memory.
     *
     * @param image_data  Raw image bytes.
     * @param image_size  Size of image data.
     * @param out_json    Output JSON string (allocated by this method,
     *                    caller must free with FreeResult).
     * @return            Error code (0 = success).
     */
    int RecognizeMemory(const unsigned char* image_data,
                        int image_size,
                        std::string& out_json);

    // ---- Information ----

    const char* GetVersion() const;
    std::string GetLastError() const;

private:
    MedicalOcrService();
    ~MedicalOcrService();
    MedicalOcrService(const MedicalOcrService&) = delete;
    MedicalOcrService& operator=(const MedicalOcrService&) = delete;

    /// Decode an image from memory bytes using OpenCV.
    /// Returns true on success.
    bool DecodeImage(const unsigned char* data, int size, cv::Mat& image);

    /// Decode an image from a file path using OpenCV.
    /// Returns true on success.
    bool DecodeImage(const std::string& path, cv::Mat& image);

    /// Run the full pipeline: quality → preprocess → OCR → extract → build JSON.
    int RunPipeline(const cv::Mat& image, std::string& out_json);

    /// Run document classification, template matching, and field extraction.
    void RunExtraction(const OcrResult& ocr_result,
                       RecognitionResult& recog_result);

    /// Mask an ID number for privacy (controlled by config).
    static std::string MaskIdNumber(const std::string& id);

    /// Build the JSON result from internal structures.
    std::string BuildResultJson(const RecognitionResult& result) const;

    /// Create the OCR engine based on configuration.
    IOcrEnginePtr CreateEngine(const std::string& engine_name);

    /// Load preprocessing config from JSON.
    PreprocessConfig LoadPreprocessConfig(const nlohmann::json& cfg) const;

    /// Load quality checker config from JSON.
    ImageQualityConfig LoadQualityConfig(const nlohmann::json& cfg) const;

    std::mutex mutex_;
    bool initialized_ = false;
    std::string last_error_;
    std::string model_directory_;
    std::string config_json_;  // Raw config content for engine init.

    IOcrEnginePtr ocr_engine_;

    // Configuration values loaded from medical_ocr.json.
    std::string ocr_engine_name_ = "mock";
    std::string templates_directory_;
    bool mask_id_number_ = true;
    bool enable_debug_image_ = false;
    bool enable_raw_text_log_ = false;

    // Preprocessing configuration.
    PreprocessConfig preproc_config_;
    ImageQualityConfig quality_config_;

    // Template repository (Phase 3).
    TemplateRepository template_repo_;
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_SERVICE_H_
