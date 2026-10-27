#include <cstdio>
#include <string>

#include "src/SpectrogramMath.h"

int main()
{
    int failed = 0;

    {
        const auto plan = BuildSpectrogramPlan(0, 4096);
        if (plan.ok) failed++;
    }

    {
        const auto plan = BuildSpectrogramPlan(20, 4096);
        if (!plan.ok) failed++;
        if (!(plan.fftSize == 16)) failed++;
        if (!(plan.numFrames == 3)) failed++;
        if (!(plan.numBins == 8)) failed++;
    }

    {
        const auto plan = BuildSpectrogramPlan(64, 4096);
        if (!plan.ok) failed++;
        if (!(plan.fftSize == 64)) failed++;
        if (!(plan.numFrames == 1)) failed++;
    }

    {
        const auto plan = BuildSpectrogramPlan(3934, 4096);
        if (!plan.ok) failed++;
        if (!(plan.fftSize == 2048)) failed++;
        if (!(plan.numFrames >= 1)) failed++;
    }

    {
        if (!(ComputeReadableSpectrogramTextureHeight(0) == 0)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(8) == 8)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(255) == 255)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(256) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(512) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(1024) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(2048) == 2048)) failed++;
        if (!(ComputeReadableSpectrogramTextureHeight(4096) == 2048)) failed++;
    }

    {
        if (!(ComputeReadableSpectrogramTextureWidth(256.0f, 2.0f, 2) == 384)) failed++;
        if (!(ComputeReadableSpectrogramTextureWidth(1200.0f, 2.0f, 2) > 1200)) failed++;
        if (!(ComputeReadableSpectrogramTextureWidth(2400.0f, 2.0f, 2) > 2048)) failed++;
        if (!(ComputeReadableSpectrogramTextureWidth(2400.0f, 2.0f, 2) == 2640)) failed++;
        if (!(ComputeReadableSpectrogramTextureWidth(4000.0f, 2.0f, 2) == 4096)) failed++;
        if (!(ComputeReadableSpectrogramTextureWidth(2400.0f, 2000.0f, 2) <= 2048)) failed++;
        if (!(ComputeReadableSpectrogramTextureWidth(2400.0f, 2.0f, 8) <= 2048)) failed++;
    }

    {
        const int fftSize = 2048;
        const int textureWidth = 101;
        const uint64_t framesRead = 48000;
        const int firstStart = ComputeSpectrogramWindowStartForColumn(0, textureWidth, framesRead, fftSize);
        const int middleStart = ComputeSpectrogramWindowStartForColumn(50, textureWidth, framesRead, fftSize);
        const int lastStart = ComputeSpectrogramWindowStartForColumn(100, textureWidth, framesRead, fftSize);
        if (!(firstStart == -1024)) failed++;
        if (!(middleStart + fftSize / 2 >= 23999 && middleStart + fftSize / 2 <= 24000)) failed++;
        if (!(lastStart + fftSize / 2 == 47999)) failed++;
    }

    {
        const int start = ComputeSpectrogramWindowStartForNorm(0.25, 48000, 2048);
        if (!(start + 1024 >= 11999 && start + 1024 <= 12000)) failed++;
    }

    {
        const bool visible = CanDisplaySpectrogramTextureForFile(
            true,
            "",
            "E:/audio/current.wav",
            "E:/audio/current.wav");
        if (!visible) failed++;
    }

    {
        const bool visible = CanDisplaySpectrogramTextureForFile(
            true,
            "E:/audio/other.wav",
            "",
            "E:/audio/current.wav");
        if (visible) failed++;
    }

    {
        const bool visible = CanDisplaySpectrogramTextureForTimeline(
            true,
            "E:/audio/current.wav",
            "",
            "E:/audio/current.wav",
            48000,
            48000,
            48000,
            48000);
        if (!visible) failed++;
    }

    {
        const bool visible = CanDisplaySpectrogramTextureForTimeline(
            true,
            "E:/audio/current.wav",
            "",
            "E:/audio/current.wav",
            44100,
            48000,
            48000,
            48000);
        if (visible) failed++;
    }

    {
        const bool visible = CanDisplaySpectrogramTextureForTimeline(
            true,
            "E:/audio/current.wav",
            "",
            "E:/audio/current.wav",
            48000,
            44100,
            48000,
            48000);
        if (visible) failed++;
    }

    {
        const bool visible = CanDisplaySpectrogramTextureForTimeline(
            true,
            "E:/audio/current.wav",
            "",
            "E:/audio/current.wav",
            0,
            48000,
            48000,
            48000);
        if (visible) failed++;
    }

    {
        const float silence = NormalizeSpectrogramDbForReadability(-120.0f);
        const float veryWeak = NormalizeSpectrogramDbForReadability(-90.0f);
        const float weak = NormalizeSpectrogramDbForReadability(-78.0f);
        const float mid = NormalizeSpectrogramDbForReadability(-48.0f);
        const float hot = NormalizeSpectrogramDbForReadability(-18.0f);

        if (!(silence == 0.0f)) failed++;
        if (!(veryWeak > 0.06f && veryWeak < 0.14f)) failed++;
        if (!(weak > 0.22f && weak < 0.34f)) failed++;
        if (!(mid > 0.58f && mid < 0.72f)) failed++;
        if (!(mid > weak && mid < hot)) failed++;
        if (!(hot > 0.90f && hot <= 1.0f)) failed++;
    }

    {
        const auto background = MapReadableSpectrogramHeatRgb(0.0f);
        const auto weak = MapReadableSpectrogramHeatRgb(0.20f);
        const auto mid = MapReadableSpectrogramHeatRgb(0.50f);
        const auto strong = MapReadableSpectrogramHeatRgb(0.75f);
        const auto transient = MapReadableSpectrogramHeatRgb(0.92f);
        const auto clipping = MapReadableSpectrogramHeatRgb(1.0f);

        auto luminance = [](SpectrogramRgb c) {
            return 0.2126f * (float)c.r + 0.7152f * (float)c.g + 0.0722f * (float)c.b;
        };

        if (!(background.r <= 5 && background.g <= 8 && background.b <= 12)) failed++;
        if (!(weak.b > weak.g && weak.g > weak.r && weak.b >= 35 && weak.b <= 70)) failed++;
        if (!(mid.r > mid.g && mid.g > mid.b && mid.r >= 95 && mid.r <= 150)) failed++;
        if (!(strong.r >= 220 && strong.g >= 105 && strong.g <= 150 && strong.b <= 45)) failed++;
        if (!(transient.r >= 250 && transient.g >= 205 && transient.g <= 235 && transient.b >= 75 && transient.b <= 120)) failed++;
        if (!(clipping.r >= 248 && clipping.g >= 240 && clipping.b >= 185)) failed++;
        if (!(luminance(background) < luminance(weak) &&
              luminance(weak) < luminance(mid) &&
              luminance(mid) < luminance(strong) &&
              luminance(strong) < luminance(transient) &&
              luminance(transient) < luminance(clipping))) failed++;
    }


    {
        const auto preset = GetReadableSpectrogramPreset();
        if (!(preset.fftSize == 4096)) failed++;
        if (!(preset.split == true)) failed++;
        if (!(preset.blend >= 0.58f && preset.blend <= 0.66f)) failed++;
        if (!(preset.textureWidth >= 1024)) failed++;
    }

    {
        if (!(ComputeReadableSpectrogramAnalysisFftSize(4096, 0.50f) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(4096, 2.00f) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(4096, 4.00f) == 2048)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(4096, 120.0f) == 2048)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(4096, 240.0f) == 4096)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(2048, 0.50f) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(1024, 2.00f) == 1024)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(512, 2.00f) == 512)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(512, 240.0f) == 512)) failed++;
        if (!(ComputeReadableSpectrogramAnalysisFftSize(8192, 0.50f) == 8192)) failed++;
    }

    {
        const auto manual512 = BuildSpectrogramMultiresFftBank(512, 2.0f);
        if (manual512.enabled) failed++;
        if (!(SelectSpectrogramFftForFrequency(manual512, 10000.0f) == 512)) failed++;

        const auto shortBank = BuildSpectrogramMultiresFftBank(4096, 2.0f);
        if (!shortBank.enabled) failed++;
        if (!(shortBank.lowFftSize == 4096)) failed++;
        if (!(shortBank.midFftSize == 2048)) failed++;
        if (!(shortBank.highFftSize == 1024)) failed++;
        if (!(SelectSpectrogramFftForFrequency(shortBank, 120.0f) == 4096)) failed++;
        if (!(SelectSpectrogramFftForFrequency(shortBank, 1000.0f) == 2048)) failed++;
        if (!(SelectSpectrogramFftForFrequency(shortBank, 9000.0f) == 1024)) failed++;
        const auto lowMidStart = SelectSpectrogramFftBlendForFrequency(shortBank, 550.0f);
        const auto lowMidCenter = SelectSpectrogramFftBlendForFrequency(shortBank, 700.0f);
        const auto lowMidEnd = SelectSpectrogramFftBlendForFrequency(shortBank, 850.0f);
        if (!(lowMidStart.primaryFftSize == 4096 && lowMidStart.secondaryFftSize == 2048 &&
              lowMidStart.secondaryWeight <= 0.001f)) failed++;
        if (!(lowMidCenter.primaryFftSize == 4096 && lowMidCenter.secondaryFftSize == 2048 &&
              lowMidCenter.secondaryWeight > 0.45f && lowMidCenter.secondaryWeight < 0.55f)) failed++;
        if (!(lowMidEnd.primaryFftSize == 2048 && lowMidEnd.secondaryFftSize == 2048 &&
              lowMidEnd.secondaryWeight == 0.0f)) failed++;

        const auto midHighCenter = SelectSpectrogramFftBlendForFrequency(shortBank, 5000.0f);
        if (!(midHighCenter.primaryFftSize == 2048 && midHighCenter.secondaryFftSize == 1024 &&
              midHighCenter.secondaryWeight > 0.45f && midHighCenter.secondaryWeight < 0.55f)) failed++;

        const auto midBank = BuildSpectrogramMultiresFftBank(2048, 10.0f);
        if (!midBank.enabled) failed++;
        if (!(midBank.lowFftSize == 2048)) failed++;
        if (!(midBank.midFftSize == 1024)) failed++;
        if (!(midBank.highFftSize == 512)) failed++;

        const auto longBank = BuildSpectrogramMultiresFftBank(4096, 240.0f);
        if (longBank.enabled) failed++;
        if (!(longBank.lowFftSize == 4096 &&
              longBank.midFftSize == 4096 &&
              longBank.highFftSize == 4096)) failed++;

        const float scale1024 = ComputeSpectrogramWindowMagnitudeScale(0.35875f * 1024.0f, 1024);
        const float scale2048 = ComputeSpectrogramWindowMagnitudeScale(0.35875f * 2048.0f, 2048);
        if (!(scale1024 > 0.0f && scale2048 > 0.0f)) failed++;
        if (!(scale1024 > scale2048 * 1.99f && scale1024 < scale2048 * 2.01f)) failed++;
        if (!(ComputeSpectrogramWindowMagnitudeScale(0.0f, 1024) == 0.0f)) failed++;
    }

    {
        const int version = GetReadableSpectrogramPresetVersion();
        if (!(version >= 4)) failed++;
        if (!ShouldApplyReadableSpectrogramPresetMigration(0, 1024, false, 0.50f)) failed++;
        if (!ShouldApplyReadableSpectrogramPresetMigration(0, 2048, false, 0.50f)) failed++;
        if (!ShouldApplyReadableSpectrogramPresetMigration(1, 4096, true, 0.62f)) failed++;
        if (!ShouldApplyReadableSpectrogramPresetMigration(2, 1024, true, 0.82f)) failed++;
        if (!ShouldApplyReadableSpectrogramPresetMigration(2, 2048, true, 0.82f)) failed++;
        if (!ShouldApplyReadableSpectrogramPresetMigration(3, 4096, true, 0.82f)) failed++;
        if (ShouldApplyReadableSpectrogramPresetMigration(version, 1024, false, 0.50f)) failed++;
        if (ShouldApplyReadableSpectrogramPresetMigration(0, 4096, false, 0.50f)) failed++;
        if (ShouldApplyReadableSpectrogramPresetMigration(1, 8192, true, 0.80f)) failed++;
        if (ShouldApplyReadableSpectrogramPresetMigration(3, 4096, true, 0.62f)) failed++;
        if (ShouldApplyReadableSpectrogramPresetMigration(3, 4096, true, 0.95f)) failed++;
        if (ShouldApplyReadableSpectrogramPresetMigration(2, 8192, true, 0.82f)) failed++;
    }

    {
        const float sampleRate = 48000.0f;
        const float minHz = GetReadableSpectrogramMinFrequencyHz();
        const float maxHz = GetReadableSpectrogramMaxFrequencyHz(sampleRate);
        const float alpha = GetReadableSpectrogramHybridFrequencyAlpha();
        const float n100 = SpectrogramFrequencyHzToReadableNorm(100.0f, sampleRate);
        const float n500 = SpectrogramFrequencyHzToReadableNorm(500.0f, sampleRate);
        const float n1k = SpectrogramFrequencyHzToReadableNorm(1000.0f, sampleRate);
        const float n10k = SpectrogramFrequencyHzToReadableNorm(10000.0f, sampleRate);
        const float n20k = SpectrogramFrequencyHzToReadableNorm(20000.0f, sampleRate);

        if (!(minHz == 0.0f)) failed++;
        if (!(maxHz == 20000.0f)) failed++;
        if (!(alpha > 0.00045f && alpha < 0.00055f)) failed++;
        if (!(n100 > 0.015f && n100 < 0.030f)) failed++;
        if (!(n500 > 0.080f && n500 < 0.110f)) failed++;
        if (!(n1k > 0.160f && n1k < 0.180f)) failed++;
        if (!(n10k > 0.740f && n10k < 0.760f)) failed++;
        if (!(n100 < n500 && n500 < n1k && n1k < n10k && n10k < n20k)) failed++;
        if (!((n20k - n10k) > 0.24f)) failed++;
        if (!((n500 - n100) > (n100 - 0.0f) * 3.0f)) failed++;

        const float roundTrip = SpectrogramReadableNormToFrequencyHz(n1k, sampleRate);
        if (!(roundTrip > 980.0f && roundTrip < 1020.0f)) failed++;
    }

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
