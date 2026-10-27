#pragma once

#include <string>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// @stage7-contract
// API contract: this inline Windows implementation preserves the text_encoding
// header's owned-string, no-retained-view, and strict/permissive conversion
// semantics; it must not be included as a separate public capability surface.

namespace Platform::TextEncoding {

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    const int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";

    std::wstring wstr(static_cast<size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), size) <= 0) {
        return L"";
    }
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";

    std::string str(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, wstr.c_str(), -1, str.data(), size, nullptr, nullptr) <= 0) {
        return "";
    }
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}

} // namespace Platform::TextEncoding

#else

namespace Platform::TextEncoding {

inline std::wstring Utf8ToWide(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

inline std::string WideToUtf8(const std::wstring& wstr) {
    return std::string(wstr.begin(), wstr.end());
}

} // namespace Platform::TextEncoding

#endif
