#include "platform/harken_user_data_layout.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Platform::HarkenUserData {
namespace {

namespace fs = std::filesystem;

std::optional<Layout> g_cached;
std::optional<ThesaurusFile> g_thesaurusCached;

bool PathExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool IsDir(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool IsFile(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

std::uintmax_t FileSize(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return ec ? 0 : size;
}

bool Openable(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return in.good();
}

bool UsableThesaurusFile(const fs::path& path) {
    if (!IsFile(path) || FileSize(path) == 0) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) return false;
    char first = 0;
    in.read(&first, 1);
    if (!in.good()) return false;
    return first == '{' || first == '[';
}

bool EnsureDir(const fs::path& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec && IsDir(path);
}

bool SameFileContents(const fs::path& src, const fs::path& dst) {
    if (!IsFile(dst) || FileSize(src) != FileSize(dst)) return false;
    std::ifstream source(src, std::ios::binary);
    std::ifstream target(dst, std::ios::binary);
    if (!source || !target) return false;
    std::array<char, 65536> sourceBytes{}, targetBytes{};
    for (;;) {
        source.read(sourceBytes.data(), sourceBytes.size());
        target.read(targetBytes.data(), targetBytes.size());
        const auto count = source.gcount();
        if (source.bad() || target.bad() || count != target.gcount() ||
            !std::equal(sourceBytes.begin(), sourceBytes.begin() + count, targetBytes.begin())) return false;
        if (source.eof() || target.eof()) return source.eof() && target.eof();
        if (!source || !target) return false;
    }
}

bool CopyOneFile(const fs::path& src, const fs::path& dst) {
    if (!IsFile(src)) return false;
    if (SameFileContents(src, dst)) return true;
    if (!EnsureDir(dst.parent_path())) return false;
    const fs::path staged = fs::path(dst.native() + L".migration-copy-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    fs::copy_file(src, staged, fs::copy_options::overwrite_existing, ec);
    if (ec) return false;
    if (!SameFileContents(src, staged)) return false;
    if (!MoveFileExW(staged.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
    return SameFileContents(src, dst);
}

bool CopyTree(const fs::path& src, const fs::path& dst) {
    if (IsFile(src)) return CopyOneFile(src, dst);
    if (!IsDir(src)) return true;
    if (!EnsureDir(dst)) return false;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(src, ec), end; it != end && !ec; it.increment(ec)) {
        const fs::path current = it->path();
        const fs::path rel = fs::relative(current, src, ec);
        if (ec) return false;
        const fs::path target = dst / rel;
        if (it->is_directory(ec)) {
            if (!EnsureDir(target)) return false;
            continue;
        }
        if (it->is_regular_file(ec)) {
            if (!CopyOneFile(current, target)) return false;
        }
    }
    return !ec;
}

struct LegacySpec {
    const wchar_t* sourceRel;
    bool roaming;
    bool fileOnly;
    enum class Dest { Settings, Database, Updates } dest;
};

// Legacy inventory: old directory literals live only in this list.
// Canonical destination: %LOCALAPPDATA%\HarkenSearch
const LegacySpec kLegacySpecs[] = {
    {L"AuroAudio", true, false, LegacySpec::Dest::Settings},
    {L"AuroAudio", false, false, LegacySpec::Dest::Settings},
    {L"ArisAudio", true, false, LegacySpec::Dest::Settings},
    {L"Auro\\Database", false, false, LegacySpec::Dest::Database},
    {L"Auro\\auro.db", false, true, LegacySpec::Dest::Database},
    {L"Aris\\updates", false, false, LegacySpec::Dest::Updates},
};

fs::path SourcePath(const Roots& roots, const LegacySpec& spec) {
    const std::wstring base = spec.roaming ? roots.roamingAppData : roots.localAppData;
    if (base.empty()) return {};
    return fs::path(base) / spec.sourceRel;
}

bool AnyLegacyPresent(const Roots& roots) {
    for (const auto& spec : kLegacySpecs) {
        if (PathExists(SourcePath(roots, spec))) return true;
    }
    return false;
}

bool SyncDatabaseSidecars(const fs::path& source, const fs::path& target) {
    for (const wchar_t* suffix : {L"-wal", L"-shm"}) {
        const fs::path sourceSidecar = fs::path(source.native() + suffix);
        const fs::path targetSidecar = fs::path(target.native() + suffix);
        std::error_code ec;
        const bool sourceExists = fs::exists(sourceSidecar, ec);
        if (ec) return false;
        if (sourceExists) {
            if (!IsFile(sourceSidecar) || !CopyOneFile(sourceSidecar, targetSidecar)) return false;
            continue;
        }
        const bool targetExists = fs::exists(targetSidecar, ec);
        if (ec) return false;
        if (targetExists && (!IsFile(targetSidecar) || !DeleteFileW(targetSidecar.c_str()))) return false;
    }
    return true;
}

bool CopyLegacy(const Roots& roots) {
    for (const auto& spec : kLegacySpecs) {
        const fs::path src = SourcePath(roots, spec);
        if (!PathExists(src)) continue;
        fs::path dst;
        if (spec.fileOnly) {
            const fs::path legacyPackage = fs::path(roots.localAppData) / L"Auro" / L"Database";
            if (IsFile(legacyPackage / L"Auro" / L"main.db") || IsFile(legacyPackage / L"Auro.db")) continue;
            dst = fs::path(CanonicalDatabaseDir(roots)) / L"Auro" / L"main.db";
            if (!CopyOneFile(src, dst)) return false;
            if (!SyncDatabaseSidecars(src, dst)) return false;
        } else if (spec.dest == LegacySpec::Dest::Settings) {
            dst = fs::path(CanonicalSettingsDir(roots));
            if (!CopyTree(src, dst)) return false;
        } else if (spec.dest == LegacySpec::Dest::Updates) {
            dst = fs::path(CanonicalUpdatesDir(roots));
            if (!CopyTree(src, dst)) return false;
        } else {
            dst = fs::path(CanonicalDatabaseDir(roots));
            if (!CopyTree(src, dst)) return false;
            const fs::path packedMain = src / L"Auro" / L"main.db";
            if (IsFile(packedMain) &&
                !SyncDatabaseSidecars(packedMain, dst / L"Auro" / L"main.db")) return false;
            const fs::path oldFlat = src / L"Auro.db";
            if (IsFile(oldFlat) && !IsFile(packedMain)) {
                const fs::path mainDb = dst / L"Auro" / L"main.db";
                if (!CopyOneFile(oldFlat, mainDb)) return false;
                if (!SyncDatabaseSidecars(oldFlat, mainDb)) return false;
            }
        }
    }
    return true;
}

bool WriteMarker(const Roots& roots) {
    const fs::path root = CanonicalRoot(roots);
    if (!EnsureDir(root)) return false;
    const fs::path marker = root / L"migration.marker";
    std::ofstream out(marker, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "harken-data-migrated\n";
    return out.good();
}

bool MarkerPresent(const Roots& roots) {
    return IsFile(fs::path(CanonicalRoot(roots)) / L"migration.marker");
}

bool RepairMarkedDatabase(const Roots& roots) {
    const fs::path databaseDir = CanonicalDatabaseDir(roots);
    const fs::path mainDb = databaseDir / L"Auro" / L"main.db";
    if (IsFile(mainDb)) return true;
    const fs::path oldDatabaseDir = fs::path(roots.localAppData) / L"Auro" / L"Database";
    const fs::path sources[] = {
        oldDatabaseDir / L"Auro" / L"main.db",
        oldDatabaseDir / L"Auro.db",
        fs::path(roots.localAppData) / L"Auro" / L"auro.db",
        databaseDir / L"Auro.db",
    };
    for (const fs::path& source : sources) {
        if (!IsFile(source)) continue;
        // main.db is the visible completion signal for marker repair. Publish it
        // only after every available sidecar has reached its final location.
        for (const wchar_t* suffix : {L"-wal", L"-shm"}) {
            const fs::path sidecar = fs::path(source.native() + suffix);
            if (IsFile(sidecar) && !CopyOneFile(sidecar, fs::path(mainDb.native() + suffix))) return false;
        }
        if (!CopyOneFile(source, mainDb)) return false;
        return true;
    }
    return true;
}

Layout MakeCanonical(const Roots& roots, LayoutKind kind, bool readOnly, const std::string& notice) {
    Layout layout;
    layout.kind = kind;
    layout.localAppData = roots.localAppData;
    layout.roamingAppData = roots.roamingAppData;
    layout.canonicalRoot = CanonicalRoot(roots);
    layout.activeSettingsDir = CanonicalSettingsDir(roots);
    layout.activeDatabaseDir = CanonicalDatabaseDir(roots);
    layout.activeUpdatesDir = CanonicalUpdatesDir(roots);
    layout.readOnly = readOnly;
    layout.noticeUtf8 = notice;
    return layout;
}

Layout MakeFallback(const Roots& roots) {
    Layout layout = MakeCanonical(roots, LayoutKind::FallbackLegacy, true, "无法把旧数据复制到新目录，已改用旧目录只读打开。原来的文件没有删除。");
    const fs::path localAuroDbDir = fs::path(roots.localAppData) / L"Auro\\Database";
    const fs::path localAuroFile = fs::path(roots.localAppData) / L"Auro\\auro.db";
    const fs::path roamingAuroAudio = fs::path(roots.roamingAppData) / L"AuroAudio";
    const fs::path localAuroAudio = fs::path(roots.localAppData) / L"AuroAudio";
    const fs::path roamingArisAudio = fs::path(roots.roamingAppData) / L"ArisAudio";
    const fs::path localUpdates = fs::path(roots.localAppData) / L"Aris\\updates";
    for (const fs::path& source : {
             localAuroDbDir / L"Auro" / L"main.db",
             localAuroDbDir / L"Auro.db",
             localAuroFile}) {
        if (!IsFile(source)) continue;
        layout.activeDatabaseDir = source.parent_path().wstring();
        layout.activeDatabaseFile = source.wstring();
        break;
    }
    std::error_code tempError;
    const fs::path tempRoot = fs::temp_directory_path(tempError);
    if (!tempError) {
        const fs::path fallbackSettings = tempRoot /
            (L"HarkenSearch-fallback-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
             std::to_wstring(std::hash<std::wstring>{}(roots.localAppData)));
        layout.activeSettingsDir = fallbackSettings.wstring();
        if (EnsureDir(fallbackSettings)) {
            for (const fs::path& source : {roamingAuroAudio, localAuroAudio, roamingArisAudio}) {
                if (IsDir(source)) CopyTree(source, fallbackSettings);
            }
        }
    }
    if (IsDir(localUpdates)) layout.activeUpdatesDir = localUpdates.wstring();
    return layout;
}

Layout ComputeLayout() {
    const Roots roots = ResolveRoots();
    if (roots.localAppData.empty()) {
        Layout layout;
        layout.kind = LayoutKind::FallbackLegacy;
        layout.readOnly = true;
        layout.noticeUtf8 = "无法把旧数据复制到新目录，已改用旧目录只读打开。原来的文件没有删除。";
        return layout;
    }
    if (MarkerPresent(roots)) {
        if (!RepairMarkedDatabase(roots)) return MakeFallback(roots);
        return MakeCanonical(roots, LayoutKind::Migrated, false, {});
    }
    if (!AnyLegacyPresent(roots)) {
        return MakeCanonical(roots, LayoutKind::FreshInstall, false, {});
    }
    if (!CopyLegacy(roots) || !WriteMarker(roots)) {
        return MakeFallback(roots);
    }
    return MakeCanonical(roots, LayoutKind::Migrated, false, "已把旧数据复制到新目录，原来的文件夹没有改动。");
}

}  // namespace

Layout EnsureLayout() {
    if (!g_cached.has_value()) g_cached = ComputeLayout();
    return *g_cached;
}

void ResetLayoutCacheForTests() { g_cached.reset(); }

void ResetThesaurusCacheForTests() { g_thesaurusCached.reset(); }

ThesaurusFile EnsureThesaurusFileUtf8(const std::string& exeDirUtf8) {
    if (g_thesaurusCached.has_value()) return *g_thesaurusCached;
    ThesaurusFile out;
    const fs::path exeDir = Utf8ToWidePath(exeDirUtf8);
    const fs::path canonical = exeDir / CanonicalThesaurusFileName();
    const fs::path legacyAris = exeDir / LegacyArisThesaurusFileName();
    const fs::path legacyAuro = exeDir / LegacyAuroThesaurusFileName();
    const fs::path marker = exeDir / ThesaurusMigrationMarkerFileName();
    out.canonicalPath = canonical.wstring();
    auto finish = [&](LayoutKind kind, const fs::path& active, bool readOnly, const char* notice) {
        out.kind = kind;
        out.activePath = active.wstring();
        out.readOnly = readOnly;
        out.noticeUtf8 = notice ? notice : "";
        g_thesaurusCached = out;
        return out;
    };
    if (IsFile(marker) && UsableThesaurusFile(canonical)) {
        return finish(LayoutKind::Migrated, canonical, false, "");
    }
    if (UsableThesaurusFile(canonical)) {
        return finish(LayoutKind::FreshInstall, canonical, false, "");
    }
    fs::path source;
    if (IsFile(legacyAris) && FileSize(legacyAris) > 0) source = legacyAris;
    else if (IsFile(legacyAuro) && FileSize(legacyAuro) > 0) source = legacyAuro;
    if (source.empty()) {
        return finish(LayoutKind::FreshInstall, canonical, false, "");
    }
    if (IsFile(canonical) && !UsableThesaurusFile(canonical)) {
        std::error_code ec;
        fs::remove(canonical, ec);
    }
    if (CopyOneFile(source, canonical)) {
        std::ofstream markerOut(marker, std::ios::binary | std::ios::trunc);
        if (markerOut) {
            markerOut << "harken-thesaurus-migrated" << char(10);
        }
        if (UsableThesaurusFile(canonical)) {
            return finish(LayoutKind::Migrated, canonical, false, "copied-old-thesaurus");
        }
    }
    return finish(LayoutKind::FallbackLegacy, source, true, "fallback-old-thesaurus");
}


std::wstring LegacyAuroDbFilePath() {
    const Roots roots = ResolveRoots();
    if (roots.localAppData.empty()) return {};
    return roots.localAppData + L"\\Auro\\auro.db";
}

std::vector<std::string> ProtectedDataRootsUtf8() {
    const Roots roots = ResolveRoots();
    std::vector<std::string> out;
    const std::wstring database = CanonicalDatabaseDir(roots);
    if (!database.empty()) out.push_back(WideToUtf8Path(database));
    if (!roots.roamingAppData.empty()) {
        out.push_back(WideToUtf8Path(roots.roamingAppData + L"\\AuroAudio"));
        out.push_back(WideToUtf8Path(roots.roamingAppData + L"\\ArisAudio"));
    }
    if (!roots.localAppData.empty()) {
        out.push_back(WideToUtf8Path(roots.localAppData + L"\\Auro\\Database"));
        out.push_back(WideToUtf8Path(roots.localAppData + L"\\AuroAudio"));
    }
    return out;
}

}  // namespace Platform::HarkenUserData
