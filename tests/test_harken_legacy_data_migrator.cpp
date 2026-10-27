#include "platform/harken_user_data_layout.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;
using Platform::HarkenUserData::EnsureLayout;
using Platform::HarkenUserData::ResetLayoutCacheForTests;
using Platform::HarkenUserData::ResetThesaurusCacheForTests;
using Platform::HarkenUserData::LayoutKind;

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

fs::path MakeTempRoot(const wchar_t* name) {
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    fs::path root = fs::path(tmp) / (L"harken-bug07-tests-" + std::to_wstring(GetCurrentProcessId())) / name;
    std::error_code ec;
    fs::create_directories(root / L"Local", ec);
    fs::create_directories(root / L"Roaming", ec);
    return root;
}

void SetRoots(const fs::path& root) {
    _wputenv_s(L"LOCALAPPDATA", (root / L"Local").c_str());
    _wputenv_s(L"APPDATA", (root / L"Roaming").c_str());
    ResetLayoutCacheForTests();
}

void WriteFile(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void TestFresh() {
    const fs::path root = MakeTempRoot(L"harken-rq06-fresh");
    SetRoots(root);
    const auto layout = EnsureLayout();
    check(layout.kind == LayoutKind::FreshInstall, "fresh install kind");
    check(!layout.readOnly, "fresh is writable");
    check(layout.noticeUtf8.empty(), "fresh has no notice");
    const std::string canon = Platform::HarkenUserData::WideToUtf8Path(layout.canonicalRoot);
    check(canon.find("HarkenSearch") != std::string::npos, "canonical root is HarkenSearch");
    check(!fs::exists(root / L"Local" / L"Auro"), "fresh does not create old Auro dir");
    check(!fs::exists(root / L"Roaming" / L"ArisAudio"), "fresh does not create old ArisAudio dir");
    check(!fs::exists(root / L"Local" / L"HarkenSearch" / L"migration.marker"), "fresh has no marker");
}

void TestMigrate() {
    const fs::path root = MakeTempRoot(L"harken-rq06-migrate");
    const fs::path local = root / L"Local";
    const fs::path roaming = root / L"Roaming";
    WriteFile(local / L"Auro" / L"Database" / L"Auro.db", "db-bytes-123456");
    WriteFile(local / L"Auro" / L"Database" / L"Auro.db-wal", "db-wal-bytes");
    WriteFile(local / L"Auro" / L"Database" / L"Auro.db-shm", "db-shm-bytes");
    WriteFile(roaming / L"AuroAudio" / L"settings.ini", "theme=dark");
    WriteFile(roaming / L"ArisAudio" / L"avatar.png", "png-bytes");
    WriteFile(local / L"Aris" / L"updates" / L"journal.txt", "journal");
    SetRoots(root);
    const auto layout = EnsureLayout();
    check(layout.kind == LayoutKind::Migrated, "migrated kind");
    check(!layout.readOnly, "migrated is writable");
    check(!layout.noticeUtf8.empty(), "migrated notice is explicit");
    const fs::path destDb = local / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    const fs::path destSettings = local / L"HarkenSearch" / L"settings.ini";
    const fs::path destAvatar = local / L"HarkenSearch" / L"avatar.png";
    const fs::path destJournal = local / L"HarkenSearch" / L"updates" / L"journal.txt";
    check(ReadFile(destDb) == "db-bytes-123456", "database copied");
    check(ReadFile(fs::path(destDb.native() + L"-wal")) == "db-wal-bytes", "database WAL copied to package");
    check(ReadFile(fs::path(destDb.native() + L"-shm")) == "db-shm-bytes", "database SHM copied to package");
    check(ReadFile(destSettings) == "theme=dark", "settings copied");
    check(ReadFile(destAvatar) == "png-bytes", "avatar copied");
    check(ReadFile(destJournal) == "journal", "updates copied");
    check(ReadFile(local / L"Auro" / L"Database" / L"Auro.db") == "db-bytes-123456", "old database kept");
    check(ReadFile(roaming / L"AuroAudio" / L"settings.ini") == "theme=dark", "old settings kept");
    check(fs::exists(local / L"HarkenSearch" / L"migration.marker"), "marker written");
}

void TestSingleFileDatabasePackageAndSidecars() {
    const fs::path root = MakeTempRoot(L"harken-bug07-db-package");
    const fs::path oldDb = root / L"Local" / L"Auro" / L"auro.db";
    WriteFile(oldDb, "main-db-bytes");
    WriteFile(fs::path(oldDb.native() + L"-wal"), "wal-bytes");
    WriteFile(fs::path(oldDb.native() + L"-shm"), "shm-bytes");
    SetRoots(root);
    const auto layout = EnsureLayout();
    const fs::path mainDb = root / L"Local" / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    check(layout.kind == LayoutKind::Migrated, "single-file database migrated");
    check(ReadFile(mainDb) == "main-db-bytes", "single-file database copied to package main.db");
    check(ReadFile(fs::path(mainDb.native() + L"-wal")) == "wal-bytes", "single-file WAL copied beside main.db");
    check(ReadFile(fs::path(mainDb.native() + L"-shm")) == "shm-bytes", "single-file SHM copied beside main.db");
}

void TestTruncatedCopyRetries() {
    const fs::path root = MakeTempRoot(L"harken-bug07-retry");
    const fs::path oldDb = root / L"Local" / L"Auro" / L"Database" / L"Auro" / L"main.db";
    const fs::path newDb = root / L"Local" / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    WriteFile(oldDb, "complete-main-db");
    WriteFile(newDb, "short");
    SetRoots(root);
    const auto layout = EnsureLayout();
    check(layout.kind == LayoutKind::Migrated, "truncated destination can retry migration");
    check(ReadFile(newDb) == "complete-main-db", "retry replaces truncated copy");
    check(fs::exists(root / L"Local" / L"HarkenSearch" / L"migration.marker"), "retry writes marker after complete copy");
}

void TestSameSizeCorruptCopyRetries() {
    const fs::path root = MakeTempRoot(L"harken-bug07-equal-size-retry");
    const fs::path oldDb = root / L"Local" / L"Auro" / L"Database" / L"Auro" / L"main.db";
    const fs::path newDb = root / L"Local" / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    const std::string original(65537, 'a');
    std::string corrupted = original;
    corrupted.back() = 'b';
    WriteFile(oldDb, original);
    WriteFile(newDb, corrupted);
    WriteFile(fs::path(oldDb.native() + L"-wal"), "correct");
    WriteFile(fs::path(newDb.native() + L"-wal"), "corrupt");
    SetRoots(root);
    check(EnsureLayout().kind == LayoutKind::Migrated, "equal-size copy can retry migration");
    check(ReadFile(newDb) == original, "retry replaces equal-size corrupt database");
    check(ReadFile(fs::path(newDb.native() + L"-wal")) == "correct", "retry replaces equal-size corrupt WAL");
    check(ReadFile(oldDb) == original, "equal-size retry keeps original database");
}

void TestRetryPrunesPackedSidecarsMissingAtSource() {
    const fs::path root = MakeTempRoot(L"harken-legacy-sidecar-packed-retry");
    const fs::path source = root / L"Local" / L"Auro" / L"Database" / L"Auro" / L"main.db";
    const fs::path target = root / L"Local" / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    WriteFile(source, "packed-current");
    WriteFile(target, "packed-stale");
    WriteFile(fs::path(target.native() + L"-wal"), "old-wal");
    WriteFile(fs::path(target.native() + L"-shm"), "old-shm");
    SetRoots(root);
    const auto result = EnsureLayout();
    check(result.kind == LayoutKind::Migrated, "packed retry migrates");
    check(ReadFile(target) == "packed-current", "packed retry copies current database");
    check(!fs::exists(fs::path(target.native() + L"-wal")), "packed retry prunes absent WAL");
    check(!fs::exists(fs::path(target.native() + L"-shm")), "packed retry prunes absent SHM");
    check(ReadFile(source) == "packed-current", "packed retry leaves source database unchanged");
}

void TestRetrySyncsFlatSidecarsIndividually() {
    const fs::path root = MakeTempRoot(L"harken-legacy-sidecar-flat-retry");
    const fs::path source = root / L"Local" / L"Auro" / L"auro.db";
    const fs::path target = root / L"Local" / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    WriteFile(source, "flat-current");
    WriteFile(fs::path(source.native() + L"-wal"), "live-wal");
    WriteFile(target, "flat-stale");
    WriteFile(fs::path(target.native() + L"-wal"), "old-wal");
    WriteFile(fs::path(target.native() + L"-shm"), "old-shm");
    SetRoots(root);
    const auto result = EnsureLayout();
    check(result.kind == LayoutKind::Migrated, "flat retry migrates");
    check(ReadFile(target) == "flat-current", "flat retry copies current database");
    check(ReadFile(fs::path(target.native() + L"-wal")) == "live-wal", "flat retry copies live WAL");
    check(!fs::exists(fs::path(target.native() + L"-shm")), "flat retry prunes absent SHM");
    check(ReadFile(source) == "flat-current", "flat retry leaves source database unchanged");
    check(ReadFile(fs::path(source.native() + L"-wal")) == "live-wal", "flat retry leaves source WAL unchanged");
}

void TestSidecarPruneFailureFallsBackWithoutChangingSource() {
    const fs::path root = MakeTempRoot(L"harken-legacy-sidecar-prune-failure");
    const fs::path source = root / L"Local" / L"Auro" / L"auro.db";
    const fs::path target = root / L"Local" / L"HarkenSearch" / L"Database" / L"Auro" / L"main.db";
    WriteFile(source, "flat-current");
    fs::create_directories(fs::path(target.native() + L"-wal"));
    SetRoots(root);
    const auto result = EnsureLayout();
    check(result.kind == LayoutKind::FallbackLegacy && result.readOnly, "unsafe sidecar target keeps read-only fallback");
    check(!fs::exists(root / L"Local" / L"HarkenSearch" / L"migration.marker"), "failed prune writes no marker");
    check(ReadFile(source) == "flat-current", "failed prune leaves source unchanged");
    check(fs::is_directory(fs::path(target.native() + L"-wal")), "failed prune does not remove unrelated directory");
}

void TestExistingMarkerRepairsOldDatabaseLocation() {
    const fs::path root = MakeTempRoot(L"harken-bug07-old-marker");
    const fs::path local = root / L"Local";
    const fs::path legacyDb = local / L"Auro" / L"auro.db";
    const fs::path canonical = local / L"HarkenSearch";
    WriteFile(legacyDb, "old-database");
    WriteFile(fs::path(legacyDb.native() + L"-wal"), "old-wal");
    WriteFile(canonical / L"Database" / L"Auro.db", "old-database");
    WriteFile(canonical / L"migration.marker", "harken-data-migrated\n");
    SetRoots(root);
    const auto layout = EnsureLayout();
    const fs::path mainDb = canonical / L"Database" / L"Auro" / L"main.db";
    check(layout.kind == LayoutKind::Migrated, "existing marker remains migrated after repair");
    check(ReadFile(mainDb) == "old-database", "existing marker repairs old database location");
    check(ReadFile(fs::path(mainDb.native() + L"-wal")) == "old-wal", "existing marker repairs WAL location");
    check(ReadFile(legacyDb) == "old-database", "existing marker repair keeps legacy source");
}

void TestMarkedDatabaseRepairRetriesAfterSidecarFailure() {
    const fs::path root = MakeTempRoot(L"harken-bug07-marker-sidecar-retry");
    const fs::path local = root / L"Local";
    const fs::path legacyDb = local / L"Auro" / L"auro.db";
    const fs::path canonical = local / L"HarkenSearch";
    const fs::path mainDb = canonical / L"Database" / L"Auro" / L"main.db";
    const fs::path wal = fs::path(mainDb.native() + L"-wal");
    WriteFile(legacyDb, "old-database");
    WriteFile(fs::path(legacyDb.native() + L"-wal"), "uncheckpointed-wal");
    WriteFile(canonical / L"migration.marker", "harken-data-migrated\n");
    fs::create_directories(wal); // A directory blocks publication of the WAL file.
    SetRoots(root);
    const auto first = EnsureLayout();
    check(first.kind == LayoutKind::FallbackLegacy, "blocked WAL keeps marker repair in fallback");
    check(!fs::exists(mainDb), "blocked WAL does not publish main database");
    check(ReadFile(legacyDb) == "old-database", "blocked WAL keeps legacy database intact");
    std::error_code ec;
    fs::remove(wal, ec);
    check(!ec, "fixture clears blocked WAL destination");
    ResetLayoutCacheForTests();
    const auto retry = EnsureLayout();
    check(retry.kind == LayoutKind::Migrated, "marker repair retries after WAL blockage clears");
    check(ReadFile(mainDb) == "old-database", "retry publishes main database");
    check(ReadFile(wal) == "uncheckpointed-wal", "retry publishes WAL before main database");
}

void TestFallback() {
    const fs::path root = MakeTempRoot(L"harken-rq06-fallback");
    const fs::path local = root / L"Local";
    const fs::path roaming = root / L"Roaming";
    WriteFile(local / L"Auro" / L"Database" / L"Auro.db", "old-db");
    WriteFile(roaming / L"AuroAudio" / L"settings.ini", "old-settings");
    WriteFile(local / L"HarkenSearch", "not-a-directory");
    SetRoots(root);
    const auto layout = EnsureLayout();
    check(layout.kind == LayoutKind::FallbackLegacy, "fallback kind");
    check(layout.readOnly, "fallback is read-only");
    check(!layout.noticeUtf8.empty(), "fallback notice is explicit");
    const std::string dbDir = Platform::HarkenUserData::WideToUtf8Path(layout.activeDatabaseDir);
    const std::string settingsDir = Platform::HarkenUserData::WideToUtf8Path(layout.activeSettingsDir);
    check(dbDir.find("Auro") != std::string::npos && dbDir.find("Database") != std::string::npos, "fallback db uses old directory");
    check(settingsDir.find("AuroAudio") == std::string::npos, "fallback settings avoids old directory");
    if (!layout.activeSettingsDir.empty()) {
        WriteFile(fs::path(layout.activeSettingsDir) / L"settings.ini", "new-settings");
    }
    check(ReadFile(local / L"Auro" / L"Database" / L"Auro.db") == "old-db", "fallback does not write old database");
    check(ReadFile(roaming / L"AuroAudio" / L"settings.ini") == "old-settings", "fallback does not write old settings");
    check(!fs::exists(local / L"HarkenSearch" / L"migration.marker"), "failed copy has no migration marker");
    std::error_code ec;
    fs::remove(local / L"HarkenSearch", ec);
    ResetLayoutCacheForTests();
    const auto retried = EnsureLayout();
    check(retried.kind == LayoutKind::Migrated, "migration retries after destination becomes writable");
    check(ReadFile(roaming / L"AuroAudio" / L"settings.ini") == "old-settings", "retry still preserves old settings");
}

void TestFlatDatabaseFallbackUsesExistingFile() {
    const std::wstring fixtureName =
        L"harken-bug07-flat-fallback-" + std::to_wstring(GetTickCount64());
    const fs::path root = MakeTempRoot(fixtureName.c_str());
    const fs::path local = root / L"Local";
    const fs::path oldDb = local / L"Auro" / L"auro.db";
    const fs::path oldDatabaseDir = local / L"Auro" / L"Database";
    WriteFile(oldDb, "flat-legacy-db");
    WriteFile(fs::path(oldDb.native() + L"-wal"), "flat-legacy-wal");
    fs::create_directories(oldDatabaseDir); // An empty old package directory must not hide the flat DB.
    WriteFile(local / L"HarkenSearch", "blocked-canonical-root");
    SetRoots(root);

    const auto layout = EnsureLayout();
    check(layout.kind == LayoutKind::FallbackLegacy, "flat legacy DB enters fallback");
    check(layout.readOnly, "flat legacy DB fallback is read-only");
    check(layout.activeDatabaseDir == oldDb.parent_path().wstring(),
          "fallback chooses the directory containing the real flat DB");
    check(layout.activeDatabaseFile == oldDb.wstring(),
          "fallback records the exact existing flat DB file");
    check(ReadFile(oldDb) == "flat-legacy-db", "fallback preserves the flat DB bytes");
    check(ReadFile(fs::path(oldDb.native() + L"-wal")) == "flat-legacy-wal",
          "fallback preserves the flat DB WAL bytes");
    check(!fs::exists(oldDb.parent_path() / L"Auro" / L"main.db"),
          "fallback does not invent a package DB below the old directory");
}

std::string NormSlash(std::string value) {
    for (char& ch : value) {
        if (ch == '\\') ch = '/';
    }
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

bool RootsOverlap(const std::string& left, const std::string& right) {
    const std::string a = NormSlash(left);
    const std::string b = NormSlash(right);
    auto contains = [](const std::string& root, const std::string& candidate) {
        return candidate == root ||
            (candidate.size() > root.size() &&
             candidate.compare(0, root.size(), root) == 0 &&
             candidate[root.size()] == '/');
    };
    return contains(a, b) || contains(b, a);
}

void TestProtectedRootsDoNotOverlapUpdates() {
    const fs::path root = MakeTempRoot(L"harken-rq06-protect");
    SetRoots(root);
    const auto protectedRoots = Platform::HarkenUserData::ProtectedDataRootsUtf8();
    check(protectedRoots.size() >= 2, "at least two protected roots");
    const auto roots = Platform::HarkenUserData::ResolveRoots();
    const std::string updates = Platform::HarkenUserData::WideToUtf8Path(
        Platform::HarkenUserData::CanonicalUpdatesDir(roots));
    const std::string database = Platform::HarkenUserData::WideToUtf8Path(
        Platform::HarkenUserData::CanonicalDatabaseDir(roots));
    const std::string canon = Platform::HarkenUserData::WideToUtf8Path(
        Platform::HarkenUserData::CanonicalRoot(roots));
    bool sawDatabase = false;
    for (size_t i = 0; i < protectedRoots.size(); ++i) {
        if (NormSlash(protectedRoots[i]) == NormSlash(database)) sawDatabase = true;
        check(!RootsOverlap(protectedRoots[i], updates), "protected root must not overlap updates");
        check(NormSlash(protectedRoots[i]) != NormSlash(canon), "canonical root is not a protected root");
        for (size_t j = i + 1; j < protectedRoots.size(); ++j) {
            check(!RootsOverlap(protectedRoots[i], protectedRoots[j]), "protected roots must not overlap each other");
        }
    }
    check(sawDatabase, "protected roots include canonical database dir");
}

void TestThesaurusFresh() {
    const fs::path root = MakeTempRoot(L"harken-rq18-thes-fresh");
    const fs::path exe = root / L"exe";
    fs::create_directories(exe);
    ResetThesaurusCacheForTests();
    const auto file = Platform::HarkenUserData::EnsureThesaurusFileUtf8(
        Platform::HarkenUserData::WideToUtf8Path(exe.wstring()));
    check(file.kind == LayoutKind::FreshInstall, "thes fresh kind");
    check(!file.readOnly, "thes fresh writable");
    const std::string active = Platform::HarkenUserData::WideToUtf8Path(file.activePath);
    check(active.find("harken_thesaurus.json") != std::string::npos, "thes fresh uses new name");
    check(!fs::exists(exe / L"aris_thesaurus.json"), "thes fresh does not invent old file");
    check(!fs::exists(exe / L"auro_thesaurus.json"), "thes fresh does not invent older file");
    check(!fs::exists(exe / L"harken_thesaurus.migration.marker"), "thes fresh has no marker");
}

void TestThesaurusMigrate() {
    const fs::path root = MakeTempRoot(L"harken-rq18-thes-migrate");
    const fs::path exe = root / L"exe";
    WriteFile(exe / L"aris_thesaurus.json", "{\"triggers\":[\"old\"],\"related\":[\"thes\"]}");
    ResetThesaurusCacheForTests();
    const auto file = Platform::HarkenUserData::EnsureThesaurusFileUtf8(
        Platform::HarkenUserData::WideToUtf8Path(exe.wstring()));
    check(file.kind == LayoutKind::Migrated, "thes migrated kind");
    check(!file.readOnly, "thes migrated writable");
    check(ReadFile(exe / L"harken_thesaurus.json") == "{\"triggers\":[\"old\"],\"related\":[\"thes\"]}", "thes copied");
    check(ReadFile(exe / L"aris_thesaurus.json") == "{\"triggers\":[\"old\"],\"related\":[\"thes\"]}", "thes old kept");
    check(fs::exists(exe / L"harken_thesaurus.migration.marker"), "thes marker written");
}

void TestThesaurusEmptyCanonicalRecoversFromLegacy() {
    const fs::path root = MakeTempRoot(L"harken-rq18-thes-empty");
    const fs::path exe = root / L"exe";
    WriteFile(exe / L"aris_thesaurus.json", "{\"triggers\":[\"fire\"],\"related\":[\"Flame\"]}");
    WriteFile(exe / L"harken_thesaurus.json", "");
    ResetThesaurusCacheForTests();
    const auto file = Platform::HarkenUserData::EnsureThesaurusFileUtf8(
        Platform::HarkenUserData::WideToUtf8Path(exe.wstring()));
    check(file.kind == LayoutKind::Migrated, "empty canonical recovers from old thesaurus");
    check(!file.readOnly, "recovered thesaurus is writable");
    check(ReadFile(exe / L"harken_thesaurus.json").find("\"triggers\"") != std::string::npos,
          "empty canonical replaced with old thesaurus bytes");
}

void TestThesaurusFallback() {
    const fs::path root = MakeTempRoot(L"harken-rq18-thes-fallback");
    const fs::path exe = root / L"exe";
    WriteFile(exe / L"aris_thesaurus.json", "{\"triggers\":[\"keep\"],\"related\":[\"old\"]}");
    fs::create_directories(exe / L"harken_thesaurus.json");
    ResetThesaurusCacheForTests();
    const auto file = Platform::HarkenUserData::EnsureThesaurusFileUtf8(
        Platform::HarkenUserData::WideToUtf8Path(exe.wstring()));
    check(file.kind == LayoutKind::FallbackLegacy, "thes fallback kind");
    check(file.readOnly, "thes fallback read-only");
    const std::string active = Platform::HarkenUserData::WideToUtf8Path(file.activePath);
    check(active.find("aris_thesaurus.json") != std::string::npos, "thes fallback uses old file");
    check(ReadFile(exe / L"aris_thesaurus.json") == "{\"triggers\":[\"keep\"],\"related\":[\"old\"]}",
          "thes fallback does not write old file");
}

int main() {
    TestFresh();
    TestMigrate();
    TestSingleFileDatabasePackageAndSidecars();
    TestTruncatedCopyRetries();
    TestSameSizeCorruptCopyRetries();
    TestRetryPrunesPackedSidecarsMissingAtSource();
    TestRetrySyncsFlatSidecarsIndividually();
    TestSidecarPruneFailureFallsBackWithoutChangingSource();
    TestExistingMarkerRepairsOldDatabaseLocation();
    TestMarkedDatabaseRepairRetriesAfterSidecarFailure();
    TestFallback();
    TestFlatDatabaseFallbackUsesExistingFile();
    TestProtectedRootsDoNotOverlapUpdates();
    TestThesaurusFresh();
    TestThesaurusMigrate();
    TestThesaurusEmptyCanonicalRecoversFromLegacy();
    TestThesaurusFallback();
    if (g_failures == 0) std::printf("all harken data migration checks pass\n");
    return g_failures == 0 ? 0 : 1;
}
