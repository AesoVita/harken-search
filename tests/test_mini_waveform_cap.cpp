#include <cstdint>
#include <cstdio>

#include "../src/WaveformBucketMath.h"

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
    std::printf("=== MiniWaveformCap Tests ===\n");

    CHECK(ComputeAdaptiveMiniPeakCount(1000000000ULL) == 1024,
          "1e9 frames clamps to 1024 (cap)");
    CHECK(ComputeAdaptiveMiniPeakCount(100ULL) == 512,
          "100 frames clamps to 512 (short-SFX readability floor)");
    CHECK(ComputeAdaptiveMiniPeakCount(200000ULL) == 512,
          "200000 frames -> 196 raw peaks, clamped to 512 floor");
    CHECK(ComputeAdaptiveMiniPeakCount(1024ULL * 2000ULL + 1ULL) == 1024,
          "just over 2000*1024 frames caps at 1024");

    std::printf("\n=== MiniWaveformCap: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}
