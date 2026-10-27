#pragma once
#include <string>
#include <cstdint>

struct BpmResult {
    float bpm        = 0.0f;
    float confidence = 0.0f;  // 0.0 ~ 1.0; mapped to colour brightness in UI
    float beatOffset = 0.0f;  // seconds from analysis start to first detected beat
    bool  ok         = false;
};

// @stage7-contract
// API contract: detection reads caller-provided PCM without retaining it and
// returns a value result with `ok=false` for invalid/unsupported input. File
// detection consumes UTF-8 paths and preserves the documented range/cap limits.
class BpmDetector {
public:
    // Analyse a pre-decoded PCM buffer.
    // pcm       : interleaved float samples
    // numFrames : number of sample frames (total samples / channels)
    // channels  : number of channels (downmixed to mono internally)
    // sampleRate: Hz
    static BpmResult detect(const float* pcm, int numFrames,
                            int channels, uint32_t sampleRate);

    // Open filePath, decode [startSec, endSec) (endSec=-1 = EOF),
    // cap at maxSec seconds, then call detect().
    static BpmResult detectFile(const std::string& filePath,
                                double startSec = 0.0,
                                double endSec   = -1.0,
                                double maxSec   = 90.0);
};
