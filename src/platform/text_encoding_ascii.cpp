#include "text_encoding.h"

namespace Platform::TextEncoding {

bool EqualsAsciiCaseInsensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const unsigned char leftByte = static_cast<unsigned char>(left[index]);
        const unsigned char rightByte = static_cast<unsigned char>(right[index]);
        const unsigned char leftFolded =
            leftByte >= 'A' && leftByte <= 'Z'
                ? static_cast<unsigned char>(leftByte + ('a' - 'A'))
                : leftByte;
        const unsigned char rightFolded =
            rightByte >= 'A' && rightByte <= 'Z'
                ? static_cast<unsigned char>(rightByte + ('a' - 'A'))
                : rightByte;
        if (leftFolded != rightFolded) return false;
    }
    return true;
}

} // namespace Platform::TextEncoding
