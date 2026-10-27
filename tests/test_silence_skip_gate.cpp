// Unit test for the ShouldApplySilenceSkip gate used by MaybeSkipLeadingSilence.
// Covers the root cause of BUG-229: deferred silence-skip seeking backward after
// the play cursor already reached/passed the first non-silent peak, which made
// the audio head play twice. The helper is a pure function — duplicated here
// verbatim from src/main.cpp to keep the test free of application dependencies.

#include <cstdio>
#include <cstdlib>

static bool ShouldApplySilenceSkip(float currentPos, float targetPos,
                                   float durationSec, float toleranceMs = 50.0f) {
    if (durationSec <= 0.0f) return true;
    if (currentPos < 0.0f || targetPos < 0.0f) return true;
    const float toleranceNorm = (toleranceMs / 1000.0f) / durationSec;
    return currentPos + toleranceNorm < targetPos;
}

static float ApplySilenceSkipPreroll(float firstSoundNorm,
                                     float durationSec,
                                     float prerollMs) {
    if (firstSoundNorm < 0.0f) return -1.0f;
    if (durationSec <= 0.0f || prerollMs <= 0.0f) {
        return firstSoundNorm < 0.0f ? 0.0f : (firstSoundNorm > 1.0f ? 1.0f : firstSoundNorm);
    }
    const float prerollNorm = (prerollMs / 1000.0f) / durationSec;
    float result = firstSoundNorm - prerollNorm;
    if (result < 0.0f) return 0.0f;
    if (result > 1.0f) return 1.0f;
    return result;
}

static int g_failed = 0;

static void check(const char* name, bool got, bool expected) {
    if (got == expected) {
        std::printf("PASS: %s (got=%d)\n", name, (int)got);
    } else {
        std::printf("FAIL: %s (got=%d expected=%d)\n", name, (int)got, (int)expected);
        g_failed++;
    }
}

static void checkNear(const char* name, float got, float expected, float eps = 0.0001f) {
    const float diff = got > expected ? got - expected : expected - got;
    if (diff <= eps) {
        std::printf("PASS: %s (got=%.6f)\n", name, got);
    } else {
        std::printf("FAIL: %s (got=%.6f expected=%.6f)\n", name, got, expected);
        g_failed++;
    }
}

int main() {
    // T1: cursor far before target -> seek
    check("T1 cursor=0.01 target=0.50 dur=2.0s -> seek",
          ShouldApplySilenceSkip(0.01f, 0.50f, 2.0f),
          true);

    // T2: cursor already past target -> no seek (rewind would replay head)
    check("T2 cursor=0.60 target=0.50 dur=2.0s -> no seek",
          ShouldApplySilenceSkip(0.60f, 0.50f, 2.0f),
          false);

    // T3: cursor inside the 50ms tolerance window (25ms before target on a 2s file)
    // 25ms / 2s = 0.0125; target - cur = 0.025. 0.025 < 0.025 is false, so skip.
    check("T3 cursor=0.475 target=0.50 dur=2.0s -> no seek (within 50ms)",
          ShouldApplySilenceSkip(0.475f, 0.50f, 2.0f),
          false);

    // T4: durationSec=0 falls back to legacy behavior (allow seek)
    check("T4 durationSec=0 -> seek (legacy fallback)",
          ShouldApplySilenceSkip(0.10f, 0.50f, 0.0f),
          true);

    // T5: negative position input falls back (allow seek)
    check("T5 cur=-1.0 -> seek (legacy fallback)",
          ShouldApplySilenceSkip(-1.0f, 0.50f, 2.0f),
          true);

    // Extra: exact boundary — cur + tolerance == target should NOT seek
    // (the comparison is strict: cur + tol < target)
    check("T6 boundary cur+tol==target -> no seek",
          ShouldApplySilenceSkip(0.475f, 0.500f, 2.0f),
          false);

    // Extra: cur + tolerance just a hair less than target -> seek
    // On a 10s file, 50ms tolerance = 0.005 norm. target=0.50, cur=0.49 -> gap 0.01 > 0.005 -> seek
    check("T7 cur=0.49 target=0.50 dur=10s -> seek (gap > tol)",
          ShouldApplySilenceSkip(0.49f, 0.50f, 10.0f),
          true);

    // Extra: negative targetPos -> fallback
    check("T8 target=-1 -> seek (legacy fallback)",
          ShouldApplySilenceSkip(0.10f, -1.0f, 2.0f),
          true);

    checkNear("T9 first sound at 3s in 10s with 1s preroll -> target 2s",
              ApplySilenceSkipPreroll(0.3f, 10.0f, 1000.0f),
              0.2f);

    checkNear("T10 preroll clamps to start when first sound is inside the reserve window",
              ApplySilenceSkipPreroll(0.05f, 10.0f, 1000.0f),
              0.0f);

    checkNear("T11 zero preroll keeps first audible point",
              ApplySilenceSkipPreroll(0.3f, 10.0f, 0.0f),
              0.3f);

    if (g_failed == 0) {
        std::printf("\nAll tests passed.\n");
        return 0;
    }
    std::printf("\n%d test(s) FAILED.\n", g_failed);
    return 1;
}
