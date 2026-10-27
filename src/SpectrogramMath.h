#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// @stage7-contract
// API contract: spectrogram planning/color helpers are pure value transforms;
// they do not allocate audio state or retain input. Invalid dimensions return
// the existing `ok=false`/empty result domains.

struct SpectrogramPlan {
    int fftSize = 0;
    int hopSize = 0;
    int numFrames = 0;
    int numBins = 0;
    bool ok = false;
};

struct SpectrogramRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct SpectrogramMultiresFftBank {
    int lowFftSize = 0;
    int midFftSize = 0;
    int highFftSize = 0;
    bool enabled = false;
};

struct SpectrogramFftBlend {
    int primaryFftSize = 0;
    int secondaryFftSize = 0;
    float secondaryWeight = 0.0f;
};

struct ReadableSpectrogramPreset {
    int fftSize = 4096;
    int textureWidth = 2048;
    float blend = 0.62f;
    bool split = true;
};

inline constexpr ReadableSpectrogramPreset GetReadableSpectrogramPreset()
{
    return {};
}

inline constexpr int GetReadableSpectrogramPresetVersion()
{
    return 4;
}

inline constexpr float GetReadableSpectrogramMinFrequencyHz()
{
    return 0.0f;
}

inline constexpr float GetReadableSpectrogramHybridFrequencyAlpha()
{
    return 0.0005f;
}

inline float GetReadableSpectrogramMaxFrequencyHz(float sampleRate)
{
    if (sampleRate <= 0.0f) {
        return 20000.0f;
    }
    return std::clamp(sampleRate * 0.5f, 1000.0f, 20000.0f);
}

inline float SpectrogramFrequencyHzToReadableNorm(float frequencyHz, float sampleRate)
{
    const float minHz = GetReadableSpectrogramMinFrequencyHz();
    const float maxHz = GetReadableSpectrogramMaxFrequencyHz(sampleRate);
    const float clampedHz = std::clamp(frequencyHz, minHz, maxHz);
    const float alpha = GetReadableSpectrogramHybridFrequencyAlpha();
    const float denom = std::log1p(alpha * maxHz);
    if (denom <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(std::log1p(alpha * clampedHz) / denom, 0.0f, 1.0f);
}

inline float SpectrogramReadableNormToFrequencyHz(float norm, float sampleRate)
{
    const float minHz = GetReadableSpectrogramMinFrequencyHz();
    const float maxHz = GetReadableSpectrogramMaxFrequencyHz(sampleRate);
    const float t = std::clamp(norm, 0.0f, 1.0f);
    const float alpha = GetReadableSpectrogramHybridFrequencyAlpha();
    if (alpha <= 0.0f) {
        return minHz;
    }
    const float denom = std::log1p(alpha * maxHz);
    if (denom <= 0.0f) {
        return minHz;
    }
    const float frequencyHz = std::expm1(t * denom) / alpha;
    return std::clamp(frequencyHz, minHz, maxHz);
}

inline bool ShouldApplyReadableSpectrogramPresetMigration(int storedPresetVersion,
                                                         int fftSize,
                                                         bool split,
                                                         float blend)
{
    if (storedPresetVersion >= GetReadableSpectrogramPresetVersion()) {
        return false;
    }

    // v4 gives waveform detail room again after the inspector-heavy 82% split.
    if (storedPresetVersion >= 3 &&
        fftSize == GetReadableSpectrogramPreset().fftSize &&
        split &&
        blend >= 0.78f && blend <= 0.86f) {
        return true;
    }

    // v3 moves readable spectrogram analysis to 4096 FFT. Earlier readable
    // presets could leave persisted sessions at 1024/2048, which makes detailed
    // harmonic detail impossible regardless of texture sampling quality.
    if (storedPresetVersion >= 2) {
        return fftSize < GetReadableSpectrogramPreset().fftSize;
    }

    // v1 shipped the readable spectrogram at 62% height. Upgrade that exact
    // prior default to the inspector-like v2 height without stealing custom
    // high-detail settings.
    if (storedPresetVersion == 1) {
        return fftSize == 4096 && split && blend < 0.75f;
    }

    // Only migrate the old low-detail default shape. Preserve high-detail or
    // already split user preferences even when they predate this version key.
    return fftSize <= 2048 && !split && blend <= 0.55f;
}

inline SpectrogramPlan BuildSpectrogramPlan(uint64_t framesRead, int requestedFftSize)
{
    SpectrogramPlan plan;
    if (framesRead == 0) {
        return plan;
    }

    int fftSize = std::clamp(requestedFftSize, 512, 16384);

    // Short SFX should still get a visible spectrogram. Reduce the FFT size
    // aggressively, then allow a single zero-padded frame when audio is even
    // shorter than the final FFT window.
    while (fftSize > (int)framesRead && fftSize > 16) {
        fftSize >>= 1;
    }
    if (fftSize < 16) fftSize = 16;

    const int hopSize = (std::max)(1, fftSize / 8);
    const int numFrames = ((int)framesRead >= fftSize)
        ? ((((int)framesRead - fftSize) / hopSize) + 1)
        : 1;

    plan.fftSize = fftSize;
    plan.hopSize = hopSize;
    plan.numFrames = (std::max)(1, numFrames);
    plan.numBins = (std::max)(1, fftSize / 2);
    plan.ok = true;
    return plan;
}

inline int ComputeReadableSpectrogramTextureHeight(int numBins)
{
    if (numBins <= 0) {
        return 0;
    }

    // Avoid magnifying very small zero-padded FFTs from tiny SFX, but keep
    // normal analyzer textures dense enough that the UI does not upscale a
    // 512-row FFT1024 texture into obvious screen blocks.
    if (numBins < 256) {
        return numBins;
    }

    return std::clamp(std::max(numBins, 1024), 1, 2048);
}

inline int ComputeReadableSpectrogramTextureWidth(float visibleWidthPx,
                                                  float durationSeconds,
                                                  int channels)
{
    const float width = std::max(visibleWidthPx, 256.0f);
    float complexityScale = 1.0f;
    int maxRequestedCols = 4096;

    if (durationSeconds >= 1800.0f) {
        complexityScale *= 0.60f;
        maxRequestedCols = 2048;
    } else if (durationSeconds >= 600.0f) {
        complexityScale *= 0.75f;
        maxRequestedCols = 2560;
    }

    if (channels >= 8) {
        complexityScale *= 0.60f;
        maxRequestedCols = std::min(maxRequestedCols, 2048);
    } else if (channels >= 5) {
        complexityScale *= 0.75f;
        maxRequestedCols = std::min(maxRequestedCols, 2560);
    }

    const int desiredCols = (int)std::ceil(width * 1.10f * complexityScale);
    return std::clamp(desiredCols, 384, maxRequestedCols);
}

inline int ComputeReadableSpectrogramAnalysisFftSize(int requestedFftSize,
                                                     float durationSeconds)
{
    const int requested = std::clamp(requestedFftSize, 512, 16384);
    const float duration = std::max(durationSeconds, 0.0f);

    // Treat the readable preset as a high-detail target, not a fixed window.
    // Short and mid-length SFX need sharper time edges; cap automatic analysis
    // lower than 4096 so the spectrogram texture does not become a broad time smear.
    // Do not key this off zoom/display state; that would rebuild the texture
    // continuously while users inspect the waveform.
    if (requested <= GetReadableSpectrogramPreset().fftSize) {
        if (duration > 0.0f && duration < 3.0f) {
            return std::min(requested, 1024);
        }
        if (duration > 0.0f && duration < 180.0f) {
            return std::min(requested, 2048);
        }
    }

    return requested;
}

inline SpectrogramMultiresFftBank BuildSpectrogramMultiresFftBank(int analysisFftSize,
                                                                  float durationSeconds)
{
    const int base = std::clamp(analysisFftSize, 512, 16384);
    const float duration = std::max(durationSeconds, 0.0f);

    SpectrogramMultiresFftBank bank;
    bank.lowFftSize = base;

    // Multires analysis is for interactive SFX/readout material. Very long
    // files keep the single-bank path so background FFT cost stays bounded.
    if (base < 1024 || duration <= 0.0f || duration >= 180.0f) {
        bank.midFftSize = base;
        bank.highFftSize = base;
        bank.enabled = false;
        return bank;
    }

    bank.midFftSize = std::max(512, base / 2);
    bank.highFftSize = std::max(512, base / 4);
    bank.enabled = (bank.highFftSize < bank.lowFftSize);
    return bank;
}

inline int SelectSpectrogramFftForFrequency(const SpectrogramMultiresFftBank& bank,
                                            float frequencyHz)
{
    if (!bank.enabled) {
        return bank.lowFftSize;
    }

    const float hz = std::max(frequencyHz, 0.0f);
    if (hz < 700.0f) {
        return bank.lowFftSize;
    }
    if (hz < 5000.0f) {
        return bank.midFftSize;
    }
    return bank.highFftSize;
}

inline float SmoothSpectrogramStep(float edge0, float edge1, float value)
{
    if (edge0 == edge1) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline SpectrogramFftBlend SelectSpectrogramFftBlendForFrequency(const SpectrogramMultiresFftBank& bank,
                                                                 float frequencyHz)
{
    SpectrogramFftBlend blend;
    blend.primaryFftSize = SelectSpectrogramFftForFrequency(bank, frequencyHz);
    blend.secondaryFftSize = blend.primaryFftSize;
    blend.secondaryWeight = 0.0f;

    if (!bank.enabled) {
        return blend;
    }

    const float hz = std::max(frequencyHz, 0.0f);
    if (hz >= 550.0f && hz < 850.0f) {
        blend.primaryFftSize = bank.lowFftSize;
        blend.secondaryFftSize = bank.midFftSize;
        blend.secondaryWeight = SmoothSpectrogramStep(550.0f, 850.0f, hz);
    } else if (hz >= 4200.0f && hz < 5800.0f) {
        blend.primaryFftSize = bank.midFftSize;
        blend.secondaryFftSize = bank.highFftSize;
        blend.secondaryWeight = SmoothSpectrogramStep(4200.0f, 5800.0f, hz);
    }

    return blend;
}

inline float ComputeSpectrogramWindowMagnitudeScale(float windowSum, int fftSize)
{
    if (windowSum <= 0.0f || fftSize <= 0) {
        return 0.0f;
    }

    // Blackman-Harris has a size-proportional coherent gain. Scale by the
    // measured window sum while preserving the legacy visual magnitude level,
    // which divided by FFT size. This makes the invariant explicit for every
    // bank and prevents future window changes from silently biasing one band.
    constexpr float kBlackmanHarrisA0 = 0.35875f;
    return kBlackmanHarrisA0 / windowSum;
}

inline int ComputeSpectrogramWindowStartForNorm(double norm,
                                                uint64_t framesRead,
                                                int fftSize)
{
    if (framesRead == 0 || fftSize <= 0) {
        return -fftSize / 2;
    }

    const double t = std::clamp(norm, 0.0, 1.0);
    const double centerFrame = t * (double)(framesRead - 1);
    return (int)std::round(centerFrame - (double)fftSize * 0.5);
}

inline int ComputeSpectrogramWindowStartForColumn(int column,
                                                  int textureWidth,
                                                  uint64_t framesRead,
                                                  int fftSize)
{
    if (textureWidth <= 1) {
        return ComputeSpectrogramWindowStartForNorm(0.0, framesRead, fftSize);
    }

    const int clampedColumn = std::clamp(column, 0, textureWidth - 1);
    const double t = (double)clampedColumn / (double)(textureWidth - 1);
    return ComputeSpectrogramWindowStartForNorm(t, framesRead, fftSize);
}

inline bool CanDisplaySpectrogramTextureForFile(bool hasTexture,
                                                const std::string& readyFilePath,
                                                const std::string& previewFilePath,
                                                const std::string& currentFilePath)
{
    if (!hasTexture || currentFilePath.empty()) {
        return false;
    }
    return readyFilePath == currentFilePath || previewFilePath == currentFilePath;
}

inline bool SpectrogramTimelineMatches(uint64_t readyFrames,
                                       uint32_t readySampleRate,
                                       uint64_t currentFrames,
                                       uint32_t currentSampleRate)
{
    if (readyFrames == 0 || currentFrames == 0 ||
        readySampleRate == 0 || currentSampleRate == 0) {
        return false;
    }
    return readyFrames == currentFrames && readySampleRate == currentSampleRate;
}

inline bool CanDisplaySpectrogramTextureForTimeline(bool hasTexture,
                                                    const std::string& readyFilePath,
                                                    const std::string& previewFilePath,
                                                    const std::string& currentFilePath,
                                                    uint64_t readyFrames,
                                                    uint32_t readySampleRate,
                                                    uint64_t currentFrames,
                                                    uint32_t currentSampleRate)
{
    if (!CanDisplaySpectrogramTextureForFile(
            hasTexture, readyFilePath, previewFilePath, currentFilePath)) {
        return false;
    }
    return SpectrogramTimelineMatches(
        readyFrames, readySampleRate, currentFrames, currentSampleRate);
}

inline float NormalizeSpectrogramDbForReadability(float db,
                                                  float floorDb = -96.0f,
                                                  float ceilingDb = -12.0f,
                                                  float gamma = 0.78f)
{
    if (floorDb >= ceilingDb) {
        return 0.0f;
    }
    if (db <= floorDb) {
        return 0.0f;
    }

    const float raw = std::clamp((db - floorDb) / (ceilingDb - floorDb), 0.0f, 1.0f);
    return std::pow(raw, std::clamp(gamma, 0.25f, 2.5f));
}

inline float NormalizeSpectrogramMagnitudeForReadability(float magnitude)
{
    const float db = 20.0f * std::log10(std::max(magnitude, 1e-12f));
    return NormalizeSpectrogramDbForReadability(db);
}

inline SpectrogramRgb MapReadableSpectrogramHeatRgb(float normalized)
{
    const float v = std::clamp(normalized, 0.0f, 1.0f);

    struct ColorStop { float pos; float r, g, b; };
    static constexpr ColorStop stops[] = {
        { 0.00f, 0.004f, 0.006f, 0.010f },
        { 0.16f, 0.016f, 0.055f, 0.145f },
        { 0.34f, 0.070f, 0.180f, 0.330f },
        { 0.48f, 0.470f, 0.260f, 0.095f },
        { 0.68f, 0.900f, 0.420f, 0.060f },
        { 0.88f, 1.000f, 0.810f, 0.330f },
        { 1.00f, 1.000f, 0.980f, 0.780f },
    };
    constexpr int numStops = (int)(sizeof(stops) / sizeof(stops[0]));

    int i = 0;
    for (; i < numStops - 1; ++i) {
        if (v < stops[i + 1].pos) {
            break;
        }
    }
    if (i >= numStops - 1) {
        i = numStops - 2;
    }

    float t = (v - stops[i].pos) / (stops[i + 1].pos - stops[i].pos);
    t = std::clamp(t, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);

    const float r = stops[i].r + t * (stops[i + 1].r - stops[i].r);
    const float g = stops[i].g + t * (stops[i + 1].g - stops[i].g);
    const float b = stops[i].b + t * (stops[i + 1].b - stops[i].b);

    return SpectrogramRgb{
        (uint8_t)std::clamp((int)std::round(r * 255.0f), 0, 255),
        (uint8_t)std::clamp((int)std::round(g * 255.0f), 0, 255),
        (uint8_t)std::clamp((int)std::round(b * 255.0f), 0, 255),
    };
}
