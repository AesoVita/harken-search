#pragma once

#include <algorithm>

namespace MiniWaveRender {

// @stage7-contract
// API contract: mini-wave rendering helpers are pure synchronous geometry
// calculations. They copy scalar inputs, return owned spans/coverage values,
// clamp coverage to the established range, and touch no renderer state.

struct Span {
    float top;
    float bottom;
};

inline Span EnvelopeSpan(float minV, float maxV, float center, float amplitude) {
    const float scale = amplitude * 0.90f;
    Span s;
    s.top = center - std::max(0.0f, maxV) * scale;
    s.bottom = center - std::min(0.0f, minV) * scale;
    if (s.bottom < s.top) {
        std::swap(s.bottom, s.top);
    }
    return s;
}

inline float VerticalCoverage(int rowY, float spanTop, float spanBottom) {
    const float rowStart = (float)rowY;
    const float rowEnd = rowStart + 1.0f;
    const float lo = std::max(rowStart, spanTop);
    const float hi = std::min(rowEnd, spanBottom);
    return std::clamp(hi - lo, 0.0f, 1.0f);
}

}  // namespace MiniWaveRender
