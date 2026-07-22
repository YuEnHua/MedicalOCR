#ifndef MEDICAL_OCR_TYPES_H_
#define MEDICAL_OCR_TYPES_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace medical_ocr {

// ============================================================================
// Geometry Types
// ============================================================================

/// A 2D point with floating-point coordinates.
struct PointF {
    float x = 0.0f;
    float y = 0.0f;
};

/// A bounding box defined by 4 corner points (order: top-left, top-right,
/// bottom-right, bottom-left).
using BBox = std::array<PointF, 4>;

// ============================================================================
// OCR Types
// ============================================================================

/// A single detected text box from the OCR engine.
struct OcrTextBox {
    /// Four corner points of the detected text region.
    BBox points;
    /// The recognized text content (UTF-8).
    std::string text;
    /// Recognition confidence in [0.0, 1.0].
    float confidence = 0.0f;
    /// Optional text direction angle in degrees (0 = horizontal, 90 = vertical).
    float direction = 0.0f;
};

/// The full result from an OCR engine for a single image.
struct OcrResult {
    /// Width of the input image in pixels.
    int imageWidth = 0;
    /// Height of the input image in pixels.
    int imageHeight = 0;
    /// All detected text boxes.
    std::vector<OcrTextBox> boxes;
};

// ============================================================================
// Document Types
// ============================================================================

/// Supported document / report types.
enum class DocumentType {
    UltrasoundReport,
    CTReport,
    MRIReport,
    XRayReport,
    LaboratoryReport,
    Unknown
};

/// Convert DocumentType to string.
inline const char* DocumentTypeToString(DocumentType t) {
    switch (t) {
        case DocumentType::UltrasoundReport: return "ultrasound_report";
        case DocumentType::CTReport:          return "ct_report";
        case DocumentType::MRIReport:         return "mri_report";
        case DocumentType::XRayReport:        return "xray_report";
        case DocumentType::LaboratoryReport:  return "laboratory_report";
        default:                              return "unknown";
    }
}

/// Convert string to DocumentType.
inline DocumentType StringToDocumentType(const std::string& s) {
    if (s == "ultrasound_report") return DocumentType::UltrasoundReport;
    if (s == "ct_report")          return DocumentType::CTReport;
    if (s == "mri_report")         return DocumentType::MRIReport;
    if (s == "xray_report")        return DocumentType::XRayReport;
    if (s == "laboratory_report")  return DocumentType::LaboratoryReport;
    return DocumentType::Unknown;
}

// ============================================================================
// Extraction Result Types
// ============================================================================

/// Validation status for extracted fields.
enum class ValidationStatus {
    Valid,
    Invalid,
    Uncertain,
    NotValidated
};

inline const char* ValidationStatusToString(ValidationStatus s) {
    switch (s) {
        case ValidationStatus::Valid:       return "valid";
        case ValidationStatus::Invalid:     return "invalid";
        case ValidationStatus::Uncertain:   return "uncertain";
        default:                            return "not_validated";
    }
}

/// Extraction method used to obtain the field value.
enum class ExtractionMethod {
    AnchorRight,
    AnchorBelow,
    AnchorRegion,
    ParagraphBetweenAnchors,
    Regex,
    Default
};

inline const char* ExtractionMethodToString(ExtractionMethod m) {
    switch (m) {
        case ExtractionMethod::AnchorRight:             return "anchor_right";
        case ExtractionMethod::AnchorBelow:             return "anchor_below";
        case ExtractionMethod::AnchorRegion:            return "anchor_region";
        case ExtractionMethod::ParagraphBetweenAnchors: return "paragraph_between_anchors";
        case ExtractionMethod::Regex:                   return "regex";
        default:                                        return "default";
    }
}

/// A single extracted field with full provenance.
struct ExtractedField {
    /// Normalized/cleaned value.
    std::string value;
    /// Raw OCR text before normalization.
    std::string raw_value;
    /// Extraction confidence [0.0, 1.0].
    float confidence = 0.0f;
    /// Source bounding box coordinates.
    BBox source_bbox;
    /// Method used to extract this field.
    ExtractionMethod extraction_method = ExtractionMethod::Default;
    /// Validation status.
    ValidationStatus validation_status = ValidationStatus::NotValidated;
};

/// Patient information extracted from the report.
struct PatientInfo {
    ExtractedField name;
    ExtractedField gender;
    ExtractedField birth_date;
    ExtractedField age;
    ExtractedField id_number;
};

/// Examination information extracted from the report.
struct ExaminationInfo {
    ExtractedField hospital_name;
    ExtractedField report_type;
    ExtractedField exam_name;
    ExtractedField exam_date;
    ExtractedField department;
    ExtractedField findings;
    ExtractedField impression;
    ExtractedField report_doctor;
    ExtractedField review_doctor;
};

// ============================================================================
// Quality Assessment Types
// ============================================================================

struct QualityInfo {
    /// Laplacian variance blur score (higher = sharper).
    double blur_score = 0.0;
    /// Whether the image is considered blurry.
    bool is_blurry = false;
    /// Whether the image appears overexposed.
    bool is_overexposed = false;
    /// Whether a document was detected in the image.
    bool document_detected = true;
};

// ============================================================================
// Document Classification Result
// ============================================================================

struct DocumentClassification {
    DocumentType type = DocumentType::Unknown;
    std::string template_id;
    std::string page_type;  // e.g. "A4", "A5", "unknown"
    int image_width = 0;
    int image_height = 0;
};

// ============================================================================
// Top-level Recognition Result
// ============================================================================

struct RecognitionResult {
    bool success = false;
    int error_code = 0;
    std::string message;
    DocumentClassification document;
    PatientInfo patient;
    ExaminationInfo examination;
    QualityInfo quality;
    std::vector<std::string> warnings;
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_TYPES_H_
