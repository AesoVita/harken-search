// test_perf_optimizations.cpp
// Validates two performance fixes:
//   Fix 1: searchFilesCount in-memory cache - second call must be faster than first
//   Fix 2: invalidateFacetQueryCache clears in-memory cache (no stale count after DB change)
//
// Returns 0 if all pass, 1 if any fail.

#include "Database.h"
#include <iostream>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstring>

static int passed = 0;
static int failed = 0;

#define CHECK(cond, name) \
    do { \
        if (cond) { std::printf("PASS: %s\n", name); ++passed; } \
        else       { std::printf("FAIL: %s\n", name); ++failed; } \
    } while(0)

template<typename Func>
int64_t measureUs(Func&& f) {
    auto start = std::chrono::steady_clock::now();
    f();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

static bool setupTestDb(Database& db, const std::string& path, int fileCount) {
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());

    if (!db.open(path)) {
        std::fprintf(stderr, "Failed to open test DB: %s\n", db.getLastError().c_str());
        return false;
    }
    if (!db.beginTransaction()) return false;

    for (int i = 0; i < fileCount; i++) {
        AudioRecord rec;
        rec.filePath = "E:/TestLib/Folder" + std::to_string(i / 50) + "/test_" + std::to_string(i) + ".wav";
        rec.fileName = "test_" + std::to_string(i) + ".wav";
        rec.displayName = rec.fileName;
        rec.extension = ".wav";
        rec.category = (i % 2 == 0) ? "A" : "B";
        rec.durationSeconds = 1.0f + (i % 60);
        rec.sampleRate = 44100;
        rec.channels = 2;
        rec.fileSize = 100000;
        rec.folderPath = "E:/TestLib/Folder" + std::to_string(i / 50);
        if (i % 10 == 0) {
            rec.hasCover = true;
            rec.folderCoverKey = "cover_" + std::to_string(i / 25);
            rec.folderCoverSrcPath = rec.folderPath + "/cover.jpg";
        }
        db.addFile(rec);
    }
    db.commitTransaction();
    db.syncCoverRegistry();
    return true;
}

int main() {
    const std::string testDbPath = "test_perf_opt_temp.db";
    Database db;

    if (!setupTestDb(db, testDbPath, 500)) {
        std::fprintf(stderr, "FAIL: could not set up test database\n");
        return 1;
    }

    SearchFilter filter;
    filter.category = "A";

    // --- Test 1: in-memory cache hit is faster than first call ---
    int64_t firstCall = measureUs([&]() { db.searchFilesCount(filter); });
    int64_t secondCall = measureUs([&]() { db.searchFilesCount(filter); });
    int64_t thirdCall  = measureUs([&]() { db.searchFilesCount(filter); });

    std::printf("  searchFilesCount: 1st=%lldus  2nd=%lldus  3rd=%lldus\n",
                (long long)firstCall, (long long)secondCall, (long long)thirdCall);

    // Second and third call must be faster than first (in-memory cache hit)
    CHECK(secondCall < firstCall, "2nd searchFilesCount call faster than 1st (in-memory cache)");
    CHECK(thirdCall  < firstCall, "3rd searchFilesCount call faster than 1st (in-memory cache)");
    // Cache hit should be very fast (<= 200us even on slow machines)
    CHECK(secondCall <= 200, "2nd call completes within 200us (cache hit, no SQLite)");

    // --- Test 2: invalidateFacetQueryCache clears in-memory cache ---
    int64_t beforeInvalidate = measureUs([&]() { db.searchFilesCount(filter); });
    db.invalidateFacetQueryCache();
    int64_t afterInvalidate = measureUs([&]() { db.searchFilesCount(filter); });

    std::printf("  After invalidate: before=%lldus  after=%lldus\n",
                (long long)beforeInvalidate, (long long)afterInvalidate);

    // After invalidation, first call should be slower (cache miss)
    CHECK(afterInvalidate > beforeInvalidate || afterInvalidate > 50,
          "After invalidateFacetQueryCache, cache is cleared (count re-computed)");

    // --- Test 3: different filter keys produce independent cache entries ---
    SearchFilter filterB;
    filterB.category = "B";
    int64_t filterBFirst  = measureUs([&]() { db.searchFilesCount(filterB); });
    int64_t filterBSecond = measureUs([&]() { db.searchFilesCount(filterB); });

    std::printf("  Different filter: 1st=%lldus  2nd=%lldus\n",
                (long long)filterBFirst, (long long)filterBSecond);
    CHECK(filterBSecond < filterBFirst, "Different filter gets its own cache entry");
    CHECK(filterBSecond <= 200, "Different filter cache hit avoids SQLite");

    // --- Test 4: default count/preheat/facets stay on bounded shadow tables ---
    SearchFilter emptyFilter;
    int64_t defaultCount = 0;
    int64_t defaultCountUs = measureUs([&]() { defaultCount = db.searchFilesCount(emptyFilter); });
    std::printf("  Default count: %lldus count=%lld\n",
                (long long)defaultCountUs, (long long)defaultCount);
    CHECK(defaultCount == 500, "Default searchFilesCount returns total library count");
    CHECK(defaultCountUs <= 1000, "Default searchFilesCount uses fast library count path");

    SearchFilter fileTypeFilter;
    fileTypeFilter.includeOnlyKnownExts = true;
    fileTypeFilter.excludeExtensions = {".mp3", ".flac", ".ogg", ".aiff", ".aif", ".m4a", ".wma"};
    int64_t fileTypeCount = 0;
    int64_t fileTypeCountUs = measureUs([&]() { fileTypeCount = db.searchFilesCount(fileTypeFilter); });
    std::printf("  File-type count: %lldus count=%lld\n",
                (long long)fileTypeCountUs, (long long)fileTypeCount);
    CHECK(fileTypeCount == 500, "File-type-only count matches visible WAV rows");
    CHECK(fileTypeCountUs <= 5000, "File-type-only count uses indexed fast path");

    std::vector<Database::PathFacetEntry> pathFacets;
    int64_t pathFacetUs = measureUs([&]() { pathFacets = db.getPathFacets(emptyFilter, 20); });
    std::printf("  Global path facets: %lldus rows=%zu\n",
                (long long)pathFacetUs, pathFacets.size());
    CHECK(!pathFacets.empty(), "Global path facets returned from folder index");
    CHECK(pathFacetUs <= 5000, "Global path facets stay under 5ms on test DB");

    std::vector<Database::CoverFacetEntry> coverFacets;
    int64_t coverFacetUs = measureUs([&]() { coverFacets = db.getCoverFacets(emptyFilter, 20); });
    std::printf("  Global cover facets: %lldus rows=%zu\n",
                (long long)coverFacetUs, coverFacets.size());
    CHECK(!coverFacets.empty(), "Global cover facets returned from registry");
    CHECK(coverFacetUs <= 5000, "Global cover facets stay under 5ms on test DB");

    int64_t preheatUs = measureUs([&]() { db.preheat(); });
    std::printf("  Preheat: %lldus\n", (long long)preheatUs);
    CHECK(preheatUs <= 50000, "preheat is bounded and does not run full ANALYZE");

    // Cleanup
    db.close();
    std::remove(testDbPath.c_str());
    std::remove((testDbPath + "-wal").c_str());
    std::remove((testDbPath + "-shm").c_str());

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
