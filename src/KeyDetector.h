#pragma once
// KeyDetector.h -- Musical key detection for F-033 Music Mode
// Algorithm: Chromagram (FFT-based) + Krumhansl-Schmuckler key profile matching
// Output: key name (e.g. "Am"), Camelot wheel code (e.g. "8A"), confidence [0,1]

#include <string>
#include <cstdint>

struct KeyResult {
    std::string key;       // e.g. "Am", "C", "F#", "Eb"
    std::string camelot;   // e.g. "8A", "8B"
    float confidence = 0.0f;  // 0.0~1.0; <0.5 displayed grey in UI
    bool  isMinor    = false;
    bool  ok         = false;
};

// @stage7-contract
// API contract: detection reads caller PCM or a UTF-8 file path without
// retaining input and returns an owned KeyResult. Invalid/unsupported input
// yields `ok=false`; key/camelot/confidence domains remain backward compatible.
class KeyDetector {
public:
    // Analyse pre-decoded PCM. Channels downmixed to mono internally.
    static KeyResult detect(const float* pcm, int numFrames,
                            int channels, uint32_t sampleRate);

    // Decode filePath [startSec, endSec) capped at maxSec, then detect.
    static KeyResult detectFile(const std::string& filePath,
                                double startSec = 0.0,
                                double endSec   = -1.0,
                                double maxSec   = 120.0);
};
