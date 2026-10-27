#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Test-only UTF-8 path encoding boundary for TestPlaylistFolderTree.
// This is not the product Utils.cpp converter and does not copy tree,
// normalize, file, database, registry, credential, or migrator behavior.

fs::path Utf8ToPath(const std::string& str) {
#ifdef _WIN32
    if (str.empty()) {
        return fs::path();
    }
    const int wideCount = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wideCount <= 0) {
        return fs::path();
    }
    std::wstring wide(static_cast<size_t>(wideCount - 1), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide.data(), wideCount) <= 0) {
        return fs::path();
    }
    return fs::path(wide);
#else
    return fs::path(str);
#endif
}

std::string PathToUtf8(const fs::path& p) {
#ifdef _WIN32
    const std::wstring wide = p.wstring();
    if (wide.empty()) {
        return std::string();
    }
    const int utf8Count = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Count <= 0) {
        return std::string();
    }
    std::string utf8(static_cast<size_t>(utf8Count - 1), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Count, nullptr, nullptr) <= 0) {
        return std::string();
    }
    return utf8;
#else
    return p.string();
#endif
}
