#pragma once
// TimeStretch.h — WSOLA (Waveform Similarity Overlap-Add) time stretcher
// Changes playback speed without affecting pitch.
// Designed for real-time callback-driven audio (miniaudio integration).

#include <cstdint>
#include <functional>
#include <vector>

// @stage7-contract
// API contract: TimeStretch owns its DSP state and is initialized before
// process calls. Tempo updates use the existing atomic/callback contract;
// callers serialize lifecycle changes and do not retain internal buffers.
class TimeStretch {
public:
    TimeStretch();

    // Must be called before process(). Safe to call multiple times (re-inits).
    void init(uint32_t channels, uint32_t sampleRate);

    // Set tempo ratio: 0.5 = half speed, 1.0 = normal, 2.0 = double speed.
    // Thread-safe (atomic store). Takes effect on next process() call.
    void setTempo(float ratio);
    float getTempo() const;

    // Returns true if tempo == 1.0 (bypass mode, caller can skip processing).
    bool isBypassed() const;

    // Process: pulls input via readInput callback, writes stretched output.
    // readInput(buf, maxFrames) should fill buf with interleaved samples and
    // return actual frames read (0 = EOF).
    // Returns number of output frames written to 'output'.
    using ReadCallback = std::function<uint32_t(float* buf, uint32_t maxFrames)>;
    uint32_t process(float* output, uint32_t requestedFrames, ReadCallback readInput);

    // Clear all internal buffers (call on file change / seek).
    void reset();

private:
    // Find best overlap offset within ±m_searchRange using cross-correlation
    int findBestOffset(const float* analysisFrame, const float* synthesisTail) const;

    float m_tempo      = 1.0f;
    uint32_t m_channels   = 2;
    uint32_t m_sampleRate = 44100;

    // WSOLA parameters (in frames, not samples)
    uint32_t m_windowSize  = 2048;
    uint32_t m_hopSize     = 1024;
    uint32_t m_searchRange = 64;

    // Pre-computed Hann window (m_windowSize entries)
    std::vector<float> m_hannWindow;

    // Internal ring buffer for input data (enough for window + search range)
    std::vector<float> m_inputBuf;    // [frames * channels]
    uint32_t m_inputFrames = 0;       // valid frames currently in buffer
    double m_inputPos      = 0.0;     // fractional read position in input stream

    // Overlap tail from previous synthesis window
    std::vector<float> m_overlapBuf;  // [m_hopSize * channels]
    bool m_hasOverlap      = false;
};
