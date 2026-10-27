// test_time_stretch.cpp — Unit tests for WSOLA TimeStretch
// Build: added to CMakeLists.txt as TestTimeStretch
// Run:   ctest --build-config Debug -R TestTimeStretch

#include "src/TimeStretch.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("PASS: %s\n", msg); g_pass++; } \
    else      { printf("FAIL: %s\n", msg); g_fail++; } \
} while(0)

// Generate a mono sine wave
static std::vector<float> genSine(float freq, uint32_t sampleRate, uint32_t frames, uint32_t channels = 2) {
    std::vector<float> buf(frames * channels, 0.0f);
    for (uint32_t i = 0; i < frames; ++i) {
        float v = sinf(2.0f * (float)M_PI * freq * i / sampleRate);
        for (uint32_t c = 0; c < channels; ++c)
            buf[i * channels + c] = v;
    }
    return buf;
}

// Measure dominant frequency in a buffer (zero-crossing method)
static float measureFreq(const float* buf, uint32_t frames, uint32_t channels, uint32_t sampleRate) {
    int crossings = 0;
    for (uint32_t i = 1; i < frames; ++i) {
        float prev = buf[(i - 1) * channels];
        float curr = buf[i * channels];
        if ((prev < 0 && curr >= 0) || (prev >= 0 && curr < 0))
            crossings++;
    }
    // Each full cycle has 2 zero crossings
    float duration = (float)frames / sampleRate;
    return crossings / (2.0f * duration);
}

void testBypass() {
    TimeStretch ts;
    ts.init(2, 44100);
    ts.setTempo(1.0f);

    CHECK(ts.isBypassed(), "Tempo 1.0 reports bypass");

    auto sine = genSine(440.0f, 44100, 4096, 2);
    uint32_t pos = 0;
    auto reader = [&](float* buf, uint32_t maxFrames) -> uint32_t {
        uint32_t avail = (uint32_t)sine.size() / 2 - pos;
        uint32_t n = std::min(maxFrames, avail);
        std::memcpy(buf, sine.data() + pos * 2, n * 2 * sizeof(float));
        pos += n;
        return n;
    };

    std::vector<float> out(4096 * 2);
    uint32_t got = ts.process(out.data(), 4096, reader);
    CHECK(got == 4096, "Bypass produces exact frame count");

    // Verify output matches input
    bool match = true;
    for (uint32_t i = 0; i < got * 2 && i < sine.size(); ++i) {
        if (std::abs(out[i] - sine[i]) > 1e-6f) { match = false; break; }
    }
    CHECK(match, "Bypass output is bit-exact with input");
}

void testSlowDown() {
    TimeStretch ts;
    ts.init(2, 44100);
    ts.setTempo(0.5f); // Half speed = double duration

    CHECK(!ts.isBypassed(), "Tempo 0.5 is not bypass");

    auto sine = genSine(440.0f, 44100, 8192, 2);
    uint32_t pos = 0;
    auto reader = [&](float* buf, uint32_t maxFrames) -> uint32_t {
        uint32_t avail = 8192 - pos;
        uint32_t n = std::min(maxFrames, avail);
        std::memcpy(buf, sine.data() + pos * 2, n * 2 * sizeof(float));
        pos += n;
        return n;
    };

    // At 0.5x tempo, 8192 input frames should produce ~16384 output frames
    std::vector<float> out(32768 * 2, 0.0f);
    uint32_t totalOut = 0;
    while (totalOut < 16384) {
        uint32_t got = ts.process(out.data() + totalOut * 2, 1024, reader);
        if (got == 0) break;
        totalOut += got;
    }

    // Should get significantly more output than input
    CHECK(totalOut > 12000, "0.5x tempo produces more frames than input");

    // Frequency should be preserved (not halved)
    float freq = measureFreq(out.data(), std::min(totalOut, 8192u), 2, 44100);
    CHECK(std::abs(freq - 440.0f) < 50.0f, "0.5x tempo preserves pitch (~440Hz)");
}

void testSpeedUp() {
    TimeStretch ts;
    ts.init(2, 44100);
    ts.setTempo(2.0f); // Double speed = half duration

    auto sine = genSine(440.0f, 44100, 8192, 2);
    uint32_t pos = 0;
    auto reader = [&](float* buf, uint32_t maxFrames) -> uint32_t {
        uint32_t avail = 8192 - pos;
        uint32_t n = std::min(maxFrames, avail);
        std::memcpy(buf, sine.data() + pos * 2, n * 2 * sizeof(float));
        pos += n;
        return n;
    };

    std::vector<float> out(8192 * 2, 0.0f);
    uint32_t totalOut = 0;
    while (totalOut < 8192) {
        uint32_t got = ts.process(out.data() + totalOut * 2, 1024, reader);
        if (got == 0) break;
        totalOut += got;
    }

    // Should get fewer frames than input
    CHECK(totalOut < 6000, "2.0x tempo produces fewer frames than input");
    CHECK(totalOut > 2000, "2.0x tempo still produces reasonable output");

    // Frequency should be preserved
    float freq = measureFreq(out.data(), std::min(totalOut, 4096u), 2, 44100);
    CHECK(std::abs(freq - 440.0f) < 50.0f, "2.0x tempo preserves pitch (~440Hz)");
}

void testReset() {
    TimeStretch ts;
    ts.init(2, 44100);
    ts.setTempo(1.5f);
    ts.reset();
    // After reset, should still work
    CHECK(std::abs(ts.getTempo() - 1.5f) < 0.001f, "Tempo preserved after reset");
}

int main() {
    printf("=== TimeStretch WSOLA Tests ===\n\n");

    testBypass();
    testSlowDown();
    testSpeedUp();
    testReset();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
