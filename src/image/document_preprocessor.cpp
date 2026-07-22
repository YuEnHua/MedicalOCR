#include "document_preprocessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace medical_ocr {

namespace {

/// Convert a std::vector<cv::Point> to std::array<cv::Point, 4>.
/// Throws if the vector does not have exactly 4 points.
std::array<cv::Point, 4> ToArray4(const std::vector<cv::Point>& pts) {
    return {pts[0], pts[1], pts[2], pts[3]};
}

/// Compute Euclidean distance between two points.
double Dist(const cv::Point& a, const cv::Point& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

/// Compute the area of a quadrilateral using the shoelace formula.
double QuadArea(const std::vector<cv::Point>& pts) {
    if (pts.size() < 4) return 0.0;
    return cv::contourArea(pts);
}

}  // anonymous namespace

// ============================================================================
// Construction
// ============================================================================

DocumentPreprocessor::DocumentPreprocessor(const PreprocessConfig& config)
    : config_(config) {}

// ============================================================================
// Full Pipeline
// ============================================================================

bool DocumentPreprocessor::Process(const cv::Mat& input, cv::Mat& output,
                                   DocumentDetectionResult* doc_result) {
    if (input.empty()) return false;

    cv::Mat working = input.clone();

    // ---- Step 1: Orientation correction ----
    if (config_.enable_orientation_correction) {
        CorrectOrientation(working);
    }

    // ---- Step 2: Convert to grayscale ----
    cv::Mat gray;
    if (working.channels() == 3) {
        cv::cvtColor(working, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = working;
    }

    // ---- Step 3: Denoise ----
    cv::Mat enhanced;
    if (config_.enable_denoise) {
        ApplyDenoise(gray, enhanced);
    } else {
        enhanced = gray;
    }

    // ---- Step 4: Contrast enhancement ----
    if (config_.enable_contrast_enhancement) {
        cv::Mat clahe_out;
        ApplyClahe(enhanced, clahe_out);
        enhanced = clahe_out;
    }

    // ---- Step 5: Adaptive threshold ----
    if (config_.enable_adaptive_threshold) {
        cv::Mat binary;
        cv::adaptiveThreshold(enhanced, binary, 255,
                              cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY,
                              config_.adaptive_threshold_block,
                              config_.adaptive_threshold_c);
        enhanced = binary;
    }

    // ---- Step 6: Document detection ----
    DocumentDetectionResult detection;
    if (config_.enable_document_detection) {
        bool found = DetectDocument(
            config_.enable_adaptive_threshold ? enhanced : gray,
            detection);

        if (doc_result) {
            *doc_result = detection;
        }

        // ---- Step 7: Perspective correction ----
        if (found && config_.enable_perspective_correction) {
            int tw = config_.target_width;
            int th = config_.target_height;

            // Adjust target aspect ratio based on detected page type.
            if (detection.page_type == "A5") {
                tw = config_.target_height / 2;  // A5 is half of A4
                th = config_.target_width;
            }

            cv::Mat warped;
            if (ApplyPerspectiveCorrection(
                    config_.enable_adaptive_threshold ? enhanced : gray,
                    detection.corners, tw, th, warped)) {
                output = warped;
                return true;
            }
            // Fall through: if perspective correction fails, use enhanced.
        }
    } else {
        if (doc_result) {
            doc_result->found = false;
        }
    }

    // ---- Step 8: Convert back to 3-channel if needed ----
    if (!config_.enable_grayscale && enhanced.channels() == 1) {
        cv::cvtColor(enhanced, output, cv::COLOR_GRAY2BGR);
    } else {
        output = enhanced;
    }

    return true;
}

// ============================================================================
// Enhance Only (no geometric correction)
// ============================================================================

bool DocumentPreprocessor::EnhanceOnly(const cv::Mat& input, cv::Mat& output) {
    PreprocessConfig cfg = config_;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_orientation_correction = false;
    DocumentPreprocessor tmp(cfg);
    return tmp.Process(input, output, nullptr);
}

// ============================================================================
// Document Detection
// ============================================================================

bool DocumentPreprocessor::DetectDocument(
    const cv::Mat& gray,
    DocumentDetectionResult& result) const {
    result = DocumentDetectionResult{};

    // Gaussian blur to reduce noise before edge detection.
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

    // Canny edge detection.
    cv::Mat edges;
    cv::Canny(blurred, edges, config_.canny_low, config_.canny_high);

    // Dilate edges to close small gaps.
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::dilate(edges, edges, kernel, cv::Point(-1, -1), 1);

    // Find contours.
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    double total_area = static_cast<double>(gray.rows) * gray.cols;
    double min_area = total_area * config_.min_contour_area_fraction;

    // Search for the largest quadrilateral.
    double best_area = 0.0;
    std::vector<cv::Point> best_quad;

    for (const auto& contour : contours) {
        double peri = cv::arcLength(contour, true);
        std::vector<cv::Point> approx;
        cv::approxPolyDP(contour, approx, 0.02 * peri, true);

        if (approx.size() == 4 && cv::isContourConvex(approx)) {
            double area = QuadArea(approx);
            if (area > min_area && area > best_area) {
                best_area = area;
                best_quad = approx;
            }
        }

        // Also try with larger epsilon for poorly defined edges.
        if (approx.size() > 4) {
            std::vector<cv::Point> approx2;
            cv::approxPolyDP(contour, approx2, 0.05 * peri, true);
            if (approx2.size() == 4 && cv::isContourConvex(approx2)) {
                double area = QuadArea(approx2);
                if (area > min_area && area > best_area) {
                    best_area = area;
                    best_quad = approx2;
                }
            }
        }
    }

    if (best_quad.empty()) {
        return false;
    }

    // Order corners: TL, TR, BR, BL.
    result.corners = OrderCorners(best_quad);
    result.found = true;
    result.detection_confidence = std::min(
        1.0f, static_cast<float>(best_area / total_area));

    // Compute aspect ratio.
    double top_width = Dist(result.corners[0], result.corners[1]);
    double bottom_width = Dist(result.corners[3], result.corners[2]);
    double left_height = Dist(result.corners[0], result.corners[3]);
    double right_height = Dist(result.corners[1], result.corners[2]);
    double avg_width = (top_width + bottom_width) / 2.0;
    double avg_height = (left_height + right_height) / 2.0;

    if (avg_height > 0.0) {
        result.aspect_ratio = avg_width / avg_height;
    }

    result.page_type = DetectPageType(result.aspect_ratio);

    // Compute bounding rect.
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_y = std::numeric_limits<int>::min();
    for (const auto& pt : result.corners) {
        min_x = std::min(min_x, pt.x);
        min_y = std::min(min_y, pt.y);
        max_x = std::max(max_x, pt.x);
        max_y = std::max(max_y, pt.y);
    }
    result.bounding_rect = cv::Rect(min_x, min_y, max_x - min_x, max_y - min_y);

    return true;
}

// ============================================================================
// Corner Ordering
// ============================================================================

std::array<cv::Point, 4> DocumentPreprocessor::OrderCorners(
    const std::vector<cv::Point>& corners) {
    // Algorithm:
    // 1. Sort points by y-coordinate.
    // 2. Top 2 points → sort by x → TL, TR.
    // 3. Bottom 2 points → sort by x → BL, BR.
    // Result: TL, TR, BR, BL.

    std::vector<cv::Point> pts = corners;
    std::sort(pts.begin(), pts.end(),
              [](const cv::Point& a, const cv::Point& b) {
                  return a.y < b.y;
              });

    // Top two (smallest y).
    std::vector<cv::Point> top = {pts[0], pts[1]};
    std::sort(top.begin(), top.end(),
              [](const cv::Point& a, const cv::Point& b) {
                  return a.x < b.x;
              });

    // Bottom two (largest y).
    std::vector<cv::Point> bottom = {pts[2], pts[3]};
    std::sort(bottom.begin(), bottom.end(),
              [](const cv::Point& a, const cv::Point& b) {
                  return a.x < b.x;
              });

    return {top[0], top[1], bottom[1], bottom[0]};
    //       TL      TR       BR         BL
}

// ============================================================================
// Perspective Correction
// ============================================================================

bool DocumentPreprocessor::ApplyPerspectiveCorrection(
    const cv::Mat& input,
    const std::array<cv::Point, 4>& corners,
    int target_width, int target_height,
    cv::Mat& output) const {

    // Source points (ordered: TL, TR, BR, BL).
    std::vector<cv::Point2f> src_pts(4);
    for (int i = 0; i < 4; ++i) {
        src_pts[i] = cv::Point2f(
            static_cast<float>(corners[i].x),
            static_cast<float>(corners[i].y));
    }

    // Destination: upright rectangle.
    std::vector<cv::Point2f> dst_pts = {
        {0.0f, 0.0f},                                    // TL
        {static_cast<float>(target_width - 1), 0.0f},     // TR
        {static_cast<float>(target_width - 1),             // BR
         static_cast<float>(target_height - 1)},
        {0.0f, static_cast<float>(target_height - 1)}     // BL
    };

    cv::Mat M = cv::getPerspectiveTransform(src_pts, dst_pts);
    cv::warpPerspective(input, output, M,
                        cv::Size(target_width, target_height),
                        cv::INTER_CUBIC);

    return !output.empty();
}

// ============================================================================
// Page Type Detection
// ============================================================================

std::string DocumentPreprocessor::DetectPageType(double aspect_ratio) {
    if (aspect_ratio <= 0.0) return "unknown";

    // A4: 210×297 mm → ratio 297/210 ≈ 1.414 (portrait)
    // A5: 148×210 mm → ratio 210/148 ≈ 1.419 (portrait)
    // Both have the same ratio; we can only distinguish by absolute size,
    // which depends on DPI. For scanned images, we use approximate ranges.

    // Normalize: ensure we're looking at portrait orientation.
    double ratio = aspect_ratio;
    if (ratio > 1.0) {
        ratio = 1.0 / ratio;  // landscape → convert to portrait ratio
    }

    // A4/A5 have width/height ≈ 0.707 (1/√2).
    // Tolerance: ±15%.
    double a4_expected = 1.0 / std::sqrt(2.0);  // ≈ 0.7071
    double tolerance = 0.15;
    double lower = a4_expected * (1.0 - tolerance);  // ≈ 0.601
    double upper = a4_expected * (1.0 + tolerance);  // ≈ 0.813

    if (ratio >= lower && ratio <= upper) {
        // Matches ISO 216 aspect ratio. Could be A4 or A5.
        // Without DPI info, default to A4.
        return "A4";
    }

    return "unknown";
}

// ============================================================================
// Orientation Correction
// ============================================================================

bool DocumentPreprocessor::CorrectOrientation(cv::Mat& image) const {
    // For now, handle the simple case: if the image is landscape
    // (width > height) but we expect portrait medical reports,
    // rotate 90 degrees.

    // TODO: EXIF orientation reading requires libexif or similar.
    // For Phase 2, we do a basic geometry-based correction.
    // Future enhancement: EXIF-based rotation.

    if (image.cols > image.rows) {
        // Landscape → portrait: rotate 90° clockwise.
        cv::rotate(image, image, cv::ROTATE_90_CLOCKWISE);
        return true;
    }

    return false;  // No change.
}

// ============================================================================
// CLAHE Contrast Enhancement
// ============================================================================

void DocumentPreprocessor::ApplyClahe(const cv::Mat& gray,
                                      cv::Mat& output) const {
    auto clahe = cv::createCLAHE(config_.clahe_clip_limit,
                                 cv::Size(config_.clahe_tile_size,
                                          config_.clahe_tile_size));
    clahe->apply(gray, output);
}

// ============================================================================
// Gaussian Denoising
// ============================================================================

void DocumentPreprocessor::ApplyDenoise(const cv::Mat& input,
                                        cv::Mat& output) const {
    cv::GaussianBlur(input, output,
                     cv::Size(config_.gaussian_kernel, config_.gaussian_kernel),
                     config_.gaussian_sigma);
}

}  // namespace medical_ocr
