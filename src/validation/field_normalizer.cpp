#include "field_normalizer.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>

namespace medical_ocr {

// ============================================================================
// Gender Normalization
// ============================================================================

std::string FieldNormalizer::NormalizeGender(const std::string& raw) {
    if (raw.empty()) return {};

    std::string s = Strip(raw);

    // Chinese
    if (s == "男" || s.find("男") != std::string::npos) return "男";
    if (s == "女" || s.find("女") != std::string::npos) return "女";

    // English (case-insensitive)
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "male" || lower == "m") return "男";
    if (lower == "female" || lower == "f") return "女";

    // Numeric
    if (s == "1") return "男";
    if (s == "0" || s == "2") return "女";

    return {};
}

// ============================================================================
// Age Extraction
// ============================================================================

int FieldNormalizer::ExtractAge(const std::string& raw) {
    if (raw.empty()) return -1;

    std::string s = Strip(raw);

    // Try: "37岁", "37", "37Y", "37y"
    // Use word boundary and leading non-digit check to avoid matching
    // negative numbers like "-1".
    std::regex re(R"((?:^|[^-\d])(\d{1,3})\s*(?:岁|[Yy]|[Yy]ears?)?)");
    std::smatch m;
    if (std::regex_search(s, m, re)) {
        int age = std::stoi(m[1].str());
        if (IsReasonableAge(age)) return age;
    }

    return -1;
}

// ============================================================================
// Strip
// ============================================================================

std::string FieldNormalizer::Strip(const std::string& raw) {
    // Remove leading and trailing whitespace.
    size_t start = 0;
    while (start < raw.size() && std::isspace(static_cast<unsigned char>(raw[start]))) {
        ++start;
    }
    size_t end = raw.size();
    while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1]))) {
        --end;
    }

    std::string s = raw.substr(start, end - start);

    // Remove trailing colons, hyphens, etc. (UTF-8 encoded).
    // U+FF1A ： = 0xEF 0xBC 0x9A
    // U+2014 — = 0xE2 0x80 0x94
    while (!s.empty()) {
        unsigned char last = static_cast<unsigned char>(s.back());
        if (last == ':' || last == '-') {
            s.pop_back();
        } else if (s.size() >= 3 &&
                   static_cast<unsigned char>(s[s.size() - 3]) == 0xEF &&
                   static_cast<unsigned char>(s[s.size() - 2]) == 0xBC &&
                   static_cast<unsigned char>(s[s.size() - 1]) == 0x9A) {
            // Remove U+FF1A (fullwidth colon).
            s.erase(s.size() - 3);
        } else if (s.size() >= 3 &&
                   static_cast<unsigned char>(s[s.size() - 3]) == 0xE2 &&
                   static_cast<unsigned char>(s[s.size() - 2]) == 0x80 &&
                   static_cast<unsigned char>(s[s.size() - 1]) == 0x94) {
            // Remove U+2014 (em dash).
            s.erase(s.size() - 3);
        } else {
            break;
        }
    }

    return s;
}

// ============================================================================
// Age Range Check
// ============================================================================

bool FieldNormalizer::IsReasonableAge(int age) {
    return age >= 0 && age <= 150;
}

}  // namespace medical_ocr
