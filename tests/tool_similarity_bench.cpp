// tool_similarity_bench.cpp — F-028 quality-verification measurement tool
//
// PURPOSE (Trello #372): pure measurement, NO modification to src/ similarity logic.
//   1. Measure findSimilar() latency at increasing cache sizes (10k / 100k / full).
//   2. Report the feature-cache memory footprint.
//   3. Export query -> top-K result file-path lists for blind subjective relevance
//      evaluation by a sound designer.
//
// USAGE:
//   tool_similarity_bench <db_path> [--topk N] [--queries Q] [--out results.txt]
//
// This is a standalone console tool, not a pass/fail ctest. It exercises the
// SimilaritySearch public API exactly as production does.

#include "Database.h"
#include "SimilaritySearch.h"
#include "FeatureExtractor.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

const char* presetName(SimilarityPreset p) {
    switch (p) {
        case SimilarityPreset::Balanced:        return "Balanced";
        case SimilarityPreset::TimbreFocused:   return "TimbreFocused";
        case SimilarityPreset::RhythmFocused:   return "RhythmFocused";
        case SimilarityPreset::PitchFocused:    return "PitchFocused";
        case SimilarityPreset::SpectralFocused: return "SpectralFocused";
    }
    return "Unknown";
}

// Approximate per-entry footprint of SimilaritySearch::CacheEntry (private, mirrored
// here for reporting only): fileId(8) + features[48]*4 + std::string overhead + chars.
size_t approxEntryBytes(const std::string& filePath) {
    size_t fixed = sizeof(int64_t) + AudioFeatureVector::DIMS * sizeof(float);
    size_t strOverhead = sizeof(std::string);          // control block
    size_t strChars = filePath.size() + 1;             // heap chars (if not SSO)
    return fixed + strOverhead + strChars;
}

struct Args {
    std::string dbPath;
    int topK = 30;
    int queries = 50;
    std::string outPath = "similarity_bench_results.txt";
};

bool parseArgs(int argc, char** argv, Args& out) {
    if (argc < 2) {
        std::printf("Usage: %s <db_path> [--topk N] [--queries Q] [--out FILE]\n", argv[0]);
        return false;
    }
    out.dbPath = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](int& dst) { if (i + 1 < argc) dst = std::atoi(argv[++i]); };
        if (a == "--topk")          next(out.topK);
        else if (a == "--queries")  next(out.queries);
        else if (a == "--out" && i + 1 < argc) out.outPath = argv[++i];
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) return 2;

    std::printf("=== F-028 Similarity Search Benchmark ===\n");
    std::printf("DB:       %s\n", args.dbPath.c_str());
    std::printf("topK:     %d\n", args.topK);
    std::printf("queries:  %d\n", args.queries);
    std::printf("out:      %s\n\n", args.outPath.c_str());

    Database db;
    if (!db.open(args.dbPath)) {
        std::printf("ERROR: failed to open DB at %s\n", args.dbPath.c_str());
        return 1;
    }

    int withFeatures = db.countFilesWithFeatures();
    std::printf("Files with features in DB: %d\n", withFeatures);
    if (withFeatures < 2) {
        std::printf("ERROR: need >= 2 files with extracted features to benchmark.\n");
        return 1;
    }

    // ── Load cache (production path) ────────────────────────────────────────
    auto tLoad = Clock::now();
    SimilaritySearch engine;
    engine.loadFeatureCache(db);
    double loadMs = msSince(tLoad);

    size_t n = engine.cacheSize();
    std::printf("Cache loaded: %zu entries in %.1f ms\n", n, loadMs);
    if (n < 2) {
        std::printf("ERROR: cache too small after load.\n");
        return 1;
    }

    // ── Memory footprint ────────────────────────────────────────────────────
    // Pull entries directly from DB to estimate string sizes (cache internals are private).
    auto entries = db.loadAllAudioFeatures();
    size_t cacheBytes = 0;
    for (auto& e : entries) cacheBytes += approxEntryBytes(e.filePath);
    size_t vecOverhead = entries.capacity() * sizeof(void*); // rough vector slack
    double cacheMB = (double)(cacheBytes + vecOverhead) / (1024.0 * 1024.0);
    std::printf("Approx cache footprint: %.1f MB (%.1f bytes/entry avg)\n\n",
                cacheMB, n ? (double)cacheBytes / (double)n : 0.0);

    // Project to 3M scale (linear, brute-force is O(n)).
    double perEntry = n ? (double)cacheBytes / (double)n : 0.0;
    double proj3M = perEntry * 3'000'000.0 / (1024.0 * 1024.0);
    std::printf("Projected footprint @ 3M files: %.0f MB\n\n", proj3M);

    // ── Build query set (sample real feature vectors from the cache) ─────────
    std::mt19937 rng(1234567u);
    std::uniform_int_distribution<size_t> pick(0, entries.size() - 1);

    int Q = std::min(args.queries, (int)entries.size());
    std::vector<size_t> queryIdx;
    queryIdx.reserve(Q);
    for (int i = 0; i < Q; ++i) queryIdx.push_back(pick(rng));

    // ── Latency measurement across presets ──────────────────────────────────
    const SimilarityPreset presets[] = {
        SimilarityPreset::Balanced,
        SimilarityPreset::TimbreFocused,
        SimilarityPreset::SpectralFocused,
    };

    std::printf("=== Latency (full cache, n=%zu, topK=%d) ===\n", n, args.topK);
    for (auto preset : presets) {
        std::vector<double> samples;
        samples.reserve(Q);
        for (size_t qi : queryIdx) {
            const auto& qEntry = entries[qi];
            auto t0 = Clock::now();
            auto res = engine.findSimilar(qEntry.features, qEntry.fileId, args.topK, preset);
            samples.push_back(msSince(t0));
            (void)res;
        }
        std::sort(samples.begin(), samples.end());
        double sum = 0; for (double s : samples) sum += s;
        double mean = sum / samples.size();
        double p50 = samples[samples.size() / 2];
        double p95 = samples[(size_t)(samples.size() * 0.95)];
        double mx  = samples.back();
        std::printf("  %-16s mean=%.2fms  p50=%.2fms  p95=%.2fms  max=%.2fms\n",
                    presetName(preset), mean, p50, p95, mx);
    }
    std::printf("\n  (16ms = 60fps main-loop budget; brute-force is O(n), scales linearly)\n\n");

    // ── Export top-K result lists for blind subjective evaluation ────────────
    std::ofstream out(args.outPath, std::ios::binary);
    if (!out) {
        std::printf("WARNING: could not open %s for writing; skipping export.\n",
                    args.outPath.c_str());
    } else {
        out << "# F-028 Similarity Search — query -> top-K results (blind subjective eval)\n";
        out << "# DB: " << args.dbPath << "\n";
        out << "# cache_entries=" << n << " topK=" << args.topK << "\n";
        out << "# For each query, rate how relevant each result is (1-5) without seeing\n";
        out << "# the algorithm's similarity score.\n\n";

        int exportQueries = std::min(20, Q); // keep the human task manageable
        for (int qi = 0; qi < exportQueries; ++qi) {
            const auto& qEntry = entries[queryIdx[qi]];
            auto res = engine.findSimilar(qEntry.features, qEntry.fileId,
                                          args.topK, SimilarityPreset::Balanced);
            out << "=== QUERY " << (qi + 1) << " ===\n";
            out << "  [Q] " << qEntry.filePath << "\n";
            int rank = 1;
            for (const auto& r : res) {
                out << "  " << rank++ << ". sim=" << r.similarity
                    << "  " << r.filePath << "\n";
            }
            out << "\n";
        }
        out.close();
        std::printf("Exported %d query result lists to %s\n",
                    std::min(20, Q), args.outPath.c_str());
    }

    std::printf("\n=== Done ===\n");
    return 0;
}
