// test_scan_perf_boom.cpp -- slice7-B perf bench
//
// Drops UI / QA driver / Auro.exe entirely. Calls Database::scanDirectoryEx
// against a real folder using the current slice7 strategy:
// completeImport=false + durable BackfillQueue pending rows + worker pool.
//
// Usage (Windows PowerShell):
//   .\build_dbg\Debug\TestScanPerfBoom.exe "H:\Sound effect\综合\BoomLibrary"
//
// First arg = path to scan. If omitted, falls back to BoomLibrary default.
// Writes a JSON report to .openclaw/reports/perf_scanboom_<ts>/perf_report.json.

#include "BackfillQueue.h"
#include "BackfillWorkerPool.h"
#include "Database.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(sz, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), sz, nullptr, nullptr);
    return out;
}
static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int sz = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(sz, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), sz);
    return out;
}
#endif

namespace fs = std::filesystem;

static double runScan(Database& db,
                      const std::string& path,
                      bool completeImport,
                      bool enqueueExtraction,
                      aura::BackfillQueue* backfillQueue,
                      const std::string& dbPath,
                      int64_t dbGeneration,
                      ScanStats& stats) {
    auto t0 = std::chrono::steady_clock::now();
    stats = db.scanDirectoryEx(
        path,
        /*recursive=*/true,
        /*updateExisting=*/false,
        /*removeMissing=*/false,
        /*callback=*/nullptr,
        /*cancelFlag=*/nullptr,
        /*workerThreads=*/0,
        /*completeImport=*/completeImport,
        /*enqueueExtraction=*/enqueueExtraction,
        backfillQueue,
        dbPath,
        dbGeneration);
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Pull argv from Windows wide-char API so non-ASCII paths survive the
    // shell handoff (regular argv is in the active code page, not UTF-8).
    int wargc = 0;
    wchar_t** wargv = ::CommandLineToArgvW(::GetCommandLineW(), &wargc);
    std::string scanRoot = (wargc >= 2)
        ? wideToUtf8(wargv[1])
        : wideToUtf8(L"H:\\Sound effect\\综合\\BoomLibrary");
    if (wargv) ::LocalFree(wargv);

    // Verify existence using the wide path (fs::exists on std::string treats
    // bytes as ACP on Windows; UTF-8 -> wide is the only safe check).
    if (!fs::exists(utf8ToWide(scanRoot))) {
        std::fprintf(stderr, "scan root does not exist: %s\n", scanRoot.c_str());
        return 1;
    }
#else
    std::string scanRoot = (argc >= 2) ? argv[1] : "H:\\Sound effect\\BoomLibrary";
    if (!fs::exists(scanRoot)) {
        std::fprintf(stderr, "scan root does not exist: %s\n", scanRoot.c_str());
        return 1;
    }
#endif

    // Timestamped report dir
    std::time_t now = std::time(nullptr);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_buf);
    fs::path reportDir = fs::path(".openclaw") / "reports" /
        (std::string("perf_scanboom_") + ts);
    fs::create_directories(reportDir);

    auto run = [&](bool completeImport,
                   bool enqueueExtraction,
                   bool startBackfillWorker,
                   const char* label) -> bool {
        fs::path dbPath = reportDir /
            (std::string(label) + "_main.db");
        if (fs::exists(dbPath)) fs::remove(dbPath);

        Database db;
        if (!db.open(dbPath.string())) {
            std::fprintf(stderr, "[%s] FAIL: db.open(%s) -> %s\n",
                label, dbPath.string().c_str(), db.getLastError().c_str());
            return false;
        }
        aura::BackfillQueue queue;
        const int64_t dbGeneration = 1;
        queue.onDbOpened(dbPath.string(), dbGeneration);
        if (startBackfillWorker) {
            aura::GlobalBackfillWorkerPool().start(
                dbPath.string(), dbGeneration, &queue, 1);
        }
        std::printf("[%s] start scan completeImport=%s\n",
            label, completeImport ? "true" : "false");
        std::fflush(stdout);

        ScanStats stats;
        double seconds = runScan(db,
                                 scanRoot,
                                 completeImport,
                                 enqueueExtraction,
                                 &queue,
                                 dbPath.string(),
                                 dbGeneration,
                                 stats);
        double fps = (seconds > 0) ? (double)stats.total / seconds : 0.0;
        const int pendingCover = db.countBackfillPendingKind(
            static_cast<int>(aura::BackfillJobKind::CoverResolveAndThumb));
        const int pendingWaveform = db.countBackfillPendingKind(
            static_cast<int>(aura::BackfillJobKind::WaveformL1Overview));
        aura::GlobalBackfillWorkerPool().stop();
        queue.shutdown();

        std::printf("[%s] DONE %.2fs total=%d added=%d updated=%d skipped=%d errors=%d pendingCover=%d pendingWaveform=%d -> %.1f files/sec\n",
            label, seconds, stats.total, stats.added, stats.updated,
            stats.skipped, stats.errors, pendingCover, pendingWaveform, fps);
        std::fflush(stdout);

        // Write per-run JSON
        std::ofstream out(reportDir / (std::string(label) + "_report.json"));
        out << "{\n"
            << "  \"label\": \"" << label << "\",\n"
            << "  \"complete_import\": " << (completeImport ? "true" : "false") << ",\n"
            << "  \"enqueue_extraction\": " << (enqueueExtraction ? "true" : "false") << ",\n"
            << "  \"worker_started\": " << (startBackfillWorker ? "true" : "false") << ",\n"
            << "  \"scan_root\": \"" << scanRoot << "\",\n"
            << "  \"db_path\": \"" << dbPath.string() << "\",\n"
            << "  \"elapsed_seconds\": " << seconds << ",\n"
            << "  \"files_total\": " << stats.total << ",\n"
            << "  \"files_added\": " << stats.added << ",\n"
            << "  \"files_updated\": " << stats.updated << ",\n"
            << "  \"files_skipped\": " << stats.skipped << ",\n"
            << "  \"files_errors\": " << stats.errors << ",\n"
            << "  \"pending_cover\": " << pendingCover << ",\n"
            << "  \"pending_waveform\": " << pendingWaveform << ",\n"
            << "  \"files_per_sec\": " << fps << "\n"
            << "}\n";
        out.close();
        return true;
    };

    std::printf("scan root  : %s\n", scanRoot.c_str());
    std::printf("report dir : %s\n\n", reportDir.string().c_str());

    // 2026-05-20: skipping baseline (completeImport=true) -- it generated an
    // 8.2GB WAL on a 57k file library and wasn't converging. The 680-file
    // run earlier already showed baseline=1367 fps / slice7-B=1996 fps, so
    // the ratio is established. This run is just the cold-cache slice7-B
    // number on a real-sized library.
    if (!run(false,
             false,
             true,
             "slice7_current_cover_pending_worker")) return 1;

    return 0;
}
