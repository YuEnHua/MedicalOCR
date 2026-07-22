#ifndef MEDICAL_OCR_UTF8_UTILS_H_
#define MEDICAL_OCR_UTF8_UTILS_H_

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace medical_ocr {
namespace utf8 {

/**
 * Convert a UTF-8 string to a wide string (UTF-16 on Windows).
 * Returns empty string on failure.
 */
inline std::wstring ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
#ifdef _WIN32
    int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                     static_cast<int>(utf8.size()),
                                     nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                          static_cast<int>(utf8.size()),
                          &result[0], size);
    return result;
#else
    // On non-Windows platforms, assume the string is already fine.
    // A real implementation would use iconv or similar.
    return std::wstring(utf8.begin(), utf8.end());
#endif
}

/**
 * Convert a wide string (UTF-16 on Windows) to UTF-8.
 * Returns empty string on failure.
 */
inline std::string FromWide(const std::wstring& wide) {
    if (wide.empty()) return {};
#ifdef _WIN32
    int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                     static_cast<int>(wide.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                          static_cast<int>(wide.size()),
                          &result[0], size, nullptr, nullptr);
    return result;
#else
    return std::string(wide.begin(), wide.end());
#endif
}

/**
 * Check if a string is valid UTF-8.
 * This is a basic check — it verifies that byte sequences follow UTF-8 rules.
 */
bool IsValidUtf8(const std::string& s);

}  // namespace utf8
}  // namespace medical_ocr

#endif  // MEDICAL_OCR_UTF8_UTILS_H_
