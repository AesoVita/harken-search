#pragma once

#include <algorithm>

// @stage7-contract
// API contract: mini-waveform geometry helpers are pure synchronous scalar
// calculations. Layouts are returned by value, normalized coordinates are
// clamped to the existing ranges, and no ImGui/render state is accessed.

struct MiniWaveformLayout {
    float minX = 0.0f;
    float minY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float seekXFromNormalized(float normalized) const {
        return minX + std::clamp(normalized, 0.0f, 1.0f) * width;
    }

    float normalizedFromX(float x) const {
        if (width <= 0.0f) return 0.0f;
        return std::clamp((x - minX) / width, 0.0f, 1.0f);
    }
};

inline MiniWaveformLayout ComputeExpandedMiniWaveformLayout(
    float cursorX,
    float cursorY,
    float contentAvailWidth,
    float rowHeight,
    float cellPaddingX)
{
    MiniWaveformLayout layout;
    layout.minX = cursorX - cellPaddingX;
    layout.minY = cursorY;
    layout.width = std::max(50.0f, contentAvailWidth + cellPaddingX * 2.0f);
    layout.height = std::max(12.0f, rowHeight);
    return layout;
}
