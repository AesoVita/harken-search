#include "Database.h"
#include "CoverModule.h"
#include "MetadataReader.h"
#include "Utils.h"
#include "sqlite3.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <cwctype>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const std::string& label) {
    if (cond) { std::cout << "PASS: " << label << "\n"; ++g_pass; }
    else      { std::cout << "FAIL: " << label << "\n"; ++g_fail; }
}

static AudioRecord makeRec(const std::string& name, const std::string& coverKey) {
    AudioRecord r;
    r.fileName = name;
    r.displayName = name;
    r.filePath = "e:\\cover_dedup_bench\\" + name;
    r.folderPath = "e:\\cover_dedup_bench";
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

static std::vector<uint8_t> MakeJpegBytes(int width, int height) {
    // Synthesize a deterministic gradient image and JPEG-encode it via WIC.
    // The image content does not matter for the benchmark — we only need a
    // realistic-sized JPEG that exercises the WIC/stb_image decode path used
    // by CoverModule::ComputePerceptualHash.
    std::vector<uint8_t> result;
    if (width <= 0 || height <= 0) return result;

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool didInit = SUCCEEDED(hrInit);

    {
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory)))) {
            if (didInit) CoUninitialize();
            return result;
        }

        ComPtr<IStream> stream;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
            if (didInit) CoUninitialize();
            return result;
        }

        ComPtr<IWICBitmapEncoder> encoder;
        if (FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder))) {
            if (didInit) CoUninitialize();
            return result;
        }
        if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
            if (didInit) CoUninitialize();
            return result;
        }

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> props;
        if (FAILED(encoder->CreateNewFrame(&frame, &props))) {
            if (didInit) CoUninitialize();
            return result;
        }
        if (FAILED(frame->Initialize(props.Get()))) {
            if (didInit) CoUninitialize();
            return result;
        }

        if (FAILED(frame->SetSize((UINT)width, (UINT)height))) {
            if (didInit) CoUninitialize();
            return result;
        }
        WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
        if (FAILED(frame->SetPixelFormat(&format))) {
            if (didInit) CoUninitialize();
            return result;
        }

        const UINT stride = ((UINT)width * 3 + 3) & ~3u;
        std::vector<uint8_t> pixels((size_t)stride * (size_t)height, 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t off = (size_t)y * stride + (size_t)x * 3;
                pixels[off + 0] = (uint8_t)((x * 255) / std::max(1, width));
                pixels[off + 1] = (uint8_t)((y * 255) / std::max(1, height));
                pixels[off + 2] = (uint8_t)(((x + y) * 127) / std::max(1, width + height));
            }
        }
        if (FAILED(frame->WritePixels((UINT)height, stride,
                                      (UINT)pixels.size(),
                                      pixels.data()))) {
            if (didInit) CoUninitialize();
            return result;
        }
        if (FAILED(frame->Commit()) || FAILED(encoder->Commit())) {
            if (didInit) CoUninitialize();
            return result;
        }

        LARGE_INTEGER zero{}; zero.QuadPart = 0;
        ULARGE_INTEGER pos{};
        stream->Seek(zero, STREAM_SEEK_END, &pos);
        const size_t total = (size_t)pos.QuadPart;
        result.resize(total);
        stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG read = 0;
        stream->Read(result.data(), (ULONG)total, &read);
        if (read != total) result.resize(read);
    }

    if (didInit) CoUninitialize();
    return result;
}

// Bulk up the test DB to production scale before benching. Production saw
// ~108k audio_files + ~10k unique covers. We use 30k/3k as a representative
// scale that runs in seconds yet exercises facet/index invalidation costs.
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

static AudioRecord makeNoiseRec(int i, const std::string& embeddedKey) {
    AudioRecord r;
    r.fileName = "noise_" + std::to_string(i) + ".wav";
    r.displayName = r.fileName;
    r.filePath = "e:\\noise\\noise_" + std::to_string(i) + ".wav";
    r.folderPath = "e:\\noise";
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
    r.embeddedCoverKey = embeddedKey;
    r.embeddedCoverKeyMtime = 1;
    return r;
}

static bool fillNoise(Database& db, const std::string& dbPath,
                      int audioFileRows, int coverRegistryRows) {
    // Audio files: use Database::addFile so schema and FTS5 triggers stay correct.
    for (int i = 0; i < audioFileRows; ++i) {
        const std::string ek = "E|noise_" + std::to_string(i);
        if (!db.addFile(makeNoiseRec(i, ek))) {
            std::cerr << "addFile failed at i=" << i << " err=" << db.getLastError() << "\n";
            return false;
        }
    }

    // Cover registry noise via raw SQL (stable schema).
    if (coverRegistryRows > 0) {
        sqlite3* raw = nullptr;
        if (sqlite3_open(dbPath.c_str(), &raw) != SQLITE_OK) {
            std::cerr << "fillNoise open failed: " << sqlite3_errmsg(raw) << "\n";
            return false;
        }
        bool ok = execSql(raw, "BEGIN IMMEDIATE;");
        const char* sql =
            "INSERT OR REPLACE INTO cover_cache_registry "
            "(cover_key, rep_path, is_embedded, normalized_key, cover_phash, cover_phash_version) "
            "VALUES (?, ?, 1, ?, ?, 2);";
        sqlite3_stmt* stmt = nullptr;
        ok = ok && sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr) == SQLITE_OK;
        for (int i = 0; ok && i < coverRegistryRows; ++i) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            const std::string key = "E|noise_" + std::to_string(i);
            const std::string rep = "e:\\noise\\noise_" + std::to_string(i) + ".wav";
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, rep.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 4, (int64_t)(0x7000000000000000ull + (uint64_t)i));
            ok = sqlite3_step(stmt) == SQLITE_DONE;
        }
        if (stmt) sqlite3_finalize(stmt);
        ok = ok && execSql(raw, "COMMIT;");
        sqlite3_close(raw);
        if (!ok) return false;
    }

    return true;
}

// Walk a real audio library and return the bytes of the first embedded
// album art we can find. This makes BENCH_FULL exercise a production-sized
// JPEG (often 1500-3000px wide) instead of a tiny WIC-generated 1024px one.
static std::vector<uint8_t> LoadRealJpegFromLibrary(const std::string& rootPathUtf8, int maxFilesToScan = 200) {
    std::vector<uint8_t> result;
    if (rootPathUtf8.empty()) return result;

    // Aura standard: paths are UTF-8 in std::string. Convert to fs::path
    // via the project's Utf8ToPath helper so non-ASCII path components
    // (e.g. Chinese characters) survive round-trip on Windows.
    fs::path root = Utf8ToPath(rootPathUtf8);
    std::error_code existsEc;
    if (!fs::exists(root, existsEc)) {
        std::cout << "[fixture] root does not exist: " << rootPathUtf8 << "\n";
        return result;
    }

    int scanned = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator() && scanned < maxFilesToScan;
         it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        const auto ext = it->path().extension().wstring();
        std::wstring lowerW = ext;
        std::transform(lowerW.begin(), lowerW.end(), lowerW.begin(),
                       [](wchar_t c){ return (wchar_t)::towlower(c); });
        if (lowerW != L".wav" && lowerW != L".mp3" && lowerW != L".flac"
            && lowerW != L".aiff" && lowerW != L".m4a") continue;
        ++scanned;
        const std::string filePathUtf8 = PathToUtf8(it->path());
        AlbumArt art;
        if (MetadataReader::readAlbumArt(filePathUtf8, art) && art.hasData() && art.data.size() > 4096) {
            result = art.data;
            std::cout << "[fixture] real JPEG from " << filePathUtf8
                      << " bytes=" << result.size() << " mime=" << art.mimeType << "\n";
            return result;
        }
    }
    std::cout << "[fixture] no embedded album art in first " << scanned
              << " audio files under " << rootPathUtf8 << "\n";
    return result;
}

struct BenchStats {
    double avgMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    int n = 0;
};

static BenchStats summarize(std::vector<double> samples) {
    BenchStats s;
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    s.n = (int)samples.size();
    s.minMs = samples.front();
    s.maxMs = samples.back();
    s.p50Ms = samples[s.n / 2];
    s.p95Ms = samples[std::min(s.n - 1, (s.n * 95) / 100)];
    s.avgMs = std::accumulate(samples.begin(), samples.end(), 0.0) / (double)s.n;
    return s;
}

static void printBench(const std::string& name, const BenchStats& s) {
    std::printf("[BENCH] %-20s n=%3d  avg=%7.2fms  p50=%7.2fms  p95=%7.2fms  min=%7.2fms  max=%7.2fms\n",
                name.c_str(), s.n, s.avgMs, s.p50Ms, s.p95Ms, s.minMs, s.maxMs);
}

int main() {
    fs::path tmpDir = fs::temp_directory_path() / "auro_cover_dedup_throughput";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);
    const std::string dbPath = (tmpDir / "test.db").string();

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: db.open: " << db.getLastError() << "\n";
        return 1;
    }
    expect(true, "temp db opened");

    // Bulk DB to production scale BEFORE benching so cache invalidations
    // and facet rebuilds run against a real workload.
    int audioNoise = 30000;
    int coverNoise = 8000;
    if (const char* env = std::getenv("AURA_BENCH_AUDIO_NOISE")) audioNoise = std::max(0, std::atoi(env));
    if (const char* env = std::getenv("AURA_BENCH_COVER_NOISE")) coverNoise = std::max(0, std::atoi(env));
    {
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = fillNoise(db, dbPath, audioNoise, coverNoise);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "[fixture] filled noise (" << audioNoise << " audio_files + "
                  << coverNoise << " cover_registry) in "
                  << ms << "ms ok=" << (ok ? 1 : 0) << "\n";
        expect(ok, "noise fill succeeded");
    }

    // Prefer a real JPEG from a real library so decode timings match production.
    // Override via env AURA_BENCH_LIB. Falls back to synthetic 1024x1024 WIC JPEG.
    std::string realLibPath;
    if (const char* env = std::getenv("AURA_BENCH_LIB")) realLibPath = env;
    if (realLibPath.empty()) realLibPath = "H:\\Sound effect\\综合\\SoundMorph音效合辑";
    auto jpeg = LoadRealJpegFromLibrary(realLibPath);
    if (jpeg.empty()) {
        std::cout << "[fixture] real JPEG unavailable, falling back to synthetic 1024x1024\n";
        jpeg = MakeJpegBytes(1024, 1024);
    }
    std::cout << "[fixture] JPEG fixture bytes=" << jpeg.size() << "\n";
    expect(jpeg.size() > 4096, "jpeg fixture is realistically sized (>4KB)");
    expect(jpeg.size() < 16 * 1024 * 1024, "jpeg fixture is reasonably sized (<16MB)");

    // Diagnostic: verify saveCoverThumb / getCoverThumb roundtrip works at all.
    {
        const std::string probeKey = "E|PROBE|0";
        const bool saved = db.saveCoverThumb(probeKey, jpeg, "image/jpeg");
        std::vector<uint8_t> got;
        std::string gotMime;
        const bool fetched = db.getCoverThumb(probeKey, got, gotMime);
        std::cout << "[diag] saveCoverThumb=" << (saved ? "ok" : "FAIL")
                  << " getCoverThumb=" << (fetched ? "ok" : "FAIL")
                  << " size=" << got.size() << " mime=" << gotMime << "\n";
        expect(saved, "saveCoverThumb returned true");
        expect(fetched && got.size() > 16, "getCoverThumb returned thumb bytes");

        // Time pHash directly to validate decode cost
        std::vector<double> phashSamples;
        for (int i = 0; i < 20; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            const uint64_t h = CoverModule::ComputePerceptualHash(jpeg.data(), jpeg.size());
            const auto t1 = std::chrono::steady_clock::now();
            phashSamples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            (void)h;
        }
        const auto phashStats = summarize(phashSamples);
        printBench("pHash direct (decode)", phashStats);
    }

    {
        const int N = 100;
        std::vector<double> samples;
        samples.reserve(N);
        for (int i = 0; i < N; ++i) {
            const std::string key = "E|BENCH_INFRA|" + std::to_string(i);
            const std::string repPath = "e:\\cover_dedup_bench\\infra_" + std::to_string(i) + ".wav";
            // Mirror production call shape: BackfillWorkerPool.cpp:359, 429
            const auto t0 = std::chrono::steady_clock::now();
            const int finalized = db.finalizeCoverRegistryDeltas({{key, repPath, /*isEmbedded=*/true}});
            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            samples.push_back(ms);
            (void)finalized;
        }
        const auto stats = summarize(samples);
        printBench("BENCH_INFRA", stats);
        expect(stats.avgMs > 0.0, "BENCH_INFRA produced positive average");
        expect(stats.p95Ms < 5000.0, "BENCH_INFRA p95 below sanity ceiling");
    }

    {
        const int N = 100;
        const auto jpegBytes = MakeJpegBytes(1024, 1024);
        if (jpegBytes.size() <= 16) {
            std::cerr << "FAIL: jpeg generation failed; skipping BENCH_FULL\n";
            ++g_fail;
        } else {
            std::vector<double> samples;
            samples.reserve(N);
            bool fixtureOk = true;
            for (int i = 0; i < N; ++i) {
                const std::string key = "E|BENCH_FULL|" + std::to_string(i);
                const std::string repPath = "e:\\cover_dedup_bench\\full_" + std::to_string(i) + ".wav";
                // Save thumb FIRST (mirrors BackfillWorkerPool.cpp:347, 416)
                if (!db.saveCoverThumb(key, jpegBytes, "image/jpeg")) {
                    expect(false, "saveCoverThumb succeeded for BENCH_FULL");
                    fixtureOk = false;
                    break;
                }
                const auto t0 = std::chrono::steady_clock::now();
                const int finalized = db.finalizeCoverRegistryDeltas({{key, repPath, /*isEmbedded=*/true}});
                const auto t1 = std::chrono::steady_clock::now();
                const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                samples.push_back(ms);
                (void)finalized;
            }
            if (fixtureOk) {
                const auto stats = summarize(samples);
                printBench("BENCH_FULL", stats);
                expect(stats.avgMs > 0.0, "BENCH_FULL produced positive average");
            }
        }
    }

    {
        const int BATCH = 64;
        const auto jpegBytes = MakeJpegBytes(1024, 1024);
        std::vector<Database::CoverRegistryDelta> batch;
        batch.reserve(BATCH);
        bool fixtureOk = jpegBytes.size() > 16;
        for (int i = 0; i < BATCH && fixtureOk; ++i) {
            const std::string key = "E|BENCH_BATCH|" + std::to_string(i);
            if (!db.saveCoverThumb(key, jpegBytes, "image/jpeg")) {
                fixtureOk = false;
                break;
            }
            const std::string repPath = "e:\\cover_dedup_bench\\batch_" + std::to_string(i) + ".wav";
            batch.push_back({key, repPath, /*isEmbedded=*/true});
        }
        if (!fixtureOk) {
            expect(false, "BENCH_BATCH fixture prepared");
        } else {
            const auto t0 = std::chrono::steady_clock::now();
            const int finalized = db.finalizeCoverRegistryDeltas(batch);
            const auto t1 = std::chrono::steady_clock::now();
            const double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double perCoverMs = totalMs / (double)BATCH;
            std::printf("[BENCH] %-20s n=%3d  total=%7.2fms  per-cover=%7.2fms  finalized=%d\n",
                        "BENCH_BATCH", BATCH, totalMs, perCoverMs, finalized);
            expect(finalized == BATCH, "BENCH_BATCH finalized all 64 deltas");
            expect(perCoverMs > 0.0, "BENCH_BATCH per-cover positive");
        }
    }

    // Note: direct timing of invalidateFacetQueryCache /
    // invalidateFolderCoverFacetIndex is not possible from a test target
    // because both are private members. Their cost is indirectly measured
    // by BENCH_INFRA: the entire BENCH_INFRA budget at scale is dominated
    // by these two per-call full-table DELETEs (see Database.cpp:3399, 3425).

    std::cout << "\n=== Cover dedup throughput report ===\n";
    std::cout << "Compare BENCH_FULL avg vs BENCH_BATCH per-cover to estimate batching speedup.\n";
    std::cout << "Compare (BENCH_FULL - BENCH_INFRA) to estimate JPEG-decode + phash share.\n";

    db.close();
    fs::remove_all(tmpDir, ec);
    std::cout << "\n=== Summary: " << g_pass << " pass, " << g_fail << " fail ===\n";
    return g_fail == 0 ? 0 : 1;
}
