#include "image_quality_checker.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace medical_ocr {

// ============================================================================
// Construction
// ============================================================================

ImageQualityChecker::ImageQualityChecker(const ImageQualityConfig& config)
    : config_(config) {}

// ============================================================================
// Main Check
// ============================================================================

bool ImageQualityChecker::Check(const cv::Mat& image, QualityInfo& info) const {
    info = QualityInfo{};  // Reset.

    if (image.empty()) {
        info.is_blurry = true;
        info.document_detected = false;
        return false;
    }

    // ---- Resolution check ----
    if (!CheckResolution(image, info)) {
        return false;
    }

    // Convert to grayscale for subsequent checks.
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // ---- Empty check ----
    if (!CheckEmpty(gray, info)) {
        return false;
    }

    // ---- Blur check ----
    info.blur_score = ComputeLaplacianVariance(gray);
    info.is_blurry = (info.blur_score < config_.blur_threshold);

    // ---- Exposure check ----
    CheckExposure(gray, info);

    // ---- Glare check (does not fail the image, just notes it) ----
    // TODO: Integrate glare into QualityInfo when the field is added.

    return true;
}

// ============================================================================
// Empty Image Check
// ============================================================================

bool ImageQualityChecker::CheckEmpty(const cv::Mat& image,
                                     QualityInfo& info) const {
    // Count non-zero (non-black) pixels.
    int non_zero = cv::countNonZero(image);
    double total = static_cast<double>(image.rows) * image.cols;
    double fraction = static_cast<double>(non_zero) / total;

    if (fraction < config_.empty_image_threshold) {
        // Almost entirely black — probably not a valid scan.
        info.document_detected = false;
        return false;
    }

    // Also check for all-white image (inverted: all pixels near 255).
    cv::Mat inverted = 255 - image;
    int non_white = cv::countNonZero(inverted);
    double white_fraction = static_cast<double>(non_white) / total;
    if (white_fraction < config_.empty_image_threshold) {
        info.document_detected = false;
        return false;
    }

    return true;
}

// ============================================================================
// Resolution Check
// ============================================================================

bool ImageQualityChecker::CheckResolution(const cv::Mat& image,
                                          QualityInfo& info) const {
    if (image.cols < config_.minimum_width ||
        image.rows < config_.minimum_height) {
        // Image is too small to process meaningfully.
        info.document_detected = false;
        return false;
    }
    return true;
}

// ============================================================================
// Laplacian Variance Blur Detection
// ============================================================================

double ImageQualityChecker::ComputeLaplacianVariance(
    const cv::Mat& gray) const {
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F);

    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);

    // Variance = stddev^2
    double variance = stddev[0] * stddev[0];
    return variance;
}

// ============================================================================
// Exposure Check
// ============================================================================

void ImageQualityChecker::CheckExposure(const cv::Mat& gray,
                                        QualityInfo& info) const {
    // Build histogram.
    int hist_size = 256;
    float range[] = {0, 256};
    const float* hist_range = {range};
    cv::Mat hist;
    cv::calcHist(&gray, 1, nullptr, cv::Mat(), hist, 1, &hist_size,
                 &hist_range, true, false);

    double total = static_cast<double>(gray.rows * gray.cols);

    // Overexposure: fraction of pixels in the top 10 bins (246–255).
    double overexposed = 0.0;
    for (int i = 246; i < 256; ++i) {
        overexposed += hist.at<float>(i);
    }
    info.is_overexposed = (overexposed / total > config_.overexposure_fraction);

    // Underexposure: fraction of pixels in the bottom 10 bins (0–9).
    double underexposed = 0.0;
    for (int i = 0; i < 10; ++i) {
        underexposed += hist.at<float>(i);
    }
    // Note: underexposure is logged but does not set is_overexposed.
    // A future revision may add a dedicated is_underexposed field.
    if (underexposed / total > config_.underexposure_fraction) {
        // Image may be too dark — still processable but worth noting.
        // Could extend QualityInfo with is_underexposed in the future.
    }
}

// ============================================================================
// Glare Detection
// ============================================================================

bool ImageQualityChecker::HasGlare(const cv::Mat& gray) const {
    // Threshold to find near-white regions.
    cv::Mat bright_mask;
    cv::threshold(gray, bright_mask, 240, 255, cv::THRESH_BINARY);

    // Find connected components in the bright mask.
    cv::Mat labels, stats, centroids;
    int n_labels = cv::connectedComponentsWithStats(
        bright_mask, labels, stats, centroids, 8, CV_32S);

    double total = static_cast<double>(gray.rows * gray.cols);
    // Skip label 0 (background).
    for (int i = 1; i < n_labels; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        double fraction = static_cast<double>(area) / total;
        // If a single bright region covers more than 5% of the image,
        // it's likely glare.
        if (fraction > 0.05) {
            return true;
        }
    }
    return false;
}

}  // namespace medical_ocr
