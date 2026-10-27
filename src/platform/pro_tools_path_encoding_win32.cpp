#include "pro_tools_path_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <limits>

namespace Platform::ProTools::PathEncoding {

std::string ToImportPath(const std::string& utf8Path) {
    if (utf8Path.empty()) return utf8Path;
    if (utf8Path.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return utf8Path;
    const int inputLen = static_cast<int>(utf8Path.size());
    const int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8Path.data(), inputLen, nullptr, 0);
    if (wideLen <= 0) return utf8Path;
    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path.data(),
        inputLen, wide.data(), wideLen) != wideLen) return utf8Path;

    BOOL usedDefault = FALSE;
    const int nativeLen = WideCharToMultiByte(CP_ACP, 0, wide.data(), wideLen,
        nullptr, 0, nullptr, &usedDefault);
    if (nativeLen <= 0 || usedDefault) return utf8Path;
    std::string nativePath(static_cast<size_t>(nativeLen), '\0');
    if (WideCharToMultiByte(CP_ACP, 0, wide.data(), wideLen, nativePath.data(),
        nativeLen, nullptr, &usedDefault) != nativeLen || usedDefault) return utf8Path;
    return nativePath;
}

} // namespace Platform::ProTools::PathEncoding
