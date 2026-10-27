/**
 * test_scan_rename_qa.cpp — F-001 QA: batch rollback + rename-as-update
 *
 * Validates (using raw SQLite, mirroring Database patterns):
 *   1. Batch rollback: partial inserts within a failed transaction are undone
 *   2. No orphaned tags/waveform placeholders after rollback
 *   3. Rename uses UPDATE (preserves rowid), not DELETE+INSERT
 *   4. Cross-extension rename (.wav → .mp3) preserves user metadata
 *   5. Folder rename updates all child paths atomically
 */

#include "sqlite3.h"

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <filesystem>

namespace fs = std::filesystem;

static std::mutex g_printMutex;
static std::atomic<int> g_failed{0};
static int g_passed{0};

static void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  FAIL: " << msg << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        g_failed++; \
    } else { \
        std::cout << "  OK: " << msg << "\n"; \
        g_passed++; \
    } \
} while(0)

static bool execSql(sqlite3* db, const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

static std::string queryScalar(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    std::string result;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* txt = (const char*)sqlite3_column_text(stmt, 0);
            if (txt) result = txt;
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    return result;
}

static int64_t queryInt(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    int64_t result = -1;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int64(stmt, 0);
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    return result;
}

static std::string tempDbPath(const std::string& name) {
    auto tmp = fs::temp_directory_path() / ("aura_test_" + name + ".db");
    for (auto ext : {".db", ".db-wal", ".db-shm"})
        fs::remove(fs::path(tmp).replace_extension(ext));
    return tmp.string();
}

static void cleanupDb(const std::string& path) {
    for (auto ext : {".db", ".db-wal", ".db-shm"})
        fs::remove(fs::path(path).replace_extension(ext));
}

// Create the application schema for audio_files, file_tags, and waveform_cache.
static void createSchema(sqlite3* db) {
    execSql(db, "PRAGMA journal_mode = WAL;");
    execSql(db, "PRAGMA busy_timeout = 5000;");
    execSql(db, R"(
        CREATE TABLE audio_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT UNIQUE NOT NULL,
            file_name TEXT,
            extension TEXT,
            folder_path TEXT,
            duration REAL DEFAULT 0,
            sample_rate INTEGER DEFAULT 0,
            channels INTEGER DEFAULT 0,
            description TEXT DEFAULT '',
            category TEXT DEFAULT '',
            keywords TEXT DEFAULT '',
            rating INTEGER DEFAULT 0,
            play_count INTEGER DEFAULT 0
        );
    )");
    execSql(db, R"(
        CREATE TABLE file_tags (
            file_id INTEGER NOT NULL,
            dimension TEXT NOT NULL,
            tag TEXT NOT NULL,
            PRIMARY KEY (file_id, dimension, tag),
            FOREIGN KEY (file_id) REFERENCES audio_files(id) ON DELETE CASCADE
        );
    )");
    execSql(db, R"(
        CREATE TABLE waveform_cache (
            file_path TEXT PRIMARY KEY,
            status TEXT DEFAULT 'pending',
            cache_key TEXT DEFAULT ''
        );
    )");
}

// Simulate addFile UPSERT pattern
static bool addFile(sqlite3* db, const std::string& path, const std::string& name, const std::string& ext) {
    const char* sql = R"(
        INSERT INTO audio_files (file_path, file_name, extension, folder_path)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(file_path) DO UPDATE SET
            file_name = excluded.file_name,
            extension = excluded.extension,
            folder_path = excluded.folder_path;
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string folder = fs::path(path).parent_path().string();
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ext.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, folder.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 1: Batch rollback undoes ALL inserts in a failed batch
// ---------------------------------------------------------------------------
static void testBatchRollbackUndoesAll() {
    printSeparator("Test 1: Batch rollback undoes ALL inserts");

    auto path = tempDbPath("batch_rollback");
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    createSchema(db);

    // Pre-seed 5 files (committed, should survive rollback)
    execSql(db, "BEGIN;");
    for (int i = 0; i < 5; i++) {
        addFile(db, "/lib/seed_" + std::to_string(i) + ".wav",
                "seed_" + std::to_string(i) + ".wav", "wav");
    }
    execSql(db, "COMMIT;");

    auto beforeCount = queryInt(db, "SELECT count(*) FROM audio_files;");
    ASSERT_TRUE(beforeCount == 5, "5 seed files committed");

    // Simulate a batch of 200 where failure occurs at row 150
    execSql(db, "BEGIN;");
    bool batchOk = true;
    for (int i = 0; i < 200; i++) {
        bool ok = addFile(db, "/lib/batch_" + std::to_string(i) + ".wav",
                          "batch_" + std::to_string(i) + ".wav", "wav");
        if (i == 149) {
            // Simulate mid-batch failure (e.g. tags write failed)
            batchOk = false;
            break;
        }
    }

    if (!batchOk) {
        execSql(db, "ROLLBACK;");
    } else {
        execSql(db, "COMMIT;");
    }

    auto afterCount = queryInt(db, "SELECT count(*) FROM audio_files;");
    ASSERT_TRUE(afterCount == 5,
                "After rollback, only seed files remain (got: " + std::to_string(afterCount) + ")");

    // Verify no batch files leaked
    auto batchCount = queryInt(db,
        "SELECT count(*) FROM audio_files WHERE file_path LIKE '/lib/batch_%';");
    ASSERT_TRUE(batchCount == 0,
                "Zero batch files after rollback (got: " + std::to_string(batchCount) + ")");

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 2: Tags and waveform placeholders also rolled back
// ---------------------------------------------------------------------------
static void testRollbackIncludesTagsAndWaveform() {
    printSeparator("Test 2: Rollback undoes tags + waveform placeholders");

    auto path = tempDbPath("rollback_tags_wf");
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    createSchema(db);

    execSql(db, "BEGIN;");

    // Insert file
    addFile(db, "/lib/test.wav", "test.wav", "wav");
    auto fileId = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/test.wav';");

    // Insert tags for this file
    execSql(db, "INSERT INTO file_tags(file_id, dimension, tag) VALUES(" +
            std::to_string(fileId) + ", 'Category', 'Ambience');");
    execSql(db, "INSERT INTO file_tags(file_id, dimension, tag) VALUES(" +
            std::to_string(fileId) + ", 'Mood', 'Dark');");

    // Insert waveform placeholder
    execSql(db, "INSERT INTO waveform_cache(file_path, status) VALUES('/lib/test.wav', 'pending');");

    // Simulate failure → rollback
    execSql(db, "ROLLBACK;");

    auto fileCount = queryInt(db, "SELECT count(*) FROM audio_files;");
    auto tagCount = queryInt(db, "SELECT count(*) FROM file_tags;");
    auto wfCount = queryInt(db, "SELECT count(*) FROM waveform_cache;");

    ASSERT_TRUE(fileCount == 0, "No files after rollback (got: " + std::to_string(fileCount) + ")");
    ASSERT_TRUE(tagCount == 0, "No tags after rollback (got: " + std::to_string(tagCount) + ")");
    ASSERT_TRUE(wfCount == 0, "No waveform entries after rollback (got: " + std::to_string(wfCount) + ")");

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 3: Rename uses UPDATE, preserves rowid and user data
// ---------------------------------------------------------------------------
static void testRenamePreservesRowId() {
    printSeparator("Test 3: Rename uses UPDATE, preserves rowid + user data");

    auto path = tempDbPath("rename_update");
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    createSchema(db);

    // Insert file with user metadata
    addFile(db, "/lib/drums/kick_01.wav", "kick_01.wav", "wav");
    execSql(db, "UPDATE audio_files SET description='Deep kick drum', "
                "category='SFX', keywords='kick drum deep', rating=5, "
                "play_count=42 WHERE file_path='/lib/drums/kick_01.wav';");

    auto originalId = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/drums/kick_01.wav';");
    ASSERT_TRUE(originalId > 0, "Original file has valid id: " + std::to_string(originalId));

    // Add tags for this file
    execSql(db, "INSERT INTO file_tags(file_id, dimension, tag) VALUES(" +
            std::to_string(originalId) + ", 'Category', 'SFX');");

    // Simulate rename (mirrors Database::updateFilePath pattern)
    const char* renameSql = R"(
        UPDATE audio_files SET
            file_path = '/lib/drums/kick_deep_01.wav',
            file_name = 'kick_deep_01.wav',
            folder_path = '/lib/drums'
        WHERE file_path = '/lib/drums/kick_01.wav';
    )";
    ASSERT_TRUE(execSql(db, renameSql), "UPDATE rename succeeds");

    // Verify same rowid
    auto newId = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/drums/kick_deep_01.wav';");
    ASSERT_TRUE(newId == originalId,
                "Rowid preserved: " + std::to_string(originalId) + " == " + std::to_string(newId));

    // Verify user data preserved
    auto desc = queryScalar(db, "SELECT description FROM audio_files WHERE id=" + std::to_string(newId) + ";");
    auto rating = queryInt(db, "SELECT rating FROM audio_files WHERE id=" + std::to_string(newId) + ";");
    auto plays = queryInt(db, "SELECT play_count FROM audio_files WHERE id=" + std::to_string(newId) + ";");

    ASSERT_TRUE(desc == "Deep kick drum", "description preserved: " + desc);
    ASSERT_TRUE(rating == 5, "rating preserved: " + std::to_string(rating));
    ASSERT_TRUE(plays == 42, "play_count preserved: " + std::to_string(plays));

    // Verify tags still reference same file_id
    auto tagCount = queryInt(db, "SELECT count(*) FROM file_tags WHERE file_id=" + std::to_string(newId) + ";");
    ASSERT_TRUE(tagCount == 1, "Tags still attached to same file_id");

    // Verify old path no longer exists
    auto oldCount = queryInt(db, "SELECT count(*) FROM audio_files WHERE file_path='/lib/drums/kick_01.wav';");
    ASSERT_TRUE(oldCount == 0, "Old path removed");

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 4: Cross-extension rename (.wav → .mp3) preserves metadata
// ---------------------------------------------------------------------------
static void testCrossExtensionRename() {
    printSeparator("Test 4: Cross-extension rename preserves metadata");

    auto path = tempDbPath("cross_ext");
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    createSchema(db);

    addFile(db, "/lib/vocal.wav", "vocal.wav", "wav");
    execSql(db, "UPDATE audio_files SET description='Vocal take', keywords='vocal singer' "
                "WHERE file_path='/lib/vocal.wav';");

    auto origId = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/vocal.wav';");

    // Cross-extension rename: .wav → .mp3
    execSql(db, R"(
        UPDATE audio_files SET
            file_path = '/lib/vocal.mp3',
            file_name = 'vocal.mp3',
            extension = 'mp3'
        WHERE file_path = '/lib/vocal.wav';
    )");

    auto newId = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/vocal.mp3';");
    ASSERT_TRUE(newId == origId, "Rowid preserved across extension change");

    auto desc = queryScalar(db, "SELECT description FROM audio_files WHERE id=" + std::to_string(newId) + ";");
    ASSERT_TRUE(desc == "Vocal take", "description preserved across extension change");

    auto ext = queryScalar(db, "SELECT extension FROM audio_files WHERE id=" + std::to_string(newId) + ";");
    ASSERT_TRUE(ext == "mp3", "extension updated to mp3");

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 5: Folder rename updates all child paths atomically
// ---------------------------------------------------------------------------
static void testFolderRenameUpdatesChildren() {
    printSeparator("Test 5: Folder rename updates all child paths atomically");

    auto path = tempDbPath("folder_rename");
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    createSchema(db);

    // Insert 10 files in /lib/OldFolder/
    execSql(db, "BEGIN;");
    for (int i = 0; i < 10; i++) {
        addFile(db, "/lib/OldFolder/sound_" + std::to_string(i) + ".wav",
                "sound_" + std::to_string(i) + ".wav", "wav");
        execSql(db, "UPDATE audio_files SET rating=" + std::to_string(i) +
                    " WHERE file_path='/lib/OldFolder/sound_" + std::to_string(i) + ".wav';");
    }
    execSql(db, "COMMIT;");

    // Capture original IDs
    std::vector<int64_t> origIds;
    for (int i = 0; i < 10; i++) {
        auto id = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/OldFolder/sound_" +
                           std::to_string(i) + ".wav';");
        origIds.push_back(id);
    }

    // Simulate folder rename: UPDATE all paths atomically (mirrors Database batch rename)
    execSql(db, "BEGIN;");
    bool allOk = true;
    for (int i = 0; i < 10; i++) {
        std::string oldPath = "/lib/OldFolder/sound_" + std::to_string(i) + ".wav";
        std::string newPath = "/lib/NewFolder/sound_" + std::to_string(i) + ".wav";
        std::string sql = "UPDATE audio_files SET file_path='" + newPath +
                          "', folder_path='/lib/NewFolder' WHERE file_path='" + oldPath + "';";
        if (!execSql(db, sql)) { allOk = false; break; }
    }

    if (allOk) {
        execSql(db, "COMMIT;");
    } else {
        execSql(db, "ROLLBACK;");
    }

    ASSERT_TRUE(allOk, "All folder rename UPDATEs succeeded");

    // Verify all paths updated
    auto oldCount = queryInt(db, "SELECT count(*) FROM audio_files WHERE folder_path='/lib/OldFolder';");
    auto newCount = queryInt(db, "SELECT count(*) FROM audio_files WHERE folder_path='/lib/NewFolder';");
    ASSERT_TRUE(oldCount == 0, "No files remain in OldFolder");
    ASSERT_TRUE(newCount == 10, "All 10 files in NewFolder");

    // Verify IDs preserved
    for (int i = 0; i < 10; i++) {
        auto newId = queryInt(db, "SELECT id FROM audio_files WHERE file_path='/lib/NewFolder/sound_" +
                             std::to_string(i) + ".wav';");
        ASSERT_TRUE(newId == origIds[i],
                    "File " + std::to_string(i) + " rowid preserved: " +
                    std::to_string(origIds[i]) + " == " + std::to_string(newId));
    }

    // Verify user metadata preserved
    for (int i = 0; i < 10; i++) {
        auto rating = queryInt(db, "SELECT rating FROM audio_files WHERE file_path='/lib/NewFolder/sound_" +
                              std::to_string(i) + ".wav';");
        ASSERT_TRUE(rating == i, "File " + std::to_string(i) + " rating preserved: " + std::to_string(rating));
    }

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 6: Folder rename partial failure → full rollback
// ---------------------------------------------------------------------------
static void testFolderRenamePartialFailureRollback() {
    printSeparator("Test 6: Folder rename partial failure → full rollback");

    auto path = tempDbPath("folder_rollback");
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    createSchema(db);

    // Insert 5 files
    for (int i = 0; i < 5; i++) {
        addFile(db, "/lib/Dir/f_" + std::to_string(i) + ".wav",
                "f_" + std::to_string(i) + ".wav", "wav");
    }

    auto beforeCount = queryInt(db, "SELECT count(*) FROM audio_files WHERE folder_path='/lib/Dir';");
    ASSERT_TRUE(beforeCount == 5, "5 files before rename attempt");

    // Simulate partial rename that fails at file 3
    execSql(db, "BEGIN;");
    bool batchOk = true;
    for (int i = 0; i < 5; i++) {
        if (i == 3) {
            batchOk = false;  // Simulate filesystem rename failure
            break;
        }
        std::string oldP = "/lib/Dir/f_" + std::to_string(i) + ".wav";
        std::string newP = "/lib/NewDir/f_" + std::to_string(i) + ".wav";
        execSql(db, "UPDATE audio_files SET file_path='" + newP +
                    "', folder_path='/lib/NewDir' WHERE file_path='" + oldP + "';");
    }

    if (!batchOk) {
        execSql(db, "ROLLBACK;");
    } else {
        execSql(db, "COMMIT;");
    }

    // All 5 files should still be at original paths
    auto oldDirCount = queryInt(db, "SELECT count(*) FROM audio_files WHERE folder_path='/lib/Dir';");
    auto newDirCount = queryInt(db, "SELECT count(*) FROM audio_files WHERE folder_path='/lib/NewDir';");
    ASSERT_TRUE(oldDirCount == 5,
                "All files remain in original dir after rollback (got: " + std::to_string(oldDirCount) + ")");
    ASSERT_TRUE(newDirCount == 0,
                "No files in new dir after rollback (got: " + std::to_string(newDirCount) + ")");

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
int main() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "  F-001 QA: Batch Rollback + Rename-as-Update Validation\n";
    std::cout << "============================================================\n";

    testBatchRollbackUndoesAll();
    testRollbackIncludesTagsAndWaveform();
    testRenamePreservesRowId();
    testCrossExtensionRename();
    testFolderRenameUpdatesChildren();
    testFolderRenamePartialFailureRollback();

    std::cout << "\n" << std::string(60, '=') << "\n";
    if (g_failed.load() == 0) {
        std::cout << "  ALL TESTS PASSED (" << g_passed << " assertions)\n";
    } else {
        std::cout << "  FAILED: " << g_failed.load() << " assertion(s), "
                  << g_passed << " passed\n";
    }
    std::cout << std::string(60, '=') << "\n\n";

    return g_failed.load() == 0 ? 0 : 1;
}
