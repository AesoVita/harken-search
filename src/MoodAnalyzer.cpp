#include "MoodAnalyzer.h"
#include <cmath>
#include <algorithm>

// Rule-based mood mapping as per F-033 Phase 1-3 design doc.
// Each feature contributes a weighted delta to valence or arousal.
// All weights are tunable; values chosen from musicology literature.

MoodResult MoodAnalyzer::analyze(const MoodInput& in) {
    MoodResult result;

    float valence = 0.0f;
    float arousal = 0.0f;
    float totalW  = 0.0f;  // denominator for weighted valence
    float totalWA = 0.0f;  // denominator for weighted arousal

    // --- Valence contributions ---

    // Key mode: major key -> positive valence, minor -> negative
    if (in.keyConfidence > 0.2f) {
        float w = in.keyConfidence;
        valence += (in.isMinorKey ? -1.0f : +1.0f) * w * 0.6f;
        totalW  += w * 0.6f;
    }

    // Spectral centroid: brighter sound (higher centroid) -> slightly more positive
    if (in.spectralCentroid > 0.0f) {
        // Expect normalised 0..1; map 0.5 to 0, scale by 0.3
        float norm = std::clamp(in.spectralCentroid, 0.0f, 1.0f);
        valence += (norm - 0.5f) * 0.6f;
        totalW  += 0.3f;
    }

    // --- Arousal contributions ---

    // BPM: 60=very slow, 120=moderate, 180=fast
    if (in.bpm > 0.0f && in.bpmConfidence > 0.2f) {
        float w   = in.bpmConfidence;
        float norm = std::clamp((in.bpm - 60.0f) / 120.0f, -1.0f, 1.0f);  // 60->-1, 120->0, 180->+1
        arousal += norm * w * 0.5f;
        totalWA += w * 0.5f;
    }

    // LUFS: louder -> more arousal (-6 LUFS very loud, -30 very quiet)
    if (in.lufs > -99.0f) {
        float norm = std::clamp((in.lufs + 30.0f) / 24.0f, -1.0f, 1.0f);  // -30->-1, -6->+1
        arousal += norm * 0.4f;
        totalWA += 0.4f;
    }

    // Spectral centroid also drives arousal (high freq -> intense)
    if (in.spectralCentroid > 0.0f) {
        float norm = std::clamp(in.spectralCentroid, 0.0f, 1.0f);
        arousal += (norm - 0.5f) * 0.6f;
        totalWA += 0.3f;
    }

    // Spectral flux: high variation -> high arousal
    if (in.spectralFlux > 0.0f) {
        float norm = std::clamp(in.spectralFlux, 0.0f, 1.0f);
        arousal += (norm - 0.3f) * 0.7f;  // bias slightly toward calm baseline
        totalWA += 0.35f;
    }

    // Zero crossing rate: high ZCR -> noisy/energetic
    if (in.zeroCrossingRate > 0.0f) {
        float norm = std::clamp(in.zeroCrossingRate, 0.0f, 1.0f);
        arousal += (norm - 0.2f) * 0.4f;
        totalWA += 0.2f;
    }

    // Normalise to [-1, 1]
    result.valence = std::clamp(totalW  > 0.0f ? valence / totalW  : 0.0f, -1.0f, 1.0f);
    result.arousal = std::clamp(totalWA > 0.0f ? arousal / totalWA : 0.0f, -1.0f, 1.0f);
    result.ok = true;
    return result;
}
