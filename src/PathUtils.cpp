// Canonical and display-path helpers.
//
// Contracts are documented in PathUtils.h.

#include "PathUtils.h"
#include "Utils.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace HarkenSearch {

namespace {

#ifdef _WIN32
// case-insensitive ASCII prefix match
// 用于检测 \\?\UNC\ / \\?\ 前缀，这两个前缀本身在不同 API/库输出中可能大小写不一
bool StartsWithAsciiCaseInsensitive(const std::string& s, const char* prefix) {
    size_t n = std::strlen(prefix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = s[i];
        char b = prefix[i];
        // ASCII case fold (仅大小写字母)
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}
#endif

// strip Windows 长路径前缀 \\?\ 和 \\?\UNC\
// 输入 / 输出都是 UTF-8
// Match Windows long-path prefixes case-insensitively.
std::string StripLongPathPrefix(std::string s) {
#ifdef _WIN32
    // \\?\UNC\server\share\... → \\server\share\...
    if (StartsWithAsciiCaseInsensitive(s, "\\\\?\\UNC\\")) {
        return std::string("\\\\") + s.substr(8);
    }
    // \\?\C:\... → C:\...
    if (StartsWithAsciiCaseInsensitive(s, "\\\\?\\")) {
        return s.substr(4);
    }
#endif
    return s;
}

std::string TrimDisplayFolderSuffix(std::string s) {
#ifdef _WIN32
    while (s.size() > 3 && (s.back() == '\\' || s.back() == '/')) {
        s.pop_back();
    }
#else
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
#endif
    return s;
}

std::string NormalizeDisplayFolderPath(std::string_view folderPath) {
    std::string s(folderPath);
    s = StripLongPathPrefix(std::move(s));
#ifdef _WIN32
    std::replace(s.begin(), s.end(), '/', '\\');
#else
    std::replace(s.begin(), s.end(), '\\', '/');
#endif
    return TrimDisplayFolderSuffix(std::move(s));
}

} // namespace

std::string CanonicalFolderPath(std::string_view rawPath) {
    if (rawPath.empty()) return std::string();

    std::string s(rawPath);

    // 1. strip 长路径前缀
    s = StripLongPathPrefix(std::move(s));
    if (s.empty()) return std::string();

#ifdef _WIN32
    // 2. 分隔符统一为反斜杠
    std::replace(s.begin(), s.end(), '/', '\\');

    // 3. 去尾部斜杠 — 但保留 drive root ("E:\")
    while (s.size() > 3 && (s.back() == '\\' || s.back() == '/')) {
        s.pop_back();
    }

    // 4. ASCII 大小写归一 (非 ASCII 字节保留原值)
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        if (c >= 'A' && c <= 'Z') {
            return static_cast<char>(c - 'A' + 'a');
        }
        return static_cast<char>(c);
    });
#else
    // POSIX: 分隔符 → /, 去尾部多余 /
    std::replace(s.begin(), s.end(), '\\', '/');
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
#endif

    return s;
}

std::string DisplayFolderFromFilePath(std::string_view filePath) {
    if (filePath.empty()) return std::string();

    // Prefer byte-stable splitting for UI display. Windows fs::path can still
    // lose exact UTF-8 display bytes for some CJK folder names with brackets.
    std::string fallback;
    {
        std::string rawDirect(filePath.data(), filePath.size());
        rawDirect = StripLongPathPrefix(std::move(rawDirect));
#ifdef _WIN32
        std::replace(rawDirect.begin(), rawDirect.end(), '/', '\\');
        const size_t slash = rawDirect.find_last_of("\\/");
        if (slash != std::string::npos) {
            if (slash == 2 && rawDirect.size() >= 3 && rawDirect[1] == ':') {
                fallback = rawDirect.substr(0, 3);
            } else if (slash > 0) {
                fallback = TrimDisplayFolderSuffix(rawDirect.substr(0, slash));
            }
        }
#else
        std::replace(rawDirect.begin(), rawDirect.end(), '\\', '/');
        const size_t slash = rawDirect.find_last_of('/');
        if (slash != std::string::npos) {
            if (slash == 0) {
                fallback = "/";
            } else {
                fallback = TrimDisplayFolderSuffix(rawDirect.substr(0, slash));
            }
        }
#endif
    }
    if (!fallback.empty()) return fallback;

    try {
        // 1. strip 长路径前缀 (用户不希望看到 \\?\ 前缀)
        std::string raw = StripLongPathPrefix(std::string(filePath.data(), filePath.size()));
        if (raw.empty()) return std::string();

        // 2. UTF-8 安全转换 → fs::path → parent_path → 回 UTF-8
        // 用 Utf8ToPath 而非 fs::path(std::string) 保证中文路径不乱码
        fs::path p = Utf8ToPath(raw);
        fs::path parent = p.parent_path();

        if (parent.empty()) return fallback;

        std::string result = PathToUtf8(parent);
        if (result.empty()) return fallback;

#ifdef _WIN32
        // 显示形态也用反斜杠 (Windows 用户预期)
        std::replace(result.begin(), result.end(), '/', '\\');

        // 去尾部多余反斜杠 (但保留 drive root 的尾)
        result = TrimDisplayFolderSuffix(std::move(result));
#endif

        return result;
    } catch (...) {
        // UI 渲染线程不能炸 — 任何 fs 异常 swallow
        return fallback;
    }
}

std::string ResolveFolderBreadcrumbDisplayPath(
    std::string_view selectedFilePath,
    std::string_view selectedRecordFolderPath,
    std::string_view activeFolderFilterPath)
{
    if (!selectedFilePath.empty()) {
        std::string folder = DisplayFolderFromFilePath(selectedFilePath);
        if (!folder.empty()) return folder;
    }
    if (!selectedRecordFolderPath.empty()) {
        return NormalizeDisplayFolderPath(selectedRecordFolderPath);
    }
    return NormalizeDisplayFolderPath(activeFolderFilterPath);
}

std::string ResolveFolderBreadcrumbDisplayLabel(
    std::string_view displayPath,
    std::string_view fallbackLabel)
{
    if (!displayPath.empty()) {
        return std::string(displayPath);
    }
    return std::string(fallbackLabel);
}

std::string ResolveActiveFolderBreadcrumbFilterPath(
    bool folderFilterActive,
    std::string_view selectedFolderPath)
{
    if (!folderFilterActive) {
        return std::string();
    }
    return std::string(selectedFolderPath);
}

} // namespace HarkenSearch
