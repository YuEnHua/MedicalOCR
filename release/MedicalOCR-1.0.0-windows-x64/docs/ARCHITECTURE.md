# Architecture Overview

## System Design

```
┌─────────────────────────────────────────────────────────┐
│                     C ABI Boundary                       │
│  OCR_Init | OCR_RecognizeFile | OCR_RecognizeMemory     │
│  OCR_FreeResult | OCR_GetVersion | OCR_Shutdown         │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│                  MedicalOcrService                       │
│  (Singleton, thread-safe init/shutdown)                  │
└────────┬───────┬────────┬────────┬──────────┬──────────┘
         │       │        │        │          │
    ┌────▼──┐ ┌──▼───┐ ┌──▼───┐ ┌──▼────┐ ┌──▼──────┐
    │Config │ │Image │ │OCR   │ │Doc    │ │Result   │
    │Loader │ │Proc  │ │Engine│ │Match  │ │Builder  │
    └───────┘ └──────┘ └──┬───┘ └───────┘ └─────────┘
                          │
              ┌───────────┼───────────┐
         ┌────▼────┐ ┌────▼────┐ ┌───▼──────┐
         │Mock     │ │Paddle   │ │ONNX      │
         │Engine   │ │Engine   │ │Engine    │
         │(Phase1) │ │(Phase4) │ │(Future)  │
         └─────────┘ └─────────┘ └──────────┘
```

## Processing Pipeline

```
Input Image
    │
    ▼
ImageQualityChecker      (Phase 2)
    │  - Empty check, blur, exposure
    ▼
DocumentPreprocessor     (Phase 2)
    │  - Grayscale, denoise, contrast
    │  - Perspective correction
    │  - Orientation handling
    ▼
IOcrEngine.Recognize()   (Phase 1: Mock, Phase 4: Paddle)
    │  - Returns OcrResult (boxes, text, confidence)
    ▼
DocumentClassifier       (Phase 3)
    │  - Rule-based classification
    ▼
TemplateMatcher          (Phase 3)
    │  - JSON template matching
    ▼
FieldExtractor           (Phase 3)
    │  - Anchor-based extraction
    │  - Paragraph extraction
    ▼
FieldValidator           (Phase 3)
    │  - ID number checksum
    │  - Date normalization
    │  - Gender standardization
    ▼
ResultBuilder            (Phase 1: basic, Phase 3: full)
    │  - UTF-8 JSON output
    ▼
DLL C API Output
```

## Key Design Decisions

### 1. IOcrEngine Abstraction

All OCR backends implement the same C++ interface. This allows:
- Testing the full pipeline without real OCR (MockOcrEngine)
- Swapping OCR backends without changing extraction logic
- Future support for ONNX, RapidOCR, etc.

### 2. Normalized Coordinates

All template coordinates use 0–1000 range. This decouples templates from
image resolution and scanning DPI.

### 3. C ABI Boundary

The DLL exposes only C functions. This prevents:
- ABI incompatibility between C++ compilers
- STL version conflicts
- Exception propagation across DLL boundaries

### 4. Privacy by Design

- Patient data never stored in global variables
- ID masking by default
- Debug output requires explicit opt-in
- No network capability

### 5. Phase-Gated Development

The system is built in phases with each phase producing a compilable,
testable artifact. The MockOcrEngine ensures the pipeline is functional
from Phase 1 without requiring heavy dependencies.

## Error Handling Strategy

1. All C API functions return error codes (int)
2. Internal C++ exceptions are caught at the DLL boundary
3. `OCR_GetLastError()` provides human-readable context
4. Thread-local error storage prevents races
5. JSON output always contains `success` and `error_code` fields

## Memory Management

- DLL-allocated strings: freed via `OCR_FreeResult`
- Internal: RAII with `std::unique_ptr` and `std::string`
- No raw `new`/`delete` across DLL boundary
- Patient data lives in stack/local scope only

## Thread Safety

| Operation | Thread Safety |
|-----------|---------------|
| OCR_Init | Mutex-serialized |
| OCR_Shutdown | Mutex-serialized |
| OCR_RecognizeFile | Reentrant (read-only shared state) |
| OCR_RecognizeMemory | Reentrant |
| OCR_GetLastError | Thread-local |
| OCR_GetVersion | Immutable, always safe |
| OCR_FreeResult | Always safe |
