#include "src/WaveformSliceAnalyzer.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

static int g_failures = 0;

static void check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

static WaveformData makeMonoWaveform(const std::vector<float>& frames, int samplesPerPeak = 480) {
    WaveformData wf;
    wf.isReady = true;
    wf.numChannels = 1;
    wf.isStereo = false;
    wf.samplesPerPeak = samplesPerPeak;
    wf.peaks = frames;
    wf.minPeaks.resize(frames.size());
    wf.rms = frames;
    for (size_t i = 0; i < frames.size(); ++i) {
        wf.minPeaks[i] = -frames[i];
    }
    return wf;
}

static WaveformData makeStereoWaveform(const std::vector<float>& monoFrames, int samplesPerPeak = 480) {
    WaveformData wf;
    wf.isReady = true;
    wf.numChannels = 2;
    wf.isStereo = true;
    wf.samplesPerPeak = samplesPerPeak;
    wf.peaks.reserve(monoFrames.size() * 2);
    wf.minPeaks.reserve(monoFrames.size() * 2);
    wf.rms.reserve(monoFrames.size() * 2);
    for (float v : monoFrames) {
        wf.peaks.push_back(v);
        wf.peaks.push_back(v * 0.5f);
        wf.minPeaks.push_back(-v);
        wf.minPeaks.push_back(-v * 0.5f);
        wf.rms.push_back(v);
        wf.rms.push_back(v * 0.5f);
    }
    return wf;
}

static std::vector<float> concat(std::initializer_list<std::vector<float>> parts) {
    std::vector<float> out;
    for (const auto& part : parts) {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

static std::vector<float> repeat(float value, int count) {
    return std::vector<float>(static_cast<size_t>(count), value);
}

static bool near(float a, float b, float eps = 0.015f) {
    return std::fabs(a - b) <= eps;
}

int main() {
    WaveformSliceParams params;
    params.silenceThresholdDb = -45.0f;
    params.minSilenceDurationSec = 0.18f;
    params.minSliceDurationSec = 0.12f;
    params.maxSlices = 99;

    {
        auto frames = concat({repeat(0.8f, 20), repeat(0.0f, 20), repeat(0.7f, 20)});
        auto wf = makeMonoWaveform(frames);
        auto result = AnalyzeWaveformSlices(wf, 0.60f, params);
        check(result.ok, "two loud regions separated by silence returns ok");
        check(result.splitPointsNorm.size() == 1, "one sustained silence gap creates one split point");
        check(result.slices.size() == 2, "one split point creates two slices");
        check(!result.splitPointsNorm.empty() && near(result.splitPointsNorm[0], 0.50f, 0.001f), "split point uses duration-consistent normalized time");
        check(result.slices.size() == 2 && near(result.slices[0].endSec, 0.30f, 0.001f), "slice seconds derive from normalized split boundary");
    }

    {
        auto wf = makeMonoWaveform(repeat(0.6f, 80));
        auto result = AnalyzeWaveformSlices(wf, 0.80f, params);
        check(result.ok, "continuous non-silent audio returns ok");
        check(result.splitPointsNorm.empty(), "continuous non-silent audio has no split points");
        check(result.slices.size() == 1, "continuous non-silent audio returns one full slice");
    }

    {
        auto cappedParams = params;
        cappedParams.minSilenceDurationSec = 0.05f;
        cappedParams.minSliceDurationSec = 0.02f;
        cappedParams.maxSlices = 3;
        auto frames = concat({
            repeat(0.8f, 10), repeat(0.0f, 10),
            repeat(0.7f, 10), repeat(0.0f, 10),
            repeat(0.8f, 10), repeat(0.0f, 10),
            repeat(0.7f, 10), repeat(0.0f, 10),
            repeat(0.8f, 10),
        });
        auto wf = makeMonoWaveform(frames);
        auto result = AnalyzeWaveformSlices(wf, 0.90f, cappedParams);
        check(result.ok, "multi-gap waveform analyzes with low maxSlices");
        check(result.splitPointsNorm.size() == 2, "maxSlices caps split count to maxSlices minus one");
        check(result.slices.size() == 3, "maxSlices cap returns no more than requested slices");
        check(result.splitPointsNorm.size() == 2 && result.splitPointsNorm[1] > result.splitPointsNorm[0], "capped split points stay sorted");
    }

    {
        auto frames = concat({repeat(0.8f, 20), repeat(0.0f, 5), repeat(0.7f, 20)});
        auto wf = makeMonoWaveform(frames);
        auto result = AnalyzeWaveformSlices(wf, 0.45f, params);
        check(result.ok, "short silence gap still returns ok");
        check(result.splitPointsNorm.empty(), "short silence below minimum duration is ignored");
        check(result.slices.size() == 1, "short silence below minimum duration keeps one slice");
    }

    {
        auto frames = concat({repeat(0.8f, 4), repeat(0.0f, 20), repeat(0.7f, 4)});
        auto wf = makeMonoWaveform(frames);
        auto result = AnalyzeWaveformSlices(wf, 0.28f, params);
        check(result.ok, "too-short candidate slices still analyze");
        check(result.splitPointsNorm.empty(), "audible-duration gate rejects short loud fragments around long silence");
        check(result.slices.size() == 1, "short audible fragments keep full file as one slice");
    }

    {
        auto frames = concat({repeat(0.8f, 20), repeat(0.0f, 20), repeat(0.7f, 20)});
        auto wf = makeStereoWaveform(frames);
        auto result = AnalyzeWaveformSlices(wf, 0.60f, params);
        check(result.ok, "stereo waveform analyzes");
        check(result.splitPointsNorm.size() == 1, "stereo waveform uses combined channel energy");
    }

    {
        auto result = AnalyzeWaveformSlices(WaveformData{}, 1.0f, params);
        check(!result.ok, "not-ready waveform is rejected");
        check(result.error == "waveform_not_ready", "not-ready waveform reports waveform_not_ready");
    }

    {
        auto wf = makeMonoWaveform(repeat(0.6f, 8));
        auto result = AnalyzeWaveformSlices(wf, 0.0f, params);
        check(!result.ok, "zero duration waveform is rejected");
        check(result.error == "invalid_duration", "zero duration reports invalid_duration");
    }

    {
        auto wf = makeMonoWaveform(repeat(0.6f, 1));
        auto result = AnalyzeWaveformSlices(wf, 1.0f, params);
        check(!result.ok, "single-frame waveform is rejected");
        check(result.error == "insufficient_peaks", "single-frame waveform reports insufficient_peaks");
    }

    return g_failures == 0 ? 0 : 1;
}
