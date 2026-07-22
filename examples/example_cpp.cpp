/**
 * Medical OCR — C++ Integration Example
 *
 * Demonstrates RAII wrapper around the C API with exception safety.
 *
 * Build (MSVC):
 *   cl /std:c++17 example_cpp.cpp /I"..\include" /link MedicalOCR.lib
 *
 * Build (MinGW):
 *   g++ -std=c++17 example_cpp.cpp -I"../include" -L"../bin" -lMedicalOCR -o example_cpp.exe
 */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "medical_ocr/medical_ocr_c_api.h"

// RAII wrapper for the OCR library.
class OcrGuard {
public:
    explicit OcrGuard(const std::string& config_path = "") {
        int rc = OCR_Init(nullptr, config_path.empty() ? nullptr : config_path.c_str());
        if (rc != 0) {
            throw std::runtime_error(
                std::string("OCR_Init failed: ") + OCR_GetLastError());
        }
    }

    ~OcrGuard() { OCR_Shutdown(); }

    OcrGuard(const OcrGuard&) = delete;
    OcrGuard& operator=(const OcrGuard&) = delete;
};

// RAII wrapper for result strings.
class OcrResult {
public:
    explicit OcrResult(char* json) : data_(json) {}
    ~OcrResult() { OCR_FreeResult(data_); }

    OcrResult(const OcrResult&) = delete;
    OcrResult& operator=(const OcrResult&) = delete;

    const char* c_str() const { return data_ ? data_ : "{}"; }
    bool valid() const { return data_ != nullptr; }

private:
    char* data_;
};

int main(int argc, char* argv[]) {
    std::string image_path = (argc > 1) ? argv[1] : "sample_report.jpg";
    std::string config_path = (argc > 2) ? argv[2] : "config/medical_ocr.json";

    try {
        std::cout << "Medical OCR C++ Example" << std::endl;
        std::cout << "=======================" << std::endl;
        std::cout << "Version: " << OCR_GetVersion() << std::endl;
        std::cout << std::endl;

        // RAII initialization.
        OcrGuard guard(config_path);
        std::cout << "Initialized." << std::endl;

        // Recognize.
        std::cout << "Recognizing: " << image_path << std::endl;
        char* raw_json = nullptr;
        int rc = OCR_RecognizeFile(image_path.c_str(), &raw_json);
        if (rc != 0) {
            throw std::runtime_error(
                std::string("OCR_RecognizeFile failed: ") + OCR_GetLastError());
        }

        OcrResult result(raw_json);
        std::cout << "Result:\n" << result.c_str() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Done." << std::endl;
    return 0;
}
