#include "Database.h"
#include "MetadataWriter.h"
#include "platform/filesystem.h"
#include "platform/filesystem_probe_path_test_hooks.h"
#include "Localization.h"

#if !defined(ARIS_PREVIEW_ISOLATION) || !ARIS_PREVIEW_ISOLATION
#error "Preview NAS tests require compile-time preview isolation"
#endif

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

constexpr const char* kFixtureRoot =
    "D:/Aris-QA/test-databases/RQ-20261010-NONDOCK-PREVIEW-nas";

static int g_fails = 0;

static bool Check(bool cond, const char* msg) {
    std::cout << (cond ? "PASS: " : "FAIL: ") << msg << std::endl;
    if (!cond) ++g_fails;
    return cond;
}

static void Touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    const char bytes[] = "RIFF....WAVEfmt ";
    out.write(bytes, sizeof(bytes) - 1);
}

static std::string FileFingerprint(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    unsigned long long h = 1469598103934665603ull;
    for (unsigned char ch : data) { h ^= ch; h *= 1099511628211ull; }
    return std::to_string(data.size()) + ":" + std::to_string(h);
}

static std::string SqlQuote(const std::string& value) {
    std::string out(1, char(39));
    for (char ch : value) {
        if (ch == 39) { out += char(39); out += char(39); }
        else out += ch;
    }
    out += char(39);
    return out;
}

static int CountAudio(Database& db) {
    const auto result = db.executeRawQuery("SELECT COUNT(*) FROM audio_files;");
    if (!result.success || result.rows.empty() || result.rows[0].empty()) return -1;
    try { return std::stoi(result.rows[0][0]); } catch (...) { return -1; }
}

static bool SeedFile(Database& db, const std::string& pathUtf8) {
    const fs::path p = fs::u8path(pathUtf8);
    const std::string sql = "INSERT INTO audio_files(file_path, file_name, search_data) VALUES(" +
        SqlQuote(pathUtf8) + ", " + SqlQuote(p.filename().u8string()) + ", " + SqlQuote("oldtoken") + ");";
    return db.executeRawQuery(sql.c_str()).success;
}

int main() {
    fs::create_directories(fs::u8path(kFixtureRoot));
    const fs::path dbDir = fs::u8path(kFixtureRoot) / "db";
    fs::create_directories(dbDir);
    const fs::path dbPath = dbDir / "nas.db";
    std::error_code ec;
    fs::remove(dbPath, ec);
    fs::remove(fs::path(dbPath.native() + L"-wal"), ec);
    fs::remove(fs::path(dbPath.native() + L"-shm"), ec);

    Database db;
    if (!Check(db.open(dbPath.u8string()), "open isolated local db")) {
        std::cout << db.getLastError() << std::endl;
        return 1;
    }

    Database unc;
    const bool uncOpen = unc.open(R"(\\nas\share\blocked.db)");
    Check(!uncOpen, "UNC db open rejected before sqlite");
    Check(unc.getLastError().find("Preview isolation") != std::string::npos,
          "UNC still hits isolation first");

    const fs::path netDb = dbDir / "forced-network.db";
    fs::remove(netDb, ec);
    Platform::FileSystem::Testing::NetworkPathForceScript force{};
    force.prefix = netDb.u8string();
    Platform::FileSystem::Testing::SetNetworkPathForceScript(&force);
    Database net;
    const bool netOpen = net.open(netDb.u8string());
    Check(!netOpen, "forced-network path inside isolation root rejected");
    Check(!fs::exists(netDb, ec), "rejected network db did not create sqlite file");
    Platform::FileSystem::Testing::SetNetworkPathForceScript(nullptr);

    const fs::path localWav = fs::u8path(kFixtureRoot) / "audio" / "local.wav";
    Touch(localWav);
    Check(SeedFile(db, localWav.u8string()), "seed local audio row");
    Check(CountAudio(db) == 1, "one indexed row");

    Platform::FileSystem::Testing::ProbePathScript unreachable{};
    unreachable.attributes = INVALID_FILE_ATTRIBUTES;
    unreachable.lastError = ERROR_BAD_NETPATH;
    Platform::FileSystem::Testing::SetProbePathScript(&unreachable);
    const int removedUnreachable = db.removeMissingFilesDatabaseWide(nullptr, nullptr);
    Check(removedUnreachable == 0, "unreachable scan does not delete index");
    Check(CountAudio(db) == 1, "index count unchanged after unreachable");
    Platform::FileSystem::Testing::SetProbePathScript(nullptr);

    fs::remove(localWav, ec);
    const int removedMissing = db.removeMissingFilesDatabaseWide(nullptr, nullptr);
    Check(removedMissing == 1, "local Missing still deletes");
    Check(CountAudio(db) == 0, "missing local row removed");

    Touch(localWav);
    Check(SeedFile(db, localWav.u8string()), "reseed local audio row");
    db.mergeLibraryRoots({R"(\\nas\share\lib)", localWav.parent_path().u8string()});
    Check(db.isNetworkLibraryRoot(R"(\\nas\share\lib)"), "network root registered");
    Check(!db.isNetworkLibraryRoot(localWav.parent_path().u8string()), "local root is not network");

    const auto recs = db.executeRawQuery("SELECT id, file_path FROM audio_files LIMIT 1;");
    Check(recs.success && !recs.rows.empty(), "have file id");
    const int64_t fileId = recs.success && !recs.rows.empty() ? std::stoll(recs.rows[0][0]) : 0;
    Platform::FileSystem::Testing::NetworkPathForceScript fileForce{};
    fileForce.prefix = localWav.u8string();
    Platform::FileSystem::Testing::SetNetworkPathForceScript(&fileForce);
    const std::string before = FileFingerprint(localWav);
    Check(!db.setFileTags(fileId, {"tag-a"}), "tags denied on network source");
    MetadataEditPatch patch;
    patch.setDescription("x");
    const auto meta = db.applyMetadataPatchBatch({fileId}, patch);
    Check(meta.failed > 0, "metadata batch denied on network source");
    Check(!MetadataWriter::writeAlbumArt(localWav.u8string(), std::vector<uint8_t>{1,2,3}, "image/jpeg"),
          "cover write denied on network source");
    Check(FileFingerprint(localWav) == before, "synthetic source fingerprint unchanged");
    Platform::FileSystem::Testing::SetNetworkPathForceScript(nullptr);

    Platform::FileSystem::Testing::ProbePathScript slow{};
    slow.attributes = INVALID_FILE_ATTRIBUTES;
    slow.lastError = ERROR_BAD_NETPATH;
    slow.delayMs = 20000;
    Platform::FileSystem::Testing::SetProbePathScript(&slow);
    std::atomic<bool> cancel{false};
    const auto t0 = std::chrono::steady_clock::now();
    const ScanStats timed = db.scanDirectoryEx(R"(\\nas\share\slow)", true, true, true, nullptr, &cancel);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    Check(elapsedMs < 12000, "network root probe timeout is bounded");
    Check(timed.unreachable > 0 || timed.cancelled, "timeout marked unreachable");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    Check(CountAudio(db) == 1, "late probe did not delete index");
    Platform::FileSystem::Testing::SetProbePathScript(nullptr);

    slow.delayMs = 20000;
    Platform::FileSystem::Testing::SetProbePathScript(&slow);
    std::atomic<bool> cancel2{false};
    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        cancel2.store(true);
    });
    const ScanStats cancelled = db.scanDirectoryEx(R"(\\nas\share\cancel)", true, true, true, nullptr, &cancel2);
    canceller.join();
    Check(cancelled.cancelled || cancelled.unreachable > 0, "cancel during probe ends bounded");
    Platform::FileSystem::Testing::SetProbePathScript(nullptr);


    {
        Platform::FileSystem::Testing::ProbePathScript unreachableKey{};
        unreachableKey.attributes = INVALID_FILE_ATTRIBUTES;
        unreachableKey.lastError = ERROR_BAD_NETPATH;
        Platform::FileSystem::Testing::SetProbePathScript(&unreachableKey);

        const AppLanguage savedLang = g_currentLanguage;
        std::vector<std::string> callbackZh;
        std::vector<std::string> callbackEn;
        g_currentLanguage = AppLanguage::Chinese;
        const ScanStats zhStats = db.scanDirectoryEx(
            R"(\\nas\share\r2-offline)",
            true,
            true,
            true,
            [&](int, int, const std::string& file, bool) { callbackZh.push_back(file); },
            nullptr);
        g_currentLanguage = AppLanguage::English;
        const ScanStats enStats = db.scanDirectoryEx(
            R"(\\nas\share\r2-offline)",
            true,
            true,
            true,
            [&](int, int, const std::string& file, bool) { callbackEn.push_back(file); },
            nullptr);
        g_currentLanguage = savedLang;
        Platform::FileSystem::Testing::SetProbePathScript(nullptr);

        const std::string key = "Scan_NetworkLibraryUnreachable";
        Check(zhStats.unreachable > 0, "unreachable scan counted");
        Check(!zhStats.errorMessages.empty(), "unreachable recorded an error");
        Check(!zhStats.errorMessages.empty() && zhStats.errorMessages.front() == key,
              "worker error is stable ASCII key not L()");
        Check(zhStats.errorMessages == enStats.errorMessages,
              "worker errors are language-independent");
        Check(!callbackZh.empty() && callbackZh == callbackEn,
              "callback payload is language-independent");
        Check(!callbackZh.empty() && callbackZh.front() == key,
              "callback carries stable key not localized text");
        Check(std::string(L(Scan_NetworkLibraryUnreachable)) != key,
              "UI L() display string is not the ledger key");
    }

    db.close();
    std::cout << (g_fails == 0 ? "NAS_DB_ALL_PASS" : "NAS_DB_HAS_FAIL") << std::endl;
    return g_fails == 0 ? 0 : 1;
}
