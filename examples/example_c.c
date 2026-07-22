/**
 * Medical OCR — C Language Integration Example
 *
 * Build (MSVC):
 *   cl example_c.c /I"..\include" /link MedicalOCR.lib
 *
 * Build (MinGW):
 *   gcc example_c.c -I"../include" -L"../bin" -lMedicalOCR -o example_c.exe
 */

#include <stdio.h>
#include <stdlib.h>

#include "medical_ocr/medical_ocr_c_api.h"

int main(int argc, char* argv[]) {
    const char* image_path = (argc > 1) ? argv[1] : "sample_report.jpg";
    const char* config_path = (argc > 2) ? argv[2] : "config/medical_ocr.json";

    printf("Medical OCR C Example\n");
    printf("=====================\n");
    printf("Library version: %s\n\n", OCR_GetVersion());

    // ---- Initialize ----
    printf("Initializing...\n");
    int rc = OCR_Init(NULL, config_path);
    if (rc != 0) {
        fprintf(stderr, "OCR_Init failed (code %d): %s\n", rc, OCR_GetLastError());
        return 1;
    }
    printf("OK\n\n");

    // ---- Recognize ----
    printf("Recognizing: %s\n", image_path);
    char* json = NULL;
    rc = OCR_RecognizeFile(image_path, &json);
    if (rc != 0) {
        fprintf(stderr, "OCR_RecognizeFile failed (code %d): %s\n",
                rc, OCR_GetLastError());
        OCR_Shutdown();
        return 1;
    }

    // ---- Output ----
    printf("Result:\n%s\n", json);

    // ---- Cleanup ----
    OCR_FreeResult(json);
    OCR_Shutdown();

    printf("Done.\n");
    return 0;
}
