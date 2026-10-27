// test_freq_shift.cpp — Unit tests for CenterSenseDsp::processFreqShift
// Tests: SSB Hilbert frequency shifter correctness, identity at 0 Hz, state reset.
//
// Layer 1: Direct function-level tests, no audio engine required.
// Output: PASS/FAIL per case, exits 0 on all pass, 1 on any failure.

#include "CenterSenseDsp.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int s_failures = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        printf("PASS: %s\n", msg);
    } else {
        printf("FAIL: %s\n", msg);
        ++s_failures;
    }
}

// ─── helpers ────────────────────────────────────────────────────────────────

// Generate a stereo sine burst (L = R = sin(2π·freq·t/sr))
static std::vector<float> makeStereoSine(float freqHz, int sampleRate, int numFrames) {
    std::vector<float> buf(numFrames * 2);
    for (int i = 0; i < numFrames; ++i) {
        float s = std::sin(2.0f * (float)M_PI * freqHz * i / (float)sampleRate);
        buf[2 * i + 0] = s;
        buf[2 * i + 1] = s;
    }
    return buf;
}

static std::vector<float> makeMonoSine(float freqHz, int sampleRate, int numFrames) {
    std::vector<float> buf(numFrames);
    for (int i = 0; i < numFrames; ++i) {
        buf[i] = std::sin(2.0f * (float)M_PI * freqHz * i / (float)sampleRate);
    }
    return buf;
}

// Estimate dominant frequency of a mono signal extracted from interleaved stereo,
// using a simple zero-crossing rate (works well for pure sine).
static float estimateDominantFreqZCR(const std::vector<float>& stereo,
                                     int channel,  // 0=L, 1=R
                                     int sampleRate,
                                     int offset = 0,  // skip first N frames (filter startup)
                                     int len    = 0)
{
    int n = (int)stereo.size() / 2;
    if (len == 0 || offset + len > n) len = n - offset;
    int crossings = 0;
    float prev = stereo[(offset) * 2 + channel];
    for (int i = offset + 1; i < offset + len; ++i) {
        float cur = stereo[i * 2 + channel];
        if ((prev < 0.0f) != (cur < 0.0f)) ++crossings;
        prev = cur;
    }
    // Each full cycle has 2 zero crossings
    return (float)crossings * (float)sampleRate / (2.0f * (float)len);
}

static float estimateDominantFreqZCRMono(const std::vector<float>& mono,
                                         int sampleRate,
                                         int offset = 0,
                                         int len = 0)
{
    int n = (int)mono.size();
    if (len == 0 || offset + len > n) len = n - offset;
    int crossings = 0;
    float prev = mono[offset];
    for (int i = offset + 1; i < offset + len; ++i) {
        float cur = mono[i];
        if ((prev < 0.0f) != (cur < 0.0f)) ++crossings;
        prev = cur;
    }
    return (float)crossings * (float)sampleRate / (2.0f * (float)len);
}

// RMSE between two stereo buffers (channel 0 = left)
static float rmse(const std::vector<float>& a, const std::vector<float>& b, int channel) {
    int n = (int)std::min(a.size(), b.size()) / 2;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        double diff = a[i * 2 + channel] - b[i * 2 + channel];
        sum += diff * diff;
    }
    return (float)std::sqrt(sum / n);
}

// ─── Test cases ─────────────────────────────────────────────────────────────

// T1: Zero shift — output should match input (within group delay)
static void test_zero_shift() {
    const int SR = 48000;
    const int N  = 4096;
    auto buf = makeStereoSine(440.0f, SR, N);
    auto ref = buf;  // copy before processing

    CenterSenseDsp::FreqShiftState state{};
    CenterSenseDsp::processFreqShift(buf.data(), N, SR, 0.0f, &state);

    // With shiftHz==0, processFreqShift returns early — output must equal input exactly
    float err = rmse(buf, ref, 0);
    check(err < 1e-9f, "zero shift: output == input (no-op)");
}

// T2: Positive shift — dominant frequency should increase by shiftHz
static void test_positive_shift() {
    const int SR      = 48000;
    const int N       = SR;        // 1 second
    const float IN_F  = 440.0f;
    const float SHIFT = 100.0f;
    const float EXP_F = IN_F + SHIFT;

    auto buf = makeStereoSine(IN_F, SR, N);
    CenterSenseDsp::FreqShiftState state{};
    CenterSenseDsp::processFreqShift(buf.data(), N, SR, SHIFT, &state);

    // Skip first 100 frames (filter group delay + startup transient)
    float freq = estimateDominantFreqZCR(buf, 0, SR, /*offset=*/100, /*len=*/N - 100);
    float err  = std::abs(freq - EXP_F);
    char msg[128];
    snprintf(msg, sizeof(msg), "shift +100Hz: output freq %.1f Hz (expected %.1f Hz, err=%.1f)", freq, EXP_F, err);
    check(err < 15.0f, msg);  // ZCR accuracy ~10 Hz for 1s window
}

// T2b: Mono positive shift - covers single-channel playback assets.
static void test_mono_positive_shift() {
    const int SR      = 48000;
    const int N       = SR;
    const float IN_F  = 440.0f;
    const float SHIFT = 100.0f;
    const float EXP_F = IN_F + SHIFT;

    auto buf = makeMonoSine(IN_F, SR, N);
    CenterSenseDsp::FreqShiftState state{};
    CenterSenseDsp::processFreqShiftMono(buf.data(), N, SR, SHIFT, &state);

    float freq = estimateDominantFreqZCRMono(buf, SR, /*offset=*/100, /*len=*/N - 100);
    float err  = std::abs(freq - EXP_F);
    char msg[128];
    snprintf(msg, sizeof(msg), "mono shift +100Hz: output freq %.1f Hz (expected %.1f Hz, err=%.1f)", freq, EXP_F, err);
    check(err < 15.0f, msg);
}

// T3: Negative shift — dominant frequency should decrease by shiftHz
static void test_negative_shift() {
    const int SR      = 48000;
    const int N       = SR;
    const float IN_F  = 800.0f;   // high enough that -200Hz shift stays audible
    const float SHIFT = -200.0f;
    const float EXP_F = IN_F + SHIFT;  // 600 Hz

    auto buf = makeStereoSine(IN_F, SR, N);
    CenterSenseDsp::FreqShiftState state{};
    CenterSenseDsp::processFreqShift(buf.data(), N, SR, SHIFT, &state);

    float freq = estimateDominantFreqZCR(buf, 0, SR, 100, N - 100);
    float err  = std::abs(freq - EXP_F);
    char msg[128];
    snprintf(msg, sizeof(msg), "shift -200Hz: output freq %.1f Hz (expected %.1f Hz, err=%.1f)", freq, EXP_F, err);
    check(err < 20.0f, msg);
}

// T4: L/R independence — stereo channels processed identically for L=R input
static void test_stereo_symmetry() {
    const int SR     = 48000;
    const int N      = 4096;
    const float SHIFT = 50.0f;

    auto buf = makeStereoSine(300.0f, SR, N);
    CenterSenseDsp::FreqShiftState state{};
    CenterSenseDsp::processFreqShift(buf.data(), N, SR, SHIFT, &state);

    float diff = rmse(buf, buf, 0);  // compare L to itself (always 0)
    // Real check: L and R are the same for symmetric input
    float lrDiff = 0.0f;
    for (int i = 0; i < N; ++i)
        lrDiff += std::abs(buf[2*i+0] - buf[2*i+1]);
    lrDiff /= N;
    char msg[128];
    snprintf(msg, sizeof(msg), "stereo symmetry: L/R diff = %.6f (should be ~0)", lrDiff);
    check(lrDiff < 1e-5f, msg);
    (void)diff;
}

// T5: State reset — re-running with a fresh state on same input should produce same output
static void test_state_reset() {
    const int SR     = 48000;
    const int N      = 2048;
    const float SHIFT = 80.0f;

    auto src = makeStereoSine(440.0f, SR, N);

    auto buf1 = src;
    CenterSenseDsp::FreqShiftState s1{};
    CenterSenseDsp::processFreqShift(buf1.data(), N, SR, SHIFT, &s1);

    auto buf2 = src;
    CenterSenseDsp::FreqShiftState s2{};  // fresh reset state
    CenterSenseDsp::processFreqShift(buf2.data(), N, SR, SHIFT, &s2);

    float err = rmse(buf1, buf2, 0);
    char msg[128];
    snprintf(msg, sizeof(msg), "state reset: two fresh runs produce identical output (RMSE=%.9f)", err);
    check(err < 1e-9f, msg);
}

// T6: Output energy is plausible (not silenced, not clipped)
static void test_output_energy() {
    const int SR     = 48000;
    const int N      = 4096;
    const float SHIFT = 150.0f;

    auto buf = makeStereoSine(440.0f, SR, N);
    CenterSenseDsp::FreqShiftState state{};
    CenterSenseDsp::processFreqShift(buf.data(), N, SR, SHIFT, &state);

    float maxAbs = 0.0f, rmsSum = 0.0f;
    int skip = CenterSenseDsp::FreqShiftState::kTaps; // skip group-delay frames
    for (int i = skip; i < N; ++i) {
        float v = buf[2*i+0];
        maxAbs  = std::max(maxAbs, std::abs(v));
        rmsSum += v * v;
    }
    float rmsOut = std::sqrt(rmsSum / (N - skip));
    char msg[128];
    snprintf(msg, sizeof(msg), "output energy: RMS=%.4f maxAbs=%.4f (expect 0.05<RMS<1.5 and maxAbs<2)", rmsOut, maxAbs);
    check(rmsOut > 0.05f && rmsOut < 1.5f && maxAbs < 2.0f, msg);
}

int main() {
    printf("=== TestFreqShift — CenterSenseDsp::processFreqShift ===\n");

    test_zero_shift();
    test_positive_shift();
    test_mono_positive_shift();
    test_negative_shift();
    test_stereo_symmetry();
    test_state_reset();
    test_output_energy();

    printf("\n%s — %d failure(s)\n",
           s_failures == 0 ? "ALL PASS" : "SOME FAILED",
           s_failures);
    return s_failures > 0 ? 1 : 0;
}
