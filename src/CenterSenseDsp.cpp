// CenterSenseDsp — 实现见 design + 收紧侧数值示例文档

#include "CenterSenseDsp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CenterSenseDsp {

namespace {
constexpr float kMidExponent = 2.0f;  // 中频 (-k)^α，默认 α=2
constexpr float kWeightLF = 0.2f;
constexpr float kWeightMid = 0.6f;
constexpr float kWeightHF = 1.0f;
} // namespace

void getCenterSenseAmounts(float k, float& amountLF, float& amountMid, float& amountHF) {
    if (k > 0.0f) {
        amountLF = amountMid = amountHF = 0.0f;
        return;
    }
    float t = -std::clamp(k, -1.0f, 0.0f);  // t in [0,1]
    amountLF = t;
    amountMid = std::pow(t, kMidExponent);
    amountHF = t;
}

void getCenterSenseWidthFactors(float k, float& widthLF, float& widthMid, float& widthHF) {
    if (k < 0.0f) {
        widthLF = widthMid = widthHF = 1.0f;
        return;
    }
    float t = std::clamp(k, 0.0f, 1.0f);
    widthLF = 1.0f + kWeightLF * t;
    widthMid = 1.0f + kWeightMid * t;
    widthHF = 1.0f + kWeightHF * t;
}

// 一阶低通系数 α = 1 - exp(-2π fc/fs)
static float onePoleAlpha(float fc, int sampleRate) {
    if (sampleRate <= 0 || fc <= 0.0f) return 0.0f;
    float x = 2.0f * static_cast<float>(M_PI) * fc / static_cast<float>(sampleRate);
    return 1.0f - std::exp(-x);
}

void processCenterSense(float* interleavedLR, size_t numFrames, int sampleRate, float k, ProcessState* state) {
    if (std::abs(k) < 1e-6f) return;
    if (!state || !interleavedLR || numFrames == 0 || sampleRate <= 0) return;

    float aLF, aMid, aHF;
    getCenterSenseAmounts(k, aLF, aMid, aHF);
    float wLF, wMid, wHF;
    getCenterSenseWidthFactors(k, wLF, wMid, wHF);

    float a80 = onePoleAlpha(80.0f, sampleRate);
    float a1000 = onePoleAlpha(1000.0f, sampleRate);

    float* lp80L = &state->lp80L;
    float* lp80R = &state->lp80R;
    float* lp1000L = &state->lp1000L;
    float* lp1000R = &state->lp1000R;

    for (size_t i = 0; i < numFrames; ++i) {
        float L = interleavedLR[2 * i + 0];
        float R = interleavedLR[2 * i + 1];

        // 80 Hz 分频：LF = lp80, high80 = in - LF
        float lfL = a80 * L + (1.0f - a80) * (*lp80L);
        float lfR = a80 * R + (1.0f - a80) * (*lp80R);
        *lp80L = lfL;
        *lp80R = lfR;
        float high80L = L - lfL;
        float high80R = R - lfR;

        // 1000 Hz 分频（对 high80）：Mid = lp1000(high80), HF = high80 - Mid
        float midL = a1000 * high80L + (1.0f - a1000) * (*lp1000L);
        float midR = a1000 * high80R + (1.0f - a1000) * (*lp1000R);
        *lp1000L = midL;
        *lp1000R = midR;
        float hfL = high80L - midL;
        float hfR = high80R - midR;

        // 每段应用收紧或拓宽后再合成
        auto tighten = [](float l, float r, float amount) -> std::pair<float, float> {
            float m = (l + r) * 0.5f;
            float lOut = (1.0f - amount) * l + amount * m;
            float rOut = (1.0f - amount) * r + amount * m;
            return { lOut, rOut };
        };
        auto widen = [](float l, float r, float width) -> std::pair<float, float> {
            float m = (l + r) * 0.5f;
            float s = (l - r) * 0.5f;
            float sOut = s * width;
            return { m + sOut, m - sOut };
        };

        float lOut, rOut;
        if (k <= 0.0f) {
            auto [lL, rL] = tighten(lfL, lfR, aLF);
            auto [lM, rM] = tighten(midL, midR, aMid);
            auto [lH, rH] = tighten(hfL, hfR, aHF);
            lOut = lL + lM + lH;
            rOut = rL + rM + rH;
        } else {
            auto [lL, rL] = widen(lfL, lfR, wLF);
            auto [lM, rM] = widen(midL, midR, wMid);
            auto [lH, rH] = widen(hfL, hfR, wHF);
            lOut = lL + lM + lH;
            rOut = rL + rM + rH;
        }

        interleavedLR[2 * i + 0] = lOut;
        interleavedLR[2 * i + 1] = rOut;
    }
}

void processBassMono(float* interleavedLR, size_t numFrames, int sampleRate, float cutoffHz, BassMonoState* state) {
    if (cutoffHz <= 0.0f) return;
    if (!state || !interleavedLR || numFrames == 0 || sampleRate <= 0) return;

    float alpha = onePoleAlpha(cutoffHz, sampleRate);
    float lpL = state->lp_L;
    float lpR = state->lp_R;

    for (size_t i = 0; i < numFrames; ++i) {
        float L = interleavedLR[2 * i + 0];
        float R = interleavedLR[2 * i + 1];

        // One-pole lowpass to extract low frequencies
        lpL = alpha * L + (1.0f - alpha) * lpL;
        lpR = alpha * R + (1.0f - alpha) * lpR;

        // High-pass residual (unmodified stereo)
        float highL = L - lpL;
        float highR = R - lpR;

        // Collapse low frequencies to mono
        float lfMono = (lpL + lpR) * 0.5f;

        interleavedLR[2 * i + 0] = lfMono + highL;
        interleavedLR[2 * i + 1] = lfMono + highR;
    }

    state->lp_L = lpL;
    state->lp_R = lpR;
}

// Frequency Shifter

namespace {
const std::array<float, FreqShiftState::kTaps>& freqShiftHilbertCoeffs() {
    static const auto kCoeffs = []() {
        std::array<float, FreqShiftState::kTaps> h{};
        constexpr int N      = FreqShiftState::kTaps;
        constexpr int center = N / 2;
        for (int n = 0; n < N; ++n) {
            int k = n - center;
            if (k == 0 || k % 2 == 0) continue;
            float hann = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * n / (N - 1)));
            h[n] = (2.0f / static_cast<float>(M_PI)) / static_cast<float>(k) * hann;
        }
        return h;
    }();
    return kCoeffs;
}
} // namespace

void processFreqShift(float* interleavedLR, size_t numFrames, int sampleRate,
                      float shiftHz, FreqShiftState* state) {
    if (!state || !interleavedLR || numFrames == 0 || sampleRate <= 0) return;
    if (std::abs(shiftHz) < 0.1f) return;

    // 63-tap Type-III FIR Hilbert transformer, Hann-windowed.
    // Even offsets (n - center) are zero; odd offsets: h[n] = (2/π)/(n-center) * hann(n).
    // Computed once at first call (thread-safe in C++11+).
    const auto& kCoeffs = freqShiftHilbertCoeffs();

    constexpr int N      = FreqShiftState::kTaps;
    constexpr int center = N / 2;  // 31 — filter group delay in samples

    // Recursive oscillator: avoids per-sample cos/sin calls.
    const float twoPi   = 2.0f * static_cast<float>(M_PI);
    const float phaseInc = shiftHz / static_cast<float>(sampleRate);
    float cosP = std::cos(state->phase * twoPi);
    float sinP = std::sin(state->phase * twoPi);
    const float cosInc = std::cos(phaseInc * twoPi);
    const float sinInc = std::sin(phaseInc * twoPi);

    int pos = state->pos;

    for (size_t i = 0; i < numFrames; ++i) {
        // Write current input sample into the circular delay lines.
        state->delayL[pos] = interleavedLR[2 * i + 0];
        state->delayR[pos] = interleavedLR[2 * i + 1];

        // Compute Hilbert FIR — result corresponds to input delayed by `center` samples.
        float hL = 0.0f, hR = 0.0f;
        for (int k = 0; k < N; ++k) {
            int idx = (pos - k + N) % N;
            hL += kCoeffs[k] * state->delayL[idx];
            hR += kCoeffs[k] * state->delayR[idx];
        }

        // Read the group-delay-aligned original signal (input at time i - center).
        int alignedIdx = (pos - center + N) % N;
        float xL = state->delayL[alignedIdx];
        float xR = state->delayR[alignedIdx];

        // SSB upper-sideband modulation: out = x·cos(φ) − H(x)·sin(φ)
        interleavedLR[2 * i + 0] = xL * cosP - hL * sinP;
        interleavedLR[2 * i + 1] = xR * cosP - hR * sinP;

        // Advance circular buffer and oscillator.
        pos = (pos + 1) % N;
        float newCos = cosP * cosInc - sinP * sinInc;
        float newSin = sinP * cosInc + cosP * sinInc;
        cosP = newCos;
        sinP = newSin;
    }

    state->pos   = pos;
    // Renormalize phase to [0, 1) to prevent accumulated drift.
    state->phase = std::fmod(state->phase + phaseInc * static_cast<float>(numFrames), 1.0f);
    if (state->phase < 0.0f) state->phase += 1.0f;
}

void processFreqShiftMono(float* mono, size_t numFrames, int sampleRate,
                          float shiftHz, FreqShiftState* state) {
    if (!state || !mono || numFrames == 0 || sampleRate <= 0) return;
    if (std::abs(shiftHz) < 0.1f) return;

    const auto& kCoeffs = freqShiftHilbertCoeffs();

    constexpr int N      = FreqShiftState::kTaps;
    constexpr int center = N / 2;

    const float twoPi    = 2.0f * static_cast<float>(M_PI);
    const float phaseInc = shiftHz / static_cast<float>(sampleRate);
    float cosP = std::cos(state->phase * twoPi);
    float sinP = std::sin(state->phase * twoPi);
    const float cosInc = std::cos(phaseInc * twoPi);
    const float sinInc = std::sin(phaseInc * twoPi);

    int pos = state->pos;

    for (size_t i = 0; i < numFrames; ++i) {
        state->delayL[pos] = mono[i];

        float h = 0.0f;
        for (int k = 0; k < N; ++k) {
            int idx = (pos - k + N) % N;
            h += kCoeffs[k] * state->delayL[idx];
        }

        int alignedIdx = (pos - center + N) % N;
        float x = state->delayL[alignedIdx];
        mono[i] = x * cosP - h * sinP;

        pos = (pos + 1) % N;
        float newCos = cosP * cosInc - sinP * sinInc;
        float newSin = sinP * cosInc + cosP * sinInc;
        cosP = newCos;
        sinP = newSin;
    }

    state->pos = pos;
    state->phase = std::fmod(state->phase + phaseInc * static_cast<float>(numFrames), 1.0f);
    if (state->phase < 0.0f) state->phase += 1.0f;
}

namespace {
constexpr float kMagicEqMinFreq = 20.0f;
constexpr float kMagicEqMaxFreq = 20000.0f;
constexpr float kMagicEqMinHalfWidth = 1.0f;
constexpr float kMagicEqMaxHalfWidth = 50.0f;
constexpr float kMagicEqQ = 0.70710678f;
constexpr float kMagicEqTransitionSeconds = 0.010f;

float clampMagicEqCenter(float value) {
    return std::clamp(std::isfinite(value) ? value : 1000.0f, kMagicEqMinFreq, kMagicEqMaxFreq);
}

float clampMagicEqHalfWidth(float value) {
    return std::clamp(std::isfinite(value) ? value : 10.0f, kMagicEqMinHalfWidth, kMagicEqMaxHalfWidth);
}

void setIdentity(MagicEQState::Biquad& b) {
    b.b0 = 1.0f; b.b1 = b.b2 = b.a1 = b.a2 = 0.0f;
}

void setLowPass(MagicEQState::Biquad& b, float frequency, int sampleRate, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * frequency / static_cast<float>(sampleRate);
    const float cosine = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b.b0 = (1.0f - cosine) * 0.5f / a0;
    b.b1 = (1.0f - cosine) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0f * cosine / a0;
    b.a2 = (1.0f - alpha) / a0;
}

void setHighPass(MagicEQState::Biquad& b, float frequency, int sampleRate, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * frequency / static_cast<float>(sampleRate);
    const float cosine = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b.b0 = (1.0f + cosine) * 0.5f / a0;
    b.b1 = -(1.0f + cosine) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0f * cosine / a0;
    b.a2 = (1.0f - alpha) / a0;
}

void setNotch(MagicEQState::Biquad& b, float frequency, int sampleRate, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * frequency / static_cast<float>(sampleRate);
    const float cosine = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b.b0 = 1.0f / a0;
    b.b1 = -2.0f * cosine / a0;
    b.b2 = b.b0;
    b.a1 = b.b1;
    b.a2 = (1.0f - alpha) / a0;
}

double processBiquad(MagicEQState::Biquad& b, double input, bool right) {
    double& z1 = right ? b.z1R : b.z1;
    double& z2 = right ? b.z2R : b.z2;
    const double output = b.b0 * input + z1;
    z1 = b.b1 * input - b.a1 * output + z2;
    z2 = b.b2 * input - b.a2 * output;
    return output;
}

int transitionFrames(int sampleRate) {
    return std::max(1, static_cast<int>(std::lround(
        static_cast<float>(sampleRate) * kMagicEqTransitionSeconds)));
}

void clearFilterBank(MagicEQState::FilterBank& bank) {
    for (int i = 0; i < MagicEQState::kStages; ++i) {
        bank.sections[i].z1 = bank.sections[i].z2 = 0.0;
        bank.sections[i].z1R = bank.sections[i].z2R = 0.0;
    }
    bank.sectionCount = 0;
    bank.prepared = false;
}

double biquadMagnitudeAt(const MagicEQState::Biquad& b, double frequency, int sampleRate) {
    const double angle = -2.0 * M_PI * frequency / sampleRate;
    const std::complex<double> z = std::polar(1.0, angle);
    const auto numerator = b.b0 + b.b1 * z + b.b2 * z * z;
    const auto denominator = 1.0 + b.a1 * z + b.a2 * z * z;
    return std::abs(numerator / denominator);
}
} // namespace

float magicEQFrequencyToPercent(float frequencyHz) {
    const float frequency = std::clamp(std::isfinite(frequencyHz) ? frequencyHz : kMagicEqMinFreq,
                                       kMagicEqMinFreq, kMagicEqMaxFreq);
    return 100.0f * (std::log10(frequency) - std::log10(kMagicEqMinFreq)) /
           (std::log10(kMagicEqMaxFreq) - std::log10(kMagicEqMinFreq));
}

float magicEQPercentToFrequency(float percent) {
    const float normalized = std::clamp(std::isfinite(percent) ? percent : 0.0f, 0.0f, 100.0f);
    const float logMin = std::log10(kMagicEqMinFreq);
    const float logMax = std::log10(kMagicEqMaxFreq);
    return std::pow(10.0f, logMin + normalized * 0.01f * (logMax - logMin));
}

namespace {
float normalizeMagicEqCenter(float centerFreq, float halfWidthPercent) {
    const float half = clampMagicEqHalfWidth(halfWidthPercent);
    const float percent = magicEQFrequencyToPercent(clampMagicEqCenter(centerFreq));
    return magicEQPercentToFrequency(std::clamp(percent, half, 100.0f - half));
}

void setMagicEqEdges(float lowPercent, float highPercent, float& centerFreq, float& halfWidthPercent) {
    constexpr float minimumSpan = 2.0f * kMagicEqMinHalfWidth;
    float low = std::clamp(std::isfinite(lowPercent) ? lowPercent : 0.0f, 0.0f, 100.0f - minimumSpan);
    float high = std::clamp(std::isfinite(highPercent) ? highPercent : 100.0f, low + minimumSpan, 100.0f);
    if (high - low > 2.0f * kMagicEqMaxHalfWidth) {
        if (low == 0.0f) high = 100.0f;
        else low = high - 100.0f;
    }
    const float centerPercent = 0.5f * (low + high);
    centerFreq = magicEQPercentToFrequency(centerPercent);
    halfWidthPercent = std::clamp(0.5f * (high - low), kMagicEqMinHalfWidth, kMagicEqMaxHalfWidth);
}

int magicEqPrototypeOrder(MagicEQSteepness steepness) {
    if (steepness == MagicEQSteepness::Soft) return 2;
    if (steepness == MagicEQSteepness::Steep) return 4;
    if (steepness == MagicEQSteepness::VerySteep) return 8;
    return 24;
}

void prepareFilterBank(MagicEQState::FilterBank& bank, int sampleRate,
                       bool enabled, MagicEQMode requestedMode,
                       float requestedCenter, float requestedHalfWidth,
                       MagicEQSteepness requestedSteepness) {
    clearFilterBank(bank);
    const MagicEQMode mode = requestedMode == MagicEQMode::Notch
        ? MagicEQMode::Notch : MagicEQMode::Pass;
    const float half = clampMagicEqHalfWidth(requestedHalfWidth);
    const float center = normalizeMagicEqCenter(requestedCenter, half);
    float low = 0.0f, high = 0.0f;
    magicEQEdges(center, half, low, high);
    bank.sampleRate = sampleRate;
    bank.mode = mode;
    bank.steepness = requestedSteepness;
    bank.centerFreq = center;
    bank.halfWidthPercent = half;
    bank.lowFreq = low;
    bank.highFreq = high;
    bank.responseGain = 1.0;
    bank.maxPoleRadius = 0.0;
    bank.enabled = enabled;

    if (enabled) {
        const double fs = static_cast<double>(sampleRate);
        const double upper = fs * 0.49;
        if (low >= upper || high >= upper) {
            bank.enabled = false;
            bank.prepared = true;
            return;
        }
        const double lowCut = std::clamp(static_cast<double>(low), 20.0, upper);
        const double highCut = std::clamp(static_cast<double>(high), lowCut + 1.0e-6, upper);
        const double wLow = 2.0 * fs * std::tan(M_PI * lowCut / fs);
        const double wHigh = 2.0 * fs * std::tan(M_PI * highCut / fs);
        const double bandwidth = wHigh - wLow;
        const double wCenter = std::sqrt(wLow * wHigh);
        const double twoFs = 2.0 * fs;
        const int order = magicEqPrototypeOrder(requestedSteepness);
        const std::complex<double> stopZero =
            (std::complex<double>(twoFs, wCenter) /
             std::complex<double>(twoFs, -wCenter));
        for (int pair = 0; pair < order / 2; ++pair) {
            const double angle = M_PI * (2.0 * pair + 1.0 + order) / (2.0 * order);
            const std::complex<double> prototypePole = std::polar(1.0, angle);
            const std::complex<double> discriminant = mode == MagicEQMode::Pass
                ? std::sqrt(bandwidth * bandwidth * prototypePole * prototypePole -
                            4.0 * wCenter * wCenter)
                : std::sqrt(bandwidth * bandwidth -
                            4.0 * wCenter * wCenter * prototypePole * prototypePole);
            for (int sign : {-1, 1}) {
                const std::complex<double> analogPole = mode == MagicEQMode::Pass
                    ? (bandwidth * prototypePole + static_cast<double>(sign) * discriminant) * 0.5
                    : (bandwidth + static_cast<double>(sign) * discriminant) / (2.0 * prototypePole);
                const std::complex<double> digitalPole = (twoFs + analogPole) / (twoFs - analogPole);
                auto& section = bank.sections[bank.sectionCount++];
                section.a1 = -2.0 * digitalPole.real();
                section.a2 = std::norm(digitalPole);
                bank.maxPoleRadius = std::max(bank.maxPoleRadius, std::abs(digitalPole));
                section.b0 = 1.0;
                section.b1 = mode == MagicEQMode::Pass ? 0.0 : -2.0 * stopZero.real();
                section.b2 = mode == MagicEQMode::Pass ? -1.0 : 1.0;
            }
        }
        double rawGain = 1.0;
        const double reference = mode == MagicEQMode::Pass ? center : 0.0;
        for (int section = 0; section < bank.sectionCount; ++section)
            rawGain *= biquadMagnitudeAt(bank.sections[section], reference, sampleRate);
        const double perSectionGain = std::pow(1.0 / rawGain, 1.0 / bank.sectionCount);
        for (int section = 0; section < bank.sectionCount; ++section) {
            auto& b = bank.sections[section];
            b.b0 *= perSectionGain;
            b.b1 *= perSectionGain;
            b.b2 *= perSectionGain;
        }
    }
    bank.prepared = true;
}

bool sameFilterSettings(const MagicEQState::FilterBank& bank, int sampleRate,
                        const MagicEQProcessParams& params) {
    if (!bank.prepared || bank.sampleRate != sampleRate || bank.enabled != params.enabled) return false;
    if (!params.enabled) return true;
    const MagicEQMode mode = params.mode == MagicEQMode::Notch
        ? MagicEQMode::Notch : MagicEQMode::Pass;
    const float half = clampMagicEqHalfWidth(params.halfWidthPercent);
    const float center = normalizeMagicEqCenter(params.centerFreq, half);
    return bank.mode == mode && bank.steepness == params.steepness && std::fabs(bank.centerFreq - center) < 1e-4f &&
        std::fabs(bank.halfWidthPercent - half) < 1e-4f;
}

void startFilterTransition(MagicEQState* state, int sampleRate,
                           const MagicEQProcessParams& params) {
    state->transitionBank = 1 - state->activeBank;
    prepareFilterBank(state->banks[state->transitionBank], sampleRate, params.enabled,
                      params.mode, params.centerFreq, params.halfWidthPercent, params.steepness);
    state->transitionTotalFrames = transitionFrames(sampleRate);
    state->transitionFramesRemaining = state->transitionTotalFrames;
}

float processFilterBank(MagicEQState::FilterBank& bank, float input, bool right) {
    if (!bank.enabled) return input;
    double value = input;
    for (int section = 0; section < bank.sectionCount; ++section)
        value = processBiquad(bank.sections[section], value, right);
    return static_cast<float>(value);
}
} // namespace

MagicEQTailEstimate estimateMagicEQTail(const MagicEQProcessParams& params, int sampleRate) {
    MagicEQTailEstimate result{};
    if (sampleRate <= 0) {
        result.reason = MagicEQTailReason::InvalidSampleRate;
        return result;
    }
    MagicEQState::FilterBank bank{};
    prepareFilterBank(bank, sampleRate, true, params.mode,
                      params.centerFreq, params.halfWidthPercent, params.steepness);
    if (!bank.enabled || !std::isfinite(bank.maxPoleRadius) ||
        bank.maxPoleRadius <= 0.0 || bank.maxPoleRadius >= 1.0) {
        result.reason = MagicEQTailReason::FrequencyLimit;
        return result;
    }
    constexpr double kMinus80DbAmplitude = 1.0e-4;
    const double seconds = std::log(kMinus80DbAmplitude) /
        (std::log(bank.maxPoleRadius) * static_cast<double>(sampleRate));
    result.estimatedSeconds = static_cast<float>(seconds);
    result.available = std::isfinite(seconds) && seconds <= 0.2;
    result.reason = result.available ? MagicEQTailReason::None : MagicEQTailReason::TailBudget;
    return result;
}

MagicEQLocalSlope measureMagicEQLocalSlope(const MagicEQProcessParams& params, int sampleRate) {
    MagicEQLocalSlope result{};
    const auto rejectBoth = [&](MagicEQSlopeUnavailable reason) {
        result.low.reason = reason;
        result.high.reason = reason;
        return result;
    };
    if (!params.enabled) return rejectBoth(MagicEQSlopeUnavailable::Bypass);
    if (sampleRate < 8000) return rejectBoth(MagicEQSlopeUnavailable::InvalidSampleRate);
    if (!std::isfinite(params.centerFreq) || !std::isfinite(params.halfWidthPercent) ||
        params.halfWidthPercent < 1.0f)
        return rejectBoth(MagicEQSlopeUnavailable::NarrowBand);

    MagicEQState::FilterBank bank{};
    prepareFilterBank(bank, sampleRate, true, params.mode, params.centerFreq,
                      params.halfWidthPercent, params.steepness);
    const float center = bank.centerFreq;
    const float low = bank.lowFreq;
    const float high = bank.highFreq;
    const float lowSpan = std::log2(center / low);
    const float highSpan = std::log2(high / center);
    if (!std::isfinite(lowSpan) || !std::isfinite(highSpan) ||
        lowSpan < 0.04f || highSpan < 0.04f)
        return rejectBoth(MagicEQSlopeUnavailable::NarrowBand);
    const float maxHz = std::min(kMagicEqMaxFreq, static_cast<float>(sampleRate) * 0.49f);
    const auto gainDb = [&](float frequency) {
        double gain = 1.0;
        for (int section = 0; section < bank.sectionCount; ++section)
            gain *= biquadMagnitudeAt(bank.sections[section], frequency, sampleRate);
        return 20.0 * std::log10(std::max(gain, 1.0e-12));
    };
    const auto measureSide = [&](float edge, float span, MagicEQLocalSlopeSide& side) {
        const float deltaOct = std::min(0.12f, span * 0.4f);
        side.fromHz = edge * std::exp2(-deltaOct);
        side.toHz = edge * std::exp2(deltaOct);
        if (side.fromHz < kMagicEqMinFreq + 0.5f || side.toHz > maxHz - 0.5f) {
            side.reason = MagicEQSlopeUnavailable::FrequencyLimit;
            return;
        }
        const double denominator = std::log2(side.toHz / side.fromHz);
        const double slope = (gainDb(side.toHz) - gainDb(side.fromHz)) / denominator;
        if (denominator < 0.02 || !std::isfinite(slope)) {
            side.reason = MagicEQSlopeUnavailable::NarrowBand;
            return;
        }
        side.dbPerOctave = static_cast<float>(slope);
        side.valid = true;
    };
    measureSide(low, lowSpan, result.low);
    measureSide(high, highSpan, result.high);
    return result;
}

void magicEQSetLowEdgePercent(float lowPercent, float& centerFreq, float& halfWidthPercent) {
    float currentLow = 0.0f, currentHigh = 0.0f;
    magicEQEdges(centerFreq, halfWidthPercent, currentLow, currentHigh);
    (void)currentLow;
    const float highPercent = magicEQFrequencyToPercent(currentHigh);
    const float pinnedLow = std::clamp(std::isfinite(lowPercent) ? lowPercent : 0.0f,
                                       std::max(0.0f, highPercent - 100.0f), highPercent - 2.0f);
    setMagicEqEdges(pinnedLow, highPercent, centerFreq, halfWidthPercent);
}

void magicEQSetHighEdgePercent(float highPercent, float& centerFreq, float& halfWidthPercent) {
    float currentLow = 0.0f, currentHigh = 0.0f;
    magicEQEdges(centerFreq, halfWidthPercent, currentLow, currentHigh);
    const float lowPercent = magicEQFrequencyToPercent(currentLow);
    const float pinnedHigh = std::clamp(std::isfinite(highPercent) ? highPercent : 100.0f,
                                        lowPercent + 2.0f, std::min(100.0f, lowPercent + 100.0f));
    setMagicEqEdges(lowPercent, pinnedHigh, centerFreq, halfWidthPercent);
}

void magicEQEdges(float centerFreq, float halfWidthPercent, float& lowFreq, float& highFreq) {
    const float half = clampMagicEqHalfWidth(halfWidthPercent);
    const float requestedCenter = magicEQFrequencyToPercent(centerFreq);
    const float center = std::clamp(requestedCenter, half, 100.0f - half);
    lowFreq = magicEQPercentToFrequency(center - half);
    highFreq = magicEQPercentToFrequency(center + half);
}

void resetMagicEQ(MagicEQState* state) {
    if (!state) return;
    *state = MagicEQState{};
}

void prepareMagicEQ(MagicEQState* state, int sampleRate, const MagicEQProcessParams& params) {
    if (!state || sampleRate <= 0) return;
    prepareFilterBank(state->banks[0], sampleRate, params.enabled, params.mode,
                      params.centerFreq, params.halfWidthPercent, params.steepness);
    state->activeBank = 0;
    state->transitionBank = 1;
    state->transitionFramesRemaining = 0;
    state->transitionTotalFrames = 0;
    state->hasPendingParams = false;
    state->sampleRate = sampleRate;
    state->mode = state->banks[0].mode;
    state->steepness = state->banks[0].steepness;
    state->centerFreq = state->banks[0].centerFreq;
    state->halfWidthPercent = state->banks[0].halfWidthPercent;
    state->lowFreq = state->banks[0].lowFreq;
    state->highFreq = state->banks[0].highFreq;
    state->prepared = true;
}

void processMagicEQ(float* interleaved, size_t numFrames, int channels, int sampleRate,
                    const MagicEQProcessParams& params, MagicEQState* state) {
    if (!interleaved || numFrames == 0 || !state || sampleRate <= 0 || (channels != 1 && channels != 2)) return;
    if (state->prepared && state->banks[state->activeBank].sampleRate != sampleRate) {
        resetMagicEQ(state);
    }
    if (!state->prepared) {
        prepareMagicEQ(state, sampleRate, params);
        if (!params.enabled) {
            state->overloadGain = 1.0;
            return;
        }
    } else if (state->transitionFramesRemaining > 0) {
        const auto& targetBank = state->banks[state->transitionBank];
        if (sameFilterSettings(targetBank, sampleRate, params)) {
            state->hasPendingParams = false;
        } else {
            state->pendingEnabled = params.enabled;
            state->pendingMode = params.mode == MagicEQMode::Notch
                ? MagicEQMode::Notch : MagicEQMode::Pass;
            state->pendingSteepness = params.steepness;
            state->pendingCenterFreq = params.centerFreq;
            state->pendingHalfWidthPercent = params.halfWidthPercent;
            state->hasPendingParams = true;
        }
    } else {
        auto& activeBank = state->banks[state->activeBank];
        if (!sameFilterSettings(activeBank, sampleRate, params)) {
            if (!activeBank.enabled && !params.enabled) {
                prepareFilterBank(activeBank, sampleRate, false, params.mode,
                                  params.centerFreq, params.halfWidthPercent, params.steepness);
            } else {
                startFilterTransition(state, sampleRate, params);
            }
        }
        if (state->transitionFramesRemaining == 0 && !activeBank.enabled && !params.enabled) {
            state->overloadGain = 1.0;
            return;
        }
    }

    // Feed-forward sample-peak protection is deliberately after the bank crossfade.
    // It has no lookahead or added delay. A peak above unity lowers one shared
    // gain for the frame; recovery is gradual and never reduces an untouched
    // normal-level stream. This is sample-peak protection, not true-peak limiting.
    constexpr double kSamplePeakCeiling = 0.999;
    const double release = std::exp(-1.0 / (0.020 * static_cast<double>(sampleRate)));
    for (size_t frame = 0; frame < numFrames; ++frame) {
        const bool transitioning = state->transitionFramesRemaining > 0;
        const float transitionMix = transitioning
            ? static_cast<float>(state->transitionTotalFrames - state->transitionFramesRemaining + 1) /
                static_cast<float>(state->transitionTotalFrames)
            : 0.0f;
        for (int channel = 0; channel < channels; ++channel) {
            const bool right = channel == 1;
            const size_t sampleIndex = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
            const float dry = interleaved[sampleIndex];
            auto& activeBank = state->banks[state->activeBank];
            const float active = processFilterBank(activeBank, dry, right);
            if (transitioning) {
                auto& targetBank = state->banks[state->transitionBank];
                const float target = processFilterBank(targetBank, dry, right);
                interleaved[sampleIndex] = active + (target - active) * transitionMix;
            } else {
                interleaved[sampleIndex] = active;
            }
        }
        double framePeak = 0.0;
        for (int channel = 0; channel < channels; ++channel) {
            const size_t index = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
            const double sample = interleaved[index];
            if (!std::isfinite(sample)) {
                // Non-finite filter output is a fault, never a deliverable audio sample.
                interleaved[index] = 0.0f;
            } else {
                framePeak = std::max(framePeak, std::abs(sample));
            }
        }
        const double recoveringGain = 1.0 - (1.0 - state->overloadGain) * release;
        state->overloadGain = framePeak > kSamplePeakCeiling
            ? std::min(recoveringGain, kSamplePeakCeiling / framePeak)
            : recoveringGain;
        for (int channel = 0; channel < channels; ++channel) {
            const size_t index = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
            interleaved[index] = static_cast<float>(static_cast<double>(interleaved[index]) * state->overloadGain);
        }
        if (state->transitionFramesRemaining > 0) {
            --state->transitionFramesRemaining;
            if (state->transitionFramesRemaining == 0) {
                state->activeBank = state->transitionBank;
                state->transitionTotalFrames = 0;
                const auto& activeBank = state->banks[state->activeBank];
                state->sampleRate = activeBank.sampleRate;
                state->mode = activeBank.mode;
                state->steepness = activeBank.steepness;
                state->centerFreq = activeBank.centerFreq;
                state->halfWidthPercent = activeBank.halfWidthPercent;
                state->lowFreq = activeBank.lowFreq;
                state->highFreq = activeBank.highFreq;
                if (state->hasPendingParams) {
                    MagicEQProcessParams pending{};
                    pending.enabled = state->pendingEnabled;
                    pending.mode = state->pendingMode;
                    pending.steepness = state->pendingSteepness;
                    pending.centerFreq = state->pendingCenterFreq;
                    pending.halfWidthPercent = state->pendingHalfWidthPercent;
                    state->hasPendingParams = false;
                    const auto& activeBank = state->banks[state->activeBank];
                    if (!sameFilterSettings(activeBank, sampleRate, pending)) {
                        if (!activeBank.enabled && !pending.enabled) {
                            prepareFilterBank(state->banks[state->activeBank], sampleRate, false,
                                pending.mode, pending.centerFreq, pending.halfWidthPercent, pending.steepness);
                        } else {
                            startFilterTransition(state, sampleRate, pending);
                        }
                    }
                }
            }
        }
    }
}

} // namespace CenterSenseDsp
