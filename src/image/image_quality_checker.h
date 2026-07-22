#ifndef MEDICAL_OCR_IMAGE_QUALITY_CHECKER_H_
#define MEDICAL_OCR_IMAGE_QUALITY_CHECKER_H_

#include <string>

#include "medical_ocr/types.h"

// Forward-declare OpenCV types to avoid leaking cv::Mat in header.
namespace cv {
class Mat;
}

namespace medical_ocr {

/**
 * Configuration for the image quality checker.
 */
struct ImageQualityConfig {
    /// Minimum acceptable image width in pixels.
    int minimum_width = 800;

    /// Minimum acceptable image height in pixels.
    int minimum_height = 800;

    /// Laplacian variance threshold. Images with variance below this
    /// value are considered blurry. Typical values: 50–150.
    double blur_threshold = 80.0;

    /// Fraction of pixels at saturation (≈255) that triggers an
    /// overexposure warning. Range: 0.0–1.0.
    double overexposure_fraction = 0.15;

    /// Fraction of pixels near zero that triggers an underexposure
    /// warning. Range: 0.0–1.0.
    double underexposure_fraction = 0.20;

    /// Minimum fraction of non-zero pixels for an image to be
    /// considered non-empty. Range: 0.0–1.0.
    double empty_image_threshold = 0.01;
};

/**
 * Assesses the quality of a medical document image.
 *
 * Checks performed:
 * - Image emptiness (all black / all white)
 * - Minimum resolution
 * - Blur detection via Laplacian variance
 * - Overexposure / underexposure detection
 * - Glare / reflection detection (large saturated regions)
 *
 * This class is stateless — all methods are const.
 */
class ImageQualityChecker {
public:
    ImageQualityChecker() = default;
    explicit ImageQualityChecker(const ImageQualityConfig& config);

    /**
     * Run all quality checks on an image.
     *
     * @param image  Input image (BGR or grayscale, OpenCV Mat).
     * @param info   Output quality assessment.
     * @return       true if the image passed minimum quality requirements,
     *               false if the image is unusable (empty, too small, etc.).
     */
    bool Check(const cv::Mat& image, QualityInfo& info) const;

    /**
     * Check whether the image is essentially empty / blank.
     */
    bool CheckEmpty(const cv::Mat& image, QualityInfo& info) const;

    /**
     * Check image resolution against minimum requirements.
     */
    bool CheckResolution(const cv::Mat& image, QualityInfo& info) const;

    /**
     * Detect blur using Laplacian variance.
     *
     * The Laplacian operator highlights regions of rapid intensity change.
     * A low variance indicates few edges → blurry image.
     *
     * Reference: Pech-Pacheco et al. (2000), "Diatom autofocusing in
     * brightfield microscopy".
     */
    double ComputeLaplacianVariance(const cv::Mat& gray) const;

    /**
     * Detect overexposure / underexposure via histogram analysis.
     */
    void CheckExposure(const cv::Mat& gray, QualityInfo& info) const;

    /**
     * Detect potential glare / reflection regions.
     * Glare appears as large contiguous saturated (white) areas.
     */
    bool HasGlare(const cv::Mat& gray) const;

private:
    ImageQualityConfig config_;
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_IMAGE_QUALITY_CHECKER_H_
