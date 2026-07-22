#ifndef MEDICAL_OCR_DATE_VALIDATOR_H_
#define MEDICAL_OCR_DATE_VALIDATOR_H_

#include <string>

namespace medical_ocr {

/**
 * Result of date parsing and validation.
 */
struct DateResult {
    /// Normalized date string (YYYY-MM-DD). Empty if parsing failed.
    std::string normalized;

    /// Whether the date is valid (exists on the calendar).
    bool valid = false;

    /// The raw input string before normalization.
    std::string raw;
};

/**
 * Validates, normalizes, and converts date strings.
 *
 * Supported input formats:
 *   - YYYY-MM-DD / YYYY/MM/DD / YYYY.MM.DD
 *   - YYYY年MM月DD日
 *   - YYYYMMDD
 *   - DD/MM/YYYY (ambiguous, tries YYYY-MM-DD first)
 *   - YYYY-M-D / YYYY年M月D日 (single-digit month/day)
 */
class DateValidator {
public:
    /**
     * Parse and normalize a date string to YYYY-MM-DD.
     *
     * @param raw  The raw date string from OCR.
     * @return     Normalized result. result.valid = false on failure.
     */
    static DateResult Normalize(const std::string& raw);

    /**
     * Check if a date exists on the calendar.
     *
     * @param year   Full year (e.g., 2026).
     * @param month  1–12.
     * @param day    1–31.
     * @return       true if the date is valid.
     */
    static bool IsValidDate(int year, int month, int day);

    /**
     * Extract age from a birth date string (YYYY-MM-DD) relative to
     * a reference date.
     *
     * @param birth_date     Birth date in YYYY-MM-DD.
     * @param reference_date Reference date in YYYY-MM-DD (default: today).
     * @return               Age in years, or -1 on error.
     */
    static int CalculateAge(const std::string& birth_date,
                            const std::string& reference_date = "");
};

}  // namespace medical_ocr

#endif  // MEDICAL_OCR_DATE_VALIDATOR_H_
