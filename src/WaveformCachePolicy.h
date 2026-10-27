#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace WaveformCachePolicy {

// @stage7-contract
// API contract: cache policy constants and mode values are read-only value
// contracts shared by producers and renderers. Callers must preserve their
// units and budget meanings; this header owns no runtime cache storage.

enum class Mode : int {
    MiniOnly = 0,
    MiniAndMain = 1,
};

inline constexpr int kShortAudioOverviewFrames = 512;
inline constexpr int kMediumAudioOverviewFrames = 384;
inline constexpr int kLongAudioOverviewFrames = 192;
inline constexpr int kVeryLongAudioOverviewFrames = 96;
inline constexpr bool kPersistPreviewMinEnvelope = false;
inline constexpr std::uint64_t kOverviewBytesPerValue = sizeof(std::uint8_t);

inline constexpr std::uint64_t kGiB = 1024ull * 1024ull * 1024ull;
inline constexpr std::uint64_t kMiB = 1024ull * 1024ull;
inline constexpr std::uint64_t kWaveformCacheTargetBytes = 512ull * kMiB;
inline constexpr std::uint64_t kWaveformCacheHardLimitBytes = 1ull * kGiB;

inline constexpr int kShortMainDetailPeaks = 3000;
inline constexpr int kMediumMainDetailPeaks = 2000;
inline constexpr int kLongMainDetailPeaks = 1000;
inline constexpr int kZoomOnlyDetailPeaks = 12000;
inline constexpr double kShortMainDetailMaxSeconds = 30.0;
inline constexpr double kMediumMainDetailMaxSeconds = 120.0;
inline constexpr double kPersistentMainDetailMaxSeconds = 300.0;

// Persist mini overviews for every file with a known positive duration.
// A nonzero threshold creates visible uncached rows during list scrolling.
// Mini blobs remain bounded by channels, overview frames, and the global
// waveform cache budget.
inline constexpr double kMiniCachePersistenceMinSeconds = 0.0;

struct PersistentMainDetailPolicy {
    bool persistMainDetail = true;
    int targetPeaks = kShortMainDetailPeaks;
};

// Returns true when scan/rebuild paths should persist the mini overview for a
// file with the given duration. With kMiniCachePersistenceMinSeconds == 0.0
// (the production value, see comment above), this collapses to "always cache":
// unknown durations (<=0) are cached conservatively, and any positive duration
// is cached unconditionally. The function is kept (rather than inlined to
// `return true`) so future tuning can reintroduce a guarded threshold without
// reshaping every call site.
inline bool ShouldPersistScanMini(double durationSeconds)
{
    if (durationSeconds <= 0.0) return true;
    return durationSeconds >= kMiniCachePersistenceMinSeconds;
}

inline int PersistentOverviewFramesForDuration(double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        return kShortAudioOverviewFrames;
    }
    if (durationSeconds <= 30.0) {
        return kShortAudioOverviewFrames;
    }
    if (durationSeconds <= 120.0) {
        return kMediumAudioOverviewFrames;
    }
    if (durationSeconds <= 600.0) {
        return kLongAudioOverviewFrames;
    }
    return kVeryLongAudioOverviewFrames;
}

inline std::uint64_t EstimateOverviewBytes(int frames, int channels)
{
    frames = std::max(1, frames);
    channels = std::max(1, channels);
    return static_cast<std::uint64_t>(frames) *
           static_cast<std::uint64_t>(channels) *
           kOverviewBytesPerValue;
}

inline bool ShouldPersistPreviewMinEnvelope()
{
    return kPersistPreviewMinEnvelope;
}

inline std::uint64_t EstimateOverviewBytesForDuration(double durationSeconds,
                                                      int channels)
{
    return EstimateOverviewBytes(
        PersistentOverviewFramesForDuration(durationSeconds),
        channels);
}

inline int ChooseCompactionTierFrames(double durationSeconds,
                                      std::uint64_t projectedBytes,
                                      std::uint64_t budgetBytes = kWaveformCacheTargetBytes)
{
    const int naturalTier = PersistentOverviewFramesForDuration(durationSeconds);
    if (projectedBytes <= budgetBytes) {
        return naturalTier;
    }
    if (naturalTier > kLongAudioOverviewFrames) {
        return kLongAudioOverviewFrames;
    }
    return kVeryLongAudioOverviewFrames;
}

inline bool ShouldPauseNewOverviewWrites(std::uint64_t projectedBytes,
                                         std::uint64_t hardLimitBytes = kWaveformCacheHardLimitBytes)
{
    return projectedBytes > hardLimitBytes;
}

inline PersistentMainDetailPolicy PersistentMainDetailForDuration(double durationSeconds)
{
    if (durationSeconds > kPersistentMainDetailMaxSeconds) {
        return {false, 0};
    }
    if (durationSeconds > kMediumMainDetailMaxSeconds) {
        return {true, kLongMainDetailPeaks};
    }
    if (durationSeconds > kShortMainDetailMaxSeconds) {
        return {true, kMediumMainDetailPeaks};
    }
    return {true, kShortMainDetailPeaks};
}

inline PersistentMainDetailPolicy PersistentMainDetailForDuration(Mode mode,
                                                                  double durationSeconds)
{
    if (mode == Mode::MiniOnly) {
        return {false, 0};
    }
    return PersistentMainDetailForDuration(durationSeconds);
}

inline bool ShouldPersistGeneratedMainDetail(double durationSeconds,
                                             std::size_t generatedFloatCount,
                                             int channels)
{
    const PersistentMainDetailPolicy policy = PersistentMainDetailForDuration(durationSeconds);
    if (!policy.persistMainDetail || generatedFloatCount == 0) {
        return false;
    }
    const int safeChannels = std::max(1, channels);
    const std::size_t generatedFrames =
        generatedFloatCount / static_cast<std::size_t>(safeChannels);
    return generatedFrames <= static_cast<std::size_t>(policy.targetPeaks);
}

inline bool ShouldPersistGeneratedMainDetail(Mode mode,
                                             double durationSeconds,
                                             std::size_t generatedFloatCount,
                                             int channels)
{
    if (mode == Mode::MiniOnly) {
        return false;
    }
    return ShouldPersistGeneratedMainDetail(durationSeconds, generatedFloatCount, channels);
}

inline const char* ModeStorageValue(Mode mode)
{
    return mode == Mode::MiniOnly ? "mini_only" : "mini_and_main";
}

inline Mode ModeFromStorageValue(const std::string& value)
{
    return value == "mini_only" ? Mode::MiniOnly : Mode::MiniAndMain;
}

} // namespace WaveformCachePolicy
