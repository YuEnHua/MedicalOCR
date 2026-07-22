#ifndef MEDICAL_OCR_FIELD_EXTRACTOR_H_
#define MEDICAL_OCR_FIELD_EXTRACTOR_H_

#include <string>
#include <vector>

#include "medical_ocr/types.h"
#include "src/document/template_repository.h"
#include "src/extraction/anchor_field_extractor.h"
#include "src/extraction/paragraph_extractor.h"

namespace medical_ocr {

/**
 * Orchestrates field extraction from OCR results using a matched template.
 *
 * For each field defined in the template, dispatches to the appropriate
 * extractor (anchor_right, anchor_below, paragraph_between_anchors).
 *
 * After extraction, applies field validation and normalization.
 */
class FieldExtractor {
public:
    /**
     * Extract all fields defined in a template from OCR results.
     *
     * @param boxes    All OCR text boxes (with normalized coordinates).
     * @param tmpl     The matched report template.
     * @param patient  Output patient info (populated in-place).
     * @param exam     Output examination info (populated in-place).
     * @param warnings Output warnings for extraction issues.
     */
    static void ExtractAll(const std::vector<OcrTextBox>& boxes,
                           const ReportTemplate& tmpl,
                           PatientInfo& patient,
                           ExaminationInfo& exam,
                           std::vector<std::string>& warnings);

    /**
     * Extract a single field from OCR text boxes.
     *
     * @param boxes     OCR text boxes.
     * @param rule      Field extraction rule.
     * @param out_field Output field.
     * @return          true if a value was extracted.
     */
    static bool ExtractField(const std::vector<OcrTextBox>& boxes,
                             const FieldRule& rule,
                             ExtractedField& out_field);

    /**
     * Apply validation and normalization to an extracted field based
     * on its data_type.
     *
     * @param field     The field to validate/normalize (modified in-place).
     * @param warnings  Output warnings for validation issues.
     */
    static void ValidateAndNormalize(ExtractedField& field,
                                     const std::string& data_type,
                                     std::vector<std::string>& warnings);

    /**
     * Cross-validate patient fields for consistency.
     *
     * Checks: birth_date vs age, birth_date vs id_number, gender vs id_number.
     * Conflicts are reported in warnings — original values are NOT overwritten.
     *
     * @param patient   Patient info.
     * @param warnings  Output warnings.
     */
    static void CrossValidate(PatientInfo& patient,
                              std::vector<std::string>& warnings);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_FIELD_EXTRACTOR_H_
