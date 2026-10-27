// test_key_detector.cpp — Unit tests for KeyDetector (musical key detection)
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include <string>
#include "../src/KeyDetector.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static constexpr float kPi = 3.14159265358979f;

// Generate a mono sine wave at given frequency
static std::vector<float> makeSine(float freqHz, int sampleRate, float durationSec,
                                    float amplitude = 0.8f) {
    int n = static_cast<int>(sampleRate * durationSec);
    std::vector<float> buf(n);
    for (int i = 0; i < n; ++i)
        buf[i] = amplitude * std::sin(2.0f * kPi * freqHz * i / sampleRate);
    return buf;
}

// Generate a chord: sum of multiple sine waves (equal amplitude)
static std::vector<float> makeChord(const float* freqs, int numFreqs,
                                     int sampleRate, float durationSec) {
    int n = static_cast<int>(sampleRate * durationSec);
    std::vector<float> buf(n, 0.0f);
    float amp = 0.7f / numFreqs;
    for (int f = 0; f < numFreqs; ++f)
        for (int i = 0; i < n; ++i)
            buf[i] += amp * std::sin(2.0f * kPi * freqs[f] * i / sampleRate);
    return buf;
}

int main() {
    std::printf("=== KeyDetector Tests ===\n");

    const int sr = 44100;
    const float dur = 3.0f;  // 3 seconds — enough for reliable detection

    // ── Null / edge safety ──
    std::printf("\n--- Null/edge safety ---\n");
    {
        KeyResult r = KeyDetector::detect(nullptr, 1000, 1, sr);
        CHECK(!r.ok, "null pcm → not ok");

        float buf[100] = {};
        r = KeyDetector::detect(buf, 100, 1, sr);
        CHECK(!r.ok, "too few frames (100 < sampleRate) → not ok");

        // Silence: all zeros → no tonal content
        std::vector<float> silence(sr * 3, 0.0f);
        r = KeyDetector::detect(silence.data(), sr * 3, 1, sr);
        CHECK(!r.ok, "silence → not ok (no tonal content)");
    }

    // ── Pure A4 (440 Hz) → should detect A major or A minor ──
    std::printf("\n--- Pure A4 sine (440 Hz) ---\n");
    {
        auto pcm = makeSine(440.0f, sr, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, sr);
        CHECK(r.ok, "A4 sine: detection succeeded");
        if (r.ok) {
            // A single sine gives ambiguous major/minor, but the root should be A
            bool isA = (r.key == "A" || r.key == "Am");
            CHECK(isA, ("A4 sine: key root is A (got " + r.key + ")").c_str());
            CHECK(r.confidence > 0.0f, "A4 sine: confidence > 0");
            CHECK(!r.camelot.empty(), "A4 sine: camelot code non-empty");
            std::printf("    key=%s camelot=%s conf=%.3f\n",
                        r.key.c_str(), r.camelot.c_str(), r.confidence);
        }
    }

    // ── C major chord (C4 + E4 + G4) ──
    std::printf("\n--- C major chord ---\n");
    {
        float freqs[] = {261.63f, 329.63f, 392.0f};  // C4, E4, G4
        auto pcm = makeChord(freqs, 3, sr, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, sr);
        CHECK(r.ok, "C major chord: detection succeeded");
        if (r.ok) {
            bool isC = (r.key == "C" || r.key == "Cm" ||
                        r.key == "Am" || r.key == "Em" || r.key == "G");
            CHECK(isC, ("C major chord: key in expected set (got " + r.key + ")").c_str());
            CHECK(r.confidence > 0.0f, "C major chord: confidence > 0");
            std::printf("    key=%s camelot=%s conf=%.3f minor=%d\n",
                        r.key.c_str(), r.camelot.c_str(), r.confidence, r.isMinor);
        }
    }

    // ── A minor chord (A3 + C4 + E4) ──
    std::printf("\n--- A minor chord ---\n");
    {
        float freqs[] = {220.0f, 261.63f, 329.63f};  // A3, C4, E4
        auto pcm = makeChord(freqs, 3, sr, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, sr);
        CHECK(r.ok, "A minor chord: detection succeeded");
        if (r.ok) {
            bool isAmOrRelative = (r.key == "Am" || r.key == "C" ||
                                    r.key == "Em" || r.key == "A");
            CHECK(isAmOrRelative, ("A minor chord: key in expected set (got " + r.key + ")").c_str());
            std::printf("    key=%s camelot=%s conf=%.3f minor=%d\n",
                        r.key.c_str(), r.camelot.c_str(), r.confidence, r.isMinor);
        }
    }

    // ── D major chord (D4 + F#4 + A4) ──
    std::printf("\n--- D major chord ---\n");
    {
        float freqs[] = {293.66f, 369.99f, 440.0f};  // D4, F#4, A4
        auto pcm = makeChord(freqs, 3, sr, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, sr);
        CHECK(r.ok, "D major chord: detection succeeded");
        if (r.ok) {
            bool isDOrRelative = (r.key == "D" || r.key == "Bm" ||
                                   r.key == "A" || r.key == "F#m");
            CHECK(isDOrRelative, ("D major chord: key in expected set (got " + r.key + ")").c_str());
            std::printf("    key=%s camelot=%s conf=%.3f minor=%d\n",
                        r.key.c_str(), r.camelot.c_str(), r.confidence, r.isMinor);
        }
    }

    // ── Stereo input (2 channels) ──
    std::printf("\n--- Stereo input ---\n");
    {
        auto mono = makeSine(440.0f, sr, dur);
        int n = (int)mono.size();
        std::vector<float> stereo(n * 2);
        for (int i = 0; i < n; ++i) {
            stereo[2 * i + 0] = mono[i];        // L
            stereo[2 * i + 1] = mono[i] * 0.8f; // R (slightly different)
        }
        KeyResult r = KeyDetector::detect(stereo.data(), n, 2, sr);
        CHECK(r.ok, "stereo A4: detection succeeded");
        if (r.ok) {
            bool isA = (r.key == "A" || r.key == "Am");
            CHECK(isA, ("stereo A4: key root is A (got " + r.key + ")").c_str());
        }
    }

    // ── Different sample rate (22050 Hz) ──
    std::printf("\n--- Different sample rate (22050 Hz) ---\n");
    {
        auto pcm = makeSine(440.0f, 22050, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, 22050);
        CHECK(r.ok, "22050 Hz A4: detection succeeded");
        if (r.ok) {
            bool isA = (r.key == "A" || r.key == "Am");
            CHECK(isA, ("22050 Hz A4: key root is A (got " + r.key + ")").c_str());
        }
    }

    // ── Camelot code consistency ──
    std::printf("\n--- Camelot code format ---\n");
    {
        auto pcm = makeSine(440.0f, sr, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, sr);
        if (r.ok) {
            CHECK(r.camelot.size() >= 2 && r.camelot.size() <= 3,
                  "camelot code length 2-3 chars");
            char last = r.camelot.back();
            CHECK(last == 'A' || last == 'B',
                  "camelot ends with A (minor) or B (major)");
            if (r.isMinor)
                CHECK(last == 'A', "minor key → camelot ends with A");
            else
                CHECK(last == 'B', "major key → camelot ends with B");
        }
    }

    // ── Rich harmonic content: C major scale run ──
    std::printf("\n--- C major scale (rich harmonic) ---\n");
    {
        // All notes of C major scale played simultaneously
        float freqs[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.0f, 440.0f, 493.88f};
        auto pcm = makeChord(freqs, 7, sr, dur);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size(), 1, sr);
        CHECK(r.ok, "C major scale: detection succeeded");
        if (r.ok) {
            bool isCOrRelative = (r.key == "C" || r.key == "Am" ||
                                   r.key == "G" || r.key == "Em" || r.key == "F");
            CHECK(isCOrRelative,
                  ("C major scale: key in expected set (got " + r.key + ")").c_str());
            std::printf("    key=%s camelot=%s conf=%.3f\n",
                        r.key.c_str(), r.camelot.c_str(), r.confidence);
        }
    }

    // ── KeyResult default state ──
    std::printf("\n--- KeyResult defaults ---\n");
    {
        KeyResult r;
        CHECK(!r.ok, "default: ok = false");
        CHECK(r.key.empty(), "default: key empty");
        CHECK(r.camelot.empty(), "default: camelot empty");
        CHECK(r.confidence == 0.0f, "default: confidence = 0");
        CHECK(!r.isMinor, "default: isMinor = false");
    }

    std::printf("\n=== KeyDetector: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
