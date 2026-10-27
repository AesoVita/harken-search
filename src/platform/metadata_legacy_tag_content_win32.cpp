#include "platform/metadata_legacy_tag_content.h"

#include <windows.h>

namespace Platform::Metadata::LegacyTagContent {

constexpr UINT kCp1252 = 1252;

std::string DecodeId3v2Encoding0(std::string_view bytes) {
    if (bytes.empty()) return {};
    int wlen = MultiByteToWideChar(kCp1252, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring wide((size_t)wlen, L'\0');
    MultiByteToWideChar(kCp1252, 0, bytes.data(), (int)bytes.size(), wide.data(), wlen);
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) return {};
    std::string out((size_t)u8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wlen, out.data(), u8len, nullptr, nullptr);
    return out;
}

std::string DecodeUtf16(const uint8_t* bytes, size_t n,
                        Utf16ByteOrder order) {
    if (!bytes || n < 2) return {};
    std::wstring wide(n / 2, L'\0');
    for (size_t i = 0; i < wide.size(); ++i) {
        uint16_t a = bytes[2 * i], b = bytes[2 * i + 1];
        wide[i] = (wchar_t)(order == Utf16ByteOrder::BigEndian
                                ? (a << 8) | b
                                : (b << 8) | a);
    }
    size_t nul = wide.find(L'\0');
    if (nul != std::wstring::npos) wide.resize(nul);
    if (wide.empty()) return {};
    int n8 = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (n8 <= 0) return {};
    std::string out((size_t)n8, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), out.data(), n8, nullptr, nullptr);
    return out;
}

} // namespace Platform::Metadata::LegacyTagContent
