#include "Database.h"
#include "PathUtils.h"
#include "Utils.h"
#include "sqlite3.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int countMatchingRows(sqlite3* db, const std::string& folderPath)
{
    if (!db) return -1;
    const std::string target = HarkenSearch::CanonicalFolderPath(folderPath);
    const std::string likePattern = target + "\\%";
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT COUNT(*) "
        "FROM audio_files "
        "WHERE folder_path = ? COLLATE NOCASE "
        "   OR folder_path LIKE ? COLLATE NOCASE;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, likePattern.c_str(), -1, SQLITE_TRANSIENT);
    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::string> sampleMatchingFolders(sqlite3* db, const std::string& folderPath, int limit)
{
    std::vector<std::string> rows;
    if (!db) return rows;
    const std::string target = HarkenSearch::CanonicalFolderPath(folderPath);
    const std::string likePattern = target + "\\%";
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT folder_path "
        "FROM audio_files "
        "WHERE folder_path = ? COLLATE NOCASE "
        "   OR folder_path LIKE ? COLLATE NOCASE "
        "GROUP BY folder_path "
        "ORDER BY length(folder_path), folder_path "
        "LIMIT ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return rows;
    }
    sqlite3_bind_text(stmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, likePattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        rows.emplace_back(text ? reinterpret_cast<const char*>(text) : "");
    }
    sqlite3_finalize(stmt);
    return rows;
}

void printUsage(const std::string& exe)
{
    std::cerr << "Usage: " << exe << " <db_path> <folder_path> [--delete-disk]\n";
    std::cerr << "   or: " << exe << " <db_path> <folder_path> --probe-delete-id <id>\n";
    std::cerr << "   or: " << exe << " <db_path> <folder_path> --probe-raw-open16-delete-id <id>\n";
    std::cerr << "   or: " << exe << " <db_path> <folder_path> --probe-raw-open8-delete-id <id>\n";
    std::cerr << "   or: " << exe << " <db_path> <folder_path> --probe-raw-open8-drop-delete-triggers-delete-id <id>\n";
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc < 3) {
        printUsage(argc > 0 ? WideToUtf8(argv[0]) : "TestRemoveFolderCli");
        return 2;
    }

    const std::string dbPath = WideToUtf8(argv[1]);
    const std::string folderPath = WideToUtf8(argv[2]);
    const bool deleteDisk = (argc >= 4 && WideToUtf8(argv[3]) == "--delete-disk");
    const bool probeDeleteId = (argc >= 5 && WideToUtf8(argv[3]) == "--probe-delete-id");
    const bool probeRawOpen16DeleteId =
        (argc >= 5 && WideToUtf8(argv[3]) == "--probe-raw-open16-delete-id");
    const bool probeRawOpen8DeleteId =
        (argc >= 5 && WideToUtf8(argv[3]) == "--probe-raw-open8-delete-id");
    const bool probeRawOpen8DropDeleteTriggersDeleteId =
        (argc >= 5 && WideToUtf8(argv[3]) == "--probe-raw-open8-drop-delete-triggers-delete-id");
    const int64_t deleteId = probeDeleteId ? std::stoll(WideToUtf8(argv[4])) : 0;
    const int64_t rawOpen16DeleteId =
        probeRawOpen16DeleteId ? std::stoll(WideToUtf8(argv[4])) : 0;
    const int64_t rawOpen8DeleteId =
        probeRawOpen8DeleteId ? std::stoll(WideToUtf8(argv[4])) : 0;
    const int64_t rawOpen8DropDeleteTriggersDeleteId =
        probeRawOpen8DropDeleteTriggersDeleteId ? std::stoll(WideToUtf8(argv[4])) : 0;

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: open db: " << db.getLastError() << "\n";
        return 1;
    }

    const std::string canonical = HarkenSearch::CanonicalFolderPath(folderPath);
    const int beforeCount = countMatchingRows(db.db(), folderPath);
    const auto beforeFolders = sampleMatchingFolders(db.db(), folderPath, 8);

    std::cout << "db_path=" << dbPath << "\n";
    std::cout << "folder_path=" << folderPath << "\n";
    std::cout << "canonical_path=" << canonical << "\n";
    std::cout << "before_count=" << beforeCount << "\n";
    for (const auto& row : beforeFolders) {
        std::cout << "before_folder=" << row << "\n";
    }

    if (probeRawOpen16DeleteId) {
        std::cout << "probe_raw_open16_delete_id=" << rawOpen16DeleteId << "\n";
        db.close();
        sqlite3* rawDb = nullptr;
#ifdef _WIN32
        const std::wstring wideDbPath = Utf8ToWide(dbPath);
        const int openRc = sqlite3_open16(wideDbPath.c_str(), &rawDb);
#else
        const int openRc = sqlite3_open(dbPath.c_str(), &rawDb);
#endif
        if (openRc != SQLITE_OK || !rawDb) {
            std::cout << "probe_raw_open_failed="
                      << (rawDb ? sqlite3_errmsg(rawDb) : "open failed") << "\n";
            if (rawDb) sqlite3_close(rawDb);
            return 1;
        }
        char* errMsg = nullptr;
        if (sqlite3_exec(rawDb, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cout << "probe_raw_begin_failed=" << (errMsg ? errMsg : sqlite3_errmsg(rawDb)) << "\n";
            if (errMsg) sqlite3_free(errMsg);
            sqlite3_close(rawDb);
            return 1;
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(rawDb, "DELETE FROM audio_files WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "probe_raw_prepare_failed=" << sqlite3_errmsg(rawDb) << "\n";
            sqlite3_exec(rawDb, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(rawDb);
            return 1;
        }
        sqlite3_bind_int64(stmt, 1, rawOpen16DeleteId);
        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cout << "probe_raw_delete_failed=" << sqlite3_errmsg(rawDb) << "\n";
        } else {
            std::cout << "probe_raw_delete_ok=1\n";
        }
        sqlite3_finalize(stmt);
        sqlite3_exec(rawDb, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(rawDb);
        return 0;
    }

    if (probeRawOpen8DeleteId) {
        std::cout << "probe_raw_open8_delete_id=" << rawOpen8DeleteId << "\n";
        db.close();
        sqlite3* rawDb = nullptr;
        const int openRc = sqlite3_open(dbPath.c_str(), &rawDb);
        if (openRc != SQLITE_OK || !rawDb) {
            std::cout << "probe_raw_open8_failed="
                      << (rawDb ? sqlite3_errmsg(rawDb) : "open failed") << "\n";
            if (rawDb) sqlite3_close(rawDb);
            return 1;
        }
        char* errMsg = nullptr;
        if (sqlite3_exec(rawDb, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cout << "probe_raw_open8_begin_failed=" << (errMsg ? errMsg : sqlite3_errmsg(rawDb)) << "\n";
            if (errMsg) sqlite3_free(errMsg);
            sqlite3_close(rawDb);
            return 1;
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(rawDb, "DELETE FROM audio_files WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "probe_raw_open8_prepare_failed=" << sqlite3_errmsg(rawDb) << "\n";
            sqlite3_exec(rawDb, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(rawDb);
            return 1;
        }
        sqlite3_bind_int64(stmt, 1, rawOpen8DeleteId);
        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cout << "probe_raw_open8_delete_failed=" << sqlite3_errmsg(rawDb) << "\n";
        } else {
            std::cout << "probe_raw_open8_delete_ok=1\n";
        }
        sqlite3_finalize(stmt);
        sqlite3_exec(rawDb, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(rawDb);
        return 0;
    }

    if (probeRawOpen8DropDeleteTriggersDeleteId) {
        std::cout << "probe_raw_open8_drop_delete_triggers_delete_id="
                  << rawOpen8DropDeleteTriggersDeleteId << "\n";
        db.close();
        sqlite3* rawDb = nullptr;
        const int openRc = sqlite3_open(dbPath.c_str(), &rawDb);
        if (openRc != SQLITE_OK || !rawDb) {
            std::cout << "probe_raw_open8_drop_delete_triggers_open_failed="
                      << (rawDb ? sqlite3_errmsg(rawDb) : "open failed") << "\n";
            if (rawDb) sqlite3_close(rawDb);
            return 1;
        }
        char* errMsg = nullptr;
        const char* dropSql =
            "DROP TRIGGER IF EXISTS audio_files_ad;"
            "DROP TRIGGER IF EXISTS audio_files_ad_trigram;";
        if (sqlite3_exec(rawDb, dropSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cout << "probe_raw_open8_drop_delete_triggers_failed="
                      << (errMsg ? errMsg : sqlite3_errmsg(rawDb)) << "\n";
            if (errMsg) sqlite3_free(errMsg);
            sqlite3_close(rawDb);
            return 1;
        }
        if (sqlite3_exec(rawDb, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cout << "probe_raw_open8_drop_delete_triggers_begin_failed="
                      << (errMsg ? errMsg : sqlite3_errmsg(rawDb)) << "\n";
            if (errMsg) sqlite3_free(errMsg);
            sqlite3_close(rawDb);
            return 1;
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(rawDb, "DELETE FROM audio_files WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "probe_raw_open8_drop_delete_triggers_prepare_failed=" << sqlite3_errmsg(rawDb) << "\n";
            sqlite3_exec(rawDb, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(rawDb);
            return 1;
        }
        sqlite3_bind_int64(stmt, 1, rawOpen8DropDeleteTriggersDeleteId);
        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cout << "probe_raw_open8_drop_delete_triggers_delete_failed=" << sqlite3_errmsg(rawDb) << "\n";
        } else {
            std::cout << "probe_raw_open8_drop_delete_triggers_delete_ok=1\n";
        }
        sqlite3_finalize(stmt);
        sqlite3_exec(rawDb, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(rawDb);
        return 0;
    }

    if (probeDeleteId) {
        std::cout << "probe_delete_id=" << deleteId << "\n";
        if (!db.beginTransaction()) {
            std::cout << "probe_begin_failed=" << db.getLastError() << "\n";
            db.close();
            return 1;
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db.db(), "DELETE FROM audio_files WHERE id = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "probe_prepare_failed=" << sqlite3_errmsg(db.db()) << "\n";
            db.rollbackTransaction();
            db.close();
            return 1;
        }
        sqlite3_bind_int64(stmt, 1, deleteId);
        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cout << "probe_delete_failed=" << sqlite3_errmsg(db.db()) << "\n";
        } else {
            std::cout << "probe_delete_ok=1\n";
        }
        sqlite3_finalize(stmt);
        db.rollbackTransaction();
        db.close();
        return 0;
    }

    const int removed = db.removeFolderSubtreeFromDb(folderPath, deleteDisk);
    const std::string lastError = db.getLastError();
    const int afterCount = countMatchingRows(db.db(), folderPath);
    const auto afterFolders = sampleMatchingFolders(db.db(), folderPath, 8);

    std::cout << "removed=" << removed << "\n";
    std::cout << "after_count=" << afterCount << "\n";
    if (!lastError.empty()) {
        std::cout << "last_error=" << lastError << "\n";
    }
    for (const auto& row : afterFolders) {
        std::cout << "after_folder=" << row << "\n";
    }

    db.close();
    return 0;
}
