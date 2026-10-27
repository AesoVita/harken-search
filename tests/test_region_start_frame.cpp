#include <cstdint>
#include <cstdio>

#include "../src/AudioEngine.h"

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
    std::printf("=== RegionStartFrame Tests ===\n");

    CHECK(AudioEngine::computeRegionStartFrame(0.5f, 0.75f, 480000, false) == 240000,
          "forward: 0.5 * 480000 == 240000");
    CHECK(AudioEngine::computeRegionStartFrame(0.0f, 1.0f, 480000, false) == 0,
          "forward full-file start == 0");
    CHECK(AudioEngine::computeRegionStartFrame(0.25f, 0.5f, 480000, true) == 240000,
          "reverse: (1-0.5) * 480000 == 240000");

    std::printf("\n=== RegionStartFrame: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}
