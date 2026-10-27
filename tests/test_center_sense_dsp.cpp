// test_center_sense_dsp.cpp — Unit tests for CenterSenseDsp (F-019)
#include <cstdio>
#include <cmath>
#include <vector>
#include "../src/CenterSenseDsp.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static bool near(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    std::printf("=== CenterSenseDsp Tests ===\n");

    // ── getCenterSenseAmounts ──
    std::printf("\n--- getCenterSenseAmounts ---\n");
    {
        float aLF, aMid, aHF;

        // k = 0 → all amounts zero
        CenterSenseDsp::getCenterSenseAmounts(0.0f, aLF, aMid, aHF);
        CHECK(near(aLF, 0.0f) && near(aMid, 0.0f) && near(aHF, 0.0f),
              "k=0: all amounts = 0");

        // k positive → amounts zero (widen side)
        CenterSenseDsp::getCenterSenseAmounts(0.5f, aLF, aMid, aHF);
        CHECK(near(aLF, 0.0f) && near(aMid, 0.0f) && near(aHF, 0.0f),
              "k=+0.5: all amounts = 0");

        // k = -1 → full tighten
        CenterSenseDsp::getCenterSenseAmounts(-1.0f, aLF, aMid, aHF);
        CHECK(near(aLF, 1.0f), "k=-1: amountLF = 1");
        CHECK(near(aMid, 1.0f), "k=-1: amountMid = 1 ((-1)^2 = 1)");
        CHECK(near(aHF, 1.0f), "k=-1: amountHF = 1");

        // k = -0.5 → partial
        CenterSenseDsp::getCenterSenseAmounts(-0.5f, aLF, aMid, aHF);
        CHECK(near(aLF, 0.5f), "k=-0.5: amountLF = 0.5");
        CHECK(near(aMid, 0.25f), "k=-0.5: amountMid = 0.25 (0.5^2)");
        CHECK(near(aHF, 0.5f), "k=-0.5: amountHF = 0.5");
    }

    // ── getCenterSenseWidthFactors ──
    std::printf("\n--- getCenterSenseWidthFactors ---\n");
    {
        float wLF, wMid, wHF;

        // k = 0 → all widths 1
        CenterSenseDsp::getCenterSenseWidthFactors(0.0f, wLF, wMid, wHF);
        CHECK(near(wLF, 1.0f) && near(wMid, 1.0f) && near(wHF, 1.0f),
              "k=0: all widths = 1");

        // k negative → all widths 1 (tighten side)
        CenterSenseDsp::getCenterSenseWidthFactors(-0.5f, wLF, wMid, wHF);
        CHECK(near(wLF, 1.0f) && near(wMid, 1.0f) && near(wHF, 1.0f),
              "k=-0.5: all widths = 1");

        // k = 1 → maximum widen
        CenterSenseDsp::getCenterSenseWidthFactors(1.0f, wLF, wMid, wHF);
        CHECK(near(wLF, 1.2f), "k=+1: widthLF = 1.2 (1 + 0.2*1)");
        CHECK(near(wMid, 1.6f), "k=+1: widthMid = 1.6 (1 + 0.6*1)");
        CHECK(near(wHF, 2.0f), "k=+1: widthHF = 2.0 (1 + 1.0*1)");

        // k = 0.5 → half widen
        CenterSenseDsp::getCenterSenseWidthFactors(0.5f, wLF, wMid, wHF);
        CHECK(near(wLF, 1.1f), "k=+0.5: widthLF = 1.1");
        CHECK(near(wMid, 1.3f), "k=+0.5: widthMid = 1.3");
        CHECK(near(wHF, 1.5f), "k=+0.5: widthHF = 1.5");
    }

    // ── processCenterSense: k=0 passthrough ──
    std::printf("\n--- processCenterSense passthrough ---\n");
    {
        float buf[] = { 1.0f, -1.0f, 0.5f, -0.5f, 0.0f, 0.0f };
        float orig[] = { 1.0f, -1.0f, 0.5f, -0.5f, 0.0f, 0.0f };
        CenterSenseDsp::ProcessState st{};
        CenterSenseDsp::processCenterSense(buf, 3, 44100, 0.0f, &st);
        bool same = true;
        for (int i = 0; i < 6; ++i) if (!near(buf[i], orig[i])) same = false;
        CHECK(same, "k=0: buffer unchanged (passthrough)");
    }

    // ── processCenterSense: k=-1 → mono collapse ──
    std::printf("\n--- processCenterSense full tighten ---\n");
    {
        const int N = 8000;
        std::vector<float> buf(N * 2);
        for (int i = 0; i < N; ++i) {
            buf[2*i + 0] = 1.0f;   // L = 1
            buf[2*i + 1] = -1.0f;  // R = -1
        }
        CenterSenseDsp::ProcessState st{};
        CenterSenseDsp::processCenterSense(buf.data(), N, 44100, -1.0f, &st);
        // After filter settles, L and R should converge toward mono (L+R)/2 = 0
        float lastL = buf[2*(N-1) + 0];
        float lastR = buf[2*(N-1) + 1];
        CHECK(std::fabs(lastL) < 0.05f, "k=-1 settled: L near 0 (mono collapse)");
        CHECK(std::fabs(lastR) < 0.05f, "k=-1 settled: R near 0 (mono collapse)");
    }

    // ── processCenterSense: null/edge safety ──
    std::printf("\n--- processCenterSense safety ---\n");
    {
        CenterSenseDsp::ProcessState st{};
        CenterSenseDsp::processCenterSense(nullptr, 100, 44100, -0.5f, &st);
        float buf[2] = {1.0f, -1.0f};
        CenterSenseDsp::processCenterSense(buf, 0, 44100, -0.5f, &st);
        CenterSenseDsp::processCenterSense(buf, 1, 0, -0.5f, &st);
        CenterSenseDsp::processCenterSense(buf, 1, 44100, -0.5f, nullptr);
        CHECK(true, "null/edge inputs: no crash");
    }

    // ── processBassMono ──
    std::printf("\n--- processBassMono ---\n");
    {
        // cutoffHz <= 0 → no-op
        float buf[] = {1.0f, -1.0f};
        CenterSenseDsp::BassMonoState bst{};
        CenterSenseDsp::processBassMono(buf, 1, 44100, 0.0f, &bst);
        CHECK(near(buf[0], 1.0f) && near(buf[1], -1.0f), "cutoff=0: no-op");

        // Run with DC stereo signal (L=1, R=-1) through 200Hz cutoff
        const int N = 8000;
        std::vector<float> dc(N * 2);
        for (int i = 0; i < N; ++i) { dc[2*i] = 1.0f; dc[2*i+1] = -1.0f; }
        CenterSenseDsp::BassMonoState bst2{};
        CenterSenseDsp::processBassMono(dc.data(), N, 44100, 200.0f, &bst2);
        // After settling, low freq component should be mono → (L+R)/2 = 0
        // So output should approach 0 for both channels at DC
        float lLast = dc[2*(N-1)];
        float rLast = dc[2*(N-1)+1];
        CHECK(std::fabs(lLast) < 0.05f, "bassMono DC settled: L near 0");
        CHECK(std::fabs(rLast) < 0.05f, "bassMono DC settled: R near 0");

        // Null safety
        CenterSenseDsp::processBassMono(nullptr, 10, 44100, 100.0f, &bst2);
        CenterSenseDsp::processBassMono(dc.data(), 10, 44100, 100.0f, nullptr);
        CHECK(true, "bassMono null safety: no crash");
    }

    // ── processFreqShift ──
    std::printf("\n--- processFreqShift ---\n");
    {
        // shiftHz near 0 → no-op
        float buf[] = {0.5f, -0.5f, 0.3f, -0.3f};
        float orig[] = {0.5f, -0.5f, 0.3f, -0.3f};
        CenterSenseDsp::FreqShiftState fst{};
        CenterSenseDsp::processFreqShift(buf, 2, 44100, 0.0f, &fst);
        CHECK(near(buf[0], orig[0]) && near(buf[1], orig[1]),
              "shift=0: no-op");

        // Run with sine wave, verify output is finite and not silent
        const int N = 4000;
        std::vector<float> sine(N * 2);
        for (int i = 0; i < N; ++i) {
            float s = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);
            sine[2*i] = s;
            sine[2*i+1] = s;
        }
        CenterSenseDsp::FreqShiftState fst2{};
        CenterSenseDsp::processFreqShift(sine.data(), N, 44100, 100.0f, &fst2);
        float maxAbs = 0.0f;
        bool allFinite = true;
        for (int i = 0; i < N*2; ++i) {
            if (std::fabs(sine[i]) > maxAbs) maxAbs = std::fabs(sine[i]);
            if (std::isnan(sine[i]) || std::isinf(sine[i])) allFinite = false;
        }
        CHECK(allFinite, "freqShift +100Hz: all samples finite");
        CHECK(maxAbs > 0.01f, "freqShift +100Hz: output not silent");
        CHECK(maxAbs < 2.0f, "freqShift +100Hz: output not clipping wildly");

        // State persists across calls
        CHECK(fst2.phase >= 0.0f && fst2.phase < 1.0f, "freqShift: phase in [0,1)");

        // Null safety
        CenterSenseDsp::processFreqShift(nullptr, 10, 44100, 50.0f, &fst2);
        CenterSenseDsp::processFreqShift(sine.data(), 10, 44100, 50.0f, nullptr);
        CHECK(true, "freqShift null safety: no crash");
    }

    std::printf("\n=== CenterSenseDsp: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
