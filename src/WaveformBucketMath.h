#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

// @stage7-contract
// API contract: waveform bucket helpers are pure, synchronous value
// transformations. Scalar inputs are copied; vector overloads mutate only
// the caller-owned buffers when they return true and retain no references.
// Invalid dimensions, channel counts, or target sizes return false/neutral
// values under the existing guards and never throw across module boundaries.

// Adaptive mini-waveform density: one signed max/min envelope
// bucket per `framesPerPeak` source frames (sample-rate independent), clamped to
// [minPeaks, maxPeaks]. New databases use this instead of a fixed 128/160 budget
// so short SFX store fewer peaks and long ambiences keep full resolution.
// adaptive floor (minPeaks=512): short SFX are often only 2-3 seconds long,
//   but DrawMiniWaveform renders ~width-pixel bins in 100-300px columns.
//   128 buckets makes fast transients look chunky/unclear; 512 keeps short
//   hits readable (~5ms buckets at 48k/2.7s) while remaining bounded.
//   Legacy callers may still pass fixed 128/160 and the extractor accepts them.
// cap (maxPeaks=1024) keeps long ambience and music beds readable in
//   mini/list views while staying bounded for 3M+ libraries. At the long-source
//   ceiling, the current float max/min pair costs about 8 KB per stereo file.
inline int ComputeAdaptiveMiniPeakCount(uint64_t totalFrames,
                                        int framesPerPeak = 1024,
                                        int minPeaks = 512,
                                        int maxPeaks = 1024)
{
    if (framesPerPeak < 1) framesPerPeak = 1;
    if (minPeaks < 1) minPeaks = 1;
    if (maxPeaks < minPeaks) maxPeaks = minPeaks;
    if (totalFrames == 0) return minPeaks;
    const uint64_t ceilPeaks =
        (totalFrames + (uint64_t)framesPerPeak - 1) / (uint64_t)framesPerPeak;
    if (ceilPeaks < (uint64_t)minPeaks) return minPeaks;
    if (ceilPeaks > (uint64_t)maxPeaks) return maxPeaks;
    return (int)ceilPeaks;
}

inline int ComputeWaveformSamplesPerPeak(uint64_t totalFrames, int targetPeaks)
{
    if (targetPeaks < 1) targetPeaks = 1;
    if (totalFrames == 0) return 1;
    const uint64_t spp64 = totalFrames / (uint64_t)targetPeaks;
    return (int)(std::max<uint64_t>(1, spp64));
}

inline int ComputeWaveformCoveredPeakCount(uint64_t totalFrames, int samplesPerPeak)
{
    if (totalFrames == 0) return 0;
    if (samplesPerPeak < 1) samplesPerPeak = 1;
    const uint64_t spp64 = (uint64_t)samplesPerPeak;
    return (int)((totalFrames + spp64 - 1) / spp64);
}

inline uint64_t ConvertFrameCountToSampleRateBasis(uint64_t frameCount,
                                                   uint32_t sourceSampleRate,
                                                   uint32_t targetSampleRate)
{
    if (frameCount == 0 || sourceSampleRate == 0 || targetSampleRate == 0) {
        return 0;
    }
    if (sourceSampleRate == targetSampleRate) {
        return frameCount;
    }

    const long double scaled =
        ((long double)frameCount * (long double)targetSampleRate) /
        (long double)sourceSampleRate;
    if (scaled >= (long double)std::numeric_limits<uint64_t>::max()) {
        return std::numeric_limits<uint64_t>::max();
    }
    return std::max<uint64_t>(1, (uint64_t)std::llround(scaled));
}

inline bool MergeWaveformTailPeaksIntoTarget(std::vector<float>& peaks,
                                             std::vector<float>& minPeaks,
                                             std::vector<float>& rms,
                                             int channels,
                                             int targetPeaks)
{
    channels = std::max(1, channels);
    if (targetPeaks <= 0 || peaks.empty() || (peaks.size() % (size_t)channels) != 0) {
        return false;
    }

    const int actualPeaks = (int)(peaks.size() / (size_t)channels);
    if (actualPeaks <= targetPeaks) {
        return false;
    }

    const size_t originalSize = peaks.size();
    const bool hasMinPeaks = minPeaks.size() == originalSize;
    const bool hasRms = rms.size() == originalSize;
    const int extraPeaks = actualPeaks - targetPeaks;
    const int tailWindow = std::min(targetPeaks, extraPeaks);
    const int mergeBase = targetPeaks - tailWindow;

    for (int srcPeak = targetPeaks; srcPeak < actualPeaks; ++srcPeak) {
        const int tailIndex = srcPeak - targetPeaks;
        const int dstPeak = mergeBase + std::min(tailWindow - 1, tailIndex * tailWindow / extraPeaks);
        for (int ch = 0; ch < channels; ++ch) {
            const size_t src = (size_t)srcPeak * (size_t)channels + (size_t)ch;
            const size_t dst = (size_t)dstPeak * (size_t)channels + (size_t)ch;
            peaks[dst] = std::max(peaks[dst], peaks[src]);
            if (hasMinPeaks) {
                minPeaks[dst] = std::min(minPeaks[dst], minPeaks[src]);
            }
            if (hasRms) {
                rms[dst] = std::max(rms[dst], rms[src]);
            }
        }
    }

    const size_t targetSize = (size_t)targetPeaks * (size_t)channels;
    peaks.resize(targetSize);
    if (hasMinPeaks) minPeaks.resize(targetSize);
    if (hasRms) rms.resize(targetSize);
    return true;
}

inline bool RebucketWaveformPeaksToTarget(std::vector<float>& peaks,
                                           std::vector<float>& minPeaks,
                                           std::vector<float>& rms,
                                           int channels,
                                           int targetPeaks)
{
    channels = std::max(1, channels);
    if (targetPeaks <= 0 || peaks.empty() || (peaks.size() % (size_t)channels) != 0) {
        return false;
    }

    const int sourcePeaks = (int)(peaks.size() / (size_t)channels);
    if (sourcePeaks <= 0 || sourcePeaks == targetPeaks || sourcePeaks < targetPeaks) {
        return false;
    }

    const size_t originalSize = peaks.size();
    const bool hasMinPeaks = minPeaks.size() == originalSize;
    const bool hasRms = rms.size() == originalSize;
    std::vector<float> rebucketed((size_t)targetPeaks * (size_t)channels, 0.0f);
    std::vector<float> rebucketedMin;
    std::vector<float> rebucketedRms;
    if (hasMinPeaks) rebucketedMin.assign(rebucketed.size(), 0.0f);
    if (hasRms) rebucketedRms.assign(rebucketed.size(), 0.0f);

    for (int dstPeak = 0; dstPeak < targetPeaks; ++dstPeak) {
        int srcBegin = (int)(((int64_t)dstPeak * (int64_t)sourcePeaks) / (int64_t)targetPeaks);
        int srcEnd = (int)((((int64_t)dstPeak + 1) * (int64_t)sourcePeaks) / (int64_t)targetPeaks);
        if (srcEnd <= srcBegin) srcEnd = srcBegin + 1;
        srcBegin = std::clamp(srcBegin, 0, sourcePeaks - 1);
        srcEnd = std::clamp(srcEnd, srcBegin + 1, sourcePeaks);
        for (int ch = 0; ch < channels; ++ch) {
            float maxPeak = -std::numeric_limits<float>::infinity();
            float minPeak = std::numeric_limits<float>::infinity();
            float maxRms = 0.0f;
            for (int srcPeak = srcBegin; srcPeak < srcEnd; ++srcPeak) {
                const size_t src = (size_t)srcPeak * (size_t)channels + (size_t)ch;
                maxPeak = std::max(maxPeak, peaks[src]);
                if (hasMinPeaks) minPeak = std::min(minPeak, minPeaks[src]);
                if (hasRms) maxRms = std::max(maxRms, rms[src]);
            }
            const size_t dst = (size_t)dstPeak * (size_t)channels + (size_t)ch;
            if (!std::isfinite(maxPeak)) maxPeak = 0.0f;
            if (!std::isfinite(minPeak)) minPeak = 0.0f;
            rebucketed[dst] = maxPeak;
            if (hasMinPeaks) rebucketedMin[dst] = minPeak;
            if (hasRms) rebucketedRms[dst] = maxRms;
        }
    }

    peaks = std::move(rebucketed);
    if (hasMinPeaks) minPeaks = std::move(rebucketedMin);
    if (hasRms) rms = std::move(rebucketedRms);
    return true;
}
