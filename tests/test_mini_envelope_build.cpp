#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/WaveformExtractor.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool condition, const char* desc) {
    if (condition) {
        ++passed;
        std::printf("  PASS: %s\n", desc);
    } else {
        ++failed;
        std::printf("  FAIL: %s\n", desc);
    }
}

int main() {
    std::printf("=== MiniEnvelopeBuild Tests ===\n");

    std::vector<float> maxDetail = {0.2f, 0.9f, 0.1f, 0.5f};
    std::vector<float> minDetail = {-0.1f, -0.3f, -0.8f, -0.2f};

    std::vector<float> outMax;
    std::vector<float> outMin;
    int outCh = 0;
    bool ok = WaveformExtractor::BuildMiniEnvelopeFromDetail(
        maxDetail, minDetail, 1, 2, false, outMax, outMin, outCh);

    CHECK(ok, "BuildMiniEnvelopeFromDetail returns true");
    CHECK(outCh == 1, "mono -> outChannels == 1");
    CHECK(outMax.size() == 2, "outMax has targetFrames entries");
    CHECK(outMin.size() == 2, "outMin has targetFrames entries");
    CHECK(std::abs(outMax[0] - 0.9f) < 1e-6f, "bin0 max == 0.9 (signed, not folded)");
    CHECK(std::abs(outMin[0] - (-0.3f)) < 1e-6f, "bin0 min == -0.3");
    CHECK(std::abs(outMax[1] - 0.5f) < 1e-6f, "bin1 max == 0.5");
    CHECK(std::abs(outMin[1] - (-0.8f)) < 1e-6f, "bin1 min == -0.8 (asymmetric preserved)");

    std::printf("\n=== MiniEnvelopeBuild: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}
