// TimeStretch.cpp — WSOLA time stretcher implementation
// See TimeStretch.h for API docs.

#include "TimeStretch.h"
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TimeStretch::TimeStretch() = default;

void TimeStretch::init(uint32_t channels, uint32_t sampleRate) {
    m_channels   = channels ? channels : 2;
    m_sampleRate = sampleRate ? sampleRate : 44100;

    // Scale window to ~46ms regardless of sample rate
    m_windowSize = (uint32_t)(0.046 * m_sampleRate);
    // Round up to nearest power of 2 for FFT-friendly size (not required but cache-friendly)
    uint32_t p = 1;
    while (p < m_windowSize) p <<= 1;
    m_windowSize = p;

    m_hopSize     = m_windowSize / 2;
    m_searchRange = 64;

    // Pre-compute Hann window
    m_hannWindow.resize(m_windowSize);
    for (uint32_t i = 0; i < m_windowSize; ++i) {
        m_hannWindow[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (m_windowSize - 1)));
    }

    reset();
}

void TimeStretch::setTempo(float ratio) {
    if (ratio < 0.25f) ratio = 0.25f;
    if (ratio > 4.0f)  ratio = 4.0f;
    m_tempo = ratio;
}

float TimeStretch::getTempo() const {
    return m_tempo;
}

bool TimeStretch::isBypassed() const {
    return std::abs(m_tempo - 1.0f) < 0.001f;
}

void TimeStretch::reset() {
    // Allocate input buffer: need at least window + 2*search range
    uint32_t inputBufFrames = m_windowSize + 2 * m_searchRange + m_hopSize;
    m_inputBuf.assign((size_t)inputBufFrames * m_channels, 0.0f);
    m_inputFrames = 0;
    m_inputPos    = 0.0;

    m_overlapBuf.assign((size_t)m_hopSize * m_channels, 0.0f);
    m_hasOverlap = false;
}

int TimeStretch::findBestOffset(const float* analysisFrame, const float* synthesisTail) const {
    // Cross-correlation search: find offset in [-searchRange, +searchRange]
    // that maximizes similarity between the tail of previous window and
    // the start of the candidate analysis frame.
    // Only use channel 0 for correlation (cheaper, usually sufficient).

    int bestOffset = 0;
    float bestCorr = -1e30f;
    uint32_t corrLen = m_hopSize < 256 ? m_hopSize : 256; // limit correlation length

    for (int off = -(int)m_searchRange; off <= (int)m_searchRange; ++off) {
        float corr = 0.0f;
        float normA = 0.0f;
        float normB = 0.0f;
        for (uint32_t i = 0; i < corrLen; ++i) {
            float a = synthesisTail[i * m_channels]; // ch0 of overlap tail
            float b = analysisFrame[((uint32_t)((int)i + off + (int)m_searchRange)) * m_channels]; // ch0 of candidate
            corr  += a * b;
            normA += a * a;
            normB += b * b;
        }
        float denom = sqrtf(normA * normB);
        if (denom > 1e-12f) corr /= denom;
        if (corr > bestCorr) {
            bestCorr = corr;
            bestOffset = off;
        }
    }
    return bestOffset;
}

uint32_t TimeStretch::process(float* output, uint32_t requestedFrames, ReadCallback readInput) {
    if (!output || requestedFrames == 0 || !readInput) return 0;

    // Bypass mode: direct passthrough
    if (isBypassed()) {
        return readInput(output, requestedFrames);
    }

    const uint32_t ch = m_channels;
    uint32_t outputWritten = 0;

    while (outputWritten < requestedFrames) {
        // How many output frames we produce per synthesis hop
        uint32_t outputHop = m_hopSize; // each synthesis step produces hopSize frames
        uint32_t remaining = requestedFrames - outputWritten;
        if (remaining < outputHop) {
            // Not enough room for a full hop — read partial from overlap
            // This handles the tail; just produce what we can
            outputHop = remaining;
        }

        // Ensure input buffer has enough data for one analysis window + search range
        uint32_t inputNeeded = m_windowSize + 2 * m_searchRange;
        while (m_inputFrames < inputNeeded) {
            uint32_t toRead = inputNeeded - m_inputFrames;
            uint32_t got = readInput(m_inputBuf.data() + (size_t)m_inputFrames * ch, toRead);
            if (got == 0) {
                // EOF — flush whatever overlap we have
                if (m_hasOverlap && outputWritten < requestedFrames) {
                    uint32_t flushFrames = std::min(m_hopSize, requestedFrames - outputWritten);
                    std::memcpy(output + (size_t)outputWritten * ch,
                                m_overlapBuf.data(),
                                (size_t)flushFrames * ch * sizeof(float));
                    outputWritten += flushFrames;
                    m_hasOverlap = false;
                }
                return outputWritten;
            }
            m_inputFrames += got;
        }

        // Find the analysis position based on tempo ratio
        // For tempo > 1.0, we advance faster through input (skip frames)
        // For tempo < 1.0, we advance slower (repeat frames)
        double inputHop = m_hopSize * (double)m_tempo;
        uint32_t analysisStart = (uint32_t)m_searchRange; // offset by search range

        // Find best overlap offset via cross-correlation
        int bestOff = 0;
        if (m_hasOverlap) {
            bestOff = findBestOffset(m_inputBuf.data() + (size_t)analysisStart * ch,
                                     m_overlapBuf.data());
        }
        uint32_t actualStart = (uint32_t)((int)analysisStart + bestOff);

        // Windowed overlap-add
        float* outPtr = output + (size_t)outputWritten * ch;
        const float* src = m_inputBuf.data() + (size_t)actualStart * ch;

        if (m_hasOverlap) {
            // First half: cross-fade overlap tail with new window start
            for (uint32_t i = 0; i < std::min(m_hopSize, outputHop); ++i) {
                float fadeIn  = m_hannWindow[i]; // rises from 0 to ~1
                float fadeOut = m_hannWindow[m_hopSize + i]; // falls from ~1 to 0
                for (uint32_t c = 0; c < ch; ++c) {
                    outPtr[i * ch + c] = m_overlapBuf[i * ch + c] * fadeOut
                                       + src[i * ch + c] * fadeIn;
                }
            }
        } else {
            // No overlap yet (first window) — just copy with window
            for (uint32_t i = 0; i < std::min(m_hopSize, outputHop); ++i) {
                for (uint32_t c = 0; c < ch; ++c) {
                    outPtr[i * ch + c] = src[i * ch + c];
                }
            }
        }

        // Save the second half of the window as overlap for next iteration
        if (m_hopSize <= m_windowSize) {
            const float* overlapSrc = src + (size_t)m_hopSize * ch;
            for (uint32_t i = 0; i < m_hopSize; ++i) {
                for (uint32_t c = 0; c < ch; ++c) {
                    m_overlapBuf[i * ch + c] = overlapSrc[i * ch + c];
                }
            }
            m_hasOverlap = true;
        }

        outputWritten += std::min(m_hopSize, outputHop);

        // Advance input buffer: consume inputHop frames worth of data
        uint32_t consumed = (uint32_t)(inputHop + 0.5);
        consumed = std::min(consumed, m_inputFrames);
        if (consumed > 0 && consumed < m_inputFrames) {
            std::memmove(m_inputBuf.data(),
                         m_inputBuf.data() + (size_t)consumed * ch,
                         (size_t)(m_inputFrames - consumed) * ch * sizeof(float));
        }
        m_inputFrames -= consumed;
        m_inputPos += inputHop;
    }

    return outputWritten;
}
