#include "id_card_validator.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace medical_ocr {

// Weight factors for the first 17 digits (GB 11643-1999).
static const int kWeights[17] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};

// Checksum mapping: remainder → checksum character.
static const char kChecksumMap[11] = {'1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2'};

// ============================================================================
// Compute Checksum
// ============================================================================

char IdCardValidator::ComputeChecksum(const std::string& first_17) {
    if (first_17.size() != 17) return '\0';

    int sum = 0;
    for (int i = 0; i < 17; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(first_17[i]))) {
            return '\0';
        }
        sum += (first_17[i] - '0') * kWeights[i];
    }
    int remainder = sum % 11;
    return kChecksumMap[remainder];
}

// ============================================================================
// OCR Error Correction
// ============================================================================

std::string IdCardValidator::CorrectOcrErrors(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];

        // Only apply corrections in positions that should be digits,
        // and only for known OCR confusion patterns.
        bool is_last = (i == raw.size() - 1 || i == 17);

        switch (c) {
            case 'O': case 'o': c = '0'; break;
            case 'I': case 'i': case 'l': case 'L': c = '1'; break;
            case 'Z': case 'z': c = '2'; break;
            case 'B': c = '8'; break;
            // × (multiplication sign U+00D7) → X
            case '\xC3':  // UTF-8 lead byte for ×
                if (i + 1 < raw.size() &&
                    static_cast<unsigned char>(raw[i + 1]) == 0x97) {
                    c = 'X';
                    ++i;  // Skip the continuation byte.
                }
                break;
            default: break;
        }

        // For the 18th position (index 17), allow 'X' or 'x'.
        if (is_last && (c == 'x' || c == 'X')) {
            c = 'X';
        }

        result.push_back(c);
    }

    return result;
}

// ============================================================================
// Extract Birth Date
// ============================================================================

std::string IdCardValidator::ExtractBirthDate(const std::string& id_number) {
    if (id_number.size() < 14) return {};

    // Birth date is at positions 6–13 (0-indexed), 8 digits: YYYYMMDD.
    std::string y = id_number.substr(6, 4);
    std::string m = id_number.substr(10, 2);
    std::string d = id_number.substr(12, 2);

    // Basic sanity check.
    int year = std::stoi(y);
    int month = std::stoi(m);
    int day = std::stoi(d);

    if (year < 1900 || year > 2100) return {};
    if (month < 1 || month > 12) return {};
    if (day < 1 || day > 31) return {};

    // Format as YYYY-MM-DD.
    std::ostringstream oss;
    oss << y << "-" << m << "-" << d;
    return oss.str();
}

// ============================================================================
// Extract Gender
// ============================================================================

std::string IdCardValidator::ExtractGender(const std::string& id_number) {
    if (id_number.size() < 17) return {};

    char c = id_number[16];  // 17th digit (0-indexed: 16).
    if (!std::isdigit(static_cast<unsigned char>(c))) return {};

    int digit = c - '0';
    return (digit % 2 == 1) ? "男" : "女";
}

// ============================================================================
// Full Validation
// ============================================================================

IdCardResult IdCardValidator::Validate(const std::string& raw_id) {
    IdCardResult result;

    // Step 1: Clean OCR errors.
    result.cleaned_number = CorrectOcrErrors(raw_id);

    // Step 2: Check format (17 digits + 1 digit/X, total 18).
    if (result.cleaned_number.size() != 18) {
        return result;  // format_valid stays false.
    }

    // Check first 17 are digits.
    for (int i = 0; i < 17; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(result.cleaned_number[i]))) {
            return result;
        }
    }
    // Check last char is digit or X.
    char last = result.cleaned_number[17];
    if (!std::isdigit(static_cast<unsigned char>(last)) && last != 'X') {
        return result;
    }

    result.format_valid = true;

    // Step 3: Compute checksum.
    std::string first_17 = result.cleaned_number.substr(0, 17);
    char expected = ComputeChecksum(first_17);
    result.checksum_valid = (expected != '\0' && expected == last);

    // Step 4: Extract birth date.
    result.birth_date = ExtractBirthDate(result.cleaned_number);

    // Step 5: Extract gender.
    result.gender = ExtractGender(result.cleaned_number);

    return result;
}

}  // namespace medical_ocr
