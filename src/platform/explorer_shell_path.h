#pragma once

#include <string>
#include <string_view>

namespace Platform::Detail {

inline wchar_t FoldExplorerPrefixAscii(wchar_t value) noexcept {
    return (value >= L'A' && value <= L'Z')
        ? static_cast<wchar_t>(value - L'A' + L'a')
        : value;
}

inline bool ExplorerPrefixEquals(std::wstring_view value,
                                 std::wstring_view prefix) noexcept {
    if (value.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (FoldExplorerPrefixAscii(value[i]) != FoldExplorerPrefixAscii(prefix[i])) {
            return false;
        }
    }
    return true;
}

// Windows filesystem APIs accept extended-length paths, but Explorer's Shell
// namespace APIs do not handle those prefixes consistently. Keep the stored
// path unchanged and strip only the Shell-facing copy.
inline std::wstring PrepareExplorerPathForShell(std::wstring_view path) {
    constexpr std::wstring_view kLongUncPrefix = LR"(\\?\UNC\)";
    constexpr std::wstring_view kLongPathPrefix = LR"(\\?\)";
    if (ExplorerPrefixEquals(path, kLongUncPrefix)) {
        return std::wstring(L"\\\\") + std::wstring(path.substr(kLongUncPrefix.size()));
    }
    if (ExplorerPrefixEquals(path, kLongPathPrefix)) {
        return std::wstring(path.substr(kLongPathPrefix.size()));
    }
    return std::wstring(path);
}

} // namespace Platform::Detail
