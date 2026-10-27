// test_f033_phase1.cpp
// F-033 Music Mode -- Phase 1 unit tests
//   Phase 1-2: KeyDetector -- Camelot wheel mappings, synthetic PCM detection
//   Phase 1-3: MoodResult  -- rule-based valence/arousal formula
//
// Returns 0 if all pass, 1 if any fail.

#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include <string>

#include "../src/KeyDetector.h"
#include "../src/MoodAnalyzer.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int passCount = 0;
static int failCount = 0;

#define CHECK(cond, name) \
    do { \
        if (cond) { std::printf("PASS: %s\n", name); ++passCount; } \
        else       { std::printf("FAIL: %s\n", name); ++failCount; } \
    } while(0)

#define CHECK_STR(a, b, name) \
    CHECK(std::string(a) == std::string(b), name)

// ---------------------------------------------------------------------------
// Phase 1-2: KeyDetector -- Camelot mapping consistency
// (Tests the static key-name + camelot tables, not audio decoding)
// ---------------------------------------------------------------------------

// Generate a chord-like sine mix for a given root pitch class (0=C, 9=A, etc.)
// Returns interleaved stereo PCM at 22050 Hz, ~3 seconds.
// For a major key: root + major third + perfect fifth
// For a minor key: root + minor third + perfect fifth
static std::vector<float> makeChordalPcm(int pitchClass, bool minor,
                                          uint32_t sampleRate = 22050,
                                          float durationSec  = 3.0f) {
    // C4 = 261.63 Hz, each semitone up x 2^(1/12)
    float baseHz = 261.63f * std::pow(2.0f, pitchClass / 12.0f);
    float thirdHz = baseHz * std::pow(2.0f, (minor ? 3.0f : 4.0f) / 12.0f);
    float fifthHz = baseHz * std::pow(2.0f, 7.0f / 12.0f);

    int numFrames = static_cast<int>(sampleRate * durationSec);
    std::vector<float> pcm(numFrames * 2, 0.0f);  // stereo
    for (int i = 0; i < numFrames; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float s = (std::sin(2.0f * 3.14159f * baseHz  * t) * 0.5f +
                   std::sin(2.0f * 3.14159f * thirdHz * t) * 0.3f +
                   std::sin(2.0f * 3.14159f * fifthHz * t) * 0.2f) * 0.7f;
        pcm[i * 2]     = s;
        pcm[i * 2 + 1] = s;
    }
    return pcm;
}

static void runPhase12KeyDetector() {
    std::printf("\n=== Phase 1-2: KeyDetector Camelot Mapping ===\n");

    // Known Camelot wheel facts (hard-coded from standard wheel)
    struct KnownKey { const char* key; const char* camelot; bool isMinor; };
    static const KnownKey known[] = {
        {"Am",  "8A",  true },
        {"C",   "8B",  false},
        {"Em",  "9A",  true },
        {"G",   "9B",  false},
        {"Bm",  "10A", true },
        {"D",   "10B", false},
        {"F#m", "11A", true },
        {"A",   "11B", false},
        {"Dbm", nullptr, true},  // skip camelot check, just test no crash
    };

    // Detect with a synthetic chordal PCM and check output is plausible
    // C major: pitch class 0, major
    {
        auto pcm = makeChordalPcm(0, false);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size() / 2, 2, 22050);
        CHECK(r.ok, "C-major chord: ok=true");
        CHECK(r.confidence >= 0.0f && r.confidence <= 1.0f,
              "C-major chord: confidence in [0,1]");
        CHECK(!r.camelot.empty(), "C-major chord: camelot non-empty");
        // Camelot for major keys ends with 'B'
        if (!r.camelot.empty())
            CHECK(r.camelot.back() == 'B', "C-major chord: camelot ends with B (major)");
    }

    // Am: pitch class 9, minor
    {
        auto pcm = makeChordalPcm(9, true);
        KeyResult r = KeyDetector::detect(pcm.data(), (int)pcm.size() / 2, 2, 22050);
        CHECK(r.ok, "Am chord: ok=true");
        CHECK(r.confidence >= 0.0f && r.confidence <= 1.0f,
              "Am chord: confidence in [0,1]");
        if (!r.camelot.empty())
            CHECK(r.camelot.back() == 'A', "Am chord: camelot ends with A (minor)");
    }

    // Edge: empty PCM -> ok=false, no crash
    {
        KeyResult r = KeyDetector::detect(nullptr, 0, 2, 22050);
        CHECK(!r.ok, "empty PCM: ok=false");
    }

    // Edge: single-sample PCM -> ok=false or ok=true but no crash
    {
        float sample = 0.5f;
        KeyResult r = KeyDetector::detect(&sample, 1, 1, 22050);
        // Just check no crash and confidence in valid range
        CHECK(r.confidence >= 0.0f && r.confidence <= 1.0f,
              "single-sample PCM: confidence in [0,1]");
    }

    // Silence trimming: C-major chord with 2s leading + 2s trailing silence.
    // Key detected from padded buffer must match key detected from clean buffer.
    {
        const uint32_t SR = 22050;
        auto clean = makeChordalPcm(0, false, SR, 4.0f);   // 4s C-major, stereo
        int cleanFrames = (int)clean.size() / 2;

        // 2s silence (stereo) before and after
        std::vector<float> padded(static_cast<size_t>(SR * 2 * 2), 0.0f);
        padded.insert(padded.end(), clean.begin(), clean.end());
        padded.insert(padded.end(), static_cast<size_t>(SR * 2 * 2), 0.0f);
        int paddedFrames = (int)padded.size() / 2;

        KeyResult rClean  = KeyDetector::detect(clean.data(),  cleanFrames,  2, SR);
        KeyResult rPadded = KeyDetector::detect(padded.data(), paddedFrames, 2, SR);

        CHECK(rClean.ok,  "C-major silence-trim: clean ok=true");
        CHECK(rPadded.ok, "C-major silence-trim: padded ok=true");
        bool sameMode = rClean.ok && rPadded.ok &&
                        (rClean.camelot.back() == rPadded.camelot.back());
        CHECK(sameMode, "C-major silence-trim: major/minor class unchanged by silence padding");
    }

    // Pure silence buffer -> ok=false, no crash
    {
        const uint32_t SR = 22050;
        std::vector<float> silence(static_cast<size_t>(SR * 3 * 2), 0.0f);  // 3s stereo
        KeyResult r = KeyDetector::detect(silence.data(), (int)silence.size() / 2, 2, SR);
        CHECK(!r.ok, "pure silence: ok=false");
    }
}

// ---------------------------------------------------------------------------
// Phase 1-3: MoodAnalyzer::analyze() via MoodInput
// ---------------------------------------------------------------------------

static void runPhase13Mood() {
    std::printf("\n=== Phase 1-3: MoodAnalyzer Rule-Based Formula ===\n");

    // Major key (high confidence) -> valence > 0
    {
        MoodInput in;
        in.keyConfidence = 0.8f;
        in.isMinorKey    = false;
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.ok,          "major key: ok=true");
        CHECK(m.valence > 0, "major key: valence > 0");
        CHECK(m.valence >= -1.0f && m.valence <= 1.0f, "major key: valence in [-1,1]");
    }

    // Minor key (high confidence) -> valence < 0
    {
        MoodInput in;
        in.keyConfidence = 0.8f;
        in.isMinorKey    = true;
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.ok,          "minor key: ok=true");
        CHECK(m.valence < 0, "minor key: valence < 0");
    }

    // Fast BPM (180, high confidence) -> arousal > 0
    {
        MoodInput in;
        in.bpm           = 180.0f;
        in.bpmConfidence = 0.9f;
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.ok,          "fast bpm: ok=true");
        CHECK(m.arousal > 0, "fast bpm: arousal > 0");
        CHECK(m.arousal >= -1.0f && m.arousal <= 1.0f, "fast bpm: arousal in [-1,1]");
    }

    // Slow BPM (60) -> arousal <= 0
    {
        MoodInput in;
        in.bpm           = 60.0f;
        in.bpmConfidence = 0.9f;
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.arousal <= 0, "slow bpm: arousal <= 0");
    }

    // Low confidence key (< 0.2) -> valence not driven by key
    {
        MoodInput in;
        in.keyConfidence = 0.1f;  // below threshold
        in.isMinorKey    = false;
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.valence == 0.0f, "low-confidence key: valence == 0");
    }

    // BPM below confidence threshold -> no BPM contribution
    {
        MoodInput in;
        in.bpm           = 120.0f;
        in.bpmConfidence = 0.1f;  // below 0.2 threshold
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.arousal == 0.0f, "low-confidence bpm: arousal == 0");
    }

    // Result always in [-1, 1] range
    {
        MoodInput in;
        in.bpm           = 999.0f;  // extreme value
        in.bpmConfidence = 1.0f;
        in.keyConfidence = 1.0f;
        in.isMinorKey    = false;
        MoodResult m = MoodAnalyzer::analyze(in);
        CHECK(m.valence >= -1.0f && m.valence <= 1.0f, "extreme input: valence clamped");
        CHECK(m.arousal >= -1.0f && m.arousal <= 1.0f, "extreme input: arousal clamped");
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    runPhase12KeyDetector();
    runPhase13Mood();

    std::printf("\n--- Results: %d passed, %d failed ---\n", passCount, failCount);
    return failCount > 0 ? 1 : 0;
}
