#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "../src/CenterSenseDsp.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#undef near
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "User32.lib")
#endif

namespace {
int failures = 0;
int checks = 0;

void check(bool condition, const char* description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("FAIL: %s\n", description);
    }
}

bool near(float a, float b, float tolerance = 1e-3f) {
    return std::fabs(a - b) <= tolerance;
}

std::vector<float> sine(float frequencyHz, int sampleRate, size_t frames, int channels) {
    std::vector<float> samples(frames * static_cast<size_t>(channels));
    for (size_t frame = 0; frame < frames; ++frame) {
        const float value = std::sin(2.0f * 3.14159265358979323846f * frequencyHz *
                                     static_cast<float>(frame) / static_cast<float>(sampleRate));
        for (int channel = 0; channel < channels; ++channel)
            samples[frame * static_cast<size_t>(channels) + static_cast<size_t>(channel)] = value;
    }
    return samples;
}

float rms(const std::vector<float>& samples, size_t firstFrame, size_t lastFrame, int channels) {
    double sum = 0.0;
    size_t count = 0;
    for (size_t frame = firstFrame; frame < lastFrame; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const double value = samples[frame * static_cast<size_t>(channels) +
                                         static_cast<size_t>(channel)];
            sum += value * value;
            ++count;
        }
    }
    return count ? static_cast<float>(std::sqrt(sum / static_cast<double>(count))) : 0.0f;
}

struct SweepMetrics {
    float maxTransitionError = 0.0f;
    float maxOutputPeak = 0.0f;
    bool finite = true;
};

SweepMetrics rapidSweep(int sampleRate, size_t blockFrames,
                        CenterSenseDsp::MagicEQMode mode, float halfWidth,
                        bool moveEndpoint) {
    const float toneHz = mode == CenterSenseDsp::MagicEQMode::Pass && halfWidth >= 10.0f
        ? 1000.0f : 100.0f;
    constexpr float toneAmplitude = 0.22f;
    constexpr size_t blocks = 256;
    const size_t settleBlocks = std::max<size_t>(8, static_cast<size_t>(sampleRate) / blockFrames / 8);
    CenterSenseDsp::MagicEQProcessParams fixed{};
    fixed.enabled = true;
    fixed.mode = mode;
    fixed.centerFreq = 6000.0f;
    fixed.halfWidthPercent = halfWidth;
    auto moving = fixed;
    CenterSenseDsp::MagicEQState fixedState{}, movingState{};
    SweepMetrics metrics;
    float low = 0.0f, high = 0.0f;
    CenterSenseDsp::magicEQEdges(fixed.centerFreq, fixed.halfWidthPercent, low, high);
    const float lowPercent = CenterSenseDsp::magicEQFrequencyToPercent(low);
    const float checkFrames = static_cast<float>(sampleRate) / 200.0f;
    const size_t transitionFrames = std::max<size_t>(1, static_cast<size_t>(checkFrames));
    std::vector<float> fixedBlock(blockFrames), movingBlock(blockFrames);
    size_t absoluteFrame = 0;
    int updateIndex = 0;
    size_t framesSinceUpdate = transitionFrames;
    for (size_t block = 0; block < settleBlocks + blocks; ++block) {
        if (block >= settleBlocks && ((block - settleBlocks) % 2u) == 0u) {
            const float offset = 0.30f * std::sin(static_cast<float>(updateIndex) * 0.37f);
            if (moveEndpoint) {
                moving = fixed;
                CenterSenseDsp::magicEQSetLowEdgePercent(
                    lowPercent + offset, moving.centerFreq, moving.halfWidthPercent);
            } else {
                const float centerPercent = CenterSenseDsp::magicEQFrequencyToPercent(fixed.centerFreq);
                moving = fixed;
                moving.centerFreq = CenterSenseDsp::magicEQPercentToFrequency(centerPercent + offset);
            }
            ++updateIndex;
            framesSinceUpdate = 0;
        }
        for (size_t i = 0; i < blockFrames; ++i, ++absoluteFrame) {
            const float tone = toneAmplitude * std::sin(
                2.0f * 3.14159265358979323846f * toneHz *
                static_cast<float>(absoluteFrame) / static_cast<float>(sampleRate));
            fixedBlock[i] = tone;
            movingBlock[i] = fixedBlock[i];
        }
        CenterSenseDsp::processMagicEQ(fixedBlock.data(), blockFrames, 1, sampleRate,
                                       fixed, &fixedState);
        CenterSenseDsp::processMagicEQ(movingBlock.data(), blockFrames, 1, sampleRate,
                                       moving, &movingState);
        for (size_t i = 0; i < blockFrames; ++i) {
            const float a = fixedBlock[i];
            const float b = movingBlock[i];
            metrics.finite = metrics.finite && std::isfinite(a) && std::isfinite(b);
            metrics.maxOutputPeak = std::max(metrics.maxOutputPeak, std::fabs(b));
            if (block >= settleBlocks && framesSinceUpdate < transitionFrames)
                metrics.maxTransitionError = std::max(metrics.maxTransitionError, std::fabs(b - a));
            if (framesSinceUpdate < transitionFrames) ++framesSinceUpdate;
        }
    }
    return metrics;
}

struct BroadbandSafetyMetrics {
    float movingPeak = 0.0f;
    float staticPeak = 0.0f;
    float maxAlignedDifference = 0.0f;
    float maxExtraStep = 0.0f;
    bool finite = true;
};

BroadbandSafetyMetrics rapidBroadbandPeak(int sampleRate, size_t blockFrames,
                                          CenterSenseDsp::MagicEQMode mode, float halfWidth,
                                          float inputPeak = 0.08f,
                                          CenterSenseDsp::MagicEQSteepness grade = CenterSenseDsp::MagicEQSteepness::Steep) {
    CenterSenseDsp::MagicEQProcessParams params{};
    params.enabled = true;
    params.mode = mode;
    params.steepness = grade;
    params.centerFreq = 6000.0f;
    params.halfWidthPercent = halfWidth;
    CenterSenseDsp::MagicEQState state{};
    CenterSenseDsp::MagicEQState staticState{};
    const auto staticParams = params;
    std::vector<float> block(blockFrames);
    std::vector<float> staticBlock(blockFrames);
    uint32_t noise = 0x13579bdfu;
    BroadbandSafetyMetrics result{};
    float previousMoving = 0.0f, previousStatic = 0.0f;
    for (size_t blockIndex = 0; blockIndex < 96; ++blockIndex) {
        if ((blockIndex % 2u) == 0u) {
            const float centerPercent = CenterSenseDsp::magicEQFrequencyToPercent(params.centerFreq);
            params.centerFreq = CenterSenseDsp::magicEQPercentToFrequency(
                centerPercent + 0.2f * std::sin(static_cast<float>(blockIndex) * 0.31f));
        }
        for (size_t i = 0; i < blockFrames; ++i) {
            noise = noise * 1664525u + 1013904223u;
            block[i] = (static_cast<float>((noise >> 8) & 0x00ffffffu) /
                        8388607.5f - 1.0f) * inputPeak;
        }
        staticBlock = block;
        CenterSenseDsp::processMagicEQ(staticBlock.data(), blockFrames, 1, sampleRate,
                                       staticParams, &staticState);
        for (float sample : staticBlock) {
            result.finite = result.finite && std::isfinite(sample);
            result.staticPeak = std::max(result.staticPeak, std::fabs(sample));
        }
        CenterSenseDsp::processMagicEQ(block.data(), blockFrames, 1, sampleRate,
                                       params, &state);
        for (size_t sampleIndex = 0; sampleIndex < blockFrames; ++sampleIndex) {
            const float moving = block[sampleIndex];
            const float stationary = staticBlock[sampleIndex];
            result.maxAlignedDifference = std::max(result.maxAlignedDifference,
                std::fabs(moving - stationary));
            result.maxExtraStep = std::max(result.maxExtraStep,
                std::fabs((moving - previousMoving) - (stationary - previousStatic)));
            previousMoving = moving;
            previousStatic = stationary;
        }
        for (float sample : block) {
            result.finite = result.finite && std::isfinite(sample);
            result.movingPeak = std::max(result.movingPeak, std::fabs(sample));
        }
    }
    return result;
}

struct TransitionMetrics {
    float maxAdjacentStep = 0.0f;
    float maxOutputPeak = 0.0f;
    size_t maxStepFrame = 0;
    bool finite = true;
};

struct ImpulseSafetyMetrics {
    float tailSeconds = 0.0f;
    float peak = 0.0f;
    double l1 = 0.0;
    bool finite = true;
};

ImpulseSafetyMetrics measureProductImpulse(const CenterSenseDsp::MagicEQProcessParams& params,
                                            int sampleRate) {
    const size_t frames = static_cast<size_t>(sampleRate) / 2;
    std::vector<float> impulse(frames, 0.0f);
    impulse[0] = 1.0f;
    CenterSenseDsp::MagicEQState state{};
    for (size_t offset = 0; offset < frames; offset += 512) {
        const size_t count = std::min(size_t{512}, frames - offset);
        CenterSenseDsp::processMagicEQ(impulse.data() + offset, count, 1,
                                       sampleRate, params, &state);
    }
    ImpulseSafetyMetrics result{};
    for (float sample : impulse) {
        result.finite = result.finite && std::isfinite(sample);
        result.peak = std::max(result.peak, std::fabs(sample));
        result.l1 += std::fabs(sample);
    }
    const float threshold = result.peak * 1.0e-4f;
    size_t lastAbove = 0;
    for (size_t frame = 0; frame < frames; ++frame)
        if (std::fabs(impulse[frame]) > threshold) lastAbove = frame;
    result.tailSeconds = static_cast<float>(lastAbove) / sampleRate;
    return result;
}

ImpulseSafetyMetrics measureWarmedImpulse(const CenterSenseDsp::MagicEQProcessParams& params,
                                           int sampleRate) {
    CenterSenseDsp::MagicEQState impulseState{}, baselineState{};
    std::vector<float> block(512), baseline(512);
    const size_t warmFrames = static_cast<size_t>(sampleRate) / 5;
    for (size_t offset = 0; offset < warmFrames; offset += block.size()) {
        const size_t count = std::min(block.size(), warmFrames - offset);
        for (size_t frame = 0; frame < count; ++frame)
            block[frame] = baseline[frame] = static_cast<float>(0.02 * std::sin(
                2.0 * 3.14159265358979323846 * 773.0 * (offset + frame) / sampleRate));
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, params, &impulseState);
        CenterSenseDsp::processMagicEQ(baseline.data(), count, 1, sampleRate, params, &baselineState);
    }
    const size_t frames = static_cast<size_t>(sampleRate) / 2;
    std::vector<float> response(frames);
    for (size_t offset = 0; offset < frames; offset += block.size()) {
        const size_t count = std::min(block.size(), frames - offset);
        std::fill(block.begin(), block.end(), 0.0f);
        std::fill(baseline.begin(), baseline.end(), 0.0f);
        if (offset == 0) block[0] = 1.0f;
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, params, &impulseState);
        CenterSenseDsp::processMagicEQ(baseline.data(), count, 1, sampleRate, params, &baselineState);
        for (size_t frame = 0; frame < count; ++frame)
            response[offset + frame] = block[frame] - baseline[frame];
    }
    ImpulseSafetyMetrics result{};
    for (float sample : response) {
        result.finite = result.finite && std::isfinite(sample);
        result.peak = std::max(result.peak, std::fabs(sample));
        result.l1 += std::fabs(sample);
    }
    const float threshold = result.peak * 1.0e-4f;
    for (size_t frame = 0; frame < frames; ++frame)
        if (std::fabs(response[frame]) > threshold)
            result.tailSeconds = static_cast<float>(frame) / sampleRate;
    return result;
}

std::complex<double> productFilterResponse(const CenterSenseDsp::MagicEQState::FilterBank& bank,
                                           double frequency, int sampleRate) {
    const auto z = std::polar(1.0, -2.0 * 3.14159265358979323846 * frequency / sampleRate);
    std::complex<double> response = 1.0;
    for (int index = 0; index < bank.sectionCount; ++index) {
        const auto& b = bank.sections[index];
        response *= (b.b0 + b.b1 * z + b.b2 * z * z) /
                    (1.0 + b.a1 * z + b.a2 * z * z);
    }
    return response;
}

struct ResidualMetrics {
    float peak = 0.0f;
    double squaredSum = 0.0;
    size_t sampleCount = 0;
    bool finite = true;

    float rms() const {
        return sampleCount == 0 ? 0.0f :
            static_cast<float>(std::sqrt(squaredSum / static_cast<double>(sampleCount)));
    }
};

ResidualMetrics measureModeSwitchTail(const CenterSenseDsp::MagicEQProcessParams& before,
                                      const CenterSenseDsp::MagicEQProcessParams& after,
                                      int sampleRate, size_t blockFrames, float toneHz) {
    CenterSenseDsp::MagicEQState actualState{};
    CenterSenseDsp::MagicEQState steadyState{};
    std::vector<float> actual(blockFrames);
    std::vector<float> steady(blockFrames);
    const size_t warmupFrames = static_cast<size_t>(sampleRate) / 2;
    const size_t observationFrames = static_cast<size_t>(sampleRate) / 10;
    const size_t tailStart = static_cast<size_t>(sampleRate) / 20;
    size_t frame = 0;
    while (frame < warmupFrames) {
        const size_t count = std::min(blockFrames, warmupFrames - frame);
        for (size_t i = 0; i < count; ++i) {
            const float input = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * toneHz *
                static_cast<float>(frame + i) / static_cast<float>(sampleRate) + 0.37f);
            actual[i] = input;
            steady[i] = input;
        }
        CenterSenseDsp::processMagicEQ(actual.data(), count, 1, sampleRate, before, &actualState);
        CenterSenseDsp::processMagicEQ(steady.data(), count, 1, sampleRate, after, &steadyState);
        frame += count;
    }

    ResidualMetrics residual;
    size_t observed = 0;
    while (observed < observationFrames) {
        const size_t count = std::min(blockFrames, observationFrames - observed);
        for (size_t i = 0; i < count; ++i) {
            const float input = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * toneHz *
                static_cast<float>(frame + i) / static_cast<float>(sampleRate) + 0.37f);
            actual[i] = input;
            steady[i] = input;
        }
        CenterSenseDsp::processMagicEQ(actual.data(), count, 1, sampleRate, after, &actualState);
        CenterSenseDsp::processMagicEQ(steady.data(), count, 1, sampleRate, after, &steadyState);
        for (size_t i = 0; i < count; ++i) {
            if (observed + i < tailStart) continue;
            const float difference = actual[i] - steady[i];
            residual.finite = residual.finite && std::isfinite(difference);
            residual.peak = std::max(residual.peak, std::fabs(difference));
            residual.squaredSum += static_cast<double>(difference) * difference;
            ++residual.sampleCount;
        }
        observed += count;
        frame += count;
    }
    return residual;
}

float steadyAdjacentStep(const CenterSenseDsp::MagicEQProcessParams& params,
                         int sampleRate, size_t blockFrames, float toneHz) {
    CenterSenseDsp::MagicEQState state{};
    std::vector<float> block(blockFrames);
    const size_t totalFrames = static_cast<size_t>(sampleRate) / 2;
    float previous = 0.0f;
    float maxStep = 0.0f;
    for (size_t start = 0; start < totalFrames; start += blockFrames) {
        const size_t count = std::min(blockFrames, totalFrames - start);
        for (size_t i = 0; i < count; ++i) {
            const size_t frame = start + i;
            block[i] = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * toneHz *
                static_cast<float>(frame) / static_cast<float>(sampleRate) + 0.37f);
        }
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, params, &state);
        if (start >= static_cast<size_t>(sampleRate) / 4) {
            for (size_t i = 0; i < count; ++i) {
                maxStep = std::max(maxStep, std::fabs(block[i] - previous));
                previous = block[i];
            }
        } else if (count > 0) {
            previous = block[count - 1];
        }
    }
    return maxStep;
}

TransitionMetrics measureParameterTransition(
    const CenterSenseDsp::MagicEQProcessParams& before,
    const CenterSenseDsp::MagicEQProcessParams& after,
    int sampleRate, size_t blockFrames, float toneHz) {
    CenterSenseDsp::MagicEQState state{};
    std::vector<float> block(blockFrames);
    TransitionMetrics metrics;
    size_t absoluteFrame = 0;
    const size_t settleFrames = static_cast<size_t>(sampleRate) / 2;
    while (absoluteFrame < settleFrames) {
        const size_t count = std::min(blockFrames, settleFrames - absoluteFrame);
        for (size_t i = 0; i < count; ++i) {
            const size_t frame = absoluteFrame + i;
            block[i] = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * toneHz *
                static_cast<float>(frame) / static_cast<float>(sampleRate) + 0.37f);
        }
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, before, &state);
        absoluteFrame += count;
    }
    float previous = block[(settleFrames - 1) % blockFrames];
    size_t measuredFrames = 0;
    const size_t transitionFrames = static_cast<size_t>(sampleRate) / 50;
    while (measuredFrames < transitionFrames) {
        const size_t count = std::min(blockFrames, transitionFrames - measuredFrames);
        for (size_t i = 0; i < count; ++i) {
            const size_t frame = absoluteFrame + i;
            block[i] = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * toneHz *
                static_cast<float>(frame) / static_cast<float>(sampleRate) + 0.37f);
        }
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, after, &state);
        for (size_t i = 0; i < count; ++i) {
            const float sample = block[i];
            metrics.finite = metrics.finite && std::isfinite(sample);
            metrics.maxOutputPeak = std::max(metrics.maxOutputPeak, std::fabs(sample));
            const float step = std::fabs(sample - previous);
            if (step > metrics.maxAdjacentStep) {
                metrics.maxAdjacentStep = step;
                metrics.maxStepFrame = absoluteFrame + i;
            }
            previous = sample;
        }
        absoluteFrame += count;
        measuredFrames += count;
    }
    return metrics;
}

TransitionMetrics measureContinuousParameterMotion(int sampleRate, size_t blockFrames) {
    CenterSenseDsp::MagicEQState state{};
    CenterSenseDsp::MagicEQProcessParams params{};
    params.enabled = true;
    params.mode = CenterSenseDsp::MagicEQMode::Pass;
    params.centerFreq = 700.0f;
    params.halfWidthPercent = 2.5f;
    std::vector<float> block(blockFrames);
    TransitionMetrics metrics;
    const size_t totalFrames = static_cast<size_t>(sampleRate);
    const size_t controlInterval = std::max<size_t>(1, static_cast<size_t>(sampleRate) / 120);
    const size_t warmupFrames = static_cast<size_t>(sampleRate) / 4;
    size_t warmupPosition = 0;
    float previous = 0.0f;
    while (warmupPosition < warmupFrames) {
        const size_t count = std::min(blockFrames, warmupFrames - warmupPosition);
        for (size_t i = 0; i < count; ++i) {
            const size_t frame = warmupPosition + i;
            block[i] = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * 600.0f *
                static_cast<float>(frame) / static_cast<float>(sampleRate) + 0.37f);
        }
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, params, &state);
        previous = block[count - 1];
        warmupPosition += count;
    }
    for (size_t start = 0, blockIndex = 0; start < totalFrames; start += blockFrames, ++blockIndex) {
        const size_t updateIndex = start / controlInterval;
        if (blockIndex == 0 || (start - blockFrames) / controlInterval != updateIndex) {
            const float phase = static_cast<float>(updateIndex) * 0.19f;
            const float centerPercent = 50.0f + 42.0f * std::sin(phase);
            params.centerFreq = CenterSenseDsp::magicEQPercentToFrequency(centerPercent);
            params.halfWidthPercent = 2.0f + 1.5f * (1.0f + std::sin(phase * 0.71f));
        }
        const size_t count = std::min(blockFrames, totalFrames - start);
        for (size_t i = 0; i < count; ++i) {
            const size_t frame = warmupFrames + start + i;
            block[i] = 0.22f * std::sin(
                2.0f * 3.14159265358979323846f * 600.0f *
                static_cast<float>(frame) / static_cast<float>(sampleRate) + 0.37f);
        }
        CenterSenseDsp::processMagicEQ(block.data(), count, 1, sampleRate, params, &state);
        for (size_t i = 0; i < count; ++i) {
            const float sample = block[i];
            metrics.finite = metrics.finite && std::isfinite(sample);
            metrics.maxOutputPeak = std::max(metrics.maxOutputPeak, std::fabs(sample));
            const float step = std::fabs(sample - previous);
            if (step > metrics.maxAdjacentStep) {
                metrics.maxAdjacentStep = step;
                metrics.maxStepFrame = start + i;
            }
            previous = sample;
        }
    }
    return metrics;
}

}

int main() {
    if (std::getenv("ARIS_MAGIC_EQ_BENCH")) {
        using Clock = std::chrono::steady_clock;
        const auto report = [](const char* kind, const char* scenario, int rate, size_t frames,
                               std::vector<double>& times, double budgetUs) {
            std::sort(times.begin(), times.end());
            const auto percentile = [&](double p) { return times[static_cast<size_t>(p * (times.size() - 1))]; };
            std::printf("PERF kind=%s scenario=%s rate=%d frames=%zu n=%zu budget_us=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f max_us=%.3f\n",
                        kind, scenario, rate, frames, times.size(), budgetUs,
                        percentile(0.50), percentile(0.95), percentile(0.99), times.back());
            if (kind[0] == 'a')
                check(percentile(0.99) < budgetUs * 0.10,
                      "Magic EQ processing p99 stays under 10% of the audio block budget");
        };
#ifdef _WIN32
        const auto memoryKb = []() -> size_t {
            PROCESS_MEMORY_COUNTERS_EX info{};
            info.cb = sizeof(info);
            return GetProcessMemoryInfo(GetCurrentProcess(),
                                        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info), sizeof(info))
                ? info.PrivateUsage / 1024 : 0;
        };
        const auto resourceSnapshot = [](const char* phase, int round) {
            PROCESS_MEMORY_COUNTERS_EX info{};
            info.cb = sizeof(info);
            GetProcessMemoryInfo(GetCurrentProcess(),
                                 reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info), sizeof(info));
            DWORD handles = 0;
            GetProcessHandleCount(GetCurrentProcess(), &handles);
            const DWORD gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            const DWORD user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
            HANDLE heaps[128]{};
            const DWORD heapCount = GetProcessHeaps(128, heaps);
            size_t liveBlocks = 0, liveBytes = 0;
            for (DWORD index = 0; index < std::min<DWORD>(heapCount, 128); ++index) {
                if (!HeapLock(heaps[index])) continue;
                PROCESS_HEAP_ENTRY entry{};
                while (HeapWalk(heaps[index], &entry)) {
                    if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
                        ++liveBlocks;
                        liveBytes += entry.cbData;
                    }
                }
                HeapUnlock(heaps[index]);
            }
            std::printf("PERF resources phase=%s round=%d private_kb=%zu working_set_kb=%zu handles=%lu gdi=%lu user=%lu heap_count=%lu live_blocks=%zu live_bytes=%zu\n",
                        phase, round, size_t(info.PrivateUsage / 1024),
                        size_t(info.WorkingSetSize / 1024), static_cast<unsigned long>(handles),
                        static_cast<unsigned long>(gdi), static_cast<unsigned long>(user),
                        static_cast<unsigned long>(heapCount), liveBlocks, liveBytes);
        };
        const size_t memoryBefore = memoryKb();
        FILETIME created{}, exited{}, kernelBefore{}, userBefore{};
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernelBefore, &userBefore);
        const auto wallBefore = Clock::now();
#endif
        volatile double sink = 0.0;
        for (int rate : {44100, 48000, 96000}) {
            for (size_t frames : {size_t(32), size_t(64), size_t(256), size_t(1024)}) {
                for (int scenario = 0; scenario < 10; ++scenario) {
                    CenterSenseDsp::MagicEQProcessParams params{};
                    params.enabled = scenario != 0;
                    params.mode = scenario >= 5 && scenario <= 8
                        ? CenterSenseDsp::MagicEQMode::Notch : CenterSenseDsp::MagicEQMode::Pass;
                    params.steepness = static_cast<CenterSenseDsp::MagicEQSteepness>(
                        scenario >= 5 && scenario <= 8 ? scenario - 5 : std::clamp(scenario - 1, 0, 3));
                    params.centerFreq = 1000.0f;
                    params.halfWidthPercent = 10.0f;
                    CenterSenseDsp::MagicEQState state{};
                    std::vector<float> block(frames * 2, 0.1f);
                    const std::vector<float> input = sine(1000.0f, rate, frames, 2);
                    std::vector<double> times;
                    times.reserve(768);
                    for (int iteration = 0; iteration < 800; ++iteration) {
                        if (scenario == 9) {
                            params.centerFreq = 300.0f + 1700.0f * float(iteration % 50) / 49.0f;
                            params.halfWidthPercent = 2.0f + float(iteration % 20);
                            params.mode = iteration % 2 ? CenterSenseDsp::MagicEQMode::Pass
                                                         : CenterSenseDsp::MagicEQMode::Notch;
                        }
                        std::copy(input.begin(), input.end(), block.begin());
                        const auto start = Clock::now();
                        CenterSenseDsp::processMagicEQ(block.data(), frames, 2, rate, params, &state);
                        const auto end = Clock::now();
                        sink += block[iteration % block.size()];
                        if (iteration >= 32)
                            times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
                    }
                    const char* names[] = {"bypass", "pass-soft", "pass-steep", "pass-very",
                                           "pass-wall", "notch-soft", "notch-steep", "notch-very",
                                           "notch-wall", "moving"};
                    report("audio", names[scenario], rate, frames, times,
                           1.0e6 * double(frames) / double(rate));
                }
            }
        }
        for (int scenario = 0; scenario < 9; ++scenario) {
            CenterSenseDsp::MagicEQProcessParams params{};
            params.enabled = scenario != 0;
            params.mode = scenario >= 5 ? CenterSenseDsp::MagicEQMode::Notch
                                        : CenterSenseDsp::MagicEQMode::Pass;
            params.steepness = static_cast<CenterSenseDsp::MagicEQSteepness>(
                scenario >= 5 ? scenario - 5 : std::max(0, scenario - 1));
            params.centerFreq = 1000.0f;
            params.halfWidthPercent = 10.0f;
            std::vector<double> times;
            times.reserve(768);
            for (int iteration = 0; iteration < 800; ++iteration) {
                const auto start = Clock::now();
                const auto slope = CenterSenseDsp::measureMagicEQLocalSlope(params, 48000);
                const auto end = Clock::now();
                sink += slope.low.dbPerOctave;
                if (iteration >= 32)
                    times.push_back(std::chrono::duration<double, std::micro>(end - start).count());
            }
            const char* names[] = {"bypass", "pass-soft", "pass-steep", "pass-very",
                                   "pass-wall", "notch-soft", "notch-steep", "notch-very",
                                   "notch-wall"};
            report("ui-slope", names[scenario], 48000, 0, times, 16667.0);
        }
#ifdef _WIN32
        {
            CenterSenseDsp::MagicEQProcessParams params{};
            params.enabled = true;
            params.mode = CenterSenseDsp::MagicEQMode::Pass;
            params.steepness = CenterSenseDsp::MagicEQSteepness::VerySteep;
            params.centerFreq = 1000.0f;
            params.halfWidthPercent = 10.0f;
            CenterSenseDsp::MagicEQState state{};
            const std::vector<float> input = sine(1000.0f, 48000, 256, 2);
            std::vector<float> block(input.size());
            const auto step = [&]() {
                std::copy(input.begin(), input.end(), block.begin());
                CenterSenseDsp::processMagicEQ(block.data(), 256, 2, 48000, params, &state);
                sink += CenterSenseDsp::measureMagicEQLocalSlope(params, 48000).low.dbPerOctave;
            };
            for (int iteration = 0; iteration < 1000; ++iteration) step();
            const size_t steadyMemory = memoryKb();
            DWORD steadyHandles = 0;
            GetProcessHandleCount(GetCurrentProcess(), &steadyHandles);
            for (int round = 1; round <= 4; ++round) {
                for (int iteration = 0; iteration < 2500; ++iteration) step();
                DWORD handles = 0;
                GetProcessHandleCount(GetCurrentProcess(), &handles);
                std::printf("PERF steady_round=%d private_memory_kb=%zu handles=%lu\n",
                            round, memoryKb(), static_cast<unsigned long>(handles));
                check(memoryKb() <= steadyMemory + 64,
                      "steady Magic EQ processing and slope measurement do not grow private memory beyond 64 KiB");
                check(handles == steadyHandles,
                      "steady Magic EQ processing and slope measurement do not retain handles");
            }
        }
        {
            CenterSenseDsp::MagicEQProcessParams params{};
            CenterSenseDsp::MagicEQState state{};
            const std::vector<float> input = sine(700.0f, 48000, 64, 2);
            std::vector<float> block(input.size());
            const auto runDragCycle = [&](int cycle) {
                for (int step = 0; step < 4000; ++step) {
                    const int serial = cycle * 4000 + step;
                    params.enabled = serial % 11 != 0;
                    params.mode = serial % 2 ? CenterSenseDsp::MagicEQMode::Notch
                                             : CenterSenseDsp::MagicEQMode::Pass;
                    params.steepness = static_cast<CenterSenseDsp::MagicEQSteepness>(serial % 4);
                    if (step % 4 == 0) {
                        params.centerFreq = CenterSenseDsp::magicEQPercentToFrequency(
                            5.0f + float(serial % 900) * 0.1f);
                    } else if (step % 4 == 1) {
                        CenterSenseDsp::magicEQSetLowEdgePercent(
                            float(serial % 450) * 0.1f, params.centerFreq, params.halfWidthPercent);
                    } else if (step % 4 == 2) {
                        CenterSenseDsp::magicEQSetHighEdgePercent(
                            55.0f + float(serial % 450) * 0.1f,
                            params.centerFreq, params.halfWidthPercent);
                    } else {
                        params.halfWidthPercent = 1.0f + float(serial % 40);
                    }
                    std::copy(input.begin(), input.end(), block.begin());
                    CenterSenseDsp::processMagicEQ(block.data(), 64, 2, 48000, params, &state);
                    for (int grade = 0; grade < 3; ++grade) {
                        auto displayParams = params;
                        displayParams.steepness = static_cast<CenterSenseDsp::MagicEQSteepness>(grade);
                        sink += CenterSenseDsp::measureMagicEQLocalSlope(displayParams, 48000).low.dbPerOctave;
                    }
                }
            };
            runDragCycle(0);
            resourceSnapshot("drag-warm", 0);
            for (int round = 1; round <= 6; ++round) {
                runDragCycle(round);
                resourceSnapshot("drag-active", round);
                for (int idle = 0; idle < 3000; ++idle) {
                    std::copy(input.begin(), input.end(), block.begin());
                    CenterSenseDsp::processMagicEQ(block.data(), 64, 2, 48000, params, &state);
                    sink += CenterSenseDsp::measureMagicEQLocalSlope(params, 48000).low.dbPerOctave;
                }
                resourceSnapshot("drag-idle", round);
            }
        }
        FILETIME kernelAfter{}, userAfter{};
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernelAfter, &userAfter);
        const auto ticks = [](const FILETIME& time) {
            return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        };
        const double cpuMs = double(ticks(kernelAfter) + ticks(userAfter) -
                                    ticks(kernelBefore) - ticks(userBefore)) / 10000.0;
        const double wallMs = std::chrono::duration<double, std::milli>(Clock::now() - wallBefore).count();
        std::printf("PERF cpu_ms=%.3f wall_ms=%.3f cpu_over_wall=%.3f private_memory_kb_before=%zu after=%zu sink=%.3f\n",
                    cpuMs, wallMs, cpuMs / wallMs, memoryBefore, memoryKb(), sink);
#endif
        return failures == 0 ? 0 : 1;
    }
    constexpr int sampleRate = 48000;
    for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass,
                            CenterSenseDsp::MagicEQMode::Notch}) {
        CenterSenseDsp::MagicEQProcessParams tailParams{};
        tailParams.enabled = true;
        tailParams.mode = mode;
        tailParams.steepness = CenterSenseDsp::MagicEQSteepness::Brickwall;
        tailParams.centerFreq = 1000.0f;
        tailParams.halfWidthPercent = 10.0f;
        const auto wide = CenterSenseDsp::estimateMagicEQTail(tailParams, sampleRate);
        check(wide.available && wide.estimatedSeconds < 0.2f,
              "near-wall grade remains usable for a sufficiently wide midband");
        tailParams.centerFreq = 100.0f;
        tailParams.halfWidthPercent = 1.0f;
        const auto narrow = CenterSenseDsp::estimateMagicEQTail(tailParams, sampleRate);
        check(!narrow.available && narrow.estimatedSeconds > 0.2f,
              "near-wall low narrow band is disabled by tail budget");
    }
    for (const int rate : {44100, 48000, 96000}) {
        for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass,
                                CenterSenseDsp::MagicEQMode::Notch}) {
            for (const auto grade : {CenterSenseDsp::MagicEQSteepness::Soft,
                                     CenterSenseDsp::MagicEQSteepness::Steep,
                                     CenterSenseDsp::MagicEQSteepness::VerySteep,
                                     CenterSenseDsp::MagicEQSteepness::Brickwall}) {
                for (const float center : {25.0f, 50.0f, 100.0f, 250.0f,
                                           1000.0f, 5000.0f, 10000.0f, 15000.0f}) {
                    for (const float width : {1.0f, 2.0f, 3.0f, 5.0f,
                                              10.0f, 20.0f, 25.0f, 40.0f}) {
                        CenterSenseDsp::MagicEQProcessParams params{};
                        params.enabled = true;
                        params.mode = mode;
                        params.steepness = grade;
                        params.centerFreq = center;
                        params.halfWidthPercent = width;
                        const auto gate = CenterSenseDsp::estimateMagicEQTail(params, rate);
                        const auto impulse = measureProductImpulse(params, rate);
                        std::printf("MagicEQTail rate=%d mode=%d grade=%d center=%.1f width=%.1f estimate=%.5f measured=%.5f impulse_peak=%.7f impulse_l1_500ms=%.5f available=%d\n",
                            rate, static_cast<int>(mode), static_cast<int>(grade), center, width,
                            gate.estimatedSeconds, impulse.tailSeconds, impulse.peak, impulse.l1,
                            gate.available ? 1 : 0);
                        check(impulse.finite && std::isfinite(impulse.l1),
                              "zero-state product impulse remains finite");
                        check(!gate.available || impulse.tailSeconds <= 0.2f,
                              "accepted grade has measured product impulse tail within 200 ms");
                    }
                }
            }
        }
    }
    for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass,
                            CenterSenseDsp::MagicEQMode::Notch}) {
        for (const auto grade : {CenterSenseDsp::MagicEQSteepness::Steep,
                                 CenterSenseDsp::MagicEQSteepness::Brickwall}) {
            CenterSenseDsp::MagicEQProcessParams params{};
            params.enabled = true;
            params.mode = mode;
            params.steepness = grade;
            params.centerFreq = 1000.0f;
            params.halfWidthPercent = 10.0f;
            const auto zero = measureProductImpulse(params, sampleRate);
            const auto warm = measureWarmedImpulse(params, sampleRate);
            std::printf("MagicEQImpulseState mode=%d grade=%d zero_peak=%.7f warm_peak=%.7f zero_l1=%.5f warm_l1=%.5f zero_tail=%.5f warm_tail=%.5f\n",
                static_cast<int>(mode), static_cast<int>(grade), zero.peak, warm.peak,
                zero.l1, warm.l1, zero.tailSeconds, warm.tailSeconds);
            check(zero.finite && warm.finite &&
                      std::fabs(zero.l1 - warm.l1) <= zero.l1 * 0.01,
                  "warmed-state impulse response matches zero-state impulse response");
        }
    }
    for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass,
                            CenterSenseDsp::MagicEQMode::Notch}) {
        for (const auto grade : {CenterSenseDsp::MagicEQSteepness::Soft,
                                 CenterSenseDsp::MagicEQSteepness::Steep,
                                 CenterSenseDsp::MagicEQSteepness::VerySteep,
                                 CenterSenseDsp::MagicEQSteepness::Brickwall}) {
            CenterSenseDsp::MagicEQProcessParams params{};
            params.enabled = true;
            params.mode = mode;
            params.steepness = grade;
            params.centerFreq = 1000.0f;
            params.halfWidthPercent = 10.0f;
            CenterSenseDsp::MagicEQState state{};
            CenterSenseDsp::prepareMagicEQ(&state, sampleRate, params);
            const auto& bank = state.banks[state.activeBank];
            const auto db = [&](double hz) {
                return 20.0 * std::log10(std::max(1.0e-12,
                    std::abs(productFilterResponse(bank, hz, sampleRate))));
            };
            const auto crossing = [&](double left, double right) {
                const double leftValue = db(left) + 3.01029995664;
                for (int iteration = 0; iteration < 48; ++iteration) {
                    const double middle = 0.5 * (left + right);
                    if ((db(middle) + 3.01029995664) * leftValue <= 0.0) right = middle;
                    else left = middle;
                }
                return 0.5 * (left + right);
            };
            const double low3 = crossing(20.0, 1000.0);
            const double high3 = crossing(1000.0, 20000.0);
            const double f = mode == CenterSenseDsp::MagicEQMode::Pass ? 1000.0 : low3 * 0.8;
            const auto below = productFilterResponse(bank, f - 1.0, sampleRate);
            const auto above = productFilterResponse(bank, f + 1.0, sampleRate);
            const double groupDelayMs = -1000.0 * std::arg(above / below) /
                (4.0 * 3.14159265358979323846);
            double maxGainDb = -240.0;
            for (int point = 0; point <= 2048; ++point) {
                const double hz = 20.0 * std::pow(1000.0, double(point) / 2048.0);
                maxGainDb = std::max(maxGainDb, db(hz));
            }
            std::printf("MagicEQProductResponse mode=%d grade=%d prototype_n=%d sections=%d total_order=%d nominal_db_oct=%.2f low_3db_hz=%.3f high_3db_hz=%.3f q=%.3f db100=%.2f db400=%.2f db1000=%.2f db2500=%.2f db10000=%.2f max_gain_db=%.4f group_delay_ms=%.3f algorithmic_latency_samples=0\n",
                static_cast<int>(mode), static_cast<int>(grade), bank.sectionCount,
                bank.sectionCount, bank.sectionCount * 2, bank.sectionCount * 6.0206,
                low3, high3, 1000.0 / (high3 - low3), db(100.0), db(400.0),
                db(1000.0), db(2500.0), db(10000.0), maxGainDb, groupDelayMs);
            check(std::isfinite(low3) && std::isfinite(high3) && low3 < 1000.0 &&
                      high3 > 1000.0 && std::isfinite(groupDelayMs),
                  "product coefficient response and group delay are finite");
            check(std::isfinite(maxGainDb) && maxGainDb <= 0.01,
                  "fixed response has no positive frequency gain beyond existing 0.01 dB tolerance");
        }
    }
    for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass,
                            CenterSenseDsp::MagicEQMode::Notch}) {
        CenterSenseDsp::MagicEQProcessParams params{};
        params.enabled = true;
        params.mode = mode;
        params.centerFreq = 1000.0f;
        params.halfWidthPercent = 10.0f;
        params.steepness = CenterSenseDsp::MagicEQSteepness::VerySteep;
        const auto currentHighest = CenterSenseDsp::measureMagicEQLocalSlope(params, sampleRate);
        params.steepness = static_cast<CenterSenseDsp::MagicEQSteepness>(3);
        const auto nearWall = CenterSenseDsp::measureMagicEQLocalSlope(params, sampleRate);
        check(currentHighest.low.valid && currentHighest.high.valid &&
              nearWall.low.valid && nearWall.high.valid &&
              std::fabs(nearWall.low.dbPerOctave) >
                  std::fabs(currentHighest.low.dbPerOctave) * 1.2f &&
              std::fabs(nearWall.high.dbPerOctave) >
                  std::fabs(currentHighest.high.dbPerOctave) * 1.2f,
              "new near-brickwall setting changes both real Pass and Notch edge responses");
    }
    {
        using Steepness = CenterSenseDsp::MagicEQSteepness;
        const Steepness choices[] = {Steepness::Soft, Steepness::Steep, Steepness::VerySteep};
        // Fixed 48 kHz reference: 1 kHz centre, 5% half-width, 700–850 Hz edge.
        // A dB difference over this finite interval is not a universal dB/oct slope.
        for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass, CenterSenseDsp::MagicEQMode::Notch}) {
            float previousDifference = -1.0f;
            for (int choice = 0; choice < 3; ++choice) {
                float response[3]{};
                for (int probe = 0; probe < 3; ++probe) {
                    const float frequency = probe == 0 ? 700.0f : (probe == 1 ? 850.0f : 1000.0f);
                    auto signal = sine(frequency, sampleRate, 48000, 1);
                    CenterSenseDsp::MagicEQProcessParams params{};
                    params.enabled = true;
                    params.mode = mode;
                    params.steepness = choices[choice];
                    params.centerFreq = 1000.0f;
                    params.halfWidthPercent = 5.0f;
                    CenterSenseDsp::MagicEQState state{};
                    CenterSenseDsp::processMagicEQ(signal.data(), signal.size(), 1, sampleRate, params, &state);
                    response[probe] = rms(signal, 24000, 48000, 1);
                    check(std::isfinite(response[probe]) && response[probe] > 0.0f,
                          "fixed reference response is finite and positive");
                }
                const float difference = 20.0f * std::log10(
                    mode == CenterSenseDsp::MagicEQMode::Pass
                        ? response[1] / response[0] : response[0] / response[1]);
                std::printf("MagicEQMeasuredEdge mode=%d choice=%d db700to850=%.3f centerRms=%.6f\n",
                            static_cast<int>(mode), choice, difference, response[2]);
                check(difference > previousDifference,
                      "fixed reference edge difference increases across three cascades");
                previousDifference = difference;
                if (mode == CenterSenseDsp::MagicEQMode::Pass)
                    check(response[2] <= 0.707107f + 0.01f,
                          "Pass reference center does not boost above input RMS");
                else
                    check(response[2] < 0.2f, "Notch reference center remains suppressed");
            }
        }
        for (const int rate : {44100, 48000, 96000}) {
            for (const auto mode : {CenterSenseDsp::MagicEQMode::Pass, CenterSenseDsp::MagicEQMode::Notch}) {
                for (const auto grade : choices) {
                    CenterSenseDsp::MagicEQProcessParams params{};
                    params.enabled = true;
                    params.mode = mode;
                    params.steepness = grade;
                    params.centerFreq = 1000.0f;
                    params.halfWidthPercent = 10.0f;
                    const auto measured = CenterSenseDsp::measureMagicEQLocalSlope(params, rate);
                    for (const auto side : {measured.low, measured.high}) {
                        check(side.valid && side.fromHz < side.toHz,
                              "current-band slope has a valid ordered local interval");
                        if (!side.valid) continue;
                        float output[2]{};
                        for (int probe = 0; probe < 2; ++probe) {
                            auto signal = sine(probe == 0 ? side.fromHz : side.toHz,
                                               rate, static_cast<size_t>(rate), 1);
                            CenterSenseDsp::MagicEQState state{};
                            CenterSenseDsp::processMagicEQ(signal.data(), signal.size(), 1, rate, params, &state);
                            output[probe] = rms(signal, signal.size() / 2, signal.size(), 1);
                        }
                        const float renderedSlope = 20.0f * std::log10(output[1] / output[0]) /
                            std::log2(side.toHz / side.fromHz);
                        check(std::isfinite(side.dbPerOctave) &&
                              std::fabs(renderedSlope - side.dbPerOctave) < 0.5f,
                              "local slope matches rendered current-parameter response");
                    }
                }
            }
        }
        CenterSenseDsp::MagicEQProcessParams boundary{};
        boundary.enabled = true;
        boundary.centerFreq = 1000.0f;
        boundary.halfWidthPercent = 0.01f;
        const auto narrow = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 48000);
        check(!narrow.low.valid && !narrow.high.valid,
              "too-narrow selection reports no invented local slope");
        boundary.enabled = false;
        const auto bypass = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 48000);
        check(!bypass.low.valid && !bypass.high.valid,
              "bypass reports no active filter slope");
        boundary.enabled = true;
        boundary.halfWidthPercent = 10.0f;
        boundary.centerFreq = 25.0f;
        const auto lowLimit = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 48000);
        check(!lowLimit.low.valid && lowLimit.low.reason ==
              CenterSenseDsp::MagicEQSlopeUnavailable::FrequencyLimit,
              "low edge near 20 Hz reports its frequency limit");
        boundary.centerFreq = 18000.0f;
        const auto highLimit = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 44100);
        check(!highLimit.high.valid && highLimit.high.reason ==
              CenterSenseDsp::MagicEQSlopeUnavailable::FrequencyLimit,
              "high edge near Nyquist reports its frequency limit");
        const auto noRate = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 0);
        check(!noRate.low.valid && noRate.low.reason ==
              CenterSenseDsp::MagicEQSlopeUnavailable::InvalidSampleRate,
              "unknown playback sample rate reports a reason instead of a number");
        boundary.centerFreq = 1000.0f;
        boundary.halfWidthPercent = 5.0f;
        const auto firstBand = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 48000);
        boundary.centerFreq = 1500.0f;
        const auto movedBand = CenterSenseDsp::measureMagicEQLocalSlope(boundary, 48000);
        check(firstBand.low.valid && movedBand.low.valid &&
              movedBand.low.fromHz > firstBand.low.fromHz &&
              std::fabs(movedBand.low.dbPerOctave - firstBand.low.dbPerOctave) > 0.01f,
              "moving the current frequency changes the measured interval and response");
        float responses[3] = {};
        for (int index = 0; index < 3; ++index) {
            CenterSenseDsp::MagicEQProcessParams params{};
            params.enabled = true;
            params.mode = CenterSenseDsp::MagicEQMode::Pass;
            params.steepness = choices[index];
            params.centerFreq = 1000.0f;
            params.halfWidthPercent = 5.0f;
            auto signal = sine(100.0f, sampleRate, 48000, 1);
            CenterSenseDsp::MagicEQState state{};
            CenterSenseDsp::processMagicEQ(signal.data(), signal.size(), 1, sampleRate, params, &state);
            responses[index] = rms(signal, 24000, 48000, 1);
            std::printf("SteepnessResponse choice=%d outOfBand100HzRms=%.9f\n", index, responses[index]);
            check(std::isfinite(responses[index]), "steepness response remains finite");
        }
        check(responses[0] > responses[1] && responses[1] > responses[2],
              "steepness choices produce strictly increasing out-of-band suppression");
    }
    {
        const float pct = CenterSenseDsp::magicEQFrequencyToPercent(1000.0f);
        const float rulerFrequencies[] = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                          1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};
        float previousPercent = -1.0f;
        for (float frequency : rulerFrequencies) {
            const float mapped = CenterSenseDsp::magicEQFrequencyToPercent(frequency);
            check(mapped > previousPercent && mapped >= 0.0f && mapped <= 100.0f,
                  "ruler major ticks follow ascending logarithmic frequency positions");
            check(near(CenterSenseDsp::magicEQPercentToFrequency(mapped), frequency, 0.02f),
                  "ruler major tick maps back to its labeled frequency");
            previousPercent = mapped;
        }
        check(near(CenterSenseDsp::magicEQFrequencyToPercent(20.0f), 0.0f, 1e-4f) &&
                  near(CenterSenseDsp::magicEQFrequencyToPercent(20000.0f), 100.0f, 1e-4f),
              "ruler endpoints cover 20 Hz to 20 kHz");
        check(near(CenterSenseDsp::magicEQPercentToFrequency(pct), 1000.0f, 0.02f),
              "logarithmic frequency mapping round trips");
        float low = 0.0f, high = 0.0f;
        CenterSenseDsp::magicEQEdges(1000.0f, 20.0f, low, high);
        check(near(std::log10(1000.0f) - std::log10(low),
                    std::log10(high) - std::log10(1000.0f), 1e-4f),
              "first-use band is symmetric on the logarithmic axis");
        check(near(CenterSenseDsp::magicEQFrequencyToPercent(high) -
                   CenterSenseDsp::magicEQFrequencyToPercent(low), 40.0f, 1e-3f),
              "first-use selection covers roughly 40 percent of the rail");
    }
    {
        float center = 1000.0f, half = 10.0f;
        float low = 0.0f, high = 0.0f;
        CenterSenseDsp::magicEQEdges(center, half, low, high);
        const float pinnedHigh = CenterSenseDsp::magicEQFrequencyToPercent(high);
        const float oldLow = CenterSenseDsp::magicEQFrequencyToPercent(low);
        CenterSenseDsp::magicEQSetLowEdgePercent(oldLow - 7.0f, center, half);
        CenterSenseDsp::magicEQEdges(center, half, low, high);
        check(near(CenterSenseDsp::magicEQFrequencyToPercent(high), pinnedHigh, 1e-3f),
              "left endpoint changes the low edge while pinning the high edge");
        check(CenterSenseDsp::magicEQFrequencyToPercent(low) < oldLow,
              "left endpoint moves the selected low frequency");
        const float pinnedLow = CenterSenseDsp::magicEQFrequencyToPercent(low);
        const float oldHigh = CenterSenseDsp::magicEQFrequencyToPercent(high);
        CenterSenseDsp::magicEQSetHighEdgePercent(oldHigh + 9.0f, center, half);
        CenterSenseDsp::magicEQEdges(center, half, low, high);
        check(near(CenterSenseDsp::magicEQFrequencyToPercent(low), pinnedLow, 1e-3f),
              "right endpoint changes the high edge while pinning the low edge");
        check(CenterSenseDsp::magicEQFrequencyToPercent(high) > oldHigh,
              "right endpoint moves the selected high frequency");
        CenterSenseDsp::magicEQSetLowEdgePercent(100.0f, center, half);
        check(half >= 1.0f && half <= 50.0f, "crossing endpoints are clamped to a valid band");
        CenterSenseDsp::MagicEQProcessParams params{};
        params.enabled = true;
        params.mode = CenterSenseDsp::MagicEQMode::Notch;
        params.centerFreq = 20000.0f;
        params.halfWidthPercent = 50.0f;
        CenterSenseDsp::MagicEQState normalized{};
        CenterSenseDsp::prepareMagicEQ(&normalized, 48000, params);
        check(near(CenterSenseDsp::magicEQFrequencyToPercent(normalized.centerFreq), 50.0f, 1e-3f),
              "center frequency stays inside a maximum-width selection");
    }
    {
        float stereo[] = {0.25f, -0.5f, 0.75f, -1.0f};
        const float original[] = {0.25f, -0.5f, 0.75f, -1.0f};
        CenterSenseDsp::MagicEQState state{};
        CenterSenseDsp::MagicEQProcessParams params{};
        CenterSenseDsp::processMagicEQ(stereo, 2, 2, sampleRate, params, &state);
        bool unchanged = true;
        for (int i = 0; i < 4; ++i) unchanged = unchanged && near(stereo[i], original[i], 1e-7f);
        check(unchanged, "bypass does not change audio samples");
    }
    {
        constexpr size_t frames = 48000;
        auto inBand = sine(1000.0f, sampleRate, frames, 2);
        auto outOfBand = sine(100.0f, sampleRate, frames, 2);
        CenterSenseDsp::MagicEQState inState{}, outState{};
        CenterSenseDsp::MagicEQProcessParams params{};
        params.enabled = true;
        params.mode = CenterSenseDsp::MagicEQMode::Pass;
        CenterSenseDsp::processMagicEQ(inBand.data(), frames, 2, sampleRate, params, &inState);
        CenterSenseDsp::processMagicEQ(outOfBand.data(), frames, 2, sampleRate, params, &outState);
        check(rms(inBand, frames / 2, frames, 2) > rms(outOfBand, frames / 2, frames, 2) * 4.0f,
              "Pass preserves frequencies inside the selection");
    }
    {
        constexpr size_t frames = 48000;
        auto center = sine(1000.0f, sampleRate, frames, 1);
        auto side = sine(300.0f, sampleRate, frames, 1);
        CenterSenseDsp::MagicEQState centerState{}, sideState{};
        CenterSenseDsp::MagicEQProcessParams params{};
        params.enabled = true;
        params.mode = CenterSenseDsp::MagicEQMode::Notch;
        CenterSenseDsp::processMagicEQ(center.data(), frames, 1, sampleRate, params, &centerState);
        CenterSenseDsp::processMagicEQ(side.data(), frames, 1, sampleRate, params, &sideState);
        check(rms(center, frames / 2, frames, 1) < rms(side, frames / 2, frames, 1) * 0.35f,
              "Notch suppresses its selected center frequency");
    }
    {
        CenterSenseDsp::MagicEQSettingsPublisher publisher;
        CenterSenseDsp::MagicEQSettings snapshot{};
        check(publisher.tryLoad(snapshot) && !snapshot.enabled,
              "audio-side parameter snapshot starts in bypass");
        snapshot.enabled = true;
        snapshot.mode = CenterSenseDsp::MagicEQMode::Notch;
        snapshot.centerFreq = 2400.0f;
        snapshot.halfWidthPercent = 7.0f;
        publisher.store(snapshot);
        CenterSenseDsp::MagicEQSettings current{};
        check(publisher.tryLoad(current) && current.enabled && current.mode == snapshot.mode &&
                  near(current.centerFreq, 2400.0f) && near(current.halfWidthPercent, 7.0f),
              "audio-side parameter snapshot publishes one complete selection");
    }

    {
        const int sampleRates[] = {44100, 96000};
        const size_t blockSizes[] = {64, 256, 1024};
        const float widths[] = {2.5f, 45.0f};
        const CenterSenseDsp::MagicEQMode modes[] = {
            CenterSenseDsp::MagicEQMode::Pass, CenterSenseDsp::MagicEQMode::Notch};
        for (int rate : sampleRates) {
            for (size_t block : blockSizes) {
                for (auto mode : modes) {
                    for (float width : widths) {
                        for (bool endpoint : {false, true}) {
                            const auto metrics = rapidSweep(rate, block, mode, width, endpoint);
                            std::printf("RapidSweep rate=%d block=%zu mode=%d width=%.1f motion=%s residual=%.6f peak=%.6f finite=%d\n",
                                        rate, block, static_cast<int>(mode), width,
                                        endpoint ? "endpoint" : "center", metrics.maxTransitionError,
                                        metrics.maxOutputPeak, metrics.finite ? 1 : 0);
                            check(metrics.finite, "rapid sweep remains finite");
                            check(metrics.maxOutputPeak <= 0.35f,
                                  "rapid sweep peak stays below the 0.35 full-scale fixture bound");
                            check(metrics.maxTransitionError <= 0.04f,
                                  "small range motion adds less than 0.04 extra transient on the unaffected tone");
                        }
                        const auto broadband = rapidBroadbandPeak(rate, block, mode, width);
                        std::printf("RapidBroadband rate=%d block=%zu mode=%d width=%.1f moving_peak=%.6f static_peak=%.6f aligned_difference=%.6f extra_step=%.6f legacy_absolute_0.16=%s\n",
                            rate, block, static_cast<int>(mode), width, broadband.movingPeak,
                            broadband.staticPeak, broadband.maxAlignedDifference,
                            broadband.maxExtraStep, broadband.movingPeak <= 0.16f ? "PASS" : "HISTORICAL_FAIL");
                        check(broadband.finite, "fixed and moving broadband responses remain finite");
                        check(broadband.staticPeak < 1.0f && broadband.movingPeak < 1.0f,
                              "bounded 0.08-peak fixture never clips at full scale");
                        check(broadband.maxExtraStep <= 0.04f,
                              "small-motion broadband adds at most the existing 0.04 step residual");
                    }
                }
            }
        }
    }
    {
        CenterSenseDsp::MagicEQProcessParams params{};
        params.enabled = true;
        params.mode = CenterSenseDsp::MagicEQMode::Pass;
        params.centerFreq = 1000.0f;
        params.halfWidthPercent = 20.0f;
        CenterSenseDsp::MagicEQState state{};
        auto quiet = sine(1000.0f, 48000, 4800, 2);
        for (float& sample : quiet) sample *= 0.08f;
        CenterSenseDsp::processMagicEQ(quiet.data(), quiet.size() / 2, 2, 48000, params, &state);
        check(state.overloadGain == 1.0, "normal-level Magic EQ remains exactly unity through protection stage");
        std::vector<float> overload(4800 * 2);
        for (size_t frame = 0; frame < 4800; ++frame) {
            const float tone = 1.8f * std::sin(2.0f * 3.14159265358979323846f * 1000.0f *
                                                static_cast<float>(frame) / 48000.0f);
            overload[frame * 2] = tone;
            overload[frame * 2 + 1] = tone * 0.5f;
        }
        CenterSenseDsp::processMagicEQ(overload.data(), 4800, 2, 48000, params, &state);
        float protectedPeak = 0.0f;
        float maximumStereoRatioError = 0.0f;
        for (size_t frame = 2000; frame < 4800; ++frame) {
            const float left = overload[frame * 2], right = overload[frame * 2 + 1];
            protectedPeak = std::max(protectedPeak, std::max(std::abs(left), std::abs(right)));
            if (std::abs(left) > 0.05f)
                maximumStereoRatioError = std::max(maximumStereoRatioError, std::abs(right / left - 0.5f));
        }
        std::printf("MagicEQOverloadStereo sample_peak=%.6f ratio_error=%.7f gain=%.6f latency_samples=0 release_ms=20\n",
                    protectedPeak, maximumStereoRatioError, state.overloadGain);
        check(protectedPeak <= 1.0f && state.overloadGain < 1.0,
              "overload-only protection bounds sample peaks");
        check(maximumStereoRatioError < 1e-3f, "overload protection links stereo channels");
        auto recovery = sine(1000.0f, 48000, 14400, 2);
        for (float& sample : recovery) sample *= 0.08f;
        CenterSenseDsp::processMagicEQ(recovery.data(), recovery.size() / 2, 2, 48000, params, &state);
        check(state.overloadGain > 0.99999, "protection returns near unity after quiet recovery");
        CenterSenseDsp::resetMagicEQ(&state);
        check(state.overloadGain == 1.0, "seek or file reset clears protection gain");
    }
    {
        const auto fullScale = rapidBroadbandPeak(44100, 1024,
            CenterSenseDsp::MagicEQMode::Notch, 2.5f, 1.0f);
        std::printf("MagicEQFullScaleBehavior static_peak=%.6f moving_peak=%.6f pcm_fullscale_exceeded=%d finite=%d\n",
            fullScale.staticPeak, fullScale.movingPeak,
            fullScale.staticPeak > 1.0f || fullScale.movingPeak > 1.0f ? 1 : 0,
            fullScale.finite ? 1 : 0);
        check(fullScale.finite, "full-scale stress remains finite with overload protection");
        check(fullScale.staticPeak <= 1.0f && fullScale.movingPeak <= 1.0f,
              "Magic EQ full-scale stress has no overfullscale sample");
    }
    {
        using Steepness = CenterSenseDsp::MagicEQSteepness;
        const Steepness choices[] = {Steepness::Soft, Steepness::Steep, Steepness::VerySteep};
        const float probeHz[] = {200.0f, 700.0f, 850.0f, 1000.0f, 1180.0f, 1450.0f, 5000.0f};
        for (const int testRate : {44100, 48000, 96000}) {
            float responses[3][7]{};
            for (int choice = 0; choice < 3; ++choice) {
                for (int point = 0; point < 7; ++point) {
                    auto signal = sine(probeHz[point], testRate, static_cast<size_t>(testRate), 1);
                    CenterSenseDsp::MagicEQProcessParams params{};
                    params.enabled = true;
                    params.mode = CenterSenseDsp::MagicEQMode::Notch;
                    params.steepness = choices[choice];
                    params.centerFreq = 1000.0f;
                    params.halfWidthPercent = 5.0f;
                    CenterSenseDsp::MagicEQState state{};
                    CenterSenseDsp::processMagicEQ(signal.data(), signal.size(), 1, testRate, params, &state);
                    responses[choice][point] = rms(signal, signal.size() / 2, signal.size(), 1);
                    check(std::isfinite(responses[choice][point]), "Notch edge response remains finite");
                }
                const float lowSlopeDb = 20.0f * std::log10(responses[choice][1] / responses[choice][2]);
                const float highSlopeDb = 20.0f * std::log10(responses[choice][5] / responses[choice][4]);
                std::printf("NotchSteepness rate=%d choice=%d farLow=%.6f outerLow=%.6f innerLow=%.6f center=%.6f innerHigh=%.6f outerHigh=%.6f farHigh=%.6f lowSlopeDb=%.3f highSlopeDb=%.3f\n",
                            testRate, choice, responses[choice][0], responses[choice][1], responses[choice][2],
                            responses[choice][3], responses[choice][4], responses[choice][5],
                            responses[choice][6], lowSlopeDb, highSlopeDb);
                check(responses[choice][0] > 0.5f && responses[choice][6] > 0.5f,
                      "Notch preserves frequencies far outside the selected band");
            }
            check(responses[0][2] > responses[1][2] && responses[1][2] > responses[2][2],
                  "Notch steepness changes low-edge attenuation");
            check(responses[0][4] > responses[1][4] && responses[1][4] > responses[2][4],
                  "Notch steepness changes high-edge attenuation");
            check(responses[0][3] < 0.2f && responses[1][3] < 0.2f && responses[2][3] < 0.2f,
                  "All Notch steepness choices suppress the selected center");
            const auto slope = [&](int choice, int outer, int inner) {
                return 20.0f * std::log10(responses[choice][outer] / responses[choice][inner]);
            };
            check(slope(0, 1, 2) < slope(1, 1, 2) && slope(1, 1, 2) < slope(2, 1, 2),
                  "Notch low edge has three increasing measured dB transitions");
            check(slope(0, 5, 4) < slope(1, 5, 4) && slope(1, 5, 4) < slope(2, 5, 4),
                  "Notch high edge has three increasing measured dB transitions");
        }
    }
    {
        using Mode = CenterSenseDsp::MagicEQMode;
        struct TransitionCase {
            const char* label;
            float toneHz;
            CenterSenseDsp::MagicEQProcessParams from;
            CenterSenseDsp::MagicEQProcessParams to;
        };
        auto makeParams = [](Mode mode, float center, float width) {
            CenterSenseDsp::MagicEQProcessParams params{};
            params.enabled = true;
            params.mode = mode;
            params.centerFreq = center;
            params.halfWidthPercent = width;
            return params;
        };
        const TransitionCase cases[] = {
            {"narrow pass large center jump", 600.0f,
             makeParams(Mode::Pass, 600.0f, 1.0f), makeParams(Mode::Pass, 9000.0f, 1.0f)},
            {"narrow notch large center jump", 600.0f,
             makeParams(Mode::Notch, 600.0f, 1.0f), makeParams(Mode::Notch, 9000.0f, 1.0f)},
            {"narrow to wide pass width jump", 600.0f,
             makeParams(Mode::Pass, 5000.0f, 1.0f), makeParams(Mode::Pass, 5000.0f, 45.0f)},
            {"pass to notch mode switch", 1000.0f,
             makeParams(Mode::Pass, 1000.0f, 5.0f), makeParams(Mode::Notch, 1000.0f, 5.0f)},
            {"notch to pass mode switch", 1000.0f,
             makeParams(Mode::Notch, 1000.0f, 5.0f), makeParams(Mode::Pass, 1000.0f, 5.0f)},
            {"pass to bypass", 1000.0f,
             makeParams(Mode::Pass, 1000.0f, 5.0f), makeParams(Mode::Pass, 1000.0f, 5.0f)},
            {"bypass to pass", 1000.0f,
             makeParams(Mode::Pass, 1000.0f, 5.0f), makeParams(Mode::Pass, 1000.0f, 5.0f)},
        };
        const int sampleRates[] = {44100, 48000, 96000};
        const size_t blockSizes[] = {32, 64, 256, 1024};
        for (int rate : sampleRates) {
            for (size_t block : blockSizes) {
                for (const auto& scenario : cases) {
                    auto from = scenario.from;
                    auto to = scenario.to;
                    if (std::string(scenario.label) == "pass to bypass") to.enabled = false;
                    if (std::string(scenario.label) == "bypass to pass") from.enabled = false;
                    const auto metrics = measureParameterTransition(
                        from, to, rate, block, scenario.toneHz);
                    const float stableBound = std::max(
                        steadyAdjacentStep(from, rate, block, scenario.toneHz),
                        steadyAdjacentStep(to, rate, block, scenario.toneHz));
                    const float allowedStep = stableBound + 0.008f;
                    std::printf("EQTransition rate=%d block=%zu case=%s step=%.6f stable=%.6f limit=%.6f peak=%.6f finite=%d\n",
                                rate, block, scenario.label, metrics.maxAdjacentStep, stableBound,
                                allowedStep, metrics.maxOutputPeak, metrics.finite ? 1 : 0);
                    check(metrics.finite, "large parameter changes remain finite");
                    check(metrics.maxOutputPeak <= 0.35f,
                          "large parameter changes remain below the 0.35 full-scale fixture bound");
                    check(metrics.maxAdjacentStep <= allowedStep,
                          "large parameter changes add at most 0.008 full-scale transient above steady response");
                }
                const auto motion = measureContinuousParameterMotion(rate, block);
                std::printf("EQContinuousMotion rate=%d block=%zu step=%.6f frame=%zu peak=%.6f finite=%d\n",
                            rate, block, motion.maxAdjacentStep, motion.maxStepFrame, motion.maxOutputPeak,
                            motion.finite ? 1 : 0);
                check(motion.finite, "continuous center and width motion remains finite");
                check(motion.maxOutputPeak <= 0.35f,
                      "continuous center and width motion stays below the fixture peak bound");
                check(motion.maxAdjacentStep <= 0.045f,
                      "continuous center and width motion has no step above 0.045 full scale");

                const auto narrowPass = makeParams(Mode::Pass, 600.0f, 1.0f);
                const auto narrowNotch = makeParams(Mode::Notch, 600.0f, 1.0f);
                for (const auto& modeCase : {
                         std::pair<const char*, std::pair<CenterSenseDsp::MagicEQProcessParams, CenterSenseDsp::MagicEQProcessParams>>{"Pass to Notch", {narrowPass, narrowNotch}},
                         {"Notch to Pass", {narrowNotch, narrowPass}}}) {
                    const auto residual = measureModeSwitchTail(
                        modeCase.second.first, modeCase.second.second, rate, block, 610.0f);
                    std::printf("EQModeTail rate=%d block=%zu case=%s peak=%.6f rms=%.6f finite=%d\n",
                                rate, block, modeCase.first, residual.peak, residual.rms(),
                                residual.finite ? 1 : 0);
                    check(residual.finite, "Pass/Notch mode switch tail stays finite");
                    check(residual.rms() <= 0.0025f,
                          "Pass/Notch mode switch residual falls below 0.0025 RMS full scale after 50 ms");
                    check(residual.peak <= 0.01f,
                          "Pass/Notch mode switch residual peak falls below 0.01 full scale after 50 ms");
                }
            }
        }
    }
    std::printf("Magic EQ DSP: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
