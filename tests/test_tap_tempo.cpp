// test_tap_tempo.cpp — Unit tests for TapTempoAccumulator
#include <cstdio>
#include <cmath>
#include <thread>
#include <chrono>
#include "../src/TapTempoAccumulator.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== TapTempoAccumulator Tests ===\n");

    // ── Initial state ──
    std::printf("\n--- Initial state ---\n");
    {
        TapTempoAccumulator acc;
        CHECK(!acc.hasResult(), "initial: hasResult = false");
        CHECK(acc.tapCount() == 0, "initial: tapCount = 0");
        CHECK(acc.getBpm() == 0.0f, "initial: getBpm = 0");
        CHECK(acc.isExpired(), "initial: isExpired = true (no taps)");
    }

    // ── Single tap ──
    std::printf("\n--- Single tap ---\n");
    {
        TapTempoAccumulator acc;
        acc.tap();
        CHECK(acc.tapCount() == 1, "1 tap: tapCount = 1");
        CHECK(!acc.hasResult(), "1 tap: hasResult = false (need 3)");
        CHECK(!acc.isExpired(), "1 tap: isExpired = false (just tapped)");
    }

    // ── Two taps ──
    std::printf("\n--- Two taps ---\n");
    {
        TapTempoAccumulator acc;
        acc.tap();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        acc.tap();
        CHECK(acc.tapCount() == 2, "2 taps: tapCount = 2");
        CHECK(!acc.hasResult(), "2 taps: hasResult = false (need 3)");
    }

    // ── Three taps → hasResult ──
    std::printf("\n--- Three taps ---\n");
    {
        TapTempoAccumulator acc;
        acc.tap();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        acc.tap();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        acc.tap();
        CHECK(acc.tapCount() == 3, "3 taps: tapCount = 3");
        CHECK(acc.hasResult(), "3 taps: hasResult = true");

        float bpm = acc.getBpm();
        CHECK(bpm > 0.0f, "3 taps: BPM > 0");
        // 100ms intervals → 600 BPM theoretically, allow wide tolerance for scheduler jitter
        CHECK(bpm > 200.0f && bpm < 1200.0f, "3 taps at ~100ms: BPM in reasonable range (200-1200)");
        std::printf("    measured BPM = %.1f\n", bpm);
    }

    // ── Reset ──
    std::printf("\n--- Reset ---\n");
    {
        TapTempoAccumulator acc;
        acc.tap();
        acc.tap();
        acc.tap();
        CHECK(acc.tapCount() == 3, "before reset: 3 taps");
        acc.reset();
        CHECK(acc.tapCount() == 0, "after reset: tapCount = 0");
        CHECK(!acc.hasResult(), "after reset: hasResult = false");
        CHECK(acc.getBpm() == 0.0f, "after reset: getBpm = 0");
    }

    // ── Multiple taps accumulate ──
    std::printf("\n--- Multiple taps ---\n");
    {
        TapTempoAccumulator acc;
        for (int i = 0; i < 5; i++) {
            acc.tap();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        CHECK(acc.tapCount() == 5, "5 taps: tapCount = 5");
        CHECK(acc.hasResult(), "5 taps: hasResult = true");
        float bpm = acc.getBpm();
        CHECK(bpm > 0.0f, "5 taps: BPM > 0");
    }

    // ── getBpm with zero total time ──
    std::printf("\n--- Edge cases ---\n");
    {
        TapTempoAccumulator acc;
        // rapid taps with no sleep (nearly simultaneous)
        acc.tap();
        acc.tap();
        acc.tap();
        float bpm = acc.getBpm();
        // should not crash; BPM may be very high or 0 if totalSec <= 0
        CHECK(bpm >= 0.0f, "rapid taps: BPM >= 0 (no crash)");
    }

    std::printf("\n=== TapTempoAccumulator: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
