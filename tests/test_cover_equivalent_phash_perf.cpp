#include "Database.h"
#include "sqlite3.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const std::string& label) {
    if (cond) {
        std::cout << "PASS: " << label << "\n";
        ++g_pass;
    } else {
        std::cout << "FAIL: " << label << "\n";
        ++g_fail;
    }
}

static AudioRecord makeRec(const std::string& name, const std::string& coverKey) {
    AudioRecord r;
    r.fileName = name;
    r.displayName = name;
    r.filePath = "e:\\cover_phash_perf\\" + name;
    r.folderPath = "e:\\cover_phash_perf";
    r.extension = ".wav";
    r.format = "WAV";
    r.durationSeconds = 1.0f;
    r.sampleRate = 48000;
    r.channels = 2;
    r.fileSize = 1024;
    r.totalFrames = 48000;
    r.dateAdded = 1;
    r.fileMtime = 1;
    r.hasCover = true;
    r.embeddedCoverKey = coverKey;
    r.embeddedCoverKeyMtime = 1;
    return r;
}

static bool execSql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "sqlite exec failed: " << (err ? err : sqlite3_errmsg(db)) << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

static bool fillCoverRegistryNoise(const std::string& dbPath,
                                   const std::vector<std::pair<std::string, int64_t>>& realKeys,
                                   int noiseRows) {
    sqlite3* raw = nullptr;
    if (sqlite3_open(dbPath.c_str(), &raw) != SQLITE_OK) {
        std::cerr << "sqlite open failed: " << sqlite3_errmsg(raw) << "\n";
        return false;
    }

    bool ok = execSql(raw, "BEGIN IMMEDIATE;");
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO cover_cache_registry "
        "(cover_key, rep_path, is_embedded, normalized_key, cover_phash, cover_phash_version) "
        "VALUES (?, ?, 1, ?, ?, 2);";
    ok = ok && sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr) == SQLITE_OK;
    auto insertOne = [&](const std::string& key, const std::string& repPath, int64_t phash) {
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, repPath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, phash);
        return sqlite3_step(stmt) == SQLITE_DONE;
    };

    if (ok) {
        for (const auto& [key, phash] : realKeys) {
            ok = ok && insertOne(key, "e:\\cover_phash_perf\\" + key + ".wav", phash);
        }
    }
    for (int i = 0; ok && i < noiseRows; ++i) {
        const std::string key = "E|noise_" + std::to_string(i);
        const int64_t phash = static_cast<int64_t>(0x7000000000000000ull + static_cast<uint64_t>(i));
        ok = insertOne(key, "e:\\cover_phash_perf\\noise_" + std::to_string(i) + ".wav", phash);
    }

    if (stmt) sqlite3_finalize(stmt);
    ok = ok && execSql(raw, "COMMIT;");
    sqlite3_close(raw);
    return ok;
}

static bool hasFile(const std::vector<AudioRecord>& rows, const std::string& name) {
    for (const auto& row : rows) {
        if (row.fileName == name) return true;
    }
    return false;
}

int main() {
    fs::path tmpDir = fs::temp_directory_path() / "auro_cover_equivalent_phash_perf";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    const std::string dbPath = (tmpDir / "test.db").string();
    const std::string targetKey = "E|target_cover";
    const std::string nearKey = "E|near_cover";
    const std::string mediumKey = "E|medium_cover";
    const std::string farKey = "E|far_cover";
    const int64_t targetPhash = static_cast<int64_t>(0x123456789abcdef0ull);
    const int64_t nearPhash = static_cast<int64_t>(0x123456789abcdef1ull);
    const int64_t mediumPhash = static_cast<int64_t>(0x123456789abcdef3ull);
    const int64_t farPhash = static_cast<int64_t>(0x123456789abcdef7ull);

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: db.open failed: " << db.getLastError() << "\n";
        return 1;
    }

    expect(db.addFile(makeRec("target.wav", targetKey)), "fixture add target cover row");
    expect(db.addFile(makeRec("near.wav", nearKey)), "fixture add near-phash cover row");
    expect(db.addFile(makeRec("medium.wav", mediumKey)), "fixture add hamming-2 cover row");
    expect(db.addFile(makeRec("far.wav", farKey)), "fixture add far-phash cover row");
    expect(fillCoverRegistryNoise(dbPath,
                                  {{targetKey, targetPhash}, {nearKey, nearPhash}, {mediumKey, mediumPhash}, {farKey, farPhash}},
                                  80000),
           "fixture add large cover registry noise");

    SearchFilter filter;
    filter.specificCoverKey = targetKey;

    std::vector<AudioRecord> rows;
    const auto t0 = std::chrono::steady_clock::now();
    rows = db.searchFiles(filter, 20, 0);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    std::cout << "first cover-key query: " << elapsedMs << "ms, rows=" << rows.size() << "\n";
    expect(elapsedMs < 250, "first cover-key query stays under 250ms with large registry");
    expect(hasFile(rows, "target.wav"), "target cover row is returned");
    expect(hasFile(rows, "near.wav"), "hamming-distance-1 phash row is returned");
    expect(hasFile(rows, "medium.wav"), "hamming-distance-2 phash row is preserved");
    expect(!hasFile(rows, "far.wav"), "hamming-distance-3 phash row is excluded");

    db.close();
    fs::remove_all(tmpDir, ec);

    std::cout << "\n=== Summary: " << g_pass << " pass, " << g_fail << " fail ===\n";
    return g_fail == 0 ? 0 : 1;
}
