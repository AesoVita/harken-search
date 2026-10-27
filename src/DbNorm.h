#pragma once

#include <cmath>

namespace aura {

// @stage7-contract
// API contract: AmplitudeToDbNorm is a pure synchronous scalar mapping. It
// retains no state, performs no IO, and clamps tiny/NaN/out-of-range inputs to
// the established normalized noise-floor/top behavior.

// Map linear amplitude [0, +inf) to a normalized dB position in [0, 1].
// - x <= ~0 (including very small) -> 0.0 (noise floor)
// - x >= 1.0 (0 dBFS or louder)     -> 1.0 (top)
// - otherwise: (20*log10(x) - floorDb) / (-floorDb), clamped to [0,1]
// floorDb is negative (e.g. -60.0f). Used for Wwise-style RMS silhouette
// rendering where the lane bottom is the noise floor and the lane top is 0 dB.
inline float AmplitudeToDbNorm(float x, float floorDb = -60.0f) {
    if (!(x > 1e-6f)) return 0.0f;           // catches NaN and tiny values
    float db = 20.0f * std::log10(x);
    if (db >= 0.0f) return 1.0f;
    if (db <= floorDb) return 0.0f;
    float n = (db - floorDb) / (-floorDb);   // floorDb = -60 -> (db + 60) / 60
    return n;
}

}
