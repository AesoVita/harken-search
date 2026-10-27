// test_db_norm.cpp — Unit tests for aura::AmplitudeToDbNorm
#include <cstdio>
#include <cmath>
#include <limits>
#include "../src/DbNorm.h"

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
    std::printf("=== DbNorm Tests ===\n");

    // ── Zero and tiny values return 0 ──
    std::printf("\n--- Zero / tiny inputs ---\n");
    {
        CHECK(near(aura::AmplitudeToDbNorm(0.0f), 0.0f), "x=0 -> 0");
        CHECK(near(aura::AmplitudeToDbNorm(-1.0f), 0.0f), "x=-1 -> 0");
        CHECK(near(aura::AmplitudeToDbNorm(1e-7f), 0.0f), "x=1e-7 -> 0 (below guard)");
        CHECK(near(aura::AmplitudeToDbNorm(1e-6f), 0.0f), "x=1e-6 -> 0 (at guard, not strictly greater)");
    }

    // ── NaN returns 0 ──
    std::printf("\n--- NaN ---\n");
    {
        float nan = std::numeric_limits<float>::quiet_NaN();
        CHECK(near(aura::AmplitudeToDbNorm(nan), 0.0f), "NaN -> 0");
    }

    // ── At or above 0 dB (x >= 1.0) returns 1 ──
    std::printf("\n--- Full scale and above ---\n");
    {
        CHECK(near(aura::AmplitudeToDbNorm(1.0f), 1.0f), "x=1.0 (0 dB) -> 1.0");
        CHECK(near(aura::AmplitudeToDbNorm(2.0f), 1.0f), "x=2.0 (+6 dB) -> 1.0");
        CHECK(near(aura::AmplitudeToDbNorm(10.0f), 1.0f), "x=10.0 (+20 dB) -> 1.0");
    }

    // ── At floor returns 0 ──
    std::printf("\n--- At floor ---\n");
    {
        // x=0.001 -> 20*log10(0.001) = -60 dB = floorDb -> return 0
        CHECK(near(aura::AmplitudeToDbNorm(0.001f), 0.0f), "x=0.001 (-60 dB) -> 0 (at floor)");
        // x=0.0001 -> -80 dB, below floor -> 0
        CHECK(near(aura::AmplitudeToDbNorm(0.0001f), 0.0f), "x=0.0001 (-80 dB) -> 0 (below floor)");
    }

    // ── Midpoint values with default floor (-60 dB) ──
    std::printf("\n--- Midpoint values (floor=-60) ---\n");
    {
        // x=0.01 -> -40 dB, n = (-40 + 60) / 60 = 20/60 = 0.3333
        CHECK(near(aura::AmplitudeToDbNorm(0.01f), 1.0f / 3.0f, 0.002f), "x=0.01 (-40 dB) -> 0.333");

        // x=0.1 -> -20 dB, n = (-20 + 60) / 60 = 40/60 = 0.6667
        CHECK(near(aura::AmplitudeToDbNorm(0.1f), 2.0f / 3.0f, 0.002f), "x=0.1 (-20 dB) -> 0.667");

        // x=sqrt(0.001) ~= 0.03162 -> -30 dB, n = (-30 + 60) / 60 = 0.5
        float halfPt = std::sqrtf(0.001f);
        CHECK(near(aura::AmplitudeToDbNorm(halfPt), 0.5f, 0.002f), "x=-30 dB -> 0.5 (midpoint)");
    }

    // ── Custom floor ──
    std::printf("\n--- Custom floorDb ---\n");
    {
        // floor=-48: x=0.01 -> -40 dB, n = (-40 + 48) / 48 = 8/48 = 0.1667
        CHECK(near(aura::AmplitudeToDbNorm(0.01f, -48.0f), 8.0f / 48.0f, 0.002f),
              "floor=-48, x=0.01 (-40 dB) -> 0.167");

        // floor=-48: x at floor = 10^(-48/20) ~= 0.003981
        float atFloor48 = std::powf(10.0f, -48.0f / 20.0f);
        CHECK(near(aura::AmplitudeToDbNorm(atFloor48, -48.0f), 0.0f, 0.002f),
              "floor=-48, x at floor -> 0");

        // floor=-96 (very wide range): x=0.1 -> -20 dB, n = (-20 + 96) / 96 = 76/96
        CHECK(near(aura::AmplitudeToDbNorm(0.1f, -96.0f), 76.0f / 96.0f, 0.002f),
              "floor=-96, x=0.1 (-20 dB) -> 0.792");
    }

    // ── Monotonicity: larger amplitude -> larger result ──
    std::printf("\n--- Monotonicity ---\n");
    {
        float prev = 0.0f;
        bool monotonic = true;
        for (int i = 1; i <= 10; ++i) {
            float x = std::powf(10.0f, -6.0f + i * 0.5f);
            float val = aura::AmplitudeToDbNorm(x);
            if (val < prev && val > 0.0f && val < 1.0f) { monotonic = false; break; }
            if (val > prev) prev = val;
        }
        CHECK(monotonic, "monotonically increasing in active range");
    }

    std::printf("\n=== DbNorm: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
