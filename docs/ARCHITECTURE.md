# Architecture Overview

## System Design (Updated — All Phases)

```
┌──────────────────────────────────────────────────────────────────┐
│                        C ABI Boundary                             │
│  OCR_Init | OCR_RecognizeFile | OCR_RecognizeMemory              │
│  OCR_FreeResult | OCR_GetVersion | OCR_GetLastError | OCR_Shutdown│
│  (src/api/medical_ocr_c_api.cpp — exception-safe wrappers)       │
└───────────────────────────┬──────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────────────┐
│                    MedicalOcrService (Singleton)                   │
│  src/core/medical_ocr_service.{h,cpp}                             │
│  • Mutex-serialized Init/Shutdown                                 │
│  • Reentrant Recognize (reads config, delegates to engine)        │
│  • Pipeline orchestration (8 stages)                              │
│  • Template loading + engine factory                              │
│  • JSON result builder + ID masking                               │
└──┬────────┬──────────┬──────────┬───────────┬───────────────────┘
   │        │          │          │           │
   ▼        ▼          ▼          ▼           ▼
┌──────┐ ┌──────┐ ┌────────┐ ┌────────┐ ┌──────────┐
│Config│ │Image │ │OCR     │ │Doc     │ │Result    │
│JSON  │ │Proc  │ │Engine  │ │Match   │ │Builder   │
└──────┘ └──┬───┘ └───┬────┘ └───┬────┘ └──────────┘
            │         │          │
    ┌───────▼──┐  ┌───▼────────▼──────┐  ┌──────────────────┐
    │Quality   │  │                    │  │TemplateRepository│
    │Checker   │  │   IOcrEngine       │  │(Load JSON files) │
    │(blur,    │  │   ┌──────────────┐ │  │                  │
    │exposure, │  │   │MockOcrEngine │ │  │DocumentClassifier│
    │empty)    │  │   │(Phase 1)     │ │  │(keyword rules)   │
    └───────┬──┘  │   ├──────────────┤ │  │                  │
            │     │   │PaddleOcrEng. │ │  │TemplateMatcher   │
    ┌───────▼──┐  │   │(Phase 4)     │ │  │(score+select)    │
    │Preproc.  │  │   ├──────────────┤ │  └───────┬──────────┘
    │(grayscale│  │   │OnnxOcrEngine │ │          │
    │CLAHE,    │  │   │(Future)      │ │  ┌───────▼──────────┐
    │perspect.)│  │   └──────────────┘ │  │FieldExtractor    │
    └──────────┘  └────────────────────┘  │(anchor+paragraph)│
                                          ├──────────────────┤
                                          │FieldValidator    │
                                          │(ID/date/gender)  │
                                          │FieldNormalizer   │
                                          │CrossValidator    │
                                          └──────────────────┘
```

## Processing Pipeline (8 Stages)

```
Stage 1: ImageQualityChecker::Check()
    ├── cv::countNonZero    → empty/all-white detection
    ├── cols/rows check     → minimum resolution (800×800 default)
    ├── cv::Laplacian       → blur detection (variance < threshold)
    ├── cv::calcHist        → overexposure (> 15% pixels near 255)
    └── connectedComponents → glare detection (> 5% area contiguous white)
    Output: QualityInfo + pass/fail

Stage 2: DocumentPreprocessor::Process()
    ├── CorrectOrientation  → landscape → rotate 90°
    ├── cvtColor            → BGR → GRAY (optional)
    ├── GaussianBlur        → denoise (3×3, sigma=1.0)
    ├── createCLAHE         → contrast enhancement (clip=2.0, tile=8×8)
    ├── adaptiveThreshold   → binarization (optional, block=11, C=2)
    ├── Canny               → edge detection (low=50, high=150)
    ├── findContours        → contour extraction
    ├── approxPolyDP        → polygon approximation
    ├── isContourConvex     → quadrilateral validation
    ├── OrderCorners        → TL, TR, BR, BL (sort by y, then by x)
    ├── getPerspectiveTransform + warpPerspective → deskew
    └── DetectPageType      → A4/A5 from aspect ratio (≈0.707 ±15%)

Stage 3: IOcrEngine::Recognize()
    ├── MockOcrEngine       → loads sample JSON or generates built-in data
    └── PaddleOcrEngine     → detection (640×640) → crop → recognition (32×320) → CTC decode

Stage 4: Coordinate Normalization
    └── geometry::NormalizeCoordinates → pixel coords → 0–1000 range

Stage 5: DocumentClassifier + TemplateMatcher
    ├── ConcatenateText     → all OCR boxes → single string
    ├── Keyword matching    → hospital/type keywords for classification
    ├── Template scoring    → required_keywords (must ALL match) + hospital×10 + title×8 + optional×2
    └── Best template       → highest score wins; "unknown" if none

Stage 6: FieldExtractor::ExtractAll()
    ├── AnchorFieldExtractor → find anchor box → search right/below → closest match
    ├── ParagraphExtractor   → find start anchor → find end anchor → collect between
    └── Fallback strategy    → if anchor_right fails, try anchor_below, and vice versa

Stage 7: FieldValidator (per field)
    ├── IdCardValidator     → OCR correction → GB 11643-1999 checksum → birth date → gender
    ├── DateValidator       → regex parse → normalize to YYYY-MM-DD → leap year check
    ├── FieldNormalizer     → gender mapping → age extraction → whitespace strip
    └── CrossValidate       → ID.birth_date vs extracted.birth_date (warn, don't overwrite)

Stage 8: ResultBuilder
    ├── Serialize patient fields   → name, gender, birth_date, age, id_number
    ├── Serialize exam fields      → hospital, exam, findings, impression, doctors
    ├── ID masking                 → keep first 4 + last 4, middle replaced with *
    ├── Quality info               → blur_score, is_blurry, is_overexposed, document_detected
    └── Warnings                   → diagnostic messages collected from all stages
```

## Key Design Decisions

### 1. IOcrEngine Abstraction

```
IOcrEngine (abstract)
    ├── MockOcrEngine     ← Always available, no external deps
    ├── PaddleOcrEngine   ← #ifdef MEDICAL_OCR_HAS_PADDLE (real) / stub (#else)
    ├── OnnxOcrEngine     ← Future
    └── RapidOcrEngine    ← Future
```

All backends return the same `OcrResult` structure. The rest of the pipeline
(document analysis, field extraction, validation) is completely agnostic to
which engine produced the OCR data.

### 2. Normalized Coordinates (0–1000)

Templates use 0–1000 coordinates, not pixels. Before template matching,
all OCR box coordinates are normalized:
```
norm_x = pixel_x * 1000 / image_width
norm_y = pixel_y * 1000 / image_height
```

This decouples templates from DPI and image resolution. A template written for
300 DPI A4 works identically on 150 DPI A5 scans.

### 3. C ABI Boundary

- Only C functions cross the DLL boundary
- `std::string`, `std::vector`, `cv::Mat`, C++ classes — never exposed
- String allocation: DLL side, freed by caller via `OCR_FreeResult`
- All exceptions caught at DLL boundary → error codes
- No C++ name mangling in exported symbols

### 4. Privacy by Design

| Concern | Implementation |
|---------|---------------|
| No network | Zero HTTP/HTTPS/socket code in the library |
| ID masking | Default ON (`mask_id_number: true`), keeps 4+4, config-controlled |
| Debug images | Default OFF (`enable_debug_image_output: false`) |
| Raw text logs | Default OFF (`enable_raw_text_log: false`) |
| Patient data in globals | Never — data lives on stack during Recognize |
| Crash reports | None — no telemetry, no upload, no auto-update |

### 5. Phase-Based Architecture

Each phase builds on the last, always producing a compilable, testable artifact:

```
Phase 1  → DLL + C API + MockOcrEngine + basic JSON        (18 tests)
Phase 2  → + QualityChecker + Preprocessor + OpenCV         (35 new tests)
Phase 3  → + Template system + Extraction + Validation      (59 new tests)
Phase 4  → + PaddleOcrEngine (real + stub)                  (0 tests, SDK optional)
Phase 5  → + Concurrency + Packaging + Examples + Deploy    (6 new tests)
Total: 118 tests, all pass
```

## Data Flow Diagram

```
caller code (C/C++/C#/Python)
    │
    │ OCR_RecognizeFile("report.jpg", &json)
    ▼
medical_ocr_c_api.cpp
    │ SafeCall lambda → catch std::exception / ... → error code
    ▼
MedicalOcrService::RecognizeFile()
    │
    ├─ DecodeImage(path) → cv::Mat (BGR, 3-channel)
    │
    ▼
MedicalOcrService::RunPipeline(image)
    │
    ├─ ImageQualityChecker::Check()      → QualityInfo
    ├─ DocumentPreprocessor::Process()   → cv::Mat (enhanced + deskewed)
    ├─ IOcrEngine::Recognize()           → OcrResult (boxes[], text, confidence)
    ├─ geometry::NormalizeCoordinates()  → boxes in 0–1000 space
    ├─ TemplateMatcher::ConcatenateText()→ all_text string
    ├─ DocumentClassifier::Classify()    → DocumentType
    ├─ TemplateMatcher::Match()          → best ReportTemplate
    ├─ FieldExtractor::ExtractAll()      → PatientInfo + ExaminationInfo
    │   ├─ AnchorFieldExtractor::Extract()  (per anchor field)
    │   ├─ ParagraphExtractor::Extract()    (per paragraph field)
    │   ├─ FieldNormalizer / DateValidator / IdCardValidator (per field)
    │   └─ CrossValidate (birth_date vs age vs ID)
    │
    ▼
MedicalOcrService::BuildResultJson()
    │
    ├─ MaskIdNumber(id) if config.mask_id_number
    ├─ nlohmann::json serialization
    └─ json.dump(2) → UTF-8 string
    │
    ▼
OCR_FreeResult(json) ← caller must call this
```

## Error Handling Strategy

```
DLL Boundary
┌──────────────────────────────────────────┐
│  extern "C" {                            │
│    int OCR_RecognizeFile(...) {          │
│      try {                               │
│        return service.RecognizeFile(...); │  ← C++ exception zone
│      } catch (std::bad_alloc& e) {       │
│        → MEDOCR_ERR_OUTPUT_ALLOC_FAILED  │
│      } catch (std::exception& e) {       │
│        → MEDOCR_ERR_INTERNAL_EXCEPTION   │
│      } catch (...) {                     │
│        → MEDOCR_ERR_UNKNOWN              │
│      }                                   │
│    }                                     │
│  }                                       │
└──────────────────────────────────────────┘
```

- All C API functions return `int` (error code)
- `OCR_GetLastError()` returns thread-local human-readable context
- JSON output always has `success`, `error_code`, `message` — even on failure

## Memory Management

| Allocation | Owner | Free |
|-----------|-------|------|
| JSON result string | DLL (`malloc`) | Caller via `OCR_FreeResult` |
| `cv::Mat` images | Stack / RAII | Automatic (destructor) |
| OCR engine | `unique_ptr<IOcrEngine>` | `Shutdown()` |
| Template data | `vector<ReportTemplate>` | `Shutdown()` |
| Patient data | Stack variables in `RunPipeline()` | Automatic |

**Rule**: No `new`/`delete` crossing the DLL boundary. `OCR_FreeResult` calls `std::free` from the same heap that allocated it.

## Thread Safety

| Component | Mechanism |
|-----------|-----------|
| `OCR_Init` / `OCR_Shutdown` | `std::mutex` |
| `LastErrorStore` | `std::mutex` (global) → `thread_local` buffer for `OCR_GetLastError` |
| `MedicalOcrService` config | Read-only after `OCR_Init` (immutable during `Recognize`) |
| `IOcrEngine` instances | Read-only model weights; reentrant `Recognize` |
| `TemplateRepository` | Loaded at init, read-only thereafter |
