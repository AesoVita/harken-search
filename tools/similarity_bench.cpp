#include "SimilaritySearch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

namespace {
constexpr std::size_t kCacheEntries = 1'000'000;
constexpr int kWarmupRuns = 5;
constexpr int kMeasuredRuns = 20;
constexpr int topK = 50;
constexpr double kP95GateMs = 100.0;
constexpr double kCacheLoadGateMs = 2'000.0;
constexpr std::size_t kPeakWorkingSetGateBytes = 768ull * 1024ull * 1024ull;
constexpr std::size_t kQueryIndex = 123'457;

AudioFeatureVector MakeFeatures(std::uint32_t seed) {
    AudioFeatureVector features;
    for (int dim = 0; dim < AudioFeatureVector::DIMS; ++dim) {
        const std::uint32_t value =
            (seed * static_cast<std::uint32_t>(dim + 3) +
             static_cast<std::uint32_t>(dim * 17)) % 4096u;
        features.features[dim] = static_cast<float>(value) / 4096.0f;
    }
    features.features[AudioFeatureVector::PITCH_OFFSET + 2] = 0.5f;
    return features;
}
}  // namespace

int main() {
    std::vector<SimilaritySearch::SimilarityFeatureRecord> entries;
    entries.reserve(kCacheEntries);

    for (std::size_t index = 0; index < kCacheEntries; ++index) {
        SimilaritySearch::SimilarityFeatureRecord entry;
        entry.fileId = static_cast<std::int64_t>(index + 1);
        // Exercise the real Windows UTF-8/UTF-16 path-identity lane rather
        // than allowing the million-record gate to cover ASCII only.
        entry.filePath = std::string(u8"SimilarityBench/音效/") +
                         std::to_string(index) + ".wav";
        entry.features = MakeFeatures(static_cast<std::uint32_t>(index + 1));
        entries.push_back(std::move(entry));
    }

    SimilaritySearch search;
    const AudioFeatureVector query = MakeFeatures(
        static_cast<std::uint32_t>(kQueryIndex + 1));
    const std::int64_t queryFileId = static_cast<std::int64_t>(kQueryIndex + 1);
    const auto cacheLoadStarted = std::chrono::steady_clock::now();
    search.replaceFeatureCache(std::move(entries));
    const double cacheLoadMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - cacheLoadStarted).count();
    std::size_t peakWorkingSetBytes = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX memoryCounters{};
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters),
            sizeof(memoryCounters))) {
        peakWorkingSetBytes = static_cast<std::size_t>(
            memoryCounters.PeakWorkingSetSize);
    }
#endif

    for (int run = 0; run < kWarmupRuns; ++run) {
        (void)search.findSimilar(query, queryFileId, topK);
    }

    std::vector<double> durationsMs;
    durationsMs.reserve(kMeasuredRuns);
    bool finiteScores = true;
    for (int run = 0; run < kMeasuredRuns; ++run) {
        const auto started = std::chrono::steady_clock::now();
        const auto results = search.findSimilar(query, queryFileId, topK);
        const auto finished = std::chrono::steady_clock::now();
        durationsMs.push_back(std::chrono::duration<double, std::milli>(
                                  finished - started)
                                  .count());
        for (const auto& result : results) {
            finiteScores = finiteScores && std::isfinite(result.similarity);
        }
    }

    std::sort(durationsMs.begin(), durationsMs.end());
    const double p50 = durationsMs[durationsMs.size() / 2];
    const std::size_t p95Index =
        (durationsMs.size() * 95 + 99) / 100 - 1;
    const double p95 = durationsMs[p95Index];
    const double maximum = durationsMs.back();
    const std::size_t estimatedCacheBytes =
        kCacheEntries * (sizeof(std::int64_t) + sizeof(std::uint64_t) +
                         sizeof(std::string) +
                         sizeof(AudioFeatureVector));

    std::cout << "SimilarityBench entries=" << kCacheEntries
              << " unicode_paths=100%"
              << " warmups=" << kWarmupRuns
              << " measured=" << kMeasuredRuns << '\n'
              << "cache_load=" << cacheLoadMs << "ms"
              << " peak_working_set=" << peakWorkingSetBytes << " bytes\n"
              << "p50=" << p50 << "ms p95=" << p95
              << "ms max=" << maximum << "ms\n"
              << "estimated cache bytes: " << estimatedCacheBytes << '\n';

    if (!finiteScores) {
        std::cerr << "SimilarityBench failed: non-finite score\n";
        return 1;
    }
    if (p95 > kP95GateMs) {
        std::cerr << "SimilarityBench failed: p95 exceeds 100ms gate\n";
        return 1;
    }
    if (cacheLoadMs > kCacheLoadGateMs) {
        std::cerr << "SimilarityBench failed: cache load exceeds 2000ms gate\n";
        return 1;
    }
    if (peakWorkingSetBytes > 0 &&
        peakWorkingSetBytes > kPeakWorkingSetGateBytes) {
        std::cerr << "SimilarityBench failed: peak working set exceeds 768MiB gate\n";
        return 1;
    }
    return 0;
}
