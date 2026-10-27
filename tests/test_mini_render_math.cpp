#include <cmath>
#include <cstdio>

#include "../src/MiniWaveformRender.h"

static int g_failed = 0;
static int g_passed = 0;

static void Check(bool condition, const char* description) {
    if (condition) {
        ++g_passed;
        std::printf("  PASS: %s\n", description);
    } else {
        ++g_failed;
        std::printf("  FAIL: %s\n", description);
    }
}

static bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

int main() {
    std::printf("=== MiniRenderMath Tests ===\n");

    Check(Near(MiniWaveRender::VerticalCoverage(10, 10.0f, 14.0f), 1.0f),
          "fully-covered row -> 1.0");
    Check(Near(MiniWaveRender::VerticalCoverage(9, 9.5f, 14.0f), 0.5f),
          "half-covered top edge -> 0.5");
    Check(Near(MiniWaveRender::VerticalCoverage(14, 10.0f, 14.3f), 0.3f),
          "fractional bottom edge -> 0.3");
    Check(Near(MiniWaveRender::VerticalCoverage(20, 10.0f, 14.0f), 0.0f),
          "row outside span -> 0.0");

    MiniWaveRender::Span symmetric = MiniWaveRender::EnvelopeSpan(-0.5f, 0.5f, 25.0f, 20.0f);
    Check(Near(symmetric.top, 16.0f), "symmetric span top == 16");
    Check(Near(symmetric.bottom, 34.0f), "symmetric span bottom == 34");

    MiniWaveRender::Span asymmetric = MiniWaveRender::EnvelopeSpan(-0.2f, 0.9f, 25.0f, 20.0f);
    Check(Near(asymmetric.top, 8.8f), "asymmetric top tracks maxV");
    Check(Near(asymmetric.bottom, 28.6f), "asymmetric bottom tracks minV");

    std::printf("\n=== MiniRenderMath: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
