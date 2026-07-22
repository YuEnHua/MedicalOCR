#include "date_validator.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace medical_ocr {

// ============================================================================
// Date Normalization
// ============================================================================

DateResult DateValidator::Normalize(const std::string& raw) {
    DateResult result;
    result.raw = raw;

    if (raw.empty()) return result;

    // Strip whitespace.
    std::string s = raw;
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char c) { return std::isspace(c); }),
             s.end());

    // Pattern 1: YYYY年MM月DD日 or YYYY年M月D日
    {
        std::regex re(R"((\d{4})\s*年\s*(\d{1,2})\s*月\s*(\d{1,2})\s*日)");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            int y = std::stoi(m[1].str());
            int mo = std::stoi(m[2].str());
            int d = std::stoi(m[3].str());
            if (IsValidDate(y, mo, d)) {
                std::ostringstream oss;
                oss << y << "-" << std::setw(2) << std::setfill('0') << mo
                    << "-" << std::setw(2) << std::setfill('0') << d;
                result.normalized = oss.str();
                result.valid = true;
                return result;
            }
        }
    }

    // Pattern 2: YYYY-MM-DD / YYYY/MM/DD / YYYY.MM.DD
    {
        std::regex re(R"((\d{4})\s*[-/\.]\s*(\d{1,2})\s*[-/\.]\s*(\d{1,2}))");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            int y = std::stoi(m[1].str());
            int mo = std::stoi(m[2].str());
            int d = std::stoi(m[3].str());
            if (IsValidDate(y, mo, d)) {
                std::ostringstream oss;
                oss << y << "-" << std::setw(2) << std::setfill('0') << mo
                    << "-" << std::setw(2) << std::setfill('0') << d;
                result.normalized = oss.str();
                result.valid = true;
                return result;
            }
        }
    }

    // Pattern 3: YYYYMMDD (8 digits)
    {
        std::regex re(R"(\b(\d{4})(\d{2})(\d{2})\b)");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            int y = std::stoi(m[1].str());
            int mo = std::stoi(m[2].str());
            int d = std::stoi(m[3].str());
            if (y >= 1900 && y <= 2100 && IsValidDate(y, mo, d)) {
                std::ostringstream oss;
                oss << y << "-" << std::setw(2) << std::setfill('0') << mo
                    << "-" << std::setw(2) << std::setfill('0') << d;
                result.normalized = oss.str();
                result.valid = true;
                return result;
            }
        }
    }

    return result;
}

// ============================================================================
// Calendar Validation
// ============================================================================

bool DateValidator::IsValidDate(int year, int month, int day) {
    if (year < 1900 || year > 2100) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;

    // Days in month.
    static const int kDaysInMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int max_day = kDaysInMonth[month - 1];

    // February leap year check.
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (leap) max_day = 29;
    }

    return day <= max_day;
}

// ============================================================================
// Age Calculation
// ============================================================================

int DateValidator::CalculateAge(const std::string& birth_date,
                                const std::string& reference_date) {
    if (birth_date.size() < 10) return -1;

    int by = std::stoi(birth_date.substr(0, 4));
    int bm = std::stoi(birth_date.substr(5, 2));
    int bd = std::stoi(birth_date.substr(8, 2));

    int ry, rm, rd;
    if (!reference_date.empty() && reference_date.size() >= 10) {
        ry = std::stoi(reference_date.substr(0, 4));
        rm = std::stoi(reference_date.substr(5, 2));
        rd = std::stoi(reference_date.substr(8, 2));
    } else {
        // Use current date.
        std::time_t now = std::time(nullptr);
        std::tm* tm = std::localtime(&now);
        ry = tm->tm_year + 1900;
        rm = tm->tm_mon + 1;
        rd = tm->tm_mday;
    }

    int age = ry - by;
    if (rm < bm || (rm == bm && rd < bd)) {
        --age;
    }
    return age;
}

}  // namespace medical_ocr
