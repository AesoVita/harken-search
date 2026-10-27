// Unit tests for BpmDetector and TapTempoAccumulator.
// Returns 0 on all-pass, 1 on any failure.
#include "../src/BpmDetector.h"
#include "../src/TapTempoAccumulator.h"
#include <cmath>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>

// Generate a synthetic click track: impulse bursts at exactly `bpm` BPM.
static std::vector<float> generateClickTrack(float bpm, float durationSec,
                                              int sampleRate = 44100) {
    int frames = static_cast<int>(durationSec * sampleRate);
    std::vector<float> pcm(static_cast<size_t>(frames), 0.0f);
    int beatFrames = static_cast<int>(60.0f / bpm * sampleRate);
    for (int i = 0; i < frames; i += beatFrames) {
        pcm[static_cast<size_t>(i)] = 1.0f;
        for (int j = 1; j < std::min(80, frames - i); ++j)
            pcm[static_cast<size_t>(i + j)] = 0.6f * std::exp(-j * 0.08f);
    }
    return pcm;
}

static bool approx(float a, float b, float tol) { return std::abs(a - b) <= tol; }

int main() {
    int passed = 0, failed = 0;
    auto check = [&](bool ok, const char* label) {
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
        ok ? ++passed : ++failed;
    };

    // --- BpmDetector tests ---

    // Test 1: 120 BPM
    {
        auto pcm = generateClickTrack(120.0f, 30.0f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 44100);
        bool ok = r.ok && approx(r.bpm, 120.0f, 5.0f);
        printf("  -> detected %.1f BPM (conf %.2f)\n", r.bpm, r.confidence);
        check(ok, "120 BPM click track");
    }

    // Test 2: 90 BPM
    {
        auto pcm = generateClickTrack(90.0f, 30.0f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 44100);
        bool ok = r.ok && approx(r.bpm, 90.0f, 5.0f);
        printf("  -> detected %.1f BPM\n", r.bpm);
        check(ok, "90 BPM click track");
    }

    // Test 3: 140 BPM
    {
        auto pcm = generateClickTrack(140.0f, 30.0f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 44100);
        bool ok = r.ok && approx(r.bpm, 140.0f, 5.0f);
        printf("  -> detected %.1f BPM\n", r.bpm);
        check(ok, "140 BPM click track");
    }

    // Test 4: null input rejected
    {
        BpmResult r = BpmDetector::detect(nullptr, 0, 1, 44100);
        check(!r.ok, "null input rejected");
    }

    // Test 5: sampleRate=0 rejected
    {
        std::vector<float> pcm(44100, 0.5f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 0);
        check(!r.ok, "sampleRate=0 rejected");
    }

    // Test 6: sub-1s input rejected
    {
        std::vector<float> pcm(1000, 0.5f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 44100);
        check(!r.ok, "sub-1s input rejected");
    }

    // Test 7: confidence in [0,1]
    {
        auto pcm = generateClickTrack(120.0f, 30.0f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 44100);
        check(r.ok && r.confidence >= 0.0f && r.confidence <= 1.0f,
              "confidence in [0,1]");
    }

    // Test 8: beatOffset in [0, 2s)
    {
        auto pcm = generateClickTrack(120.0f, 30.0f);
        BpmResult r = BpmDetector::detect(pcm.data(), (int)pcm.size(), 1, 44100);
        check(r.ok && r.beatOffset >= 0.0f && r.beatOffset < 2.0f,
              "beatOffset in [0, 2s)");
    }

    // --- TapTempoAccumulator tests ---

    // Test 9: 4 taps at ~120 BPM (500ms apart)
    {
        TapTempoAccumulator acc;
        for (int i = 0; i < 4; ++i) {
            acc.tap();
            if (i < 3) std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        bool ok = acc.hasResult() && approx(acc.getBpm(), 120.0f, 12.0f);
        printf("  -> tap BPM = %.1f (%d taps)\n", acc.getBpm(), acc.tapCount());
        check(ok, "tap tempo ~120 BPM (4 taps, 500ms)");
    }

    // Test 10: fewer than 3 taps -> no result
    {
        TapTempoAccumulator acc;
        acc.tap(); acc.tap();
        check(!acc.hasResult(), "2 taps -> hasResult() false");
    }

    // Test 11: timeout resets accumulator
    {
        TapTempoAccumulator acc;
        acc.tap(); acc.tap(); acc.tap();
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        acc.tap();
        check(acc.tapCount() == 1, "timeout resets accumulator");
    }

    // Test 12: reset() clears state
    {
        TapTempoAccumulator acc;
        acc.tap(); acc.tap(); acc.tap();
        acc.reset();
        check(!acc.hasResult() && acc.tapCount() == 0, "reset() clears taps");
    }

    // Test 13: isExpired() true when empty
    {
        TapTempoAccumulator acc;
        check(acc.isExpired(), "isExpired() true when empty");
    }

    // --- Silence trimming tests ---

    // Test 14: 5s leading silence + 30s 120 BPM music -> BPM still detectable
    {
        const int SR = 44100;
        auto music = generateClickTrack(120.0f, 30.0f, SR);
        std::vector<float> padded(static_cast<size_t>(SR * 5), 0.0f);
        padded.insert(padded.end(), music.begin(), music.end());
        BpmResult r = BpmDetector::detect(padded.data(), (int)padded.size(), 1, SR);
        bool ok = r.ok && approx(r.bpm, 120.0f, 5.0f);
        printf("  -> leading-silence BPM = %.1f (ok=%d)\n", r.bpm, r.ok);
        check(ok, "120 BPM: 5s leading silence trimmed");
    }

    // Test 15: 10s 120 BPM music + 10s trailing silence -> BPM same as clean
    {
        const int SR = 44100;
        auto music = generateClickTrack(120.0f, 10.0f, SR);
        std::vector<float> padded = music;
        padded.insert(padded.end(), static_cast<size_t>(SR * 10), 0.0f);
        BpmResult rClean  = BpmDetector::detect(music.data(),  (int)music.size(),  1, SR);
        BpmResult rPadded = BpmDetector::detect(padded.data(), (int)padded.size(), 1, SR);
        bool ok = rClean.ok && rPadded.ok && approx(rClean.bpm, rPadded.bpm, 2.0f);
        printf("  -> clean=%.1f padded=%.1f\n", rClean.bpm, rPadded.bpm);
        check(ok, "120 BPM: trailing silence yields same BPM as clean");
    }

    // Test 16: silence sandwich - 15s silence + 15s music + 15s silence -> detectable
    {
        const int SR = 44100;
        auto music = generateClickTrack(120.0f, 15.0f, SR);
        std::vector<float> padded(static_cast<size_t>(SR * 15), 0.0f);
        padded.insert(padded.end(), music.begin(), music.end());
        padded.insert(padded.end(), static_cast<size_t>(SR * 15), 0.0f);
        BpmResult r = BpmDetector::detect(padded.data(), (int)padded.size(), 1, SR);
        bool ok = r.ok && approx(r.bpm, 120.0f, 5.0f);
        printf("  -> silence-sandwich BPM = %.1f (ok=%d)\n", r.bpm, r.ok);
        check(ok, "120 BPM: 15s silence on each side trimmed");
    }

    // Test 17: pure silence buffer -> ok=false (no crash, clean rejection)
    {
        const int SR = 44100;
        std::vector<float> silence(static_cast<size_t>(SR * 5), 0.0f);
        BpmResult r = BpmDetector::detect(silence.data(), (int)silence.size(), 1, SR);
        check(!r.ok, "pure silence: ok=false");
    }

    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
