#include "BackfillQueue.h"
#include "BackfillWorkerPool.h"
#include "Database.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
namespace {
constexpr int kFiles = 100, kScanThreads = 2, kBackfillThreads = 2, kFrames = 800;
constexpr int kTerminalTimeoutSeconds = 30;
struct Result {
    int code = 1;
    int resolvedCoverFiles = 0;
    std::string status = "failed";
    std::string stage, reason, json;
};
void put32(std::ofstream& out, uint32_t value) { out.write(reinterpret_cast<const char*>(&value), 4); }
void put16(std::ofstream& out, uint16_t value) { out.write(reinterpret_cast<const char*>(&value), 2); }
bool writeWav(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    constexpr uint32_t dataBytes = kFrames * 2;
    constexpr uint32_t riffBytes = 36 + dataBytes;
    put32(out, 0x46464952); put32(out, riffBytes); put32(out, 0x45564157);
    put32(out, 0x20746d66); put32(out, 16); put16(out, 1); put16(out, 1);
    put32(out, 8000); put32(out, 16000); put16(out, 2); put16(out, 16);
    put32(out, 0x61746164); put32(out, dataBytes);
    std::vector<int16_t> silence(kFrames, 0);
    out.write(reinterpret_cast<const char*>(silence.data()), dataBytes);
    return out.good();
}
bool writeTinyCoverImage(const fs::path& path) {
    const unsigned char png[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0,0,0,0x0D,0x49,0x48,0x44,0x52,0,0,0,1,0,0,0,1,
        8,2,0,0,0,0x90,0x77,0x53,0xDE,0,0,0,0x0C,0x49,0x44,0x41,0x54,8,0xD7,0x63,0xF8,0xCF,0xC0,0,0,3,1,1,0,
        0x18,0xDD,0x8D,0xB0,0,0,0,0,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82};
    std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(png), sizeof(png));
    return out.good();
}
bool makeFixture(fs::path& root, std::string& reason) {
    try {
        const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
        root = fs::temp_directory_path() / ("aris_stage6_scan_terminal_" + std::to_string(stamp));
        const fs::path audioRoot = root / "cover_indexing";
        fs::create_directories(audioRoot);
        if (!writeTinyCoverImage(root / "cover.jpg")) { reason = "fixture cover creation failed"; return false; }
        for (int i = 0; i < kFiles; ++i) {
            const auto path = audioRoot / ("terminal_" + std::to_string(i) + ".wav");
            if (!writeWav(path)) { reason = "fixture file creation failed"; return false; }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "fixture exception: " << e.what() << "\n";
        reason = "fixture host-policy failure"; return false;
    }
}
std::string makeJson(const Result& result, const ScanStats* stats,
                     const ScanForegroundCompletionCounts* counts,
                     double elapsedMs, double filesPerSecond) {
    std::ostringstream out;
    out << std::setprecision(10) << "{\"schema\":1,\"status\":\"" << result.status << "\"";
    if (!result.stage.empty()) out << ",\"stage\":\"" << result.stage << "\",\"reason\":\"" << result.reason << "\"";
    if (stats && counts) {
        out << ",\"fixture\":{\"format\":\"pcm_s16_wav\",\"files\":100,\"sample_rate\":8000,\"channels\":1,\"frames_per_file\":800}"
            << ",\"workers\":{\"scan_threads\":2,\"backfill_threads\":2}"
            << ",\"scan\":{\"total\":" << stats->total << ",\"added\":" << stats->added
            << ",\"updated\":" << stats->updated << ",\"skipped\":" << stats->skipped
            << ",\"ingested\":" << stats->ingested << ",\"errors\":" << stats->errors
            << ",\"ingest_duration_ms\":" << stats->ingestDurationMs << "}"
            << ",\"terminal\":{\"total\":" << counts->total << ",\"pending_files\":" << counts->pendingFiles
            << ",\"mini_terminal\":" << counts->miniTerminal << ",\"cover_terminal\":" << counts->coverTerminal
            << ",\"metadata_terminal\":" << counts->metadataTerminal << ",\"mini_unresolved\":" << counts->miniUnresolved
            << ",\"cover_unresolved\":" << counts->coverUnresolved << ",\"metadata_unresolved\":" << counts->metadataUnresolved
            << ",\"non_success_terminal\":" << counts->failureTerminal << ",\"resolved_cover_files\":" << result.resolvedCoverFiles << ",\"elapsed_ms\":" << elapsedMs
            << ",\"files_per_second\":" << filesPerSecond << "}"
            << ",\"value_files_per_second\":" << filesPerSecond;
    }
    return out.str() + "}";
}
Result runProbe() {
    Result result;
    fs::path root;
    std::string fixtureReason;
    if (!makeFixture(root, fixtureReason)) {
        std::error_code ec;
        if (!root.empty()) fs::remove_all(root, ec);
        if (ec) {
            result.stage = "cleanup"; result.reason = "fixture cleanup failed";
        } else {
            result.code = 2; result.status = "environment_gate";
            result.stage = "fixture"; result.reason = fixtureReason;
        }
        result.json = makeJson(result, nullptr, nullptr, 0, 0); return result;
    }
    Database db;
    aura::BackfillQueue queue;
    aura::BackfillWorkerPool worker;
    const fs::path dbPath = root / "scan.db";
    constexpr int64_t generation = 1;
    constexpr int64_t session = 6101;
    std::atomic<bool> cancel{false};
    bool opened = false, workerStarted = false;
    ScanStats stats;
    ScanForegroundCompletionCounts counts;
    int resolvedCoverFiles = 0;
    double elapsedMs = 0, filesPerSecond = 0;
    std::string failure;
    try {
        opened = db.open(dbPath.u8string());
        if (!opened) failure = "database open failed";
        else if (!db.resetScanForegroundCompletionSession(session)) failure = "foreground session reset failed";
        if (failure.empty()) {
            queue.onDbOpened(dbPath.u8string(), generation);
            worker.start(dbPath.u8string(), generation, &queue, kBackfillThreads);
            workerStarted = true;
            // completeImport=false, enqueueExtraction=true are part of this terminal boundary.
            const bool completeImport = false;
            const bool enqueueExtraction = true;
            const auto start = Clock::now();
            stats = db.scanDirectoryEx(root.u8string(), true, false, false, nullptr, &cancel,
                                       kScanThreads, completeImport, enqueueExtraction,
                                       &queue, dbPath.u8string(), generation, session);
            const auto terminalDeadline = Clock::now() + std::chrono::seconds(kTerminalTimeoutSeconds);
            bool complete = false;
            while (Clock::now() < terminalDeadline) {
                counts = db.getScanForegroundCompletionCounts(session);
                if (counts.complete() && counts.total == kFiles && counts.pendingFiles == 0 && counts.miniTerminal == kFiles &&
                    counts.coverTerminal == kFiles && counts.metadataTerminal == kFiles && counts.miniUnresolved == 0 &&
                    counts.coverUnresolved == 0 && counts.metadataUnresolved == 0 && db.countBackfillPending() == 0) {
                    complete = true; break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            counts = db.getScanForegroundCompletionCounts(session);
            for (int i = 0; i < kFiles; ++i) {
                const auto path = root / "cover_indexing" / ("terminal_" + std::to_string(i) + ".wav");
                const AudioRecord rec = db.getFileByPath(path.u8string());
                if (rec.id > 0 && !rec.folderCoverKey.empty()) ++resolvedCoverFiles;
            }
            if (!complete) failure = "terminal ledger timeout";
            else if (!counts.complete() || stats.total != kFiles || stats.added != kFiles || stats.updated != 0 || stats.skipped != 0 ||
                     stats.ingested != kFiles || stats.errors != 0 || counts.total != kFiles || counts.pendingFiles != 0 ||
                     counts.miniTerminal != kFiles || counts.coverTerminal != kFiles || counts.metadataTerminal != kFiles ||
                     counts.miniUnresolved != 0 || counts.coverUnresolved != 0 || counts.metadataUnresolved != 0 ||
                     counts.failureTerminal != kFiles || resolvedCoverFiles != kFiles || db.countBackfillPending() != 0) failure = "scan terminal invariant failed";
            else if (!(std::isfinite(elapsedMs) && elapsedMs > 0)) failure = "invalid timing";
            else filesPerSecond = static_cast<double>(counts.total) / (elapsedMs / 1000.0);
        }
    } catch (const std::exception& e) {
        std::cerr << "probe exception: " << e.what() << "\n";
        failure = "probe exception";
    }
    if (workerStarted) worker.stop();
    queue.shutdown();
    if (opened) db.close();
    std::error_code ec;
    fs::remove_all(root, ec);
    if (ec && failure.empty()) failure = "fixture cleanup failed";
    if (failure.empty()) {
        result.code = 0; result.status = "measured";
        result.resolvedCoverFiles = resolvedCoverFiles;
        result.json = makeJson(result, &stats, &counts, elapsedMs, filesPerSecond);
    } else {
        std::cerr << "scan=" << stats.total << "," << stats.added << "," << stats.updated << "," << stats.skipped
                  << "," << stats.ingested << "," << stats.errors << " terminal=" << counts.total << ","
                  << counts.pendingFiles << "," << counts.miniTerminal << "," << counts.coverTerminal << ","
                  << counts.metadataTerminal << "," << counts.miniUnresolved << "," << counts.coverUnresolved << ","
                  << counts.metadataUnresolved << "," << counts.failureTerminal << "\n";
        result.stage = "scan_terminal"; result.reason = failure;
        result.json = makeJson(result, nullptr, nullptr, 0, 0);
    }
    return result;
}
}
int main() {
    std::ostringstream captured;
    auto* old = std::cout.rdbuf(captured.rdbuf());
    Result result;
    try { result = runProbe(); }
    catch (const std::exception& e) {
        std::cerr << "unexpected exception: " << e.what() << "\n";
        result.status = "failed"; result.stage = "exception"; result.reason = "unexpected exception";
        result.json = makeJson(result, nullptr, nullptr, 0, 0);
    }
    std::cout.rdbuf(old);
    if (result.code != 0 && !captured.str().empty()) std::cerr << captured.str();
    std::cout << result.json << "\n";
    return result.code;
}
