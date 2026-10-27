// test_mini_waveform_geometry.cpp — Unit tests for MiniWaveformLayout
#include <cstdio>
#include <cmath>
#include "../src/MiniWaveformGeometry.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static bool near(float a, float b, float tol = 0.01f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    std::printf("=== MiniWaveformGeometry Tests ===\n");

    // ── seekXFromNormalized ──
    std::printf("\n--- seekXFromNormalized ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 100.0f;
        layout.width = 200.0f;

        CHECK(near(layout.seekXFromNormalized(0.0f), 100.0f), "normalized=0 -> minX");
        CHECK(near(layout.seekXFromNormalized(1.0f), 300.0f), "normalized=1 -> minX+width");
        CHECK(near(layout.seekXFromNormalized(0.5f), 200.0f), "normalized=0.5 -> midpoint");
        CHECK(near(layout.seekXFromNormalized(0.25f), 150.0f), "normalized=0.25 -> quarter");
    }

    // ── seekXFromNormalized: clamping ──
    std::printf("\n--- seekXFromNormalized clamping ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 50.0f;
        layout.width = 100.0f;

        CHECK(near(layout.seekXFromNormalized(-0.5f), 50.0f), "negative normalized clamped to minX");
        CHECK(near(layout.seekXFromNormalized(1.5f), 150.0f), "over-1 normalized clamped to minX+width");
        CHECK(near(layout.seekXFromNormalized(-100.0f), 50.0f), "very negative clamped");
        CHECK(near(layout.seekXFromNormalized(100.0f), 150.0f), "very large clamped");
    }

    // ── normalizedFromX ──
    std::printf("\n--- normalizedFromX ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 100.0f;
        layout.width = 200.0f;

        CHECK(near(layout.normalizedFromX(100.0f), 0.0f), "x=minX -> 0");
        CHECK(near(layout.normalizedFromX(300.0f), 1.0f), "x=minX+width -> 1");
        CHECK(near(layout.normalizedFromX(200.0f), 0.5f), "x=midpoint -> 0.5");
        CHECK(near(layout.normalizedFromX(150.0f), 0.25f), "x=quarter -> 0.25");
    }

    // ── normalizedFromX: clamping ──
    std::printf("\n--- normalizedFromX clamping ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 100.0f;
        layout.width = 200.0f;

        CHECK(near(layout.normalizedFromX(50.0f), 0.0f), "x before minX -> 0");
        CHECK(near(layout.normalizedFromX(400.0f), 1.0f), "x beyond end -> 1");
    }

    // ── normalizedFromX: zero width ──
    std::printf("\n--- normalizedFromX zero width ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 100.0f;
        layout.width = 0.0f;

        CHECK(near(layout.normalizedFromX(100.0f), 0.0f), "zero width -> 0");
        CHECK(near(layout.normalizedFromX(200.0f), 0.0f), "zero width, any x -> 0");
    }

    // ── normalizedFromX: negative width ──
    std::printf("\n--- normalizedFromX negative width ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 100.0f;
        layout.width = -10.0f;

        CHECK(near(layout.normalizedFromX(100.0f), 0.0f), "negative width -> 0");
    }

    // ── Round-trip: seekX -> normalizedFromX ──
    std::printf("\n--- Round-trip ---\n");
    {
        MiniWaveformLayout layout;
        layout.minX = 30.0f;
        layout.width = 400.0f;

        for (float n = 0.0f; n <= 1.0f; n += 0.1f) {
            float x = layout.seekXFromNormalized(n);
            float back = layout.normalizedFromX(x);
            if (!near(back, n, 0.02f)) {
                CHECK(false, "round-trip failed");
                break;
            }
        }
        CHECK(true, "round-trip seekX->normalized consistent");
    }

    // ── ComputeExpandedMiniWaveformLayout: basic ──
    std::printf("\n--- ComputeExpandedMiniWaveformLayout basic ---\n");
    {
        auto layout = ComputeExpandedMiniWaveformLayout(
            10.0f, 20.0f, 200.0f, 30.0f, 5.0f);

        CHECK(near(layout.minX, 10.0f - 5.0f), "minX = cursorX - cellPaddingX");
        CHECK(near(layout.minY, 20.0f), "minY = cursorY");
        CHECK(near(layout.width, 200.0f + 5.0f * 2.0f), "width = contentAvailWidth + 2*padding");
        CHECK(near(layout.height, 30.0f), "height = rowHeight");
    }

    // ── ComputeExpandedMiniWaveformLayout: min-size clamping ──
    std::printf("\n--- ComputeExpandedMiniWaveformLayout min-size ---\n");
    {
        auto layout = ComputeExpandedMiniWaveformLayout(
            10.0f, 20.0f, 0.0f, 5.0f, 2.0f);

        // width = max(50, 0 + 4) = 50
        CHECK(layout.width >= 50.0f, "width clamped to min 50");
        // height = max(12, 5) = 12
        CHECK(layout.height >= 12.0f, "height clamped to min 12");
    }

    // ── ComputeExpandedMiniWaveformLayout: large values ──
    std::printf("\n--- ComputeExpandedMiniWaveformLayout large ---\n");
    {
        auto layout = ComputeExpandedMiniWaveformLayout(
            0.0f, 0.0f, 1000.0f, 80.0f, 10.0f);

        CHECK(near(layout.width, 1020.0f), "large width = 1000 + 20");
        CHECK(near(layout.height, 80.0f), "large height = 80");
    }

    std::printf("\n=== MiniWaveformGeometry: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
