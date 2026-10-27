#include <cstdio>
#include <vector>

#include "src/WaveformBucketMath.h"

int main()
{
    int failed = 0;

    // Typical short MP3/VBR-like case from diagnostics:
    // totalFrames=84567, req=12000, old logic only covered 84000 frames.
    {
        const int samplesPerPeak = ComputeWaveformSamplesPerPeak(84567, 12000);
        const int coveredPeaks = ComputeWaveformCoveredPeakCount(84567, samplesPerPeak);
        const uint64_t coveredFrames = (uint64_t)coveredPeaks * (uint64_t)samplesPerPeak;
        if (!(samplesPerPeak == 7))
            failed++;
        if (!(coveredPeaks > 12000))
            failed++;
        if (!(coveredFrames >= 84567))
            failed++;
    }

    // Exact multiple should not over-allocate peaks.
    {
        const int samplesPerPeak = ComputeWaveformSamplesPerPeak(84000, 12000);
        const int coveredPeaks = ComputeWaveformCoveredPeakCount(84000, samplesPerPeak);
        if (!(samplesPerPeak == 7))
            failed++;
        if (!(coveredPeaks == 12000))
            failed++;
    }

    // Very short clips must still produce at least one-sample buckets
    // and enough buckets to cover the whole clip.
    {
        const int samplesPerPeak = ComputeWaveformSamplesPerPeak(3934, 12000);
        const int coveredPeaks = ComputeWaveformCoveredPeakCount(3934, samplesPerPeak);
        if (!(samplesPerPeak == 1))
            failed++;
        if (!(coveredPeaks == 3934))
            failed++;
    }

    // Main waveform detail can be decoded at the source file sample rate while
    // playback runs at the engine sample rate. FileInfo.totalFrames must stay
    // in playback frame basis so playhead/seek math does not look double length.
    {
        const uint64_t sourceFrames96k = 739200;
        const uint64_t playbackFrames48k =
            ConvertFrameCountToSampleRateBasis(sourceFrames96k, 96000, 48000);
        if (!(playbackFrames48k == 369600))
            failed++;
        if (!(ConvertFrameCountToSampleRateBasis(sourceFrames96k, 96000, 96000) == sourceFrames96k))
            failed++;
        if (!(ConvertFrameCountToSampleRateBasis(sourceFrames96k, 0, 48000) == 0))
            failed++;
    }

    // A full decode can produce a few tail buckets beyond the requested detail
    // count. Those buckets must be merged back into the final display count so
    // progressive and final main waveforms keep the same X-axis basis.
    {
        std::vector<float> peaks = {1.0f, 2.0f, 3.0f, 4.0f, 10.0f, 20.0f};
        std::vector<float> mins = {-1.0f, -2.0f, -3.0f, -4.0f, -10.0f, -20.0f};
        std::vector<float> rms = {0.1f, 0.2f, 0.3f, 0.4f, 1.0f, 2.0f};
        if (!MergeWaveformTailPeaksIntoTarget(peaks, mins, rms, 1, 4))
            failed++;
        if (!(peaks.size() == 4 && mins.size() == 4 && rms.size() == 4))
            failed++;
        if (!(peaks[0] == 1.0f && peaks[1] == 2.0f && peaks[2] == 10.0f && peaks[3] == 20.0f))
            failed++;
        if (!(mins[2] == -10.0f && mins[3] == -20.0f))
            failed++;
        if (!(rms[2] == 1.0f && rms[3] == 2.0f))
            failed++;
    }

    // Global rebucketing preserves timeline distribution instead of forcing
    // all extra buckets into the tail.
    {
        std::vector<float> peaks = {1.0f, 20.0f, 2.0f, 30.0f, 3.0f, 40.0f};
        std::vector<float> mins = {-1.0f, -20.0f, -2.0f, -30.0f, -3.0f, -40.0f};
        std::vector<float> rms = {0.1f, 2.0f, 0.2f, 3.0f, 0.3f, 4.0f};
        if (!RebucketWaveformPeaksToTarget(peaks, mins, rms, 1, 3))
            failed++;
        if (!(peaks.size() == 3 && mins.size() == 3 && rms.size() == 3))
            failed++;
        if (!(peaks[0] == 20.0f && peaks[1] == 30.0f && peaks[2] == 40.0f))
            failed++;
        if (!(mins[0] == -20.0f && mins[1] == -30.0f && mins[2] == -40.0f))
            failed++;
        if (!(rms[0] == 2.0f && rms[1] == 3.0f && rms[2] == 4.0f))
            failed++;
    }

    // Signed peak streams can be entirely negative when a channel is inverted.
    // Rebucket must preserve the least-negative max peak instead of injecting
    // a synthetic zero that renders the bucket as empty.
    {
        std::vector<float> peaks = {-0.20f, -0.40f, -0.10f, -0.30f};
        std::vector<float> mins = {-0.50f, -0.80f, -0.60f, -0.70f};
        std::vector<float> rms = {0.2f, 0.4f, 0.1f, 0.3f};
        if (!RebucketWaveformPeaksToTarget(peaks, mins, rms, 1, 2))
            failed++;
        if (!(peaks.size() == 2 && mins.size() == 2 && rms.size() == 2))
            failed++;
        if (!(peaks[0] == -0.20f && peaks[1] == -0.10f))
            failed++;
        if (!(mins[0] == -0.80f && mins[1] == -0.70f))
            failed++;
    }

    // Rebucketing is a downsampling operation. Upsampling would fabricate
    // repeated buckets and can make an incomplete cache look complete.
    {
        std::vector<float> peaks = {0.1f, 0.2f};
        std::vector<float> mins = {-0.1f, -0.2f};
        std::vector<float> rms;
        if (RebucketWaveformPeaksToTarget(peaks, mins, rms, 1, 4))
            failed++;
        if (!(peaks.size() == 2 && mins.size() == 2))
            failed++;
    }

    // Adaptive mini-waveform density: 1 peak per 1024 frames, clamped [512, 1024].
    {
        // 8s loop @48k = 384000 frames -> ceil(384000/1024)=375 -> floor 512.
        if (!(ComputeAdaptiveMiniPeakCount(384000) == 512))
            failed++;
        // ~161s @48k = 7728000 frames -> 7547 raw, but mini overviews clamp to 1024.
        if (!(ComputeAdaptiveMiniPeakCount(7728000) == 1024))
            failed++;
        // Very long file is capped at 1024 (300s @48k = 14.4M frames -> 14063 -> 1024).
        if (!(ComputeAdaptiveMiniPeakCount(14400000) == 1024))
            failed++;
        // Short click below the 512-peak floor clamps up (0.5s @48k = 24000 -> 24 -> 512).
        if (!(ComputeAdaptiveMiniPeakCount(24000) == 512))
            failed++;
        // Zero frames degrades to the floor, never zero peaks.
        if (!(ComputeAdaptiveMiniPeakCount(0) == 512))
            failed++;
        // 84567 frames -> ceil(84567/1024)=83 raw -> clamped up to floor 512.
        if (!(ComputeAdaptiveMiniPeakCount(84567) == 512))
            failed++;
        // Short SFX remain at the display-readability floor instead of 141 buckets.
        // 3s @48k = 144000 frames -> ceil(144000/1024)=141 -> 512.
        if (!(ComputeAdaptiveMiniPeakCount(144000) == 512))
            failed++;
    }

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
