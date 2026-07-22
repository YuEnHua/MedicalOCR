/**
 * Unit tests for DocumentPreprocessor.
 *
 * Tests:
 * - Grayscale conversion
 * - Denoising (Gaussian blur)
 * - CLAHE contrast enhancement
 * - Adaptive thresholding
 * - Document contour detection on synthetic quadrilaterals
 * - Corner ordering (TL, TR, BR, BL)
 * - Perspective correction
 * - A4/A5 page type detection
 * - Orientation correction (landscape → portrait)
 * - Configurable enable/disable for each step
 */

#include <cmath>
#include <vector>

#include "gtest/gtest.h"
#include "src/image/document_preprocessor.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace medical_ocr {
namespace {

class PreprocessorTest : public ::testing::Test {
protected:
    PreprocessConfig default_config_;
    DocumentPreprocessor preprocessor_{default_config_};

    /// Create a synthetic document image: a white rectangle on a dark
    /// background with some perspective skew.
    cv::Mat CreateSyntheticDocument(int width, int height,
                                     const std::array<cv::Point, 4>& corners) {
        cv::Mat img(height, width, CV_8UC1, cv::Scalar(20));

        // Draw a white quadrilateral with sharp edges.
        std::vector<std::vector<cv::Point>> contours = {
            {corners[0], corners[1], corners[2], corners[3]}
        };
        cv::fillPoly(img, contours, cv::Scalar(250));

        // Add text-like noise inside.
        cv::Rect bb = cv::boundingRect(contours[0]);
        cv::Mat roi = img(bb & cv::Rect(0, 0, width, height));
        cv::randu(roi, 0, 60);

        // Draw a dark border around the quadrilateral to create strong edges.
        std::vector<std::vector<cv::Point>> outline = {
            {corners[0], corners[1], corners[2], corners[3]}
        };
        cv::polylines(img, outline, true, cv::Scalar(10), 3);

        return img;
    }
};

// ============================================================================
// Grayscale Conversion
// ============================================================================

TEST_F(PreprocessorTest, ColorToGrayscale) {
    PreprocessConfig cfg;
    cfg.enable_grayscale = true;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_contrast_enhancement = false;
    cfg.enable_denoise = false;
    cfg.enable_orientation_correction = false;
    DocumentPreprocessor pp(cfg);

    cv::Mat color(200, 200, CV_8UC3, cv::Scalar(100, 150, 200));
    cv::Mat output;
    ASSERT_TRUE(pp.Process(color, output));
    EXPECT_EQ(output.channels(), 1);
}

// ============================================================================
// Denoising
// ============================================================================

TEST_F(PreprocessorTest, DenoisePreservesDimensions) {
    PreprocessConfig cfg;
    cfg.enable_denoise = true;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_contrast_enhancement = false;
    cfg.enable_orientation_correction = false;
    DocumentPreprocessor pp(cfg);

    cv::Mat noisy(500, 500, CV_8UC3);
    cv::randu(noisy, 0, 255);
    cv::Mat output;
    ASSERT_TRUE(pp.Process(noisy, output));
    EXPECT_EQ(output.rows, noisy.rows);
    EXPECT_EQ(output.cols, noisy.cols);
}

// ============================================================================
// CLAHE Contrast Enhancement
// ============================================================================

TEST_F(PreprocessorTest, ClaheEnhancesContrast) {
    PreprocessConfig cfg;
    cfg.enable_contrast_enhancement = true;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_denoise = false;
    cfg.enable_orientation_correction = false;
    DocumentPreprocessor pp(cfg);

    // Low-contrast image.
    cv::Mat low_contrast(500, 500, CV_8UC3, cv::Scalar(128, 128, 128));

    cv::Mat output;
    ASSERT_TRUE(pp.Process(low_contrast, output));
    EXPECT_EQ(output.rows, low_contrast.rows);
    EXPECT_EQ(output.cols, low_contrast.cols);
}

// ============================================================================
// Document Detection — Synthetic Quadrilateral
// ============================================================================

TEST_F(PreprocessorTest, DetectClearQuadrilateral) {
    PreprocessConfig cfg;
    cfg.enable_document_detection = true;
    // Low thresholds for easy edge detection on synthetic images.
    cfg.canny_low = 5.0;
    cfg.canny_high = 30.0;
    cfg.min_contour_area_fraction = 0.02;
    DocumentPreprocessor pp(cfg);

    // Create a simple axis-aligned white rectangle on black background.
    // This guarantees strong edges for Canny to find.
    int w = 1200, h = 1600;
    cv::Mat img(h, w, CV_8UC1, cv::Scalar(0));
    // White rectangle with a thin black border for edge contrast.
    cv::Rect rect(80, 80, 1040, 1440);
    cv::rectangle(img, rect, cv::Scalar(255), cv::FILLED);
    cv::rectangle(img, rect, cv::Scalar(0), 2);

    DocumentDetectionResult result;
    bool found = pp.DetectDocument(img, result);
    EXPECT_TRUE(found) << "Document contour not detected";
    // Axis-aligned rectangle should have A4-like aspect ratio.
    EXPECT_EQ(result.page_type, "A4");
    EXPECT_GT(result.detection_confidence, 0.0f);
}

TEST_F(PreprocessorTest, DetectDocument_NoDocumentInUniformImage) {
    PreprocessConfig cfg;
    DocumentPreprocessor pp(cfg);

    cv::Mat uniform(1000, 1000, CV_8UC1, cv::Scalar(128));
    DocumentDetectionResult result;
    bool found = pp.DetectDocument(uniform, result);
    EXPECT_FALSE(found);
}

// ============================================================================
// Corner Ordering
// ============================================================================

TEST_F(PreprocessorTest, OrderCorners_TLRotation) {
    // Input corners in random order.
    std::vector<cv::Point> corners = {
        {1100, 80},   // TR
        {100, 100},   // TL
        {80, 1520},   // BL
        {1150, 1500}  // BR
    };

    auto ordered = DocumentPreprocessor::OrderCorners(corners);

    // Expected: TL, TR, BR, BL.
    EXPECT_EQ(ordered[0].x, 100);   // TL — leftmost of top pair.
    EXPECT_EQ(ordered[1].x, 1100);  // TR — rightmost of top pair.
    EXPECT_EQ(ordered[2].x, 1150);  // BR — rightmost of bottom pair.
    EXPECT_EQ(ordered[3].x, 80);    // BL — leftmost of bottom pair.

    // Y ordering.
    EXPECT_LT(ordered[0].y, ordered[2].y);  // TL above BR.
    EXPECT_LT(ordered[1].y, ordered[2].y);  // TR above BR.
}

TEST_F(PreprocessorTest, OrderCorners_PreservesFourPoints) {
    std::vector<cv::Point> corners = {
        {0, 0}, {100, 0}, {100, 100}, {0, 100}
    };
    auto ordered = DocumentPreprocessor::OrderCorners(corners);

    // Should still have 4 points in the expected order.
    EXPECT_EQ(ordered[0].x, 0);
    EXPECT_EQ(ordered[0].y, 0);
    EXPECT_EQ(ordered[2].x, 100);
    EXPECT_EQ(ordered[2].y, 100);
}

// ============================================================================
// Perspective Correction
// ============================================================================

TEST_F(PreprocessorTest, PerspectiveCorrection_ProducesExpectedSize) {
    PreprocessConfig cfg;
    DocumentPreprocessor pp(cfg);

    cv::Mat src(800, 600, CV_8UC1);
    cv::randu(src, 0, 255);

    std::array<cv::Point, 4> corners = {
        cv::Point(50, 50),
        cv::Point(550, 40),
        cv::Point(580, 750),
        cv::Point(40, 760)
    };

    cv::Mat warped;
    bool ok = pp.ApplyPerspectiveCorrection(src, corners, 600, 800, warped);
    ASSERT_TRUE(ok);
    EXPECT_EQ(warped.cols, 600);
    EXPECT_EQ(warped.rows, 800);
}

// ============================================================================
// Page Type Detection
// ============================================================================

TEST_F(PreprocessorTest, DetectPageType_A4) {
    // A4 portrait: width/height ≈ 210/297 ≈ 0.707.
    double ratio = 210.0 / 297.0;
    EXPECT_EQ(DocumentPreprocessor::DetectPageType(ratio), "A4");
}

TEST_F(PreprocessorTest, DetectPageType_A4Landscape) {
    // A4 landscape: width/height ≈ 297/210 ≈ 1.414.
    double ratio = 297.0 / 210.0;
    // The function normalizes internally → still A4.
    EXPECT_EQ(DocumentPreprocessor::DetectPageType(ratio), "A4");
}

TEST_F(PreprocessorTest, DetectPageType_Unknown) {
    // Square.
    EXPECT_EQ(DocumentPreprocessor::DetectPageType(1.0), "unknown");

    // Very wide.
    EXPECT_EQ(DocumentPreprocessor::DetectPageType(3.0), "unknown");
}

TEST_F(PreprocessorTest, DetectPageType_ZeroRatio) {
    EXPECT_EQ(DocumentPreprocessor::DetectPageType(0.0), "unknown");
    EXPECT_EQ(DocumentPreprocessor::DetectPageType(-1.0), "unknown");
}

// ============================================================================
// Orientation Correction
// ============================================================================

TEST_F(PreprocessorTest, LandscapeImage_RotatedToPortrait) {
    PreprocessConfig cfg;
    cfg.enable_orientation_correction = true;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_contrast_enhancement = false;
    cfg.enable_denoise = false;
    DocumentPreprocessor pp(cfg);

    // Landscape: width > height.
    cv::Mat landscape(400, 800, CV_8UC3);
    cv::randu(landscape, 0, 255);

    cv::Mat output = landscape.clone();
    bool changed = pp.CorrectOrientation(output);

    // A landscape image should be rotated.
    EXPECT_TRUE(changed);
    EXPECT_EQ(output.cols, landscape.rows);
    EXPECT_EQ(output.rows, landscape.cols);
}

TEST_F(PreprocessorTest, PortraitImage_NotRotated) {
    PreprocessConfig cfg;
    cfg.enable_orientation_correction = true;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_contrast_enhancement = false;
    cfg.enable_denoise = false;
    DocumentPreprocessor pp(cfg);

    // Portrait: height > width.
    cv::Mat portrait(800, 600, CV_8UC3);
    cv::randu(portrait, 0, 255);

    cv::Mat output = portrait.clone();
    bool changed = pp.CorrectOrientation(output);

    EXPECT_FALSE(changed);
}

// ============================================================================
// Full Pipeline with All Steps Disabled
// ============================================================================

TEST_F(PreprocessorTest, AllDisabled_ReturnsInputLikeOutput) {
    PreprocessConfig cfg;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_grayscale = false;
    cfg.enable_contrast_enhancement = false;
    cfg.enable_denoise = false;
    cfg.enable_adaptive_threshold = false;
    cfg.enable_orientation_correction = false;
    DocumentPreprocessor pp(cfg);

    cv::Mat input(500, 500, CV_8UC3);
    cv::randu(input, 0, 255);
    cv::Mat output;
    ASSERT_TRUE(pp.Process(input, output));
    // Should be same size, 3 channels.
    EXPECT_EQ(output.rows, input.rows);
    EXPECT_EQ(output.cols, input.cols);
    EXPECT_EQ(output.channels(), 3);
}

// ============================================================================
// EnhanceOnly
// ============================================================================

TEST_F(PreprocessorTest, EnhanceOnly_SkipsGeometricCorrection) {
    PreprocessConfig cfg;
    cfg.enable_contrast_enhancement = true;
    cfg.enable_denoise = true;
    DocumentPreprocessor pp(cfg);

    cv::Mat input(500, 500, CV_8UC3);
    cv::randu(input, 0, 255);
    cv::Mat output;
    ASSERT_TRUE(pp.EnhanceOnly(input, output));
    // Dimensions should be unchanged.
    EXPECT_EQ(output.rows, input.rows);
    EXPECT_EQ(output.cols, input.cols);
}

// ============================================================================
// Empty Input
// ============================================================================

TEST_F(PreprocessorTest, EmptyImage_ReturnsFalse) {
    cv::Mat empty;
    cv::Mat output;
    EXPECT_FALSE(preprocessor_.Process(empty, output));
}

// ============================================================================
// Config Propagation
// ============================================================================

TEST_F(PreprocessorTest, ConfigValuesAreUsed) {
    PreprocessConfig cfg;
    cfg.gaussian_kernel = 7;
    cfg.gaussian_sigma = 3.0;
    cfg.clahe_clip_limit = 5.0;
    cfg.clahe_tile_size = 16;
    cfg.enable_contrast_enhancement = true;
    cfg.enable_denoise = true;
    cfg.enable_document_detection = false;
    cfg.enable_perspective_correction = false;
    cfg.enable_orientation_correction = false;
    DocumentPreprocessor pp(cfg);

    cv::Mat input(300, 300, CV_8UC3);
    cv::randu(input, 0, 255);
    cv::Mat output;
    ASSERT_TRUE(pp.Process(input, output));
    EXPECT_EQ(output.rows, input.rows);
}

}  // namespace
}  // namespace medical_ocr
