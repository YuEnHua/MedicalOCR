#ifndef MEDICAL_OCR_DOCUMENT_PREPROCESSOR_H_
#define MEDICAL_OCR_DOCUMENT_PREPROCESSOR_H_

#include <array>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace medical_ocr {

/**
 * Configuration for the document preprocessor.
 *
 * Each preprocessing step can be individually toggled via the JSON
 * configuration file.
 */
struct PreprocessConfig {
    // ---- Document detection ----
    /// Enable document contour detection and perspective correction.
    bool enable_document_detection = true;

    /// Enable perspective correction via warpPerspective.
    bool enable_perspective_correction = true;

    /// Expected target width after perspective correction.
    int target_width = 2480;   // A4 at 300 DPI

    /// Expected target height after perspective correction.
    int target_height = 3508;  // A4 at 300 DPI

    /// Canny edge detection low threshold.
    double canny_low = 50.0;

    /// Canny edge detection high threshold.
    double canny_high = 150.0;

    /// Minimum contour area as fraction of total image area for a
    /// contour to be considered as a potential document.
    double min_contour_area_fraction = 0.05;

    // ---- Image enhancement ----
    /// Convert to grayscale before OCR.
    bool enable_grayscale = false;

    /// Enable contrast enhancement via CLAHE.
    bool enable_contrast_enhancement = true;

    /// CLAHE clip limit.
    double clahe_clip_limit = 2.0;

    /// CLAHE tile grid size.
    int clahe_tile_size = 8;

    /// Enable adaptive thresholding (binarization).
    bool enable_adaptive_threshold = false;

    /// Adaptive threshold block size (must be odd).
    int adaptive_threshold_block = 11;

    /// Adaptive threshold constant subtracted from mean.
    double adaptive_threshold_c = 2.0;

    // ---- Denoising ----
    /// Enable Gaussian denoising.
    bool enable_denoise = true;

    /// Gaussian kernel size (must be odd).
    int gaussian_kernel = 3;

    /// Gaussian sigma.
    double gaussian_sigma = 1.0;

    // ---- Orientation ----
    /// Enable automatic orientation correction (EXIF + content-based).
    bool enable_orientation_correction = true;
};

/**
 * Result of document detection within an image.
 */
struct DocumentDetectionResult {
    /// Whether a document quadrilateral was found.
    bool found = false;

    /// The 4 corners of the detected document (TL, TR, BR, BL).
    std::array<cv::Point, 4> corners;

    /// The axis-aligned bounding rectangle of the detected document.
    cv::Rect bounding_rect;

    /// Detected page type: "A4", "A5", or "unknown".
    std::string page_type = "unknown";

    /// Aspect ratio (width/height) of the detected quadrilateral.
    double aspect_ratio = 0.0;

    /// Confidence in the document detection [0.0, 1.0].
    float detection_confidence = 0.0f;
};

/**
 * Preprocesses medical document images for OCR.
 *
 * Pipeline (each step configurable):
 * 1. EXIF / image orientation correction
 * 2. Convert to grayscale
 * 3. Denoise (Gaussian)
 * 4. Contrast enhancement (CLAHE)
 * 5. Adaptive thresholding (binarization)
 * 6. Document contour detection → largest quadrilateral
 * 7. Corner ordering (TL, TR, BR, BL)
 * 8. Perspective correction (warpPerspective)
 * 9. Auto orientation detection (horizontal vs vertical)
 * 10. A4/A5 aspect ratio classification
 *
 * All processing is stateless — configuration is passed at construction.
 */
class DocumentPreprocessor {
public:
    DocumentPreprocessor() = default;
    explicit DocumentPreprocessor(const PreprocessConfig& config);

    /**
     * Run the full preprocessing pipeline.
     *
     * @param input       Input image (BGR, as loaded by OpenCV).
     * @param output      Output image ready for OCR.
     * @param doc_result  Optional document detection result (may be nullptr).
     * @return            true on success.
     */
    bool Process(const cv::Mat& input, cv::Mat& output,
                 DocumentDetectionResult* doc_result = nullptr);

    /**
     * Run only image enhancement steps (no geometric correction).
     * Used when document detection is disabled.
     */
    bool EnhanceOnly(const cv::Mat& input, cv::Mat& output);

    // ---- Individual steps (public for testing) ----

    /// Detect the largest quadrilateral document contour.
    bool DetectDocument(const cv::Mat& gray,
                        DocumentDetectionResult& result) const;

    /// Order 4 corners as TL, TR, BR, BL.
    static std::array<cv::Point, 4> OrderCorners(
        const std::vector<cv::Point>& corners);

    /// Apply perspective correction using the 4 detected corners.
    bool ApplyPerspectiveCorrection(const cv::Mat& input,
                                    const std::array<cv::Point, 4>& corners,
                                    int target_width, int target_height,
                                    cv::Mat& output) const;

    /// Detect page type (A4/A5) from aspect ratio.
    static std::string DetectPageType(double aspect_ratio);

    /// Auto-detect and correct image orientation.
    bool CorrectOrientation(cv::Mat& image) const;

    /// Apply CLAHE contrast enhancement to a grayscale image.
    void ApplyClahe(const cv::Mat& gray, cv::Mat& output) const;

    /// Apply Gaussian denoising.
    void ApplyDenoise(const cv::Mat& input, cv::Mat& output) const;

private:
    PreprocessConfig config_;
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_DOCUMENT_PREPROCESSOR_H_
