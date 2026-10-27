#include "platform/pro_tools_path_encoding.h"

#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
std::string Reference(const std::string& utf8) {
#ifdef _WIN32
    if (utf8.empty()) return utf8;
    if (utf8.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return utf8;
    const int inputLen = static_cast<int>(utf8.size());
    const int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), inputLen, nullptr, 0);
    if (wideLen <= 0) return utf8;
    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
        inputLen, wide.data(), wideLen) != wideLen) return utf8;
    BOOL usedDefault = FALSE;
    const int nativeLen = WideCharToMultiByte(CP_ACP, 0, wide.data(), wideLen,
        nullptr, 0, nullptr, &usedDefault);
    if (nativeLen <= 0 || usedDefault) return utf8;
    std::string native(static_cast<size_t>(nativeLen), '\0');
    if (WideCharToMultiByte(CP_ACP, 0, wide.data(), wideLen, native.data(),
        nativeLen, nullptr, &usedDefault) != nativeLen || usedDefault) return utf8;
    return native;
#else
    return utf8;
#endif
}
}

int main() {
    const std::vector<std::string> cases = {
        {}, "C:\\Audio Files\\kick.wav", u8"C:\\音频\\脚步.wav",
        u8"C:\\emoji-😀.wav", std::string("bad\xFFutf8", 8),
        std::string("A\0B", 3),
    };
    for (const auto& input : cases) {
        const std::string expected = Reference(input);
        const std::string actual = Platform::ProTools::PathEncoding::ToImportPath(input);
        if (actual != expected) {
            std::cerr << "PTSL path encoding mismatch\n";
            return 1;
        }
    }
    return 0;
}
