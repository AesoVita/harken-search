#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "AudioEngine.h"

// @stage7-contract
// API contract: waveform presentation policies are pure synchronous helpers
// over caller-owned waveform snapshots and scalar settings. They do not
// schedule work, touch caches, or retain references; boolean results only
// indicate eligibility/shape under the existing policy thresholds.

// Main waveform cache misses should reveal left-to-right from the decoder.
// Short files may finish before the first partial publish; do not delay them
// just to make the progressive state visible.

enum class PlaybackStartWaveformWork {
    CurrentFilePreview,
    CurrentFileMainDetail,
    CacheHitDisplay,
    UnrelatedPrewarm,
    MiniWaveformBackground,
    CoverPrefetch,
    DatabaseWrite
};

inline bool ShouldRunWaveformWorkDuringPlaybackStart(PlaybackStartWaveformWork work,
                                                     bool playbackStartWindowActive,
                                                     bool isCurrentFile,
                                                     bool hasUsableCache)
{
    if (!playbackStartWindowActive) return true;
    if (work == PlaybackStartWaveformWork::CacheHitDisplay) return hasUsableCache;
    if (work == PlaybackStartWaveformWork::CurrentFilePreview) return isCurrentFile;
    if (work == PlaybackStartWaveformWork::CurrentFileMainDetail) return isCurrentFile;
    return false;
}

inline const char* PlaybackStartWaveformWorkName(PlaybackStartWaveformWork work)
{
    switch (work) {
    case PlaybackStartWaveformWork::CurrentFilePreview: return "current_file_preview";
    case PlaybackStartWaveformWork::CurrentFileMainDetail: return "current_file_main_detail";
    case PlaybackStartWaveformWork::CacheHitDisplay: return "cache_hit_display";
    case PlaybackStartWaveformWork::UnrelatedPrewarm: return "unrelated_prewarm";
    case PlaybackStartWaveformWork::MiniWaveformBackground: return "mini_waveform_background";
    case PlaybackStartWaveformWork::CoverPrefetch: return "cover_prefetch";
    case PlaybackStartWaveformWork::DatabaseWrite: return "database_write";
    }
    return "unknown";
}

inline int GetWaveformFrameCount(const WaveformData& waveform)
{
    const int channels = std::max(1, waveform.numChannels);
    if (waveform.peaks.empty()) return 0;
    return (int)waveform.peaks.size() / channels;
}

inline void NormalizeFinalWaveformFrameMetadata(WaveformData& waveform)
{
    const int frameCount = GetWaveformFrameCount(waveform);
    waveform.targetPeaks = frameCount;
    waveform.completedPeaks = frameCount;
    waveform.isProgressiveDetail = false;
}

inline bool IsUsableWaveformDetailCacheShape(std::size_t peakValueCount,
                                              int detailChannels,
                                              int fileChannels)
{
    if (peakValueCount == 0) return false;
    if (detailChannels <= 0 || detailChannels > 16) return false;
    if (fileChannels > 0 && detailChannels != fileChannels) return false;
    return (peakValueCount % (std::size_t)detailChannels) == 0;
}

inline float ComputeWaveformSignalTailRatio(const std::vector<float>& peaks,
                                            int channels,
                                            float threshold = 1e-4f)
{
    channels = std::max(1, channels);
    if (peaks.empty()) return 0.0f;
    const int frameCount = (int)peaks.size() / channels;
    if (frameCount <= 0) return 0.0f;

    for (int frame = frameCount - 1; frame >= 0; --frame) {
        for (int ch = 0; ch < channels; ++ch) {
            const size_t idx = (size_t)frame * (size_t)channels + (size_t)ch;
            if (idx < peaks.size() && std::abs(peaks[idx]) > threshold) {
                return (float)(frame + 1) / (float)frameCount;
            }
        }
    }
    return 0.0f;
}

inline bool IsMainDetailCacheContradictedByMiniEnvelope(
    const std::vector<float>& detailPeaks,
    int detailChannels,
    const std::vector<float>& miniPeaks,
    int miniChannels)
{
    detailChannels = std::max(1, detailChannels);
    miniChannels = std::max(1, miniChannels);
    const int detailFrames = detailPeaks.empty() ? 0 : (int)detailPeaks.size() / detailChannels;
    const int miniFrames = miniPeaks.empty() ? 0 : (int)miniPeaks.size() / miniChannels;
    if (detailFrames < 2000 || miniFrames < 16) return false;

    const float detailTail = ComputeWaveformSignalTailRatio(detailPeaks, detailChannels);
    const float miniTail = ComputeWaveformSignalTailRatio(miniPeaks, miniChannels);
    if (miniTail < 0.75f || detailTail <= 0.0f) return false;
    if (detailTail > 0.35f) return false;
    return detailTail * 1.8f < miniTail;
}

inline bool IsMainDetailCacheShapeContradictedByMiniEnvelope(
    const std::vector<float>& detailPeaks,
    const std::vector<float>& detailMinPeaks,
    int detailChannels,
    const std::vector<float>& miniPeaks,
    int miniChannels)
{
    detailChannels = std::max(1, detailChannels);
    miniChannels = std::max(1, miniChannels);
    const int detailFrames = detailPeaks.empty() ? 0 : (int)detailPeaks.size() / detailChannels;
    const int miniFrames = miniPeaks.empty() ? 0 : (int)miniPeaks.size() / miniChannels;
    if (detailFrames < 2000 || miniFrames < 32) return false;

    std::vector<float> miniEnvelope((size_t)miniFrames, 0.0f);
    std::vector<float> detailEnvelope((size_t)miniFrames, 0.0f);
    float miniMax = 0.0f;
    float detailMax = 0.0f;

    for (int frame = 0; frame < miniFrames; ++frame) {
        float value = 0.0f;
        for (int ch = 0; ch < miniChannels; ++ch) {
            const size_t idx = (size_t)frame * (size_t)miniChannels + (size_t)ch;
            if (idx < miniPeaks.size()) {
                value = std::max(value, std::abs(miniPeaks[idx]));
            }
        }
        miniEnvelope[(size_t)frame] = value;
        miniMax = std::max(miniMax, value);
    }

    const bool hasMinDetail = detailMinPeaks.size() == detailPeaks.size();
    for (int dst = 0; dst < miniFrames; ++dst) {
        int begin = (int)(((int64_t)dst * (int64_t)detailFrames) / (int64_t)miniFrames);
        int end = (int)((((int64_t)dst + 1) * (int64_t)detailFrames) / (int64_t)miniFrames);
        if (end <= begin) end = begin + 1;
        begin = std::clamp(begin, 0, detailFrames - 1);
        end = std::clamp(end, begin + 1, detailFrames);

        float value = 0.0f;
        for (int frame = begin; frame < end; ++frame) {
            for (int ch = 0; ch < detailChannels; ++ch) {
                const size_t idx = (size_t)frame * (size_t)detailChannels + (size_t)ch;
                if (idx < detailPeaks.size()) {
                    value = std::max(value, std::abs(detailPeaks[idx]));
                }
                if (hasMinDetail && idx < detailMinPeaks.size()) {
                    value = std::max(value, std::abs(detailMinPeaks[idx]));
                }
            }
        }
        detailEnvelope[(size_t)dst] = value;
        detailMax = std::max(detailMax, value);
    }

    if (miniMax < 1e-4f || detailMax < 1e-4f) return false;

    double sumAbs = 0.0;
    double sumSq = 0.0;
    int compared = 0;
    for (int i = 0; i < miniFrames; ++i) {
        const float miniNorm = miniEnvelope[(size_t)i] / miniMax;
        const float detailNorm = detailEnvelope[(size_t)i] / detailMax;
        if (miniNorm < 0.02f && detailNorm < 0.02f) continue;
        const double diff = (double)miniNorm - (double)detailNorm;
        sumAbs += std::abs(diff);
        sumSq += diff * diff;
        ++compared;
    }

    if (compared < std::max(8, miniFrames / 8)) return false;
    const double mae = sumAbs / (double)compared;
    const double rmse = std::sqrt(sumSq / (double)compared);
    return mae > 0.28 && rmse > 0.38;
}

inline bool IsMainDetailCacheRepeatedCopyOfMiniEnvelope(
    const std::vector<float>& detailPeaks,
    const std::vector<float>& detailMinPeaks,
    int detailChannels,
    const std::vector<float>& miniPeaks,
    int miniChannels)
{
    detailChannels = std::max(1, detailChannels);
    miniChannels = std::max(1, miniChannels);
    const int detailFrames = detailPeaks.empty() ? 0 : (int)detailPeaks.size() / detailChannels;
    const int miniFrames = miniPeaks.empty() ? 0 : (int)miniPeaks.size() / miniChannels;
    if (detailFrames < 2000 || miniFrames < 32) return false;

    const bool hasMinDetail = detailMinPeaks.size() == detailPeaks.size();

    auto buildMiniEnvelope = [&]() {
        std::vector<float> out((size_t)miniFrames, 0.0f);
        for (int frame = 0; frame < miniFrames; ++frame) {
            float value = 0.0f;
            for (int ch = 0; ch < miniChannels; ++ch) {
                const size_t idx = (size_t)frame * (size_t)miniChannels + (size_t)ch;
                if (idx < miniPeaks.size()) {
                    value = std::max(value, std::abs(miniPeaks[idx]));
                }
            }
            out[(size_t)frame] = value;
        }
        return out;
    };

    auto buildDetailEnvelopeRange = [&](int rangeBegin, int rangeEnd) {
        std::vector<float> out((size_t)miniFrames, 0.0f);
        rangeBegin = std::clamp(rangeBegin, 0, detailFrames);
        rangeEnd = std::clamp(rangeEnd, rangeBegin, detailFrames);
        const int rangeFrames = rangeEnd - rangeBegin;
        if (rangeFrames <= 0) return out;

        for (int dst = 0; dst < miniFrames; ++dst) {
            int begin = rangeBegin + (int)(((int64_t)dst * (int64_t)rangeFrames) / (int64_t)miniFrames);
            int end = rangeBegin + (int)((((int64_t)dst + 1) * (int64_t)rangeFrames) / (int64_t)miniFrames);
            if (end <= begin) end = begin + 1;
            begin = std::clamp(begin, rangeBegin, rangeEnd - 1);
            end = std::clamp(end, begin + 1, rangeEnd);

            float value = 0.0f;
            for (int frame = begin; frame < end; ++frame) {
                for (int ch = 0; ch < detailChannels; ++ch) {
                    const size_t idx = (size_t)frame * (size_t)detailChannels + (size_t)ch;
                    if (idx < detailPeaks.size()) {
                        value = std::max(value, std::abs(detailPeaks[idx]));
                    }
                    if (hasMinDetail && idx < detailMinPeaks.size()) {
                        value = std::max(value, std::abs(detailMinPeaks[idx]));
                    }
                }
            }
            out[(size_t)dst] = value;
        }
        return out;
    };

    auto maxValue = [](const std::vector<float>& values) {
        float maxAbs = 0.0f;
        for (float v : values) maxAbs = std::max(maxAbs, std::abs(v));
        return maxAbs;
    };

    std::vector<float> miniEnvelope = buildMiniEnvelope();
    std::vector<float> detailEnvelope = buildDetailEnvelopeRange(0, detailFrames);
    std::vector<float> firstHalfEnvelope = buildDetailEnvelopeRange(0, detailFrames / 2);
    std::vector<float> secondHalfEnvelope = buildDetailEnvelopeRange(detailFrames / 2, detailFrames);

    const float miniMax = maxValue(miniEnvelope);
    const float detailMax = maxValue(detailEnvelope);
    const float firstMax = maxValue(firstHalfEnvelope);
    const float secondMax = maxValue(secondHalfEnvelope);
    if (miniMax < 1e-4f || detailMax < 1e-4f ||
        firstMax < 1e-4f || secondMax < 1e-4f) {
        return false;
    }

    int miniActiveBins = 0;
    for (float v : miniEnvelope) {
        if ((v / miniMax) > 0.05f) ++miniActiveBins;
    }
    if (miniActiveBins < 3 || miniActiveBins > (int)(miniFrames * 0.85f)) {
        return false;
    }

    auto normalizedMae = [&](const std::vector<float>& a, float aMax,
                             const std::vector<float>& b, float bMax) {
        double sumAbs = 0.0;
        int compared = 0;
        const int n = std::min((int)a.size(), (int)b.size());
        for (int i = 0; i < n; ++i) {
            const float an = aMax > 0.0f ? a[(size_t)i] / aMax : 0.0f;
            const float bn = bMax > 0.0f ? b[(size_t)i] / bMax : 0.0f;
            if (an < 0.02f && bn < 0.02f) continue;
            sumAbs += std::abs((double)an - (double)bn);
            ++compared;
        }
        if (compared < std::max(8, miniFrames / 8)) return 1.0;
        return sumAbs / (double)compared;
    };

    const double fullMae = normalizedMae(detailEnvelope, detailMax, miniEnvelope, miniMax);
    const double firstMae = normalizedMae(firstHalfEnvelope, firstMax, miniEnvelope, miniMax);
    const double secondMae = normalizedMae(secondHalfEnvelope, secondMax, miniEnvelope, miniMax);

    return fullMae > 0.26 && firstMae < 0.18 && secondMae < 0.18;
}

inline int NormalizeMainWaveformRequestPeaks(int requestedPeaks, int defaultDetailPeaks)
{
    if (defaultDetailPeaks < 1) defaultDetailPeaks = 1;
    if (requestedPeaks <= 0) return defaultDetailPeaks;
    return std::max(requestedPeaks, defaultDetailPeaks);
}

inline constexpr int kWaveformStyleRms = 2;
inline constexpr int kWaveformStyleSolid = 3;

inline constexpr int DefaultMainWaveformStyle()
{
    return kWaveformStyleSolid;
}

inline int NormalizeMainWaveformConfiguredStyle(int configuredStyle)
{

    return configuredStyle == kWaveformStyleSolid ? kWaveformStyleSolid : kWaveformStyleRms;
}

inline int ResolveMainWaveformDrawStyleForView(int configuredStyle,
                                               float /*displayZoom*/,
                                               bool trimRefineActive)
{
    configuredStyle = NormalizeMainWaveformConfiguredStyle(configuredStyle);
    if (trimRefineActive && configuredStyle != kWaveformStyleRms) {
        return kWaveformStyleSolid;
    }
    return configuredStyle;
}

inline bool IsMainWaveformEligibleForProgressiveDecode(float /*durationSeconds*/,
                                                       int targetPeaks,
                                                       int defaultDetailPeaks)
{
    return targetPeaks >= defaultDetailPeaks;
}

inline bool ShouldUseProgressiveMainWaveDecode(bool hasUsableMainDetailCache,
                                               float durationSeconds,
                                               int targetPeaks,
                                               int defaultDetailPeaks)
{
    if (hasUsableMainDetailCache) return false;
    if (!IsMainWaveformEligibleForProgressiveDecode(
            durationSeconds,
            targetPeaks,
            defaultDetailPeaks)) {
        return false;
    }

    // Progressive first-paint is reserved for default-detail cache misses.
    // Zoom-only requests should decode as explicit inspection work, not as
    // the playback-start progressive lane.
    if (targetPeaks != defaultDetailPeaks) {
        return false;
    }

    // Mini-only libraries should not block first paint on a full main-detail
    // decode. Once the default main-detail lane misses cache, publish partial
    // detail left-to-right for every file duration.
    (void)durationSeconds;
    return true;
}

inline bool ShouldWriteDecodedMainWaveformCache(bool usedApproximateLod,
                                                int decodedPeakValues,
                                                int expectedPeakValues)
{
    if (usedApproximateLod || expectedPeakValues <= 0) return false;
    return decodedPeakValues >= expectedPeakValues;
}

struct MainWaveformDetailRequestTracker {
    std::string pendingFilePath;
    int pendingPeaks = 0;
    std::string submittedFilePath;
    int submittedPeaks = 0;
};

inline bool ShouldEnqueueMainWaveformDetailRequest(
    const MainWaveformDetailRequestTracker& tracker,
    const std::string& filePath,
    int requestedPeaks,
    int defaultDetailPeaks)
{
    if (filePath.empty()) return false;

    const int normalizedPeaks =
        NormalizeMainWaveformRequestPeaks(requestedPeaks, defaultDetailPeaks);
    if (tracker.pendingFilePath == filePath &&
        tracker.pendingPeaks >= normalizedPeaks) {
        return false;
    }
    if (tracker.submittedFilePath == filePath &&
        tracker.submittedPeaks >= normalizedPeaks) {
        return false;
    }
    return true;
}

inline void RecordPendingMainWaveformDetailRequest(
    MainWaveformDetailRequestTracker& tracker,
    const std::string& filePath,
    int requestedPeaks,
    int defaultDetailPeaks)
{
    tracker.pendingFilePath = filePath;
    tracker.pendingPeaks =
        NormalizeMainWaveformRequestPeaks(requestedPeaks, defaultDetailPeaks);
    if (tracker.submittedFilePath != filePath) {
        tracker.submittedFilePath.clear();
        tracker.submittedPeaks = 0;
    }
}

inline void RecordSubmittedMainWaveformDetailRequest(
    MainWaveformDetailRequestTracker& tracker)
{
    tracker.submittedFilePath = tracker.pendingFilePath;
    tracker.submittedPeaks = tracker.pendingPeaks;
    tracker.pendingFilePath.clear();
    tracker.pendingPeaks = 0;
}

inline void ClearMainWaveformDetailRequestTracker(
    MainWaveformDetailRequestTracker& tracker)
{
    tracker = MainWaveformDetailRequestTracker{};
}

inline int MainWaveformMinProgressivePreviewPeaks(int targetPeaks)
{
    if (targetPeaks <= 0) return 0;
    const int scaledPreview = std::clamp(targetPeaks / 24, 384, 1200);
    return std::min(targetPeaks, scaledPreview);
}

inline bool IsFinalPrecisionProgressiveMainWave(const WaveformData& waveform,
                                                float durationSeconds,
                                                int defaultDetailPeaks)
{
    if (!waveform.isReady ||
        waveform.isFallbackPlaceholder ||
        waveform.peaks.empty() ||
        !waveform.isProgressiveDetail) {
        return false;
    }

    if (!IsMainWaveformEligibleForProgressiveDecode(
            durationSeconds,
            waveform.targetPeaks,
            defaultDetailPeaks)) {
        return false;
    }

    if (waveform.completedPeaks <= 0 || waveform.completedPeaks >= waveform.targetPeaks) {
        return false;
    }

    if (waveform.completedPeaks < MainWaveformMinProgressivePreviewPeaks(waveform.targetPeaks)) {
        return false;
    }

    return GetWaveformFrameCount(waveform) >= waveform.targetPeaks;
}

inline bool IsStableFinalMainWaveform(const WaveformData& waveform,
                                      bool detailPending,
                                      int defaultDetailPeaks)
{
    if (!waveform.isReady ||
        waveform.isFallbackPlaceholder ||
        waveform.peaks.empty() ||
        waveform.isProgressiveDetail ||
        detailPending) {
        return false;
    }

    const int frameCount = GetWaveformFrameCount(waveform);
    const int targetPeaks = waveform.targetPeaks > 0 ? waveform.targetPeaks : frameCount;
    const int completedPeaks = waveform.completedPeaks > 0 ? waveform.completedPeaks : frameCount;

    if (targetPeaks < defaultDetailPeaks || frameCount < defaultDetailPeaks) {
        return false;
    }

    return completedPeaks >= targetPeaks;
}
