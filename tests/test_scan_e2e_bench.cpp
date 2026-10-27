// End-to-end scan benchmark.
// Measures total wall time of Database::scanDirectoryEx on a real audio
// library (default H:\Sound effect\综合\SoundMorph音效合辑) using a fresh
// temp DB. Reports per-stage breakdown and overall throughput.
//
// Override scan root via env AURA_SCAN_BENCH_ROOT.
// Skip extraction (mini waveform) via env AURA_SCAN_BENCH_NO_EXTRACT=1.

#include "Database.h"
#include "Utils.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using clock = std::chrono::steady_clock;

    std::string scanRoot = "H:\\Sound effect\\综合\\SoundMorph音效合辑";
    if (const char* env = std::getenv("AURA_SCAN_BENCH_ROOT")) scanRoot = env;
    const bool enqueueExtraction =
        std::getenv("AURA_SCAN_BENCH_NO_EXTRACT") == nullptr;

    std::printf("=========================================================================\n");
    std::printf("  SoundMorph E2E Scan Benchmark\n");
    std::printf("  Scan root: %s\n", scanRoot.c_str());
    std::printf("  Mini waveform extraction (sync wait at scan end): %s\n",
                enqueueExtraction ? "ENABLED (slower; matches scanForeground)" : "DISABLED (matches BackfillQueue async path)");
    std::printf("=========================================================================\n\n");
    std::fflush(stdout);

    fs::path scanRootPath = Utf8ToPath(scanRoot);
    std::error_code ec;
    if (!fs::exists(scanRootPath, ec)) {
        std::cerr << "FAIL: scan root does not exist: " << scanRoot << "\n";
        return 1;
    }

    fs::path tmpDir = fs::temp_directory_path() / "auro_scan_e2e_bench";
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);
    const std::string dbPath = (tmpDir / "scan_e2e.db").string();

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: db.open: " << db.getLastError() << "\n";
        return 1;
    }
    std::printf("[setup] fresh DB at %s\n", dbPath.c_str());

    auto formatProgress = [](int cur, int total, const std::string& msg) {
        std::string clipped = msg;
        if (clipped.size() > 80) clipped = clipped.substr(0, 77) + "...";
        std::printf("[scan] %6d / %6d  %s\n", cur, total, clipped.c_str());
    };

    int lastReported = 0;
    auto progressStart = clock::now();
    auto callback = [&](int cur, int total, const std::string& msg,
                        bool /*indeterminate*/) {
        // Throttle: report every 500 files (in scan stage) OR every 5s elapsed
        const auto now = clock::now();
        const double elapsed = std::chrono::duration<double>(now - progressStart).count();
        const bool everyN = cur - lastReported >= 500;
        const bool everyT = std::chrono::duration<double>(now - progressStart).count() > 0
            && (int)elapsed % 5 == 0 && cur != lastReported;
        if (everyN || (cur >= total && cur != lastReported)) {
            const double rate = elapsed > 0 ? cur / elapsed : 0;
            std::string clipped = msg;
            if (clipped.size() > 60) clipped = clipped.substr(0, 57) + "...";
            std::printf("[scan] %6d / %6d  %5.1f f/s  %s\n",
                        cur, total, rate, clipped.c_str());
            std::fflush(stdout);
            lastReported = cur;
        }
        (void)everyT;
    };

    std::atomic<bool> cancelFlag{false};

    auto t0 = clock::now();
    auto stats = db.scanDirectoryEx(
        scanRoot,
        /*recursive=*/true,
        /*updateExisting=*/false,
        /*removeMissing=*/false,
        callback,
        &cancelFlag,
        /*workerThreads=*/0,        // 0 → auto-detect
        /*completeImport=*/true,
        enqueueExtraction,
        /*backfillQueue=*/nullptr,  // no async backfill (covers/main wave deferred)
        /*backfillDbPathSnapshot=*/dbPath,
        /*backfillDbGenerationSnapshot=*/0,
        /*scanForegroundSessionId=*/0);
    auto t1 = clock::now();

    const double elapsedSec = std::chrono::duration<double>(t1 - t0).count();
    const int processed = stats.added + stats.updated + stats.skipped;
    const double rate = elapsedSec > 0 ? processed / elapsedSec : 0;

    std::printf("\n=========================================================================\n");
    std::printf("  Scan completed in %.2f minutes (%.1f seconds)\n",
                elapsedSec / 60.0, elapsedSec);
    std::printf("=========================================================================\n");
    std::printf("  total processed:  %d\n", processed);
    std::printf("  added:            %d\n", stats.added);
    std::printf("  updated:          %d\n", stats.updated);
    std::printf("  skipped:          %d\n", stats.skipped);
    std::printf("  removed:          %d\n", stats.removed);
    std::printf("  errors:           %d\n", stats.errors);
    std::printf("  ingested:         %d\n", stats.ingested);
    std::printf("  ingest ms:        %lld\n", (long long)stats.ingestDurationMs);
    std::printf("  waveform pending: %d  cache reused: %d  detail skipped: %d\n",
                stats.waveformPendingCreated,
                stats.waveformCacheReused,
                stats.waveformLongDetailSkipped);
    std::printf("  --\n");
    std::printf("  rate:             %.1f files/s  (overall)\n", rate);
    std::printf("  per-file:         %.2f ms/file  (overall)\n",
                processed > 0 ? (elapsedSec * 1000.0) / processed : 0);
    std::printf("  --\n");

    db.close();
    fs::remove_all(tmpDir, ec);
    return 0;
}
