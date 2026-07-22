/**
 * Unit tests for the Medical OCR DLL C API.
 *
 * Tests cover:
 * - Initialization lifecycle
 * - Double init protection
 * - Recognize with missing file
 * - Recognize with invalid arguments
 * - Mock OCR recognition
 * - Version and error info
 * - OCR_FreeResult safety
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "medical_ocr/medical_ocr_c_api.h"
#include "medical_ocr/error_codes.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

class DllApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state before each test.
        OCR_Shutdown();
    }

    void TearDown() override {
        OCR_Shutdown();
    }
};

// ============================================================================
// Init / Shutdown lifecycle
// ============================================================================

TEST_F(DllApiTest, InitWithNoArgs_UsesDefaults) {
    int rc = OCR_Init(nullptr, nullptr);
    EXPECT_EQ(rc, MEDOCR_OK) << "Error: " << OCR_GetLastError();
    EXPECT_STREQ(OCR_GetVersion(), "1.0.0");
}

TEST_F(DllApiTest, InitWithConfigFile) {
    int rc = OCR_Init(nullptr, "config/medical_ocr.json");
    EXPECT_EQ(rc, MEDOCR_OK) << "Error: " << OCR_GetLastError();
}

TEST_F(DllApiTest, DoubleInitReturnsError) {
    int rc1 = OCR_Init(nullptr, nullptr);
    ASSERT_EQ(rc1, MEDOCR_OK);

    int rc2 = OCR_Init(nullptr, nullptr);
    EXPECT_EQ(rc2, MEDOCR_ERR_ALREADY_INITIALIZED);
}

TEST_F(DllApiTest, ShutdownWithoutInitIsSafe) {
    // Should not crash.
    OCR_Shutdown();
    OCR_Shutdown();  // Double shutdown also safe.
    SUCCEED();
}

TEST_F(DllApiTest, ReinitAfterShutdown) {
    int rc = OCR_Init(nullptr, nullptr);
    ASSERT_EQ(rc, MEDOCR_OK) << OCR_GetLastError();

    OCR_Shutdown();

    rc = OCR_Init(nullptr, nullptr);
    EXPECT_EQ(rc, MEDOCR_OK) << OCR_GetLastError();
}

// ============================================================================
// RecognizeFile
// ============================================================================

TEST_F(DllApiTest, RecognizeFile_NotInitialized) {
    char* json = nullptr;
    int rc = OCR_RecognizeFile("nonexistent.jpg", &json);
    EXPECT_EQ(rc, MEDOCR_ERR_NOT_INITIALIZED);
    EXPECT_EQ(json, nullptr);
}

TEST_F(DllApiTest, RecognizeFile_NullPath) {
    ASSERT_EQ(OCR_Init(nullptr, nullptr), MEDOCR_OK);

    char* json = nullptr;
    int rc = OCR_RecognizeFile(nullptr, &json);
    EXPECT_EQ(rc, MEDOCR_ERR_INVALID_ARGUMENT);
}

TEST_F(DllApiTest, RecognizeFile_NullOutput) {
    ASSERT_EQ(OCR_Init(nullptr, nullptr), MEDOCR_OK);

    int rc = OCR_RecognizeFile("test.jpg", nullptr);
    EXPECT_EQ(rc, MEDOCR_ERR_INVALID_ARGUMENT);
}

TEST_F(DllApiTest, RecognizeFile_FileNotFound) {
    ASSERT_EQ(OCR_Init(nullptr, nullptr), MEDOCR_OK);

    char* json = nullptr;
    int rc = OCR_RecognizeFile("C:\\nonexistent\\path\\file.jpg", &json);
    EXPECT_EQ(rc, MEDOCR_ERR_FILE_NOT_FOUND);
    EXPECT_EQ(json, nullptr);
}

// ============================================================================
// RecognizeMemory
// ============================================================================

TEST_F(DllApiTest, RecognizeMemory_NotInitialized) {
    unsigned char dummy[10] = {};
    char* json = nullptr;
    int rc = OCR_RecognizeMemory(dummy, 10, &json);
    EXPECT_EQ(rc, MEDOCR_ERR_NOT_INITIALIZED);
}

TEST_F(DllApiTest, RecognizeMemory_NullData) {
    ASSERT_EQ(OCR_Init(nullptr, nullptr), MEDOCR_OK);

    char* json = nullptr;
    int rc = OCR_RecognizeMemory(nullptr, 100, &json);
    EXPECT_EQ(rc, MEDOCR_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(json, nullptr);
}

TEST_F(DllApiTest, RecognizeMemory_ZeroSize) {
    ASSERT_EQ(OCR_Init(nullptr, nullptr), MEDOCR_OK);

    unsigned char dummy[10] = {};
    char* json = nullptr;
    int rc = OCR_RecognizeMemory(dummy, 0, &json);
    EXPECT_EQ(rc, MEDOCR_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(json, nullptr);
}

TEST_F(DllApiTest, RecognizeMemory_NegativeSize) {
    ASSERT_EQ(OCR_Init(nullptr, nullptr), MEDOCR_OK);

    unsigned char dummy[10] = {};
    char* json = nullptr;
    int rc = OCR_RecognizeMemory(dummy, -1, &json);
    EXPECT_EQ(rc, MEDOCR_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(json, nullptr);
}

// ============================================================================
// OCR_FreeResult
// ============================================================================

TEST_F(DllApiTest, FreeResult_NullIsSafe) {
    OCR_FreeResult(nullptr);
    SUCCEED();
}

// ============================================================================
// Version and Error Info
// ============================================================================

TEST_F(DllApiTest, GetVersion_AlwaysAvailable) {
    const char* ver = OCR_GetVersion();
    EXPECT_NE(ver, nullptr);
    EXPECT_GT(std::strlen(ver), 0u);
}

TEST_F(DllApiTest, GetLastError_BeforeInit) {
    // Should return something sensible even if no error occurred.
    const char* err = OCR_GetLastError();
    EXPECT_NE(err, nullptr);
}

// ============================================================================
// Config file error handling
// ============================================================================

TEST_F(DllApiTest, InitWithMissingConfigFile) {
    int rc = OCR_Init(nullptr, "nonexistent_config.json");
    EXPECT_EQ(rc, MEDOCR_ERR_CONFIG_NOT_FOUND);
}

// ============================================================================
// Memory allocation edge cases
// ============================================================================

TEST_F(DllApiTest, RecognizeMemory_WithMockEngine_Succeeds) {
    ASSERT_EQ(OCR_Init(nullptr, "config/medical_ocr.json"), MEDOCR_OK);

    // Create a valid image using OpenCV (large enough to pass quality checks)
    // and encode to PNG.
    cv::Mat img(1000, 1000, CV_8UC3);
    cv::randu(img, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    // Add some edges so it passes the blur check.
    cv::line(img, {100, 100}, {900, 900}, {0, 0, 0}, 3);
    cv::line(img, {100, 900}, {900, 100}, {0, 0, 0}, 3);
    std::vector<unsigned char> buf;
    ASSERT_TRUE(cv::imencode(".png", img, buf));
    ASSERT_GT(buf.size(), 0u);

    char* json = nullptr;
    int rc = OCR_RecognizeMemory(buf.data(), static_cast<int>(buf.size()), &json);
    EXPECT_EQ(rc, MEDOCR_OK) << "Error: " << OCR_GetLastError();
    ASSERT_NE(json, nullptr);
    EXPECT_GT(std::strlen(json), 0u);

    // Verify it's valid JSON by checking for opening brace.
    EXPECT_EQ(json[0], '{');

    OCR_FreeResult(json);
}
