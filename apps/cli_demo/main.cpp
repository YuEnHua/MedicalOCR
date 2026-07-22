/**
 * Medical OCR CLI Demo
 *
 * A command-line tool that demonstrates the Medical OCR DLL functionality.
 *
 * Usage:
 *   medical_ocr_cli.exe <image_path> [config_path]
 *
 * Example:
 *   medical_ocr_cli.exe sample_report.jpg config/medical_ocr.json
 *   medical_ocr_cli.exe --version
 *
 * Build:
 *   This executable links against the MedicalOCR DLL.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "medical_ocr/medical_ocr_c_api.h"
#include "medical_ocr/error_codes.h"

static void PrintUsage() {
    std::printf("Medical OCR CLI Demo v1.0.0\n");
    std::printf("Usage:\n");
    std::printf("  medical_ocr_cli.exe <image_path> [config_path]\n");
    std::printf("  medical_ocr_cli.exe --version\n");
    std::printf("  medical_ocr_cli.exe --help\n");
    std::printf("\n");
    std::printf("Arguments:\n");
    std::printf("  image_path   Path to the medical report image (JPEG, PNG, BMP, TIFF)\n");
    std::printf("  config_path  Path to medical_ocr.json (optional, uses defaults if omitted)\n");
    std::printf("\n");
}

static void PrintVersion() {
    std::printf("Medical OCR CLI Demo v1.0.0\n");
    std::printf("Library version: %s\n", OCR_GetVersion());
}

static std::string ReadFileToString(const std::string& path) {
    // On Windows, use wide-char API for Unicode paths.
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);

    FILE* f = _wfopen(wpath.c_str(), L"rb");
#else
    FILE* f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string buf(size, '\0');
    std::fread(&buf[0], 1, size, f);
    std::fclose(f);
    return buf;
}

int main(int argc, char* argv[]) {
    // Parse arguments
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string arg1 = argv[1];
    if (arg1 == "--help" || arg1 == "-h") {
        PrintUsage();
        return 0;
    }
    if (arg1 == "--version" || arg1 == "-v") {
        PrintVersion();
        return 0;
    }

    std::string image_path = arg1;
    std::string config_path = (argc >= 3) ? argv[2] : "";

    // Resolve config path relative to executable location if needed.
    // For simplicity, try a few common locations.
    std::printf("Medical OCR CLI Demo\n");
    std::printf("====================\n");
    std::printf("Image:  %s\n", image_path.c_str());
    std::printf("Config: %s\n", config_path.empty() ? "(defaults)" : config_path.c_str());
    std::printf("\n");

    // ---- Initialize ----
    std::printf("[1/3] Initializing OCR library...\n");
    int rc = OCR_Init(nullptr, config_path.empty() ? nullptr : config_path.c_str());
    if (rc != MEDOCR_OK) {
        std::fprintf(stderr, "ERROR: OCR_Init failed (code %d): %s\n",
                     rc, OCR_GetLastError());
        return rc;
    }
    std::printf("      Library initialized successfully.\n");

    // ---- Recognize ----
    std::printf("[2/3] Recognizing medical report...\n");
    char* json_output = nullptr;
    rc = OCR_RecognizeFile(image_path.c_str(), &json_output);
    if (rc != MEDOCR_OK) {
        std::fprintf(stderr, "ERROR: OCR_RecognizeFile failed (code %d): %s\n",
                     rc, OCR_GetLastError());
        OCR_Shutdown();
        return rc;
    }

    // ---- Output ----
    std::printf("[3/3] Recognition complete.\n");
    std::printf("\n");
    if (json_output) {
        std::printf("%s\n", json_output);
        OCR_FreeResult(json_output);
        json_output = nullptr;
    }

    // ---- Shutdown ----
    OCR_Shutdown();
    std::printf("\nDone.\n");
    return 0;
}
