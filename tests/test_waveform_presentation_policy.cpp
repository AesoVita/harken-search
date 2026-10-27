#include <cstdio>
#include <string>

#include "src/WaveformPresentationPolicy.h"

static WaveformData MakeWaveform(int frameCount,
                                 int targetPeaks,
                                 int completedPeaks,
                                 bool isProgressiveDetail,
                                 bool isReady = true)
{
    WaveformData waveform;
    waveform.isReady = isReady;
    waveform.numChannels = 1;
    waveform.isFallbackPlaceholder = false;
    waveform.targetPeaks = targetPeaks;
    waveform.completedPeaks = completedPeaks;
    waveform.isProgressiveDetail = isProgressiveDetail;
    waveform.peaks.assign((size_t)frameCount, 0.5f);
    return waveform;
}

int main()
{
    int failed = 0;
    const int defaultDetailPeaks = 3000;

    if (NormalizeMainWaveformRequestPeaks(1000, defaultDetailPeaks) != defaultDetailPeaks)
        failed++;
    if (NormalizeMainWaveformRequestPeaks(defaultDetailPeaks * 2, defaultDetailPeaks) != defaultDetailPeaks * 2)
        failed++;

    const int minUsefulPreviewPeaks = MainWaveformMinProgressivePreviewPeaks(defaultDetailPeaks);
    if (minUsefulPreviewPeaks < 384 || minUsefulPreviewPeaks > 1200)
        failed++;
    if (MainWaveformMinProgressivePreviewPeaks(0) != 0)
        failed++;

    if (!ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::CurrentFilePreview, true, true, false))
        failed++;
    if (!ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::CurrentFileMainDetail, true, true, false))
        failed++;
    if (!ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::CacheHitDisplay, true, false, true))
        failed++;
    if (ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::UnrelatedPrewarm, true, false, false))
        failed++;
    if (ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::MiniWaveformBackground, true, false, false))
        failed++;
    if (ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::DatabaseWrite, true, true, false))
        failed++;
    if (!ShouldRunWaveformWorkDuringPlaybackStart(
            PlaybackStartWaveformWork::UnrelatedPrewarm, false, false, false))
        failed++;
    if (std::string(PlaybackStartWaveformWorkName(
            PlaybackStartWaveformWork::DatabaseWrite)) != "database_write")
        failed++;

    if (!IsMainWaveformEligibleForProgressiveDecode(0.5f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!IsMainWaveformEligibleForProgressiveDecode(60.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (IsMainWaveformEligibleForProgressiveDecode(240.0f, defaultDetailPeaks - 1, defaultDetailPeaks))
        failed++;

    if (ShouldUseProgressiveMainWaveDecode(true, 240.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 240.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 0.5f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 7.9f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 30.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (ShouldUseProgressiveMainWaveDecode(false, 240.0f, defaultDetailPeaks - 1, defaultDetailPeaks))
        failed++;

    if (!ShouldWriteDecodedMainWaveformCache(false, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (ShouldWriteDecodedMainWaveformCache(true, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (ShouldWriteDecodedMainWaveformCache(false, defaultDetailPeaks - 1, defaultDetailPeaks))
        failed++;

    {
        MainWaveformDetailRequestTracker tracker;
        const std::string path = "C:/library/punch.wav";

        if (!ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        RecordPendingMainWaveformDetailRequest(
            tracker, path, defaultDetailPeaks, defaultDetailPeaks);
        if (ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        RecordSubmittedMainWaveformDetailRequest(tracker);
        if (ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        if (!ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks * 2, defaultDetailPeaks))
            failed++;

        RecordPendingMainWaveformDetailRequest(
            tracker, path, defaultDetailPeaks * 2, defaultDetailPeaks);
        RecordSubmittedMainWaveformDetailRequest(tracker);
        if (ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        if (!ShouldEnqueueMainWaveformDetailRequest(
                tracker, "C:/library/kick.wav", defaultDetailPeaks, defaultDetailPeaks))
            failed++;
    }

    if (!IsUsableWaveformDetailCacheShape((size_t)defaultDetailPeaks * 2, 2, 2))
        failed++;
    if (IsUsableWaveformDetailCacheShape((size_t)defaultDetailPeaks * 2, 1, 2))
        failed++;
    if (IsUsableWaveformDetailCacheShape((size_t)defaultDetailPeaks * 2 + 1, 2, 2))
        failed++;

    if (NormalizeMainWaveformConfiguredStyle(0) != 2)
        failed++;
    if (NormalizeMainWaveformConfiguredStyle(1) != 2)
        failed++;
    if (NormalizeMainWaveformConfiguredStyle(2) != 2)
        failed++;
    if (NormalizeMainWaveformConfiguredStyle(3) != 3)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(0, 1.0f, false) != 2)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(1, 4.0f, false) != 2)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(2, 4.0f, false) != 2)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(3, 4.0f, false) != 3)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(3, 4.0f, true) != 3)
        failed++;
    if (DefaultMainWaveformStyle() != 3)
        failed++;

    {
        WaveformData stereoFinal;
        stereoFinal.isReady = true;
        stereoFinal.numChannels = 2;
        stereoFinal.isStereo = true;
        stereoFinal.isProgressiveDetail = true;
        stereoFinal.targetPeaks = defaultDetailPeaks * 2;
        stereoFinal.completedPeaks = defaultDetailPeaks * 2;
        stereoFinal.peaks.assign((size_t)defaultDetailPeaks * 2, 0.25f);

        NormalizeFinalWaveformFrameMetadata(stereoFinal);
        if (stereoFinal.targetPeaks != defaultDetailPeaks)
            failed++;
        if (stereoFinal.completedPeaks != defaultDetailPeaks)
            failed++;
        if (stereoFinal.isProgressiveDetail)
            failed++;
    }

    if (ShouldUseProgressiveMainWaveDecode(true, 240.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 240.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 7.9f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (!ShouldUseProgressiveMainWaveDecode(false, 30.0f, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (ShouldUseProgressiveMainWaveDecode(false, 240.0f, 8192, defaultDetailPeaks))
        failed++;

    if (!ShouldWriteDecodedMainWaveformCache(false, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (ShouldWriteDecodedMainWaveformCache(true, defaultDetailPeaks, defaultDetailPeaks))
        failed++;
    if (ShouldWriteDecodedMainWaveformCache(false, defaultDetailPeaks - 1, defaultDetailPeaks))
        failed++;

    {
        MainWaveformDetailRequestTracker tracker;
        const std::string path = "C:/library/punch.wav";

        if (!ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        RecordPendingMainWaveformDetailRequest(
            tracker, path, defaultDetailPeaks, defaultDetailPeaks);
        if (ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        RecordSubmittedMainWaveformDetailRequest(tracker);
        if (ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        if (!ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks * 2, defaultDetailPeaks))
            failed++;

        RecordPendingMainWaveformDetailRequest(
            tracker, path, defaultDetailPeaks * 2, defaultDetailPeaks);
        RecordSubmittedMainWaveformDetailRequest(tracker);
        if (ShouldEnqueueMainWaveformDetailRequest(
                tracker, path, defaultDetailPeaks, defaultDetailPeaks))
            failed++;

        if (!ShouldEnqueueMainWaveformDetailRequest(
                tracker, "C:/library/kick.wav", defaultDetailPeaks, defaultDetailPeaks))
            failed++;
    }

    if (!IsUsableWaveformDetailCacheShape((size_t)defaultDetailPeaks * 2, 2, 2))
        failed++;
    if (IsUsableWaveformDetailCacheShape((size_t)defaultDetailPeaks * 2, 1, 2))
        failed++;
    if (IsUsableWaveformDetailCacheShape((size_t)defaultDetailPeaks * 2 + 1, 2, 2))
        failed++;

    if (NormalizeMainWaveformConfiguredStyle(0) != 2)
        failed++;
    if (NormalizeMainWaveformConfiguredStyle(1) != 2)
        failed++;
    if (NormalizeMainWaveformConfiguredStyle(2) != 2)
        failed++;
    if (NormalizeMainWaveformConfiguredStyle(3) != 3)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(0, 1.0f, false) != 2)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(1, 4.0f, false) != 2)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(2, 4.0f, false) != 2)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(3, 4.0f, false) != 3)
        failed++;
    if (ResolveMainWaveformDrawStyleForView(3, 4.0f, true) != 3)
        failed++;

    {
        WaveformData stereoFinal;
        stereoFinal.isReady = true;
        stereoFinal.numChannels = 2;
        stereoFinal.isStereo = true;
        stereoFinal.isProgressiveDetail = true;
        stereoFinal.targetPeaks = defaultDetailPeaks * 2;
        stereoFinal.completedPeaks = defaultDetailPeaks * 2;
        stereoFinal.peaks.assign((size_t)defaultDetailPeaks * 2, 0.25f);

        NormalizeFinalWaveformFrameMetadata(stereoFinal);
        if (stereoFinal.targetPeaks != defaultDetailPeaks)
            failed++;
        if (stereoFinal.completedPeaks != defaultDetailPeaks)
            failed++;
        if (stereoFinal.isProgressiveDetail)
            failed++;
    }

    {
        const WaveformData progressive = MakeWaveform(
            defaultDetailPeaks,
            defaultDetailPeaks,
            defaultDetailPeaks / 3,
            true);
        if (!IsFinalPrecisionProgressiveMainWave(progressive, 120.0f, defaultDetailPeaks))
            failed++;
        if (IsStableFinalMainWaveform(progressive, true, defaultDetailPeaks))
            failed++;
    }

    {
        const WaveformData lowPrecisionProgressive = MakeWaveform(
            defaultDetailPeaks / 2,
            defaultDetailPeaks / 2,
            defaultDetailPeaks / 2,
            true);
        if (IsFinalPrecisionProgressiveMainWave(lowPrecisionProgressive, 120.0f, defaultDetailPeaks))
            failed++;
    }

    {
        const WaveformData shortAudioProgressive = MakeWaveform(
            defaultDetailPeaks,
            defaultDetailPeaks,
            defaultDetailPeaks / 2,
            true);
        if (!IsFinalPrecisionProgressiveMainWave(shortAudioProgressive, 7.0f, defaultDetailPeaks))
            failed++;
    }

    {
        const WaveformData tooEarlyProgressive = MakeWaveform(
            defaultDetailPeaks,
            defaultDetailPeaks,
            minUsefulPreviewPeaks - 1,
            true);
        if (IsFinalPrecisionProgressiveMainWave(tooEarlyProgressive, 120.0f, defaultDetailPeaks))
            failed++;

        const WaveformData usefulProgressive = MakeWaveform(
            defaultDetailPeaks,
            defaultDetailPeaks,
            minUsefulPreviewPeaks,
            true);
        if (!IsFinalPrecisionProgressiveMainWave(usefulProgressive, 120.0f, defaultDetailPeaks))
            failed++;
    }

    {
        const WaveformData stableFinal = MakeWaveform(
            defaultDetailPeaks,
            defaultDetailPeaks,
            defaultDetailPeaks,
            false);
        if (!IsStableFinalMainWaveform(stableFinal, false, defaultDetailPeaks))
            failed++;
        if (IsStableFinalMainWaveform(stableFinal, true, defaultDetailPeaks))
            failed++;
    }

    {
        std::vector<float> mini((size_t)64 * 2, 0.2f);
        mini[63 * 2 + 0] = 0.0f;
        mini[63 * 2 + 1] = 0.0f;

        std::vector<float> truncatedDetail((size_t)defaultDetailPeaks * 2, 0.0f);
        for (int i = 0; i < 500; ++i) {
            truncatedDetail[(size_t)i * 2] = 0.2f;
            truncatedDetail[(size_t)i * 2 + 1] = 0.2f;
        }

        if (!IsMainDetailCacheContradictedByMiniEnvelope(
                truncatedDetail, 2, mini, 2))
            failed++;
    }

    {
        std::vector<float> mini((size_t)64 * 2, 0.0f);
        std::vector<float> detail((size_t)defaultDetailPeaks * 2, 0.0f);
        for (int i = 0; i < 500; ++i) {
            detail[(size_t)i * 2] = 0.2f;
            detail[(size_t)i * 2 + 1] = 0.2f;
        }
        for (int i = 0; i < 3; ++i) {
            mini[(size_t)i * 2] = 0.2f;
            mini[(size_t)i * 2 + 1] = 0.2f;
        }

        if (IsMainDetailCacheContradictedByMiniEnvelope(detail, 2, mini, 2))
            failed++;
    }

    {
        std::vector<float> mini(128, 0.0f);
        for (int i = 36; i <= 56; ++i) {
            const float x = (float)(i - 46) / 10.0f;
            mini[(size_t)i] = 1.0f - std::min(1.0f, std::abs(x));
        }

        std::vector<float> detail((size_t)defaultDetailPeaks * 2, 0.0f);
        std::vector<float> minDetail(detail.size(), 0.0f);
        auto addDetailHump = [&](int centerBin) {
            const int center = centerBin * defaultDetailPeaks / 128;
            const int radius = defaultDetailPeaks / 128 * 10;
            for (int i = center - radius; i <= center + radius; ++i) {
                if (i < 0 || i >= defaultDetailPeaks) continue;
                const float amp = 1.0f - std::min(1.0f, std::abs((float)(i - center) / (float)radius));
                detail[(size_t)i * 2] = std::max(detail[(size_t)i * 2], amp);
                minDetail[(size_t)i * 2 + 1] = std::min(minDetail[(size_t)i * 2 + 1], -amp);
            }
        };
        addDetailHump(18);
        addDetailHump(79);

        if (!IsMainDetailCacheShapeContradictedByMiniEnvelope(
                detail, minDetail, 2, mini, 1))
            failed++;
    }

    {
        std::vector<float> mini(128, 0.0f);
        std::vector<float> detail((size_t)defaultDetailPeaks, 0.0f);
        std::vector<float> minDetail(detail.size(), 0.0f);
        for (int i = 36; i <= 56; ++i) {
            const float x = (float)(i - 46) / 10.0f;
            mini[(size_t)i] = 1.0f - std::min(1.0f, std::abs(x));
        }
        const int center = 46 * defaultDetailPeaks / 128;
        const int radius = defaultDetailPeaks / 128 * 10;
        for (int i = center - radius; i <= center + radius; ++i) {
            if (i < 0 || i >= defaultDetailPeaks) continue;
            const float amp = 1.0f - std::min(1.0f, std::abs((float)(i - center) / (float)radius));
            detail[(size_t)i] = amp;
            minDetail[(size_t)i] = -amp;
        }

        if (IsMainDetailCacheShapeContradictedByMiniEnvelope(
                detail, minDetail, 1, mini, 1))
            failed++;
    }

    {
        std::vector<float> mini(128, 0.0f);
        auto addMiniHump = [&](int centerBin, int radius, float gain) {
            for (int i = centerBin - radius; i <= centerBin + radius; ++i) {
                if (i < 0 || i >= (int)mini.size()) continue;
                const float x = (float)(i - centerBin) / (float)std::max(1, radius);
                mini[(size_t)i] = std::max(
                    mini[(size_t)i],
                    gain * (1.0f - std::min(1.0f, std::abs(x))));
            }
        };
        addMiniHump(18, 4, 0.85f);
        addMiniHump(51, 7, 0.45f);
        addMiniHump(96, 5, 0.70f);

        std::vector<float> repeatedDetail((size_t)defaultDetailPeaks, 0.0f);
        std::vector<float> repeatedMin(repeatedDetail.size(), 0.0f);
        const int half = defaultDetailPeaks / 2;
        for (int i = 0; i < defaultDetailPeaks; ++i) {
            const int halfFrame = (i < half) ? i : (i - half);
            const int miniIdx = std::clamp(halfFrame * (int)mini.size() / half, 0, (int)mini.size() - 1);
            repeatedDetail[(size_t)i] = mini[(size_t)miniIdx];
            repeatedMin[(size_t)i] = -mini[(size_t)miniIdx];
        }

        if (!IsMainDetailCacheRepeatedCopyOfMiniEnvelope(
                repeatedDetail, repeatedMin, 1, mini, 1))
            failed++;

        std::vector<float> matchingDetail((size_t)defaultDetailPeaks, 0.0f);
        std::vector<float> matchingMin(matchingDetail.size(), 0.0f);
        for (int i = 0; i < defaultDetailPeaks; ++i) {
            const int miniIdx = std::clamp(i * (int)mini.size() / defaultDetailPeaks, 0, (int)mini.size() - 1);
            matchingDetail[(size_t)i] = mini[(size_t)miniIdx];
            matchingMin[(size_t)i] = -mini[(size_t)miniIdx];
        }

        if (IsMainDetailCacheRepeatedCopyOfMiniEnvelope(
                matchingDetail, matchingMin, 1, mini, 1))
            failed++;
    }

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
