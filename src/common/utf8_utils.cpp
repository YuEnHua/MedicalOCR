#include "utf8_utils.h"

namespace medical_ocr {
namespace utf8 {

bool IsValidUtf8(const std::string& s) {
    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(s.data());
    const size_t len = s.size();
    size_t i = 0;

    while (i < len) {
        unsigned char c = data[i];

        if (c <= 0x7F) {
            // Single-byte (ASCII)
            i += 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            // Two-byte sequence
            if (i + 1 >= len) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            // Three-byte sequence
            if (i + 2 >= len) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if ((data[i + 2] & 0xC0) != 0x80) return false;
            // Check for overlong encoding
            if (c == 0xE0 && (data[i + 1] & 0xE0) == 0x80) return false;
            // Check for surrogate pairs (should be encoded as 4-byte)
            if (c == 0xED && (data[i + 1] & 0xE0) == 0xA0) return false;
            i += 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            // Four-byte sequence
            if (i + 3 >= len) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if ((data[i + 2] & 0xC0) != 0x80) return false;
            if ((data[i + 3] & 0xC0) != 0x80) return false;
            // Check for overlong encoding
            if (c == 0xF0 && (data[i + 1] & 0xF0) == 0x80) return false;
            // Check for out-of-range (> U+10FFFF)
            if (c == 0xF4 && (data[i + 1] & 0xF0) != 0x80) return false;
            i += 4;
        } else {
            // Invalid leading byte
            return false;
        }
    }

    return true;
}

}  // namespace utf8
}  // namespace medical_ocr
