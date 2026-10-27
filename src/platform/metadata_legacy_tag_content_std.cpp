#include "platform/metadata_legacy_tag_content.h"

#include <string>

namespace Platform::Metadata::LegacyTagContent {

std::string DecodeId3v2Encoding0(std::string_view b) {
    std::string out;
    out.reserve(b.size() * 2);
    for (unsigned char c : b) {
        if (c < 0x80) {
            out.push_back((char)c);
        } else {
            out.push_back((char)(0xC0|(c>>6)));
            out.push_back((char)(0x80|(c&0x3F)));
        }
    }
    return out;
}

std::string DecodeUtf16(const uint8_t* bytes, size_t n,
                        Utf16ByteOrder order) {
    if (!bytes || n < 2) return {};
    std::wstring wide(n/2, L'\0');
    for (size_t i = 0; i < wide.size(); ++i) {
        uint16_t a = bytes[2 * i], c = bytes[2 * i + 1];
        wide[i] = (wchar_t)(order == Utf16ByteOrder::BigEndian
                                ? (a << 8) | c
                                : (c << 8) | a);
    }
    size_t nul = wide.find(L'\0');
    if (nul != std::wstring::npos) wide.resize(nul);
    std::string out;
    out.reserve(wide.size()*3);
    for (wchar_t wc : wide) {
        uint32_t c = (uint16_t)wc;
        if (c < 0x80) {
            out.push_back((char)c);
        } else if (c < 0x800) {
            out.push_back((char)(0xC0 | (c >> 6)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        } else {
            out.push_back((char)(0xE0|(c>>12)));
            out.push_back((char)(0x80|((c>>6)&0x3F)));
            out.push_back((char)(0x80|(c&0x3F)));
        }
    }
    return out;
}

} // namespace Platform::Metadata::LegacyTagContent
