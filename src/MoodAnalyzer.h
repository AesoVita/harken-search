#pragma once
// MoodAnalyzer.h -- Rule-based mood analysis for F-033 Music Mode
// Computes mood_valence [-1,1] and mood_arousal [-1,1] from audio features.
// P1: rule-based; P3: swap for ONNX model without changing interface.

#include <string>

// @stage7-contract
// API contract: mood analysis is a pure value transform over caller-provided
// audio features. It returns bounded valence/arousal values and `ok=false` for
// invalid input without retaining the input object.
struct MoodInput {
    float bpm          = 0.0f;   // beats per minute (0 = unknown)
    float bpmConfidence= 0.0f;
    bool  isMinorKey   = false;  // true = minor key (tends negative valence)
    float keyConfidence= 0.0f;
    float lufs         = -99.0f; // integrated loudness (negative, typical -30..-6)
    float spectralCentroid = 0.0f; // Hz, normalised 0..1 by caller or raw Hz
    float spectralFlux     = 0.0f; // frame-to-frame change, 0..1 normalised
    float zeroCrossingRate = 0.0f; // 0..1 normalised
};

struct MoodResult {
    float valence = 0.0f;   // -1 (negative/sad) to +1 (positive/happy)
    float arousal = 0.0f;   // -1 (calm/slow)    to +1 (energetic/intense)
    bool  ok      = false;
};

class MoodAnalyzer {
public:
    // Compute mood from pre-extracted features.
    static MoodResult analyze(const MoodInput& in);
};
