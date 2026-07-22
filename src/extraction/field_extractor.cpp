#include "field_extractor.h"

#include <algorithm>
#include <map>
#include <sstream>

#include "src/validation/date_validator.h"
#include "src/validation/field_normalizer.h"
#include "src/validation/id_card_validator.h"

namespace medical_ocr {

// ============================================================================
// Extract All Fields
// ============================================================================

void FieldExtractor::ExtractAll(const std::vector<OcrTextBox>& boxes,
                                const ReportTemplate& tmpl,
                                PatientInfo& patient,
                                ExaminationInfo& exam,
                                std::vector<std::string>& warnings) {

    // Map field names to their destination pointers.
    struct Target {
        ExtractedField* field;
        std::string data_type;
    };
    std::map<std::string, Target> field_map = {
        {"name",          {&patient.name,          "string"}},
        {"gender",        {&patient.gender,        "gender"}},
        {"birth_date",    {&patient.birth_date,    "date"}},
        {"age",           {&patient.age,           "age"}},
        {"id_number",     {&patient.id_number,     "id_number"}},
        {"hospital_name", {&exam.hospital_name,    "string"}},
        {"report_type",   {&exam.report_type,      "string"}},
        {"exam_name",     {&exam.exam_name,        "string"}},
        {"exam_date",     {&exam.exam_date,        "date"}},
        {"department",    {&exam.department,       "string"}},
        {"findings",      {&exam.findings,         "text"}},
        {"impression",    {&exam.impression,       "text"}},
        {"report_doctor", {&exam.report_doctor,    "string"}},
        {"review_doctor", {&exam.review_doctor,    "string"}},
    };

    for (const auto& rule : tmpl.fields) {
        ExtractedField extracted;

        bool found = ExtractField(boxes, rule, extracted);

        // If not found by the rule's extraction type, try the opposite direction.
        if (!found && rule.extraction_type == "anchor_right") {
            FieldRule below_rule = rule;
            below_rule.search_direction = "below";
            below_rule.extraction_type = "anchor_below";
            found = AnchorFieldExtractor::Extract(boxes, below_rule, extracted);
        } else if (!found && rule.extraction_type == "anchor_below") {
            FieldRule right_rule = rule;
            right_rule.search_direction = "right";
            right_rule.extraction_type = "anchor_right";
            found = AnchorFieldExtractor::Extract(boxes, right_rule, extracted);
        }

        if (!found && rule.data_type != "text") {
            warnings.push_back("Field '" + rule.field_name +
                               "' could not be extracted");
            continue;
        }

        if (found) {
            // Determine data_type from the rule, overriding the default.
            std::string dtype = rule.data_type;

            // Apply validation and normalization.
            ValidateAndNormalize(extracted, dtype, warnings);

            // Store in the appropriate target.
            auto it = field_map.find(rule.field_name);
            if (it != field_map.end()) {
                *it->second.field = extracted;
            }
        }
    }

    // Cross-validate patient fields.
    CrossValidate(patient, warnings);
}

// ============================================================================
// Extract Single Field
// ============================================================================

bool FieldExtractor::ExtractField(const std::vector<OcrTextBox>& boxes,
                                  const FieldRule& rule,
                                  ExtractedField& out_field) {

    if (rule.extraction_type == "paragraph_between_anchors") {
        return ParagraphExtractor::Extract(boxes, rule, out_field);
    }

    // Default: anchor_right or anchor_below.
    return AnchorFieldExtractor::Extract(boxes, rule, out_field);
}

// ============================================================================
// Validate and Normalize
// ============================================================================

void FieldExtractor::ValidateAndNormalize(
    ExtractedField& field,
    const std::string& data_type,
    std::vector<std::string>& warnings) {

    if (field.value.empty()) return;

    // Strip whitespace and common artifacts.
    field.value = FieldNormalizer::Strip(field.value);

    if (data_type == "gender") {
        std::string normalized = FieldNormalizer::NormalizeGender(field.value);
        if (!normalized.empty()) {
            field.raw_value = field.value;
            field.value = normalized;
            field.validation_status = ValidationStatus::Valid;
        } else {
            field.validation_status = ValidationStatus::Uncertain;
            warnings.push_back("Gender value could not be normalized: '" +
                               field.raw_value + "'");
        }
    } else if (data_type == "date") {
        DateResult dr = DateValidator::Normalize(field.value);
        if (dr.valid) {
            field.raw_value = field.value;
            field.value = dr.normalized;
            field.validation_status = ValidationStatus::Valid;
        } else {
            field.validation_status = ValidationStatus::Uncertain;
            warnings.push_back("Date could not be normalized: '" +
                               field.value + "'");
        }
    } else if (data_type == "age") {
        int age = FieldNormalizer::ExtractAge(field.value);
        if (age >= 0) {
            field.raw_value = field.value;
            field.value = std::to_string(age);
            if (FieldNormalizer::IsReasonableAge(age)) {
                field.validation_status = ValidationStatus::Valid;
            } else {
                field.validation_status = ValidationStatus::Invalid;
                warnings.push_back("Age out of range: " + field.value);
            }
        } else {
            field.validation_status = ValidationStatus::Uncertain;
            warnings.push_back("Age could not be parsed: '" +
                               field.value + "'");
        }
    } else if (data_type == "id_number") {
        IdCardResult id_result = IdCardValidator::Validate(field.value);
        field.raw_value = field.value;

        if (id_result.format_valid && id_result.checksum_valid) {
            field.value = id_result.cleaned_number;
            field.validation_status = ValidationStatus::Valid;

            // Note: ID masking is done at JSON build time, not here.
            // The raw ID number is stored; masking is controlled by config.
        } else if (id_result.format_valid && !id_result.checksum_valid) {
            field.value = id_result.cleaned_number;
            field.validation_status = ValidationStatus::Uncertain;
            warnings.push_back("ID number checksum failed");
        } else {
            field.validation_status = ValidationStatus::Invalid;
            warnings.push_back("ID number format invalid: '" +
                               field.value + "'");
        }
    } else {
        // String / text type: basic validation only.
        if (field.value.empty()) {
            field.validation_status = ValidationStatus::Invalid;
        } else {
            field.validation_status = ValidationStatus::Valid;
        }
    }
}

// ============================================================================
// Cross-Validation
// ============================================================================

void FieldExtractor::CrossValidate(PatientInfo& patient,
                                   std::vector<std::string>& warnings) {

    // ---- Check: birth_date from ID vs extracted birth_date ----
    if (patient.id_number.validation_status == ValidationStatus::Valid &&
        patient.birth_date.validation_status == ValidationStatus::Valid) {

        std::string id_birth = IdCardValidator::ExtractBirthDate(
            patient.id_number.value);
        if (!id_birth.empty() && id_birth != patient.birth_date.value) {
            warnings.push_back(
                "Birth date conflict: ID number implies '" + id_birth +
                "' but extracted value is '" + patient.birth_date.value + "'");
            // Do NOT overwrite — leave both values as-is.
        }
    }

    // ---- Check: gender from ID vs extracted gender ----
    if (patient.id_number.validation_status == ValidationStatus::Valid &&
        patient.gender.validation_status == ValidationStatus::Valid) {

        std::string id_gender = IdCardValidator::ExtractGender(
            patient.id_number.value);
        if (!id_gender.empty() && id_gender != patient.gender.value) {
            warnings.push_back(
                "Gender conflict: ID number implies '" + id_gender +
                "' but extracted value is '" + patient.gender.value + "'");
        }
    }

    // ---- Check: age vs birth_date ----
    if (patient.birth_date.validation_status == ValidationStatus::Valid &&
        patient.age.validation_status == ValidationStatus::Valid) {

        int age_from_birth = DateValidator::CalculateAge(
            patient.birth_date.value);
        int extracted_age = std::stoi(patient.age.value);

        if (age_from_birth >= 0 && std::abs(age_from_birth - extracted_age) > 1) {
            warnings.push_back(
                "Age conflict: birth date implies age " +
                std::to_string(age_from_birth) +
                " but extracted age is " + std::to_string(extracted_age));
        }
    }
}

}  // namespace medical_ocr
