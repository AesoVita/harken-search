#include "cover_discovery.h"

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <vector>

namespace Platform::CoverDiscovery {
namespace {
namespace fs = std::filesystem;
static constexpr std::string_view kStrictNames[] = {
    "cover", "folder", "front", "albumart", "artwork", "label"};
static constexpr std::string_view kStrictExtensions[] = {".jpg", ".png", ".jpeg", ".bmp"};
#ifdef ARIS_COVER_DISCOVERY_TESTING
int g_testFailAfterEntries = -1;
#endif

std::string LowerAscii(std::string value) {
    for (char& c : value)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}

std::string Trimmed(std::string value) {
#ifdef _WIN32
    if (value.rfind("\\\\?\\UNC\\", 0) == 0) value = "\\\\" + value.substr(8);
    else if (value.rfind("\\\\?\\", 0) == 0) value.erase(0, 4);
#endif
    while (!value.empty() && (value.back() == '/' || value.back() == '\\')) value.pop_back();
    return value;
}

fs::path NativePath(const std::string& utf8) {
    fs::path path = fs::u8path(utf8);
#ifdef _WIN32
    const std::wstring native = path.native();
    if (native.rfind(L"\\\\?\\", 0) != 0 && native.size() >= 3 && native[1] == L':')
        return fs::path(L"\\\\?\\" + native);
#endif
    return path;
}

bool IsStrictName(const fs::path& path) {
    const std::string stem = LowerAscii(path.stem().u8string());
    const std::string extension = LowerAscii(path.extension().u8string());
    return std::find(std::begin(kStrictNames), std::end(kStrictNames), stem) != std::end(kStrictNames) &&
           std::find(std::begin(kStrictExtensions), std::end(kStrictExtensions), extension) != std::end(kStrictExtensions);
}

bool IsHeuristicImage(const fs::path& path) {
    const std::string extension = LowerAscii(path.extension().u8string());
    return extension == ".jpg" || extension == ".png" || extension == ".jpeg";
}

bool IsAdvertiserStem(const fs::path& path) {
    const std::string stem = LowerAscii(path.stem().u8string());
    static constexpr std::string_view patterns[] = {
        u8"公众号", u8"免费领", u8"扫码", u8"关注", u8"领资源", u8"添加微信", u8"加微信",
        "wechat", "weixin", "qrcode", "qr code", "follow", "scan", "promo"};
    return std::any_of(std::begin(patterns), std::end(patterns), [&](std::string_view pattern) {
        return stem.find(pattern) != std::string::npos;
    });
}

bool UnderOrEqualRoot(const std::string& path, const std::string& root) {
    if (root.empty()) return true;
    const std::string normalizedPath = Trimmed(path);
    const std::string normalizedRoot = Trimmed(root);
    if (normalizedPath == normalizedRoot) return true;
    return normalizedPath.size() > normalizedRoot.size() &&
           normalizedPath.compare(0, normalizedRoot.size(), normalizedRoot) == 0 &&
           (normalizedPath[normalizedRoot.size()] == '/' || normalizedPath[normalizedRoot.size()] == '\\');
}

Result FindStrictOnly(const fs::path& directory) {
    std::error_code error;
    std::vector<fs::path> matches;
    fs::directory_iterator iterator(directory, error), end;
    if (error) return {Status::ProbeError, {}};
    while (iterator != end) {
        std::error_code entryError;
        const bool regular = iterator->is_regular_file(entryError);
        if (entryError) return {Status::ProbeError, {}};
        if (regular && IsStrictName(iterator->path())) matches.push_back(iterator->path());
        iterator.increment(error);
        if (error) return {Status::ProbeError, {}};
    }
    for (std::string_view name : kStrictNames)
        for (std::string_view extension : kStrictExtensions)
            for (const fs::path& path : matches)
                if (LowerAscii(path.stem().u8string()) == name &&
                    LowerAscii(path.extension().u8string()) == extension)
                    return {Status::Found, path.u8string()};
    return {};
}

Result FindLocal(const std::string& folder, const std::string& root, bool deep, int depth) {
    if (folder.empty()) return {};
    const fs::path directory = NativePath(folder);
#ifdef _WIN32
    if (directory.native().size() >= 32767) return {Status::ProbeError, {}};
#endif
    std::error_code error;
    const fs::file_status status = fs::status(directory, error);
    if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory)
        return {};
    if (error) return {Status::ProbeError, {}};
    if (!fs::is_directory(status)) return {};

    std::string candidate;
    int imageCount = 0;
    fs::directory_iterator iterator(directory, error), end;
    if (error) return {Status::ProbeError, {}};
#ifdef ARIS_COVER_DISCOVERY_TESTING
    if (g_testFailAfterEntries == 0) {
        g_testFailAfterEntries = -1;
        return {Status::ProbeError, {}};
    }
#endif
    while (iterator != end) {
        std::error_code entryError;
        const bool regular = iterator->is_regular_file(entryError);
        if (entryError) return {Status::ProbeError, {}};
        if (regular) {
            const fs::path path = iterator->path();
            if (IsStrictName(path)) return {Status::Found, path.u8string()};
            if (IsHeuristicImage(path)) {
                const auto size = iterator->file_size(entryError);
                if (entryError) return {Status::ProbeError, {}};
                if (size > 5120 && !IsAdvertiserStem(path)) {
                    if (candidate.empty()) candidate = path.u8string();
                    ++imageCount;
                }
            }
        }
#ifdef ARIS_COVER_DISCOVERY_TESTING
        if (g_testFailAfterEntries > 0 && --g_testFailAfterEntries == 0) {
            g_testFailAfterEntries = -1;
            return {Status::ProbeError, {}};
        }
#endif
        iterator.increment(error);
        if (error) return {Status::ProbeError, {}};
    }
    if (!candidate.empty() && imageCount <= 10) return {Status::Found, candidate};

    if (!root.empty() && Trimmed(folder) == Trimmed(root)) {
        const fs::path parent = directory.parent_path();
        for (const char* siblingName : {"Artwork", "Covers", "Documents"}) {
            const fs::path sibling = parent / siblingName;
            const fs::file_status siblingStatus = fs::status(sibling, error);
            if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) {
                error.clear();
                continue;
            }
            if (error) return {Status::ProbeError, {}};
            if (!fs::is_directory(siblingStatus)) continue;
            Result result = FindStrictOnly(sibling);
            if (result.status != Status::NotFound) return result;
        }
    }
    if (deep && depth > 0) {
        const fs::path parent = directory.parent_path();
        if (parent != directory && !parent.empty()) {
            const std::string parentUtf8 = parent.u8string();
            if (UnderOrEqualRoot(parentUtf8, root))
                return FindLocal(parentUtf8, root, true, depth - 1);
        }
    }
    return {};
}
} // namespace

Result Find(const std::string& folderPathUtf8, const std::string& libraryRootUtf8,
            bool useDeepSearch, int maxDepth) {
    try {
        return FindLocal(folderPathUtf8, libraryRootUtf8, useDeepSearch, std::max(0, maxDepth));
    } catch (...) {
        return {Status::ProbeError, {}};
    }
}

#ifdef ARIS_COVER_DISCOVERY_TESTING
void Testing::FailAfterEntries(int count) { g_testFailAfterEntries = count; }
#endif

} // namespace Platform::CoverDiscovery
