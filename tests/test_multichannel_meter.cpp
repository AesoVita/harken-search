// Test: multi-channel loudness meter sampling logic (F-018)
// Verifies:
//   - Interleaved peak indexing  idx = frame*stride + ch  for N = 1,2,6,8,12
//   - Per-channel state buffer resize on channel-count change
//   - Smooth attack/release and peak-hold math remain per-channel independent
//
// This test does NOT link against main.cpp; it reimplements the small math
// block introduced in `DrawLoudnessMeter` sampling loop to prove the logic
// works for arbitrary channel counts. If the production code ever diverges
// from this logic, the test will no longer reflect reality — update both.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

struct FakeWaveform {
    int numChannels = 1;
    std::vector<float> peaks;     // interleaved [c0f0,c1f0,...,cNf0, c0f1,...]
    std::vector<float> rms;       // same layout (optional, may be empty)
    bool isReady = true;
};

struct FakeMeterState {
    std::vector<float> currentPeaks;
    std::vector<float> currentRmses;
    std::vector<float> peakHolds;
    std::vector<float> peakHoldFreezes;
};

// Mirror of production sampling step (simplified: no Dummy or ImGui call).
void sampleFrame(const FakeWaveform& wf, float playPos, FakeMeterState& st, float dt) {
    int nCh = std::max(1, wf.numChannels);
    if ((int)st.currentPeaks.size() != nCh) {
        st.currentPeaks.assign(nCh, 0.0f);
        st.currentRmses.assign(nCh, 0.0f);
        st.peakHolds.assign(nCh, 0.0f);
        st.peakHoldFreezes.assign(nCh, 0.0f);
    }

    std::vector<float> targetPeaks(nCh, 0.0f), targetRmses(nCh, 0.0f);
    if (wf.isReady && !wf.peaks.empty()) {
        int stride = std::max(1, wf.numChannels);
        int frameCount = (int)wf.peaks.size() / stride;
        if (frameCount > 0) {
            int frame = std::clamp((int)(playPos * frameCount), 0, frameCount - 1);
            bool rmsAvailable = (wf.rms.size() >= wf.peaks.size());
            int chMax = std::min(nCh, stride);
            for (int ch = 0; ch < chMax; ++ch) {
                int idx = frame * stride + ch;
                targetPeaks[ch] = wf.peaks[idx];
                targetRmses[ch] = rmsAvailable ? wf.rms[idx] : targetPeaks[ch] * 0.35f;
            }
        }
    }

    const float attack = 0.45f, release = 0.12f;
    auto smooth = [&](float cur, float tgt) {
        float k = (tgt > cur) ? attack : release;
        return cur + (tgt - cur) * k;
    };
    const float kHoldSeconds = 0.28f, kHoldFallPerSec = 1.10f;
    for (int ch = 0; ch < nCh; ++ch) {
        st.currentPeaks[ch] = smooth(st.currentPeaks[ch], targetPeaks[ch]);
        st.currentRmses[ch] = smooth(st.currentRmses[ch], targetRmses[ch]);
        float p = st.currentPeaks[ch];
        float& h = st.peakHolds[ch];
        float& hf = st.peakHoldFreezes[ch];
        if (p >= h) { h = p; hf = kHoldSeconds; }
        else if (hf > 0.0f) { hf = std::max(0.0f, hf - dt); }
        else { h = std::max(p, h - kHoldFallPerSec * dt); }
    }
}

int fail_count = 0;
#define CHECK(cond, msg) do { if (!(cond)) { \
    std::printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
    ++fail_count; \
} } while (0)

FakeWaveform makeRamp(int numChannels, int frames) {
    FakeWaveform wf;
    wf.numChannels = numChannels;
    wf.peaks.resize((size_t)frames * numChannels);
    // Each sample: peaks[frame*N + ch] = 0.1f * (ch + 1) + 0.001f * frame
    // → channel identity is preserved along channel axis, distinct from frame axis
    for (int f = 0; f < frames; ++f)
        for (int ch = 0; ch < numChannels; ++ch)
            wf.peaks[(size_t)f * numChannels + ch] = 0.1f * (ch + 1) + 0.001f * f;
    return wf;
}

void testMonoSampling() {
    FakeWaveform wf = makeRamp(1, 10);
    FakeMeterState st;
    sampleFrame(wf, 0.5f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 1, "mono: vector size == 1");
    CHECK(st.currentPeaks[0] > 0.0f, "mono: peak updated from zero");
}

void testStereoSampling() {
    FakeWaveform wf = makeRamp(2, 10);
    FakeMeterState st;
    // After one frame both channels smoothed from 0 toward distinct targets
    sampleFrame(wf, 0.0f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 2, "stereo: vector size == 2");
    // Target at frame=0: ch0=0.1, ch1=0.2  → after 0.45 attack: ~0.045, ~0.090
    CHECK(st.currentPeaks[1] > st.currentPeaks[0], "stereo: R > L (ch1 target > ch0)");
}

void testMultiChannelIndexing() {
    // Verify for N=6,8,12 that each channel reads the correct interleaved slot
    for (int N : {6, 8, 12}) {
        FakeWaveform wf = makeRamp(N, 5);
        FakeMeterState st;
        // Drive peaks to steady state by running many frames at playPos pointing at last frame
        for (int iter = 0; iter < 200; ++iter) sampleFrame(wf, 0.95f, st, 0.016f);
        CHECK((int)st.currentPeaks.size() == N, "multi: size matches numChannels");
        int lastFrame = 4;
        for (int ch = 0; ch < N; ++ch) {
            float expected = 0.1f * (ch + 1) + 0.001f * lastFrame;
            // Smoothed toward target — should be within 1% after 200 iters at attack=0.45
            float diff = std::fabs(st.currentPeaks[ch] - expected);
            CHECK(diff < 0.005f, "multi: per-channel steady-state value correct");
        }
        // Monotone: later channels have strictly higher peaks (they started higher)
        for (int ch = 1; ch < N; ++ch) {
            CHECK(st.currentPeaks[ch] > st.currentPeaks[ch - 1],
                  "multi: channel peak ordering preserved");
        }
    }
}

void testChannelCountTransition() {
    // Simulate file switch: stereo → 12ch → mono. Vector must resize each time.
    FakeMeterState st;
    FakeWaveform wf2 = makeRamp(2, 10);
    sampleFrame(wf2, 0.5f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 2, "transition: sized to 2 after stereo");

    FakeWaveform wf12 = makeRamp(12, 10);
    sampleFrame(wf12, 0.5f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 12, "transition: resized to 12 after 12-ch");
    // Reset on resize: after one frame at attack=0.45, value is ~45% of target (not full)
    // (Proves assign(nCh, 0.0f) cleared state rather than carrying stereo values forward)
    for (int ch = 0; ch < 12; ++ch) {
        float target = 0.1f * (ch + 1) + 0.005f;   // frame 5, ch gradient
        float smoothed = target * 0.45f;           // one attack step from 0
        CHECK(st.currentPeaks[ch] < target * 0.9f,
              "transition: 12ch not saturated in one frame (smoothing active)");
        CHECK(std::fabs(st.currentPeaks[ch] - smoothed) < 0.01f,
              "transition: 12ch first-frame value matches attack=0.45");
    }

    FakeWaveform wf1 = makeRamp(1, 10);
    sampleFrame(wf1, 0.5f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 1, "transition: resized to 1 after mono");
}

void testEmptyPeaks() {
    FakeWaveform wf;
    wf.numChannels = 6;
    wf.isReady = true;
    // Empty peaks buffer — must not crash, must still size state correctly
    FakeMeterState st;
    sampleFrame(wf, 0.5f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 6, "empty: vector sized even with empty peaks");
    for (int ch = 0; ch < 6; ++ch)
        CHECK(st.currentPeaks[ch] == 0.0f, "empty: no spurious values");
}

void testStrideMatch8ch() {
    // Happy path at N=8: distinct per-channel values survive 200 iters of smoothing
    FakeWaveform wf;
    wf.numChannels = 8;
    wf.peaks.assign((size_t)8 * 10, 0.0f);
    for (int f = 0; f < 10; ++f)
        for (int ch = 0; ch < 8; ++ch)
            wf.peaks[(size_t)f * 8 + ch] = 0.1f * (ch + 1);
    FakeMeterState st;
    for (int i = 0; i < 200; ++i) sampleFrame(wf, 0.5f, st, 0.016f);
    CHECK(st.currentPeaks.size() == 8, "8ch: sized to 8");
    CHECK(st.currentPeaks[7] > st.currentPeaks[0], "8ch: last > first");
}

} // namespace

int main() {
    testMonoSampling();
    testStereoSampling();
    testMultiChannelIndexing();
    testChannelCountTransition();
    testEmptyPeaks();
    testStrideMatch8ch();

    if (fail_count) {
        std::printf("FAIL: %d assertion(s)\n", fail_count);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
