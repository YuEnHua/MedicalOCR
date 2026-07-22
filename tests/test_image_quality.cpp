/**
 * Unit tests for ImageQualityChecker.
 *
 * Tests:
 * - Empty image detection (all black, all white)
 * - Resolution check (too small)
 * - Blur detection via Laplacian variance
 * - Exposure check (over/under)
 * - Normal good image
 * - Configurable thresholds
 */

#include <cmath>

#include "gtest/gtest.h"
#include "src/image/image_quality_checker.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace medical_ocr {
namespace {

class ImageQualityTest : public ::testing::Test {
protected:
    ImageQualityConfig default_config_;
    ImageQualityChecker checker_{default_config_};
};

// ============================================================================
// Empty Images
// ============================================================================

TEST_F(ImageQualityTest, EmptyBlackImage_Fails) {
    cv::Mat black = cv::Mat::zeros(1000, 1000, CV_8UC1);
    QualityInfo info;
    bool ok = checker_.Check(black, info);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(info.document_detected);
}

TEST_F(ImageQualityTest, EmptyWhiteImage_Fails) {
    cv::Mat white(1000, 1000, CV_8UC1, cv::Scalar(255));
    QualityInfo info;
    bool ok = checker_.Check(white, info);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(info.document_detected);
}

TEST_F(ImageQualityTest, EmptyThreeChannelImage_Fails) {
    cv::Mat black3 = cv::Mat::zeros(1000, 1000, CV_8UC3);
    QualityInfo info;
    bool ok = checker_.Check(black3, info);
    EXPECT_FALSE(ok);
}

// ============================================================================
// Resolution Check
// ============================================================================

TEST_F(ImageQualityTest, TooSmallImage_Fails) {
    cv::Mat tiny(100, 100, CV_8UC1, cv::Scalar(128));
    QualityInfo info;
    bool ok = checker_.Check(tiny, info);
    EXPECT_FALSE(ok);
}

TEST_F(ImageQualityTest, ExactlyMinimumSize_Passes) {
    cv::Mat ok_img(default_config_.minimum_height,
                   default_config_.minimum_width,
                   CV_8UC1, cv::Scalar(128));
    // Add some edge-like variation so the blur check passes.
    cv::randu(ok_img, 0, 255);

    QualityInfo info;
    bool ok = checker_.Check(ok_img, info);
    // May still fail on blur, but should NOT fail on resolution.
    EXPECT_TRUE(info.document_detected);
}

// ============================================================================
// Blur Detection
// ============================================================================

TEST_F(ImageQualityTest, SharpSyntheticImage_HasHighVariance) {
    // Create a checkerboard pattern — very sharp.
    cv::Mat sharp(1000, 1000, CV_8UC1);
    for (int y = 0; y < sharp.rows; ++y) {
        for (int x = 0; x < sharp.cols; ++x) {
            sharp.at<uint8_t>(y, x) = ((x / 10 + y / 10) % 2) ? 255 : 0;
        }
    }

    double var = checker_.ComputeLaplacianVariance(sharp);
    EXPECT_GT(var, default_config_.blur_threshold);
}

TEST_F(ImageQualityTest, BlurredImage_HasLowVariance) {
    // Create a sharp pattern, then blur heavily.
    cv::Mat sharp(1000, 1000, CV_8UC1);
    for (int y = 0; y < sharp.rows; ++y) {
        for (int x = 0; x < sharp.cols; ++x) {
            sharp.at<uint8_t>(y, x) = ((x / 10 + y / 10) % 2) ? 255 : 0;
        }
    }

    cv::Mat blurred;
    cv::GaussianBlur(sharp, blurred, cv::Size(31, 31), 10.0);

    double var = checker_.ComputeLaplacianVariance(blurred);
    EXPECT_LT(var, default_config_.blur_threshold);
}

TEST_F(ImageQualityTest, BlurryImage_ReportsBlurry) {
    cv::Mat sharp(1000, 1000, CV_8UC3);
    cv::randu(sharp, 0, 255);
    cv::Mat blurred;
    cv::GaussianBlur(sharp, blurred, cv::Size(31, 31), 10.0);

    QualityInfo info;
    checker_.Check(blurred, info);
    EXPECT_TRUE(info.is_blurry);
    EXPECT_LT(info.blur_score, default_config_.blur_threshold);
}

TEST_F(ImageQualityTest, SharpImage_ReportsNotBlurry) {
    cv::Mat sharp(1000, 1000, CV_8UC3);
    cv::randu(sharp, 0, 255);
    // Add strong edges.
    cv::line(sharp, {100, 100}, {900, 900}, {255, 255, 255}, 3);
    cv::line(sharp, {100, 900}, {900, 100}, {255, 255, 255}, 3);

    QualityInfo info;
    checker_.Check(sharp, info);
    EXPECT_FALSE(info.is_blurry);
}

// ============================================================================
// Exposure Check
// ============================================================================

TEST_F(ImageQualityTest, OverexposedImage_Detected) {
    // Mostly white image.
    cv::Mat overexposed(1000, 1000, CV_8UC1, cv::Scalar(250));
    cv::randu(overexposed, 240, 256);

    QualityInfo info;
    checker_.CheckExposure(overexposed, info);
    EXPECT_TRUE(info.is_overexposed);
}

TEST_F(ImageQualityTest, NormalExposure_NotOverexposed) {
    cv::Mat normal(1000, 1000, CV_8UC1);
    cv::randu(normal, 50, 200);

    QualityInfo info;
    checker_.CheckExposure(normal, info);
    EXPECT_FALSE(info.is_overexposed);
}

// ============================================================================
// Glare Detection
// ============================================================================

TEST_F(ImageQualityTest, ImageWithGlare_Detected) {
    cv::Mat img(1000, 1000, CV_8UC1, cv::Scalar(128));
    // Create a large bright region in the corner (simulating glare).
    cv::rectangle(img, {0, 0}, {300, 300}, cv::Scalar(255), cv::FILLED);

    EXPECT_TRUE(checker_.HasGlare(img));
}

TEST_F(ImageQualityTest, ImageWithoutGlare_NotDetected) {
    cv::Mat img(1000, 1000, CV_8UC1);
    cv::randu(img, 50, 200);

    EXPECT_FALSE(checker_.HasGlare(img));
}

// ============================================================================
// Configurable Thresholds
// ============================================================================

TEST_F(ImageQualityTest, CustomBlurThreshold) {
    ImageQualityConfig cfg;
    cfg.blur_threshold = 1.0;  // Very low — almost everything is "sharp".
    ImageQualityChecker checker(cfg);

    cv::Mat slightly_blurred(1000, 1000, CV_8UC3);
    cv::randu(slightly_blurred, 0, 255);
    cv::Mat blurred;
    cv::GaussianBlur(slightly_blurred, blurred, cv::Size(5, 5), 2.0);

    QualityInfo info;
    checker.Check(blurred, info);
    // With threshold=1.0, even a slightly blurred image should pass.
    EXPECT_FALSE(info.is_blurry);
}

TEST_F(ImageQualityTest, CustomMinResolution) {
    ImageQualityConfig cfg;
    cfg.minimum_width = 2000;
    cfg.minimum_height = 2000;
    ImageQualityChecker checker(cfg);

    cv::Mat img(1500, 1500, CV_8UC3);
    cv::randu(img, 0, 255);

    QualityInfo info;
    bool ok = checker.Check(img, info);
    EXPECT_FALSE(ok);
}

// ============================================================================
// Empty Mat
// ============================================================================

TEST_F(ImageQualityTest, EmptyMat_Fails) {
    cv::Mat empty;
    QualityInfo info;
    bool ok = checker_.Check(empty, info);
    EXPECT_FALSE(ok);
}

// ============================================================================
// Laplacian Variance is Positive
// ============================================================================

TEST_F(ImageQualityTest, LaplacianVariance_AlwaysNonNegative) {
    cv::Mat img(500, 500, CV_8UC1);
    cv::randu(img, 0, 255);

    double var = checker_.ComputeLaplacianVariance(img);
    EXPECT_GE(var, 0.0);
}

}  // namespace
}  // namespace medical_ocr
