#ifndef MEDICAL_OCR_FIELD_NORMALIZER_H_
#define MEDICAL_OCR_FIELD_NORMALIZER_H_

#include <string>

namespace medical_ocr {

/**
 * Normalizes extracted field values to standard formats.
 *
 * Transformations:
 *   - Gender: "男/女/Male/Female/M/F/1/0" → "男" / "女" / ""
 *   - Age: "37岁/37/37Y" → "37"
 *   - Strip surrounding whitespace and common separators
 */
class FieldNormalizer {
public:
    /**
     * Normalize a gender field value.
     *
     * Accepts:
     *   Chinese: 男, 女
     *   English: Male, Female, M, F
     *   Digits: 1 (male), 0 or 2 (female)
     *
     * @param raw  Raw gender string from OCR.
     * @return     "男", "女", or "" (unrecognized).
     */
    static std::string NormalizeGender(const std::string& raw);

    /**
     * Extract a numeric age from a string.
     *
     * Accepts:
     *   "37岁", "37", "37Y", "37y", "Age: 37"
     *
     * @param raw  Raw age string from OCR.
     * @return     Numeric age, or -1 if parsing failed.
     */
    static int ExtractAge(const std::string& raw);

    /**
     * Strip common OCR artifacts and whitespace from a field value.
     *
     * @param raw  Raw string from OCR.
     * @return     Cleaned string.
     */
    static std::string Strip(const std::string& raw);

    /**
     * Check if an age value is within a reasonable range.
     *
     * @param age  Age value.
     * @return     true if 0 <= age <= 150.
     */
    static bool IsReasonableAge(int age);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_FIELD_NORMALIZER_H_
