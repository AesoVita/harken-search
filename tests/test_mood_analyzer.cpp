// test_mood_analyzer.cpp — Unit tests for MoodAnalyzer (F-028)
#include <cstdio>
#include <cmath>
#include "../src/MoodAnalyzer.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static bool near(float a, float b, float tol = 0.05f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    std::printf("=== MoodAnalyzer Tests ===\n");

    // ── Default / zero input ──
    std::printf("\n--- Default input ---\n");
    {
        MoodInput in;
        MoodResult r = MoodAnalyzer::analyze(in);
        CHECK(r.ok, "default input: ok = true");
        CHECK(near(r.valence, 0.0f), "default input: valence = 0");
        CHECK(near(r.arousal, 0.0f), "default input: arousal = 0");
    }

    // ── Always returns ok=true ──
    std::printf("\n--- Always ok ---\n");
    {
        MoodInput in;
        in.bpm = 120.0f;
        in.bpmConfidence = 0.9f;
        MoodResult r = MoodAnalyzer::analyze(in);
        CHECK(r.ok, "with bpm: ok = true");

        MoodInput in2;
        in2.isMinorKey = true;
        in2.keyConfidence = 0.8f;
        r = MoodAnalyzer::analyze(in2);
        CHECK(r.ok, "with key: ok = true");
    }

    // ── Key mode affects valence ──
    std::printf("\n--- Key mode -> valence ---\n");
    {
        MoodInput major;
        major.isMinorKey = false;
        major.keyConfidence = 0.9f;
        MoodResult rMaj = MoodAnalyzer::analyze(major);

        MoodInput minor;
        minor.isMinorKey = true;
        minor.keyConfidence = 0.9f;
        MoodResult rMin = MoodAnalyzer::analyze(minor);

        CHECK(rMaj.valence > rMin.valence, "major key -> higher valence than minor");
        CHECK(rMaj.valence > 0.0f, "major key -> positive valence");
        CHECK(rMin.valence < 0.0f, "minor key -> negative valence");
    }

    // ── Key confidence below threshold -> no contribution ──
    std::printf("\n--- Key confidence threshold ---\n");
    {
        MoodInput lowConf;
        lowConf.isMinorKey = true;
        lowConf.keyConfidence = 0.1f;
        MoodResult r = MoodAnalyzer::analyze(lowConf);
        CHECK(near(r.valence, 0.0f), "keyConfidence 0.1 -> valence = 0 (below threshold)");

        MoodInput atThreshold;
        atThreshold.isMinorKey = true;
        atThreshold.keyConfidence = 0.2f;
        MoodResult r2 = MoodAnalyzer::analyze(atThreshold);
        CHECK(near(r2.valence, 0.0f), "keyConfidence 0.2 -> valence = 0 (at threshold)");

        MoodInput aboveThreshold;
        aboveThreshold.isMinorKey = true;
        aboveThreshold.keyConfidence = 0.3f;
        MoodResult r3 = MoodAnalyzer::analyze(aboveThreshold);
        CHECK(r3.valence < 0.0f, "keyConfidence 0.3 -> negative valence (minor contributes)");
    }

    // ── BPM affects arousal ──
    std::printf("\n--- BPM -> arousal ---\n");
    {
        MoodInput fast;
        fast.bpm = 180.0f;
        fast.bpmConfidence = 0.9f;
        MoodResult rFast = MoodAnalyzer::analyze(fast);

        MoodInput slow;
        slow.bpm = 60.0f;
        slow.bpmConfidence = 0.9f;
        MoodResult rSlow = MoodAnalyzer::analyze(slow);

        CHECK(rFast.arousal > rSlow.arousal, "180 BPM -> higher arousal than 60 BPM");
        CHECK(rFast.arousal > 0.0f, "180 BPM -> positive arousal");
    }

    // ── BPM confidence below threshold -> no contribution ──
    std::printf("\n--- BPM confidence threshold ---\n");
    {
        MoodInput lowBpmConf;
        lowBpmConf.bpm = 180.0f;
        lowBpmConf.bpmConfidence = 0.1f;
        MoodResult r = MoodAnalyzer::analyze(lowBpmConf);
        CHECK(near(r.arousal, 0.0f), "bpmConfidence 0.1 -> arousal = 0 (below threshold)");

        MoodInput zeroBpm;
        zeroBpm.bpm = 0.0f;
        zeroBpm.bpmConfidence = 0.9f;
        MoodResult r2 = MoodAnalyzer::analyze(zeroBpm);
        CHECK(near(r2.arousal, 0.0f), "bpm=0 -> arousal = 0 (zero bpm ignored)");
    }

    // ── LUFS affects arousal ──
    std::printf("\n--- LUFS -> arousal ---\n");
    {
        MoodInput loud;
        loud.lufs = -10.0f;
        MoodResult rLoud = MoodAnalyzer::analyze(loud);

        MoodInput quiet;
        quiet.lufs = -40.0f;
        MoodResult rQuiet = MoodAnalyzer::analyze(quiet);

        CHECK(rLoud.arousal > rQuiet.arousal, "loud (-10 LUFS) -> higher arousal than quiet (-40)");

        MoodInput defaultLufs;
        defaultLufs.lufs = -99.0f;
        MoodResult rDef = MoodAnalyzer::analyze(defaultLufs);
        CHECK(near(rDef.arousal, 0.0f), "lufs=-99 -> no contribution (sentinel)");
    }

    // ── Spectral features affect arousal (all normalised 0..1) ──
    std::printf("\n--- Spectral features -> arousal ---\n");
    {
        MoodInput bright;
        bright.spectralCentroid = 0.9f;
        bright.spectralFlux = 0.8f;
        bright.zeroCrossingRate = 0.7f;
        MoodResult rBright = MoodAnalyzer::analyze(bright);

        MoodInput dark;
        dark.spectralCentroid = 0.1f;
        dark.spectralFlux = 0.01f;
        dark.zeroCrossingRate = 0.01f;
        MoodResult rDark = MoodAnalyzer::analyze(dark);

        CHECK(rBright.arousal > rDark.arousal,
              "high spectral features -> higher arousal than low");
    }

    // ── Spectral centroid also affects valence (expects normalised 0..1) ──
    std::printf("\n--- Spectral centroid -> valence ---\n");
    {
        MoodInput bright;
        bright.spectralCentroid = 0.9f;
        MoodResult rBright = MoodAnalyzer::analyze(bright);

        MoodInput dark;
        dark.spectralCentroid = 0.1f;
        MoodResult rDark = MoodAnalyzer::analyze(dark);

        CHECK(rBright.valence > rDark.valence,
              "high centroid -> higher valence (brighter = more positive)");
    }

    // ── Zero spectral features -> no contribution ──
    std::printf("\n--- Zero spectral -> no contribution ---\n");
    {
        MoodInput zeroSpec;
        zeroSpec.spectralCentroid = 0.0f;
        zeroSpec.spectralFlux = 0.0f;
        zeroSpec.zeroCrossingRate = 0.0f;
        MoodResult r = MoodAnalyzer::analyze(zeroSpec);
        CHECK(near(r.arousal, 0.0f), "zero spectral features -> arousal = 0");
        CHECK(near(r.valence, 0.0f), "zero spectral features -> valence = 0");
    }

    // ── Combined: upbeat major ──
    std::printf("\n--- Combined: upbeat major ---\n");
    {
        MoodInput upbeat;
        upbeat.bpm = 140.0f;
        upbeat.bpmConfidence = 0.9f;
        upbeat.isMinorKey = false;
        upbeat.keyConfidence = 0.9f;
        upbeat.lufs = -14.0f;
        upbeat.spectralCentroid = 0.7f;
        upbeat.spectralFlux = 0.5f;
        upbeat.zeroCrossingRate = 0.4f;
        MoodResult r = MoodAnalyzer::analyze(upbeat);
        CHECK(r.ok, "upbeat major: ok");
        CHECK(r.valence > 0.0f, "upbeat major: positive valence");
        CHECK(r.arousal > 0.0f, "upbeat major: positive arousal");
    }

    // ── Combined: slow minor ──
    std::printf("\n--- Combined: slow minor ---\n");
    {
        MoodInput somber;
        somber.bpm = 60.0f;
        somber.bpmConfidence = 0.9f;
        somber.isMinorKey = true;
        somber.keyConfidence = 0.9f;
        somber.lufs = -30.0f;
        somber.spectralCentroid = 0.2f;
        somber.spectralFlux = 0.05f;
        somber.zeroCrossingRate = 0.05f;
        MoodResult r = MoodAnalyzer::analyze(somber);
        CHECK(r.ok, "slow minor: ok");
        CHECK(r.valence < 0.0f, "slow minor: negative valence");
    }

    // ── Result clamping ──
    std::printf("\n--- Result clamping ---\n");
    {
        MoodInput extreme;
        extreme.bpm = 300.0f;
        extreme.bpmConfidence = 1.0f;
        extreme.isMinorKey = false;
        extreme.keyConfidence = 1.0f;
        extreme.lufs = 0.0f;
        extreme.spectralCentroid = 1.0f;
        extreme.spectralFlux = 1.0f;
        extreme.zeroCrossingRate = 1.0f;
        MoodResult r = MoodAnalyzer::analyze(extreme);
        CHECK(r.valence >= -1.0f && r.valence <= 1.0f, "extreme input: valence in [-1,1]");
        CHECK(r.arousal >= -1.0f && r.arousal <= 1.0f, "extreme input: arousal in [-1,1]");
    }

    // ── MoodResult defaults ──
    std::printf("\n--- MoodResult defaults ---\n");
    {
        MoodResult r;
        CHECK(near(r.valence, 0.0f, 1e-6f), "default: valence = 0");
        CHECK(near(r.arousal, 0.0f, 1e-6f), "default: arousal = 0");
        CHECK(!r.ok, "default: ok = false");
    }

    std::printf("\n=== MoodAnalyzer: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
