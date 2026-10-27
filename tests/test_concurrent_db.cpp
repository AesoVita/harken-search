/**
 * test_concurrent_db.cpp — F-001 concurrent DB access validation
 *
 * Tests WAL-mode concurrent access patterns using raw SQLite C API:
 *   1. WAL mode + busy_timeout PRAGMA verification
 *   2. Multi-thread: reader threads + writer thread
 *   3. Multi-connection: dual writers (simulating multi-process)
 *   4. Transaction contention resolved by busy_timeout
 *   5. Read-only connection during write transaction
 *
 * This test uses raw SQLite to avoid the heavy Database class dependency chain,
 * while validating the same WAL + busy_timeout behavior that Database::open() configures.
 */

#include "sqlite3.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <cstring>

namespace fs = std::filesystem;

static std::mutex g_printMutex;
static std::atomic<int> g_failed{0};

static void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        std::lock_guard<std::mutex> lk(g_printMutex); \
        std::cerr << "  FAIL: " << msg << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        g_failed++; \
    } else { \
        std::lock_guard<std::mutex> lk(g_printMutex); \
        std::cout << "  OK: " << msg << "\n"; \
    } \
} while(0)

// ---------------------------------------------------------------------------
// Helper: open a DB with the same PRAGMAs as Database::open()
// ---------------------------------------------------------------------------
static sqlite3* openWithHarkenSearchPragmas(const std::string& path, bool readOnly = false) {
    sqlite3* db = nullptr;
    int flags = readOnly
        ? (SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX)
        : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);

    int rc = sqlite3_open_v2(path.c_str(), &db, flags, nullptr);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return nullptr;
    }

    // Mirror Database::open() PRAGMAs
    sqlite3_exec(db, "PRAGMA busy_timeout = 5000;", nullptr, nullptr, nullptr);
    if (!readOnly) {
        sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
    }
    sqlite3_exec(db, "PRAGMA mmap_size = 268435456;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA cache_size = -64000;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA temp_store = MEMORY;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA read_uncommitted = true;", nullptr, nullptr, nullptr);

    return db;
}

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

// ---------------------------------------------------------------------------
// Helper: temp DB path with cleanup
// ---------------------------------------------------------------------------
static std::string tempDbPath(const std::string& name) {
    auto tmp = fs::temp_directory_path() / ("aura_test_" + name + ".db");
    for (auto ext : {".db", ".db-wal", ".db-shm"}) {
        fs::remove(fs::path(tmp).replace_extension(ext));
    }
    return tmp.string();
}

static void cleanupDb(const std::string& path) {
    for (auto ext : {".db", ".db-wal", ".db-shm"}) {
        fs::remove(fs::path(path).replace_extension(ext));
    }
}

// ---------------------------------------------------------------------------
// Test 1: WAL mode and busy_timeout are properly set
// ---------------------------------------------------------------------------
static void testWalAndBusyTimeout() {
    printSeparator("Test 1: WAL mode + busy_timeout after open");

    auto path = tempDbPath("wal_check");
    auto* db = openWithHarkenSearchPragmas(path);
    ASSERT_TRUE(db != nullptr, "open succeeds");

    auto mode = queryScalar(db, "PRAGMA journal_mode;");
    ASSERT_TRUE(mode == "wal", "journal_mode is WAL (got: " + mode + ")");

    auto timeout = queryScalar(db, "PRAGMA busy_timeout;");
    ASSERT_TRUE(timeout == "5000", "busy_timeout is 5000ms (got: " + timeout + ")");

    sqlite3_close(db);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 2: Concurrent reads while writing (multi-thread)
// ---------------------------------------------------------------------------
static void testConcurrentReadWrite() {
    printSeparator("Test 2: Concurrent reads during writes (multi-thread)");

    auto path = tempDbPath("concurrent_rw");
    auto* writerDb = openWithHarkenSearchPragmas(path);
    ASSERT_TRUE(writerDb != nullptr, "open writer DB");

    execSql(writerDb, "CREATE TABLE test_data(id INTEGER PRIMARY KEY, value TEXT);");

    const int NUM_WRITES = 200;
    const int NUM_READERS = 4;
    std::atomic<bool> writerDone{false};
    std::atomic<int> readSuccessCount{0};
    std::atomic<int> readErrorCount{0};

    // Writer thread
    auto writerFn = [&]() {
        execSql(writerDb, "BEGIN TRANSACTION;");
        for (int i = 0; i < NUM_WRITES; i++) {
            std::string sql = "INSERT INTO test_data(id, value) VALUES(" +
                              std::to_string(i) + ", 'row_" + std::to_string(i) + "');";
            execSql(writerDb, sql);
        }
        execSql(writerDb, "COMMIT;");
        writerDone.store(true);
    };

    // Reader threads: each opens its own read-only connection
    auto readerFn = [&](int id) {
        auto* rdb = openWithHarkenSearchPragmas(path, true);
        if (!rdb) { readErrorCount++; return; }

        int localReads = 0;
        while (!writerDone.load()) {
            auto count = queryScalar(rdb, "SELECT count(*) FROM test_data;");
            if (!count.empty()) {
                localReads++;
            } else {
                readErrorCount++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Final read
        auto count = queryScalar(rdb, "SELECT count(*) FROM test_data;");
        if (!count.empty()) localReads++;
        readSuccessCount += localReads;
        sqlite3_close(rdb);
    };

    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; i++)
        readers.emplace_back(readerFn, i);
    std::thread writer(writerFn);

    writer.join();
    for (auto& t : readers) t.join();

    ASSERT_TRUE(readErrorCount.load() == 0,
                "No read errors during concurrent write (errors: " +
                std::to_string(readErrorCount.load()) + ")");
    ASSERT_TRUE(readSuccessCount.load() > 0,
                "Readers completed reads: " + std::to_string(readSuccessCount.load()));

    auto finalCount = queryScalar(writerDb, "SELECT count(*) FROM test_data;");
    ASSERT_TRUE(finalCount == std::to_string(NUM_WRITES),
                "All " + std::to_string(NUM_WRITES) + " rows written (got: " + finalCount + ")");

    sqlite3_close(writerDb);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 3: Two writable connections (simulates multi-process)
// ---------------------------------------------------------------------------
static void testDualWriterConnections() {
    printSeparator("Test 3: Dual writer connections (multi-process simulation)");

    auto path = tempDbPath("dual_writer");
    auto* db1 = openWithHarkenSearchPragmas(path);
    auto* db2 = openWithHarkenSearchPragmas(path);
    ASSERT_TRUE(db1 && db2, "both connections open");

    execSql(db1, "CREATE TABLE shared_data(id INTEGER PRIMARY KEY, src TEXT);");

    const int ROWS_EACH = 50;
    std::atomic<int> errors{0};

    auto insertFn = [&](sqlite3* db, const std::string& src, int start) {
        for (int i = 0; i < ROWS_EACH; i++) {
            std::string sql = "INSERT OR IGNORE INTO shared_data(id, src) VALUES(" +
                              std::to_string(start + i) + ", '" + src + "');";
            if (!execSql(db, sql)) errors++;
        }
    };

    std::thread t1(insertFn, db1, "proc1", 0);
    std::thread t2(insertFn, db2, "proc2", ROWS_EACH);
    t1.join();
    t2.join();

    ASSERT_TRUE(errors.load() == 0,
                "No write errors from dual connections (errors: " +
                std::to_string(errors.load()) + ")");

    auto total = queryScalar(db1, "SELECT count(*) FROM shared_data;");
    ASSERT_TRUE(total == std::to_string(ROWS_EACH * 2),
                "All rows present: expected " + std::to_string(ROWS_EACH * 2) +
                ", got " + total);

    sqlite3_close(db1);
    sqlite3_close(db2);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 4: Transaction contention — busy_timeout resolves it
// ---------------------------------------------------------------------------
static void testTransactionContention() {
    printSeparator("Test 4: Transaction contention resolved by busy_timeout");

    auto path = tempDbPath("contention");
    auto* db1 = openWithHarkenSearchPragmas(path);
    auto* db2 = openWithHarkenSearchPragmas(path);
    ASSERT_TRUE(db1 && db2, "both connections open");

    execSql(db1, "CREATE TABLE ctest(id INTEGER PRIMARY KEY AUTOINCREMENT, v INTEGER);");

    std::atomic<bool> db1InTxn{false};
    std::atomic<bool> db2Success{false};

    // db1: long write transaction
    auto longWriter = [&]() {
        execSql(db1, "BEGIN IMMEDIATE;");
        for (int i = 0; i < 100; i++) {
            execSql(db1, "INSERT INTO ctest(v) VALUES(" + std::to_string(i) + ");");
        }
        db1InTxn.store(true);
        // Hold lock for 200ms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        execSql(db1, "COMMIT;");
    };

    // db2: tries to write while db1 holds lock — busy_timeout should wait
    auto contentionWriter = [&]() {
        while (!db1InTxn.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        auto start = std::chrono::steady_clock::now();
        bool ok = execSql(db2, "INSERT INTO ctest(v) VALUES(9999);");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        db2Success.store(ok);
        {
            std::lock_guard<std::mutex> lk(g_printMutex);
            std::cout << "  (db2 waited ~" << elapsed << "ms for lock)\n";
        }
    };

    std::thread t1(longWriter);
    std::thread t2(contentionWriter);
    t1.join();
    t2.join();

    ASSERT_TRUE(db2Success.load(),
                "db2 write succeeded after busy-wait (not SQLITE_BUSY)");

    auto total = queryScalar(db1, "SELECT count(*) FROM ctest;");
    ASSERT_TRUE(total == "101", "All rows present: expected 101, got " + total);

    sqlite3_close(db1);
    sqlite3_close(db2);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 5: Read-only connection reads during write transaction
// ---------------------------------------------------------------------------
static void testReadOnlyDuringWrite() {
    printSeparator("Test 5: Read-only connection during write transaction");

    auto path = tempDbPath("ro_during_write");
    auto* dbW = openWithHarkenSearchPragmas(path);
    ASSERT_TRUE(dbW != nullptr, "open writer");

    execSql(dbW, "CREATE TABLE ro_test(id INTEGER PRIMARY KEY);");
    execSql(dbW, "INSERT INTO ro_test(id) VALUES(1);");

    auto* dbR = openWithHarkenSearchPragmas(path, true);
    ASSERT_TRUE(dbR != nullptr, "open read-only");

    // Verify busy_timeout on read-only
    auto bt = queryScalar(dbR, "PRAGMA busy_timeout;");
    ASSERT_TRUE(bt == "5000", "read-only busy_timeout is 5000ms (got: " + bt + ")");

    // Start a write transaction, then read
    execSql(dbW, "BEGIN IMMEDIATE;");
    for (int i = 2; i <= 50; i++)
        execSql(dbW, "INSERT INTO ro_test(id) VALUES(" + std::to_string(i) + ");");

    // WAL allows readers even during write transaction
    auto count = queryScalar(dbR, "SELECT count(*) FROM ro_test;");
    ASSERT_TRUE(!count.empty(), "Read-only query succeeds during write txn (count: " + count + ")");

    execSql(dbW, "COMMIT;");

    // After commit, reader sees all rows
    auto finalCount = queryScalar(dbR, "SELECT count(*) FROM ro_test;");
    ASSERT_TRUE(finalCount == "50",
                "After commit, reader sees all 50 rows (got: " + finalCount + ")");

    sqlite3_close(dbR);
    sqlite3_close(dbW);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
// Test 6: Without busy_timeout, SQLITE_BUSY occurs immediately
// (Proves busy_timeout is necessary for safety)
// ---------------------------------------------------------------------------
static void testNoBusyTimeoutFails() {
    printSeparator("Test 6: Without busy_timeout, contention causes SQLITE_BUSY");

    auto path = tempDbPath("no_timeout");
    sqlite3* db1 = nullptr;
    sqlite3* db2 = nullptr;
    sqlite3_open_v2(path.c_str(), &db1,
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    sqlite3_open_v2(path.c_str(), &db2,
                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);

    // Enable WAL but NO busy_timeout
    sqlite3_exec(db1, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

    execSql(db1, "CREATE TABLE bt_test(v INTEGER);");

    // db1 holds an IMMEDIATE transaction
    execSql(db1, "BEGIN IMMEDIATE;");
    execSql(db1, "INSERT INTO bt_test(v) VALUES(1);");

    // db2 tries to write — should fail immediately with SQLITE_BUSY
    char* err = nullptr;
    int rc = sqlite3_exec(db2, "INSERT INTO bt_test(v) VALUES(2);", nullptr, nullptr, &err);
    bool gotBusy = (rc == SQLITE_BUSY);
    if (err) sqlite3_free(err);

    ASSERT_TRUE(gotBusy,
                "Without busy_timeout, concurrent write returns SQLITE_BUSY (rc=" +
                std::to_string(rc) + ")");

    execSql(db1, "COMMIT;");
    sqlite3_close(db1);
    sqlite3_close(db2);
    cleanupDb(path);
}

// ---------------------------------------------------------------------------
int main() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "  F-001: Concurrent DB Access Validation\n";
    std::cout << "  WAL mode + busy_timeout safety tests\n";
    std::cout << "  (raw SQLite C API, mirrors Database::open() PRAGMAs)\n";
    std::cout << "============================================================\n";

    testWalAndBusyTimeout();
    testConcurrentReadWrite();
    testDualWriterConnections();
    testTransactionContention();
    testReadOnlyDuringWrite();
    testNoBusyTimeoutFails();

    std::cout << "\n" << std::string(60, '=') << "\n";
    if (g_failed.load() == 0) {
        std::cout << "  ALL TESTS PASSED (" << 6 << " tests)\n";
    } else {
        std::cout << "  FAILED: " << g_failed.load() << " assertion(s)\n";
    }
    std::cout << std::string(60, '=') << "\n\n";

    return g_failed.load() == 0 ? 0 : 1;
}
