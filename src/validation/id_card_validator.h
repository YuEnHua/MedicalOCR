#ifndef MEDICAL_OCR_ID_CARD_VALIDATOR_H_
#define MEDICAL_OCR_ID_CARD_VALIDATOR_H_

#include <string>

namespace medical_ocr {

/**
 * Result of Chinese 18-digit ID number validation.
 */
struct IdCardResult {
    /// Whether the checksum is valid.
    bool checksum_valid = false;

    /// Birth date extracted from the ID number (YYYY-MM-DD), empty on failure.
    std::string birth_date;

    /// Gender inferred from the ID number (digit 17: odd=male, even=female).
    /// Values: "男", "女", "" (unknown).
    std::string gender;

    /// The cleaned ID number after OCR confusion correction.
    std::string cleaned_number;

    /// Whether the ID number format is valid (17 digits + 1 digit/X).
    bool format_valid = false;
};

/**
 * Validates and extracts information from Chinese 18-digit ID numbers.
 *
 * Implements GB 11643-1999 (公民身份号码) checksum algorithm.
 *
 * Also corrects common OCR errors in the ID number region:
 *   O → 0, I/l → 1, Z → 2, B → 8, ×/x → X
 * This correction is ONLY applied when explicitly called for ID fields.
 */
class IdCardValidator {
public:
    /**
     * Validate an 18-digit Chinese ID number.
     *
     * @param raw_id  The raw OCR-recognized ID string (may contain OCR errors).
     * @return        Validation result with extracted info.
     */
    static IdCardResult Validate(const std::string& raw_id);

    /**
     * Compute the checksum digit for a 17-digit ID number prefix.
     *
     * @param first_17  The first 17 digits.
     * @return          The expected checksum character ('0'-'9' or 'X').
     *                  Returns '\0' on invalid input.
     */
    static char ComputeChecksum(const std::string& first_17);

    /**
     * Apply OCR confusion correction to a string that is known to be
     * an ID number.
     *
     * Rules:
     *   O, o → 0
     *   I, i, l, L → 1
     *   Z, z → 2
     *   B → 8
     *   ×, x, X → X (only in the last position)
     *
     * @param raw  The raw OCR string.
     * @return     The corrected string.
     */
    static std::string CorrectOcrErrors(const std::string& raw);

    /**
     * Extract birth date from positions 7–14 of an ID number.
     *
     * @param id_number  18-digit ID number (already cleaned).
     * @return           Birth date in YYYY-MM-DD format, or empty string.
     */
    static std::string ExtractBirthDate(const std::string& id_number);

    /**
     * Extract gender from position 17 of an ID number.
     *
     * @param id_number  18-digit ID number.
     * @return           "男" if digit 17 is odd, "女" if even, "" if invalid.
     */
    static std::string ExtractGender(const std::string& id_number);
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_ID_CARD_VALIDATOR_H_
