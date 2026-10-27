// End-to-end scan backfill completion perf bench.
//
// This intentionally measures the same layer as the scanner dialog:
// durable pending rows -> BackfillQueue -> BackfillWorkerPool ->
// derived worker job -> foreground terminal mark -> pending clear.
//
// Manual usage:
//   build\Debug\TestScanWaveformBackfillPerf.exe "H:\Sound effect\..." 200 2,8 waveform
//   build\Debug\TestScanWaveformBackfillPerf.exe "H:\Sound effect\..." 200 2,8 cover

#include "BackfillQueue.h"
#include "BackfillWorkerPool.h"
#include "Database.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                   nullptr, 0, nullptr, nullptr);
    std::string out(sz, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                          out.data(), sz, nullptr, nullptr);
    return out;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int sz = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                   nullptr, 0);
    std::wstring out(sz, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                          out.data(), sz);
    return out;
}

fs::path pathFromUtf8(const std::string& s) {
    return fs::path(utf8ToWide(s));
}

std::string pathToUtf8(const fs::path& p) {
    return wideToUtf8(p.native());
}
#else
fs::path pathFromUtf8(const std::string& s) {
    return fs::path(s);
}

std::string pathToUtf8(const fs::path& p) {
    return p.u8string();
}
#endif

std::string lowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool isSupportedAudioPath(const fs::path& p) {
    const std::string ext = lowerAscii(pathToUtf8(p.extension()));
    return ext == ".wav" || ext == ".wave" || ext == ".aif" ||
           ext == ".aiff" || ext == ".flac" || ext == ".mp3" ||
           ext == ".ogg";
}

int64_t fileMtimeSeconds(const fs::path& p) {
    std::error_code ec;
    auto ft = fs::last_write_time(p, ec);
    if (ec) return 0;
    auto st = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::seconds>(
        st.time_since_epoch()).count();
}

std::vector<fs::path> collectAudioFiles(const std::string& rootUtf8, int limit) {
    std::vector<fs::path> files;
    std::error_code ec;
    const fs::path root = pathFromUtf8(rootUtf8);
    fs::recursive_directory_iterator it(
        root,
        fs::directory_options::skip_permission_denied,
        ec);
    fs::recursive_directory_iterator end;
    while (!ec && it != end && static_cast<int>(files.size()) < limit) {
        const fs::directory_entry entry = *it;
        if (entry.is_regular_file(ec) && !ec && isSupportedAudioPath(entry.path())) {
            files.push_back(entry.path());
        }
        it.increment(ec);
    }
    return files;
}

AudioRecord makeRecord(const fs::path& p) {
    AudioRecord rec;
    rec.filePath = pathToUtf8(p);
    rec.fileName = pathToUtf8(p.filename());
    rec.displayName = rec.fileName;
    rec.folderPath = pathToUtf8(p.parent_path());
    rec.extension = lowerAscii(pathToUtf8(p.extension()));
    if (!rec.extension.empty() && rec.extension.front() == '.') {
        rec.format = lowerAscii(rec.extension.substr(1));
    }

    std::error_code ec;
    rec.fileSize = static_cast<uint64_t>(fs::file_size(p, ec));
    if (ec) rec.fileSize = 0;
    rec.fileMtime = fileMtimeSeconds(p);
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    rec.dateAdded = now;
    rec.lastIndexed = now;
    return rec;
}

void removeDbArtifacts(const fs::path& dbPath) {
    std::error_code ec;
    fs::remove(dbPath, ec);
    fs::remove(dbPath.string() + "-wal", ec);
    fs::remove(dbPath.string() + "-shm", ec);
    fs::remove(dbPath.parent_path() / "auro_blobs.db", ec);
    fs::remove(dbPath.parent_path() / "auro_blobs.db-wal", ec);
    fs::remove(dbPath.parent_path() / "auro_blobs.db-shm", ec);
    fs::remove(dbPath.parent_path() / "auro_waveforms.db", ec);
    fs::remove(dbPath.parent_path() / "auro_waveforms.db-wal", ec);
    fs::remove(dbPath.parent_path() / "auro_waveforms.db-shm", ec);
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::vector<int> parseThreadsCsv(const std::string& csv) {
    std::vector<int> values;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        int v = std::atoi(item.c_str());
        if (v > 0) values.push_back(v);
    }
    if (values.empty()) values = {2, 8};
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

aura::BackfillJobKind parseKindArg(std::string arg) {
    arg = lowerAscii(arg);
    if (arg == "cover" || arg == "covers" || arg == "0") {
        return aura::BackfillJobKind::CoverResolveAndThumb;
    }
    if (arg == "metadata" || arg == "meta" || arg == "2") {
        return aura::BackfillJobKind::MetadataRefresh;
    }
    return aura::BackfillJobKind::WaveformL1Overview;
}

const char* kindName(aura::BackfillJobKind kind) {
    switch (kind) {
        case aura::BackfillJobKind::CoverResolveAndThumb:
            return "cover";
        case aura::BackfillJobKind::WaveformL1Overview:
            return "waveform";
        case aura::BackfillJobKind::MetadataRefresh:
            return "metadata";
    }
    return "unknown";
}

bool seedPendingBackfillRows(Database& db,
                             const std::vector<fs::path>& files,
                             aura::BackfillJobKind kind,
                             int64_t scanSessionId) {
    if (!db.beginTransaction()) {
        std::cerr << "FAIL: beginTransaction: " << db.getLastError() << "\n";
        return false;
    }

    bool ok = true;
    std::vector<int64_t> fileIds;
    fileIds.reserve(files.size());
    for (const fs::path& p : files) {
        AudioRecord rec = makeRecord(p);
        if (!db.addFile(rec)) {
            std::cerr << "FAIL: addFile: " << rec.filePath << " err=" << db.getLastError() << "\n";
            ok = false;
            break;
        }
        const AudioRecord stored = db.getFileByPath(rec.filePath);
        if (stored.id <= 0 ||
            !db.markBackfillPending(
                stored.id,
                static_cast<int>(kind))) {
            std::cerr << "FAIL: markBackfillPending: " << rec.filePath
                      << " err=" << db.getLastError() << "\n";
            ok = false;
            break;
        }
        fileIds.push_back(stored.id);
    }

    if (ok && scanSessionId > 0) {
        ok = db.seedScanForegroundCompletionRows(
            scanSessionId,
            fileIds,
            kind == aura::BackfillJobKind::WaveformL1Overview,
            kind == aura::BackfillJobKind::CoverResolveAndThumb,
            kind == aura::BackfillJobKind::MetadataRefresh);
        if (!ok) {
            std::cerr << "FAIL: seedScanForegroundCompletionRows: " << db.getLastError() << "\n";
        }
    }

    if (ok) {
        ok = db.commitTransaction();
    } else {
        db.rollbackTransaction();
    }
    if (!ok) {
        std::cerr << "FAIL: seed transaction: " << db.getLastError() << "\n";
    }
    return ok;
}

struct RunResult {
    std::string kind;
    int threads = 0;
    int files = 0;
    int completed = 0;
    int pendingBefore = 0;
    int pendingAfter = 0;
    bool drained = false;
    double seconds = 0.0;
    double filesPerSecond = 0.0;
    fs::path dbPath;
};

RunResult runBackfillBench(const std::vector<fs::path>& files,
                           const fs::path& reportDir,
                           aura::BackfillJobKind kind,
                           int threads) {
    RunResult result;
    result.kind = kindName(kind);
    result.threads = threads;
    result.files = static_cast<int>(files.size());
    result.dbPath = reportDir / (result.kind + "_backfill_t" + std::to_string(threads) + ".db");
    removeDbArtifacts(result.dbPath);

    Database db;
    if (!db.open(result.dbPath.string())) {
        std::cerr << "FAIL: db.open: " << db.getLastError() << "\n";
        return result;
    }
    constexpr int64_t kScanSessionId = 1;
    if (!db.resetScanForegroundCompletionSession(kScanSessionId) ||
        !seedPendingBackfillRows(db, files, kind, kScanSessionId)) {
        db.close();
        return result;
    }

    constexpr int64_t kDbGeneration = 1;
    aura::BackfillQueue queue;
    queue.onDbOpened(result.dbPath.string(), kDbGeneration);
    queue.rebuildFromDatabase(&db, static_cast<int>(files.size()));
    result.pendingBefore = db.countBackfillPendingKind(
        static_cast<int>(kind));

    aura::BackfillWorkerPool workers;
    const auto start = std::chrono::steady_clock::now();
    workers.start(result.dbPath.string(), kDbGeneration, &queue, threads);

    const auto timeout = start + std::chrono::seconds(
        std::max(30, static_cast<int>(files.size()) * 2));
    while (std::chrono::steady_clock::now() < timeout) {
        result.pendingAfter = db.countBackfillPendingKind(
            static_cast<int>(kind));
        if (result.pendingAfter == 0) {
            result.drained = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    workers.stop();
    result.seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    result.pendingAfter = db.countBackfillPendingKind(
        static_cast<int>(kind));
    result.drained = result.pendingAfter == 0;

    const ScanForegroundCompletionCounts counts =
        db.getScanForegroundCompletionCounts(kScanSessionId);
    if (kind == aura::BackfillJobKind::WaveformL1Overview) {
        result.completed = counts.miniTerminal;
    } else if (kind == aura::BackfillJobKind::CoverResolveAndThumb) {
        result.completed = counts.coverTerminal;
    } else {
        result.completed = counts.metadataTerminal;
    }

    if (result.seconds > 0.0) {
        result.filesPerSecond = static_cast<double>(result.completed) / result.seconds;
    }

    queue.shutdown();
    db.close();
    return result;
}

void writeRunJson(const fs::path& reportDir,
                  const std::string& root,
                  int sampleLimit,
                  const RunResult& r) {
    std::ofstream out(reportDir / (r.kind + "_backfill_t" + std::to_string(r.threads) + ".json"));
    out << "{\n"
        << "  \"scan_root\": \"" << jsonEscape(root) << "\",\n"
        << "  \"kind\": \"" << r.kind << "\",\n"
        << "  \"sample_limit\": " << sampleLimit << ",\n"
        << "  \"threads\": " << r.threads << ",\n"
        << "  \"files\": " << r.files << ",\n"
        << "  \"completed\": " << r.completed << ",\n"
        << "  \"pending_before\": " << r.pendingBefore << ",\n"
        << "  \"pending_after\": " << r.pendingAfter << ",\n"
        << "  \"drained\": " << (r.drained ? "true" : "false") << ",\n"
        << "  \"elapsed_seconds\": " << r.seconds << ",\n"
        << "  \"files_per_second\": " << r.filesPerSecond << ",\n"
        << "  \"db_path\": \"" << jsonEscape(r.dbPath.string()) << "\"\n"
        << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string root;
    int sampleLimit = 200;
    std::string threadsCsv = "2,8";
    std::string kindArg = "waveform";

#ifdef _WIN32
    int wargc = 0;
    wchar_t** wargv = ::CommandLineToArgvW(::GetCommandLineW(), &wargc);
    if (wargc >= 2) root = wideToUtf8(wargv[1]);
    if (wargc >= 3) sampleLimit = std::max(1, std::min(2000, std::atoi(wideToUtf8(wargv[2]).c_str())));
    if (wargc >= 4) threadsCsv = wideToUtf8(wargv[3]);
    if (wargc >= 5) kindArg = wideToUtf8(wargv[4]);
    if (wargv) ::LocalFree(wargv);
#else
    if (argc >= 2) root = argv[1];
    if (argc >= 3) sampleLimit = std::max(1, std::min(2000, std::atoi(argv[2])));
    if (argc >= 4) threadsCsv = argv[3];
    if (argc >= 5) kindArg = argv[4];
#endif

    if (root.empty()) {
        std::cerr << "Usage: TestScanWaveformBackfillPerf <audio-root> [sample-count] [threads-csv] [waveform|cover|metadata]\n";
        return 1;
    }
    if (!fs::exists(pathFromUtf8(root))) {
        std::cerr << "FAIL: root does not exist: " << root << "\n";
        return 1;
    }

    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tmBuf);
    const fs::path reportDir = fs::path(".openclaw") / "reports" /
        (std::string("perf_scan_backfill_") + ts);
    fs::create_directories(reportDir);
    const aura::BackfillJobKind kind = parseKindArg(kindArg);

    const std::vector<fs::path> files = collectAudioFiles(root, sampleLimit);
    if (files.empty()) {
        std::cerr << "FAIL: no supported audio files under root: " << root << "\n";
        return 1;
    }

    std::cout << "root       : " << root << "\n";
    std::cout << "kind       : " << kindName(kind) << "\n";
    std::cout << "sample     : " << files.size() << "\n";
    std::cout << "report dir : " << reportDir.string() << "\n";

    bool allDrained = true;
    for (int threads : parseThreadsCsv(threadsCsv)) {
        RunResult result = runBackfillBench(files, reportDir, kind, threads);
        writeRunJson(reportDir, root, sampleLimit, result);
        std::cout << "[kind=" << result.kind << " threads=" << threads
                  << "] completed=" << result.completed
                  << "/" << result.files
                  << " pendingBefore=" << result.pendingBefore
                  << " pendingAfter=" << result.pendingAfter
                  << " elapsed=" << result.seconds
                  << " filesPerSec=" << result.filesPerSecond
                  << "\n";
        allDrained = allDrained && result.drained && result.completed == result.files;
    }

    return allDrained ? 0 : 2;
}
