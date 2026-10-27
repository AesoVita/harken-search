// test_time_stretch.cpp — Unit tests for TimeStretch (WSOLA)
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include "../src/TimeStretch.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static bool near(float a, float b, float tol = 1e-3f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    std::printf("=== TimeStretch Tests ===\n");

// Default state
    std::printf("\n--- Default state ---\n");
    {
        TimeStretch ts;
        CHECK(near(ts.getTempo(), 1.0f), "default tempo = 1.0");
        CHECK(ts.isBypassed(), "default: isBypassed = true");
    }

// setTempo / getTempo
    std::printf("\n--- setTempo / getTempo ---\n");
    {
        TimeStretch ts;
        ts.setTempo(1.5f);
        CHECK(near(ts.getTempo(), 1.5f), "setTempo(1.5) → 1.5");
        CHECK(!ts.isBypassed(), "tempo 1.5 → not bypassed");

        ts.setTempo(1.0f);
        CHECK(ts.isBypassed(), "tempo 1.0 → bypassed");

        ts.setTempo(0.9995f);
        CHECK(ts.isBypassed(), "tempo 0.9995 → bypassed (within 0.001 tolerance)");

        ts.setTempo(1.002f);
        CHECK(!ts.isBypassed(), "tempo 1.002 → not bypassed");
    }

// Tempo clamping
    std::printf("\n--- Tempo clamping ---\n");
    {
        TimeStretch ts;
        ts.setTempo(0.1f);
        CHECK(near(ts.getTempo(), 0.25f), "below min → clamped to 0.25");

        ts.setTempo(10.0f);
        CHECK(near(ts.getTempo(), 4.0f), "above max → clamped to 4.0");

        ts.setTempo(0.25f);
        CHECK(near(ts.getTempo(), 0.25f), "0.25 → accepted (min boundary)");

        ts.setTempo(4.0f);
        CHECK(near(ts.getTempo(), 4.0f), "4.0 → accepted (max boundary)");
    }

// Init + reset
    std::printf("\n--- Init / reset ---\n");
    {
        TimeStretch ts;
        ts.init(2, 44100);
        CHECK(near(ts.getTempo(), 1.0f), "after init: tempo = 1.0");

        ts.setTempo(2.0f);
        CHECK(near(ts.getTempo(), 2.0f), "set to 2.0");

        ts.reset();
        CHECK(near(ts.getTempo(), 2.0f), "reset does not change tempo");

        // Re-init should work without crash
        ts.init(1, 22050);
        ts.init(2, 48000);
        CHECK(true, "re-init multiple times: no crash");
    }

// Process: null/edge safety
    std::printf("\n--- Process safety ---\n");
    {
        TimeStretch ts;
        ts.init(2, 44100);
        ts.setTempo(0.5f);

        auto dummyCb = [](float*, uint32_t) -> uint32_t { return 0; };

        uint32_t r = ts.process(nullptr, 100, dummyCb);
        CHECK(r == 0, "null output → 0 frames");

        float buf[4];
        r = ts.process(buf, 0, dummyCb);
        CHECK(r == 0, "0 requestedFrames → 0 frames");

        r = ts.process(buf, 2, nullptr);
        CHECK(r == 0, "null callback → 0 frames");
    }

// Process: bypass passthrough
    std::printf("\n--- Bypass passthrough ---\n");
    {
        TimeStretch ts;
        ts.init(1, 44100);
        ts.setTempo(1.0f);

        // Prepare source: 1000 frames mono sine
        const int N = 1000;
        std::vector<float> src(N);
        for (int i = 0; i < N; ++i)
            src[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);

        int readPos = 0;
        auto readCb = [&](float* buf, uint32_t maxFrames) -> uint32_t {
            uint32_t avail = (uint32_t)(N - readPos);
            uint32_t toRead = std::min(maxFrames, avail);
            if (toRead > 0) {
                std::memcpy(buf, src.data() + readPos, toRead * sizeof(float));
                readPos += (int)toRead;
            }
            return toRead;
        };

        std::vector<float> out(N);
        uint32_t got = ts.process(out.data(), N, readCb);
        CHECK(got == (uint32_t)N, "bypass: got all N frames");

        bool match = true;
        for (int i = 0; i < N; ++i)
            if (!near(out[i], src[i], 1e-6f)) { match = false; break; }
        CHECK(match, "bypass: output matches input exactly");
    }

// Process: slow tempo produces more output
    std::printf("\n--- Slow tempo (0.5x) ---\n");
    {
        TimeStretch ts;
        ts.init(1, 44100);
        ts.setTempo(0.5f);

        const int srcFrames = 16000;
        std::vector<float> src(srcFrames);
        for (int i = 0; i < srcFrames; ++i)
            src[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);

        int readPos = 0;
        auto readCb = [&](float* buf, uint32_t maxFrames) -> uint32_t {
            uint32_t avail = (uint32_t)(srcFrames - readPos);
            uint32_t toRead = std::min(maxFrames, avail);
            if (toRead > 0) {
                std::memcpy(buf, src.data() + readPos, toRead * sizeof(float));
                readPos += (int)toRead;
            }
            return toRead;
        };

        const int outBufFrames = 64000;
        std::vector<float> out(outBufFrames, 0.0f);
        uint32_t got = ts.process(out.data(), outBufFrames, readCb);

        CHECK(got > (uint32_t)(srcFrames * 1.2f), "0.5x tempo: output > 1.2x input");

        // Output should be finite
        bool allFinite = true;
        for (uint32_t i = 0; i < got; ++i)
            if (std::isnan(out[i]) || std::isinf(out[i])) { allFinite = false; break; }
        CHECK(allFinite, "0.5x tempo: all output samples finite");

        // Output should not be silent
        float maxAbs = 0.0f;
        for (uint32_t i = 0; i < got; ++i)
            if (std::fabs(out[i]) > maxAbs) maxAbs = std::fabs(out[i]);
        CHECK(maxAbs > 0.1f, "0.5x tempo: output not silent");
    }

// Process: fast tempo produces less output
    std::printf("\n--- Fast tempo (2.0x) ---\n");
    {
        TimeStretch ts;
        ts.init(1, 44100);
        ts.setTempo(2.0f);

        const int srcFrames = 16000;
        std::vector<float> src(srcFrames);
        for (int i = 0; i < srcFrames; ++i)
            src[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);

        int readPos = 0;
        auto readCb = [&](float* buf, uint32_t maxFrames) -> uint32_t {
            uint32_t avail = (uint32_t)(srcFrames - readPos);
            uint32_t toRead = std::min(maxFrames, avail);
            if (toRead > 0) {
                std::memcpy(buf, src.data() + readPos, toRead * sizeof(float));
                readPos += (int)toRead;
            }
            return toRead;
        };

        const int outBufFrames = 16000;
        std::vector<float> out(outBufFrames, 0.0f);
        uint32_t got = ts.process(out.data(), outBufFrames, readCb);

        CHECK(got > 0, "2.0x tempo: produced output");
        CHECK(got < (uint32_t)srcFrames, "2.0x tempo: output < input (consumed faster)");

        bool allFinite = true;
        for (uint32_t i = 0; i < got; ++i)
            if (std::isnan(out[i]) || std::isinf(out[i])) { allFinite = false; break; }
        CHECK(allFinite, "2.0x tempo: all output samples finite");
    }

// Process: stereo
    std::printf("\n--- Stereo processing ---\n");
    {
        TimeStretch ts;
        ts.init(2, 44100);
        ts.setTempo(1.5f);

        const int srcFrames = 8000;
        std::vector<float> src(srcFrames * 2);
        for (int i = 0; i < srcFrames; ++i) {
            float s = std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);
            src[2 * i + 0] = s;         // L
            src[2 * i + 1] = s * 0.5f;  // R (half amplitude)
        }

        int readPos = 0;
        auto readCb = [&](float* buf, uint32_t maxFrames) -> uint32_t {
            uint32_t avail = (uint32_t)(srcFrames - readPos);
            uint32_t toRead = std::min(maxFrames, avail);
            if (toRead > 0) {
                std::memcpy(buf, src.data() + readPos * 2, toRead * 2 * sizeof(float));
                readPos += (int)toRead;
            }
            return toRead;
        };

        std::vector<float> out(srcFrames * 2, 0.0f);
        uint32_t got = ts.process(out.data(), srcFrames, readCb);
        CHECK(got > 0, "stereo 1.5x: produced output");

        bool allFinite = true;
        for (uint32_t i = 0; i < got * 2; ++i)
            if (std::isnan(out[i]) || std::isinf(out[i])) { allFinite = false; break; }
        CHECK(allFinite, "stereo 1.5x: all output samples finite");
    }

// Process: EOF callback
    std::printf("\n--- EOF callback ---\n");
    {
        TimeStretch ts;
        ts.init(1, 44100);
        ts.setTempo(0.8f);

        // Source that immediately returns 0 (EOF)
        auto eofCb = [](float*, uint32_t) -> uint32_t { return 0; };

        float out[1024];
        uint32_t got = ts.process(out, 1024, eofCb);
        CHECK(got == 0, "immediate EOF: got 0 frames");
    }

    std::printf("\n=== TimeStretch: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
