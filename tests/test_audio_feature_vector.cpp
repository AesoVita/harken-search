// test_audio_feature_vector.cpp — Unit tests for AudioFeatureVector (F-028)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include "../src/FeatureExtractor.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static bool near(float a, float b, float tol = 1e-5f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    std::printf("=== AudioFeatureVector Tests ===\n");

    // ── Constants ──
    std::printf("\n--- Constants ---\n");
    {
        CHECK(AudioFeatureVector::VERSION == 1, "VERSION = 1");
        CHECK(AudioFeatureVector::DIMS == 48, "DIMS = 48");
        CHECK(AudioFeatureVector::MFCC_OFFSET == 0, "MFCC_OFFSET = 0");
        CHECK(AudioFeatureVector::MFCC_D_OFFSET == 13, "MFCC_D_OFFSET = 13");
        CHECK(AudioFeatureVector::SPECTRAL_OFFSET == 26, "SPECTRAL_OFFSET = 26");
        CHECK(AudioFeatureVector::PITCH_OFFSET == 31, "PITCH_OFFSET = 31");
        CHECK(AudioFeatureVector::ENERGY_OFFSET == 34, "ENERGY_OFFSET = 34");
        CHECK(AudioFeatureVector::GLOBAL_OFFSET == 44, "GLOBAL_OFFSET = 44");
    }

    // ── Default initialization ──
    std::printf("\n--- Default init ---\n");
    {
        AudioFeatureVector v;
        bool allZero = true;
        for (int i = 0; i < AudioFeatureVector::DIMS; ++i)
            if (v.features[i] != 0.0f) allZero = false;
        CHECK(allZero, "default: all features = 0");
        CHECK(v.isValid(), "default: isValid = true (zeros are valid)");
    }

    // ── Convenience accessors ──
    std::printf("\n--- Accessors ---\n");
    {
        AudioFeatureVector v;
        v.features[26] = 1.0f;  // spectralCentroid
        v.features[27] = 2.0f;  // spectralBandwidth
        v.features[28] = 3.0f;  // spectralRolloff
        v.features[29] = 4.0f;  // spectralFlux
        v.features[30] = 5.0f;  // spectralFlatness
        v.features[31] = 6.0f;  // medianF0
        v.features[32] = 7.0f;  // f0Std
        v.features[33] = 8.0f;  // voicedRatio
        v.features[42] = 9.0f;  // attackTime (ENERGY_OFFSET + 8)
        v.features[43] = 10.0f; // zeroCrossingRate (ENERGY_OFFSET + 9)

        CHECK(near(v.spectralCentroid(),  1.0f), "spectralCentroid accessor");
        CHECK(near(v.spectralBandwidth(), 2.0f), "spectralBandwidth accessor");
        CHECK(near(v.spectralRolloff(),   3.0f), "spectralRolloff accessor");
        CHECK(near(v.spectralFlux(),      4.0f), "spectralFlux accessor");
        CHECK(near(v.spectralFlatness(),  5.0f), "spectralFlatness accessor");
        CHECK(near(v.medianF0(),          6.0f), "medianF0 accessor");
        CHECK(near(v.f0Std(),             7.0f), "f0Std accessor");
        CHECK(near(v.voicedRatio(),       8.0f), "voicedRatio accessor");
        CHECK(near(v.attackTime(),        9.0f), "attackTime accessor");
        CHECK(near(v.zeroCrossingRate(), 10.0f), "zeroCrossingRate accessor");
    }

    // ── Serialize / Deserialize roundtrip ──
    std::printf("\n--- Serialize/Deserialize ---\n");
    {
        AudioFeatureVector orig;
        for (int i = 0; i < AudioFeatureVector::DIMS; ++i)
            orig.features[i] = (float)(i * 0.1);

        auto blob = orig.serialize();
        int expectedSize = (int)(sizeof(int) + AudioFeatureVector::DIMS * sizeof(float));
        CHECK((int)blob.size() == expectedSize, "serialized size correct");

        // Check version header
        int ver = 0;
        std::memcpy(&ver, blob.data(), sizeof(int));
        CHECK(ver == AudioFeatureVector::VERSION, "serialized version = 1");

        // Deserialize
        AudioFeatureVector restored;
        bool ok = AudioFeatureVector::deserialize(blob.data(), (int)blob.size(), restored);
        CHECK(ok, "deserialize succeeds");

        bool match = true;
        for (int i = 0; i < AudioFeatureVector::DIMS; ++i)
            if (!near(orig.features[i], restored.features[i])) match = false;
        CHECK(match, "roundtrip: all 48 dims match");
    }

    // ── Deserialize: bad inputs ──
    std::printf("\n--- Deserialize edge cases ---\n");
    {
        AudioFeatureVector out;

        // null blob
        CHECK(!AudioFeatureVector::deserialize(nullptr, 100, out), "null blob → false");

        // too small
        CHECK(!AudioFeatureVector::deserialize("abc", 3, out), "too small → false");

        // wrong version
        std::vector<uint8_t> badVer(sizeof(int) + 48 * sizeof(float), 0);
        int wrongVer = 99;
        std::memcpy(badVer.data(), &wrongVer, sizeof(int));
        CHECK(!AudioFeatureVector::deserialize(badVer.data(), (int)badVer.size(), out),
              "wrong version → false");

        // exact minimum size with correct version
        std::vector<uint8_t> minBlob(sizeof(int) + 48 * sizeof(float), 0);
        int goodVer = 1;
        std::memcpy(minBlob.data(), &goodVer, sizeof(int));
        CHECK(AudioFeatureVector::deserialize(minBlob.data(), (int)minBlob.size(), out),
              "minimum valid size → true");
    }

    // ── isValid: NaN / Inf detection ──
    std::printf("\n--- isValid ---\n");
    {
        AudioFeatureVector v;
        CHECK(v.isValid(), "all zeros → valid");

        v.features[10] = std::nanf("");
        CHECK(!v.isValid(), "NaN in dim 10 → invalid");

        v.features[10] = 0.0f;
        v.features[44] = std::numeric_limits<float>::infinity();
        CHECK(!v.isValid(), "+Inf in dim 44 → invalid");

        v.features[44] = -std::numeric_limits<float>::infinity();
        CHECK(!v.isValid(), "-Inf in dim 44 → invalid");

        v.features[44] = -999.0f;
        CHECK(v.isValid(), "large negative → valid (not NaN/Inf)");
    }

    std::printf("\n=== AudioFeatureVector: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
