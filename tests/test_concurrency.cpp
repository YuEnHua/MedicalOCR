/**
 * Multi-threading and stress tests for the Medical OCR DLL.
 *
 * Tests:
 * - Concurrent RecognizeMemory calls from multiple threads
 * - Repeated init/shutdown cycles
 * - OCR_FreeResult called on all threads
 * - Thread-local GetLastError isolation
 * - No data races or crashes under load
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "medical_ocr/medical_ocr_c_api.h"
#include "medical_ocr/error_codes.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

// Helper: create a valid small image for testing.
std::vector<unsigned char> CreateTestImage() {
    cv::Mat img(1000, 1000, CV_8UC3);
    cv::randu(img, cv::Scalar(30, 30, 30), cv::Scalar(225, 225, 225));
    cv::line(img, {100, 100}, {900, 900}, {0, 0, 0}, 3);
    cv::line(img, {100, 900}, {900, 100}, {0, 0, 0}, 3);

    std::vector<unsigned char> buf;
    cv::imencode(".png", img, buf);
    return buf;
}

class ConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        OCR_Shutdown();
        test_image_ = CreateTestImage();
        int rc = OCR_Init(nullptr, "config/medical_ocr.json");
        ASSERT_EQ(rc, MEDOCR_OK) << OCR_GetLastError();
    }

    void TearDown() override {
        OCR_Shutdown();
    }

    std::vector<unsigned char> test_image_;
};

// ============================================================================
// Concurrent Recognition
// ============================================================================

TEST_F(ConcurrencyTest, TwoThreads_ConcurrentRecognize) {
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};

    auto worker = [&]() {
        for (int i = 0; i < 5; ++i) {
            char* json = nullptr;
            int rc = OCR_RecognizeMemory(
                test_image_.data(),
                static_cast<int>(test_image_.size()),
                &json);
            if (rc == MEDOCR_OK && json && json[0] == '{') {
                ++successes;
            } else {
                ++failures;
            }
            OCR_FreeResult(json);
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    EXPECT_EQ(failures, 0);
    EXPECT_EQ(successes, 10);  // 5 per thread.
}

TEST_F(ConcurrencyTest, FourThreads_ConcurrentRecognize) {
    const int kThreads = 4;
    const int kIterations = 3;
    std::atomic<int> successes{0};
    std::atomic<int> failures{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < kIterations; ++i) {
                char* json = nullptr;
                int rc = OCR_RecognizeMemory(
                    test_image_.data(),
                    static_cast<int>(test_image_.size()),
                    &json);
                if (rc == MEDOCR_OK && json && json[0] == '{') {
                    ++successes;
                } else {
                    ++failures;
                }
                OCR_FreeResult(json);
            }
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(failures, 0);
    EXPECT_EQ(successes, kThreads * kIterations);
}

// ============================================================================
// Thread-Local Error Isolation
// ============================================================================

TEST_F(ConcurrencyTest, GetLastError_ThreadIsolated) {
    std::string t1_error, t2_error;

    auto worker = [&](int id, std::string& out_error) {
        // Each thread calls a failing API first.
        char* json = nullptr;
        OCR_RecognizeFile("nonexistent_file_thread.jpg", &json);
        const char* err = OCR_GetLastError();
        if (err) out_error = err;
        OCR_FreeResult(json);
    };

    std::thread t1(worker, 1, std::ref(t1_error));
    std::thread t2(worker, 2, std::ref(t2_error));
    t1.join();
    t2.join();

    // Each thread should have gotten an error (file not found or similar).
    EXPECT_FALSE(t1_error.empty());
    EXPECT_FALSE(t2_error.empty());
}

// ============================================================================
// Init/Shutdown Stress
// ============================================================================

TEST_F(ConcurrencyTest, RepeatedInitShutdown) {
    // Shutdown first since SetUp already called Init.
    OCR_Shutdown();

    for (int i = 0; i < 20; ++i) {
        int rc = OCR_Init(nullptr, "config/medical_ocr.json");
        EXPECT_EQ(rc, MEDOCR_OK) << "Failed at iteration " << i;
        OCR_Shutdown();
    }
}

// ============================================================================
// Rapid OCR_FreeResult(NULL) from Many Threads
// ============================================================================

TEST_F(ConcurrencyTest, FreeResultNull_MultiThreaded) {
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < 100; ++j) {
                OCR_FreeResult(nullptr);
            }
        });
    }
    for (auto& t : threads) t.join();
    SUCCEED();  // No crash = pass.
}

// ============================================================================
// Memory Allocation Stress (large JSON strings on multiple threads)
// ============================================================================

TEST_F(ConcurrencyTest, MemoryStress_AllocAndFree) {
    std::vector<std::thread> threads;
    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 10; ++i) {
                char* json = nullptr;
                int rc = OCR_RecognizeMemory(
                    test_image_.data(),
                    static_cast<int>(test_image_.size()),
                    &json);
                EXPECT_EQ(rc, MEDOCR_OK) << "Thread " << t << " iter " << i;
                if (json) {
                    OCR_FreeResult(json);
                }
            }
        });
    }
    for (auto& t : threads) t.join();
    SUCCEED();
}

}  // namespace
