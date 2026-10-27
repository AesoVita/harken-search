#pragma once

#include <algorithm>
#include <cmath>

// @stage7-contract
// API contract: waveform-region helpers are pure synchronous math over copied
// scalars and caller-owned fade structs. They retain no references, perform no
// rendering/IO, and preserve existing clamping/eligibility booleans for UI and
// playback consumers.
// Pure math helpers for waveform region editor (no ImGui / AudioEngine).

enum class HarkenSearchFadeSide {
    FadeIn,
    FadeOut,
};

enum class HarkenSearchRegionEdgeTarget {
    None,
    In,
    Out,
};

struct HarkenSearchRegionFadeNorms {
    float fadeInNorm = 0.0f;
    float fadeOutNorm = 0.0f;
};

struct HarkenSearchTrimRefineWindow {
    float zoom = 1.0f;
    float center = 0.5f;
};

inline HarkenSearchRegionFadeNorms ClampRegionFades(
    const HarkenSearchRegionFadeNorms& fades,
    HarkenSearchFadeSide editedSide,
    float regionNorm)
{
    HarkenSearchRegionFadeNorms out = fades;
    float cap = std::max(0.0f, regionNorm);

    out.fadeInNorm = std::max(0.0f, out.fadeInNorm);
    out.fadeOutNorm = std::max(0.0f, out.fadeOutNorm);

    if (out.fadeInNorm + out.fadeOutNorm <= cap) {
        return out;
    }

    if (editedSide == HarkenSearchFadeSide::FadeIn) {
        if (out.fadeInNorm >= cap) {
            out.fadeInNorm = cap;
            out.fadeOutNorm = 0.0f;
        } else {
            out.fadeOutNorm = std::min(out.fadeOutNorm, cap - out.fadeInNorm);
        }
    } else {
        if (out.fadeOutNorm >= cap) {
            out.fadeOutNorm = cap;
            out.fadeInNorm = 0.0f;
        } else {
            out.fadeInNorm = std::min(out.fadeInNorm, cap - out.fadeOutNorm);
        }
    }

    return out;
}

// Target visible span = kSpanMul * regionWidth (normalized to file), capped to full file.
// zoom is "magnification": visible fraction of file ~= 1/zoom (zoom >= 1 shows a narrower slice).
// anchorEdge: which edge is being dragged — center is biased toward the dragged edge.
inline HarkenSearchTrimRefineWindow ComputeTrimRefineWindow(
    float inPoint,
    float outPoint,
    const HarkenSearchTrimRefineWindow& current,
    HarkenSearchRegionEdgeTarget anchorEdge = HarkenSearchRegionEdgeTarget::None)
{
    constexpr float kSpanMul = 4.0f;
    constexpr float kMinVisibleSpan = 0.10f;
    constexpr float kMaxZoom = 4.0f;

    const float regionW = std::max(0.0f, outPoint - inPoint);
    float visibleSpan = kSpanMul * regionW;
    visibleSpan = std::max(visibleSpan, kMinVisibleSpan);
    visibleSpan = std::min(visibleSpan, 1.0f);

    float targetZoom = 1.0f / visibleSpan;
    targetZoom = std::min(targetZoom, kMaxZoom);

    float center;
    if (anchorEdge == HarkenSearchRegionEdgeTarget::In)
        center = inPoint + regionW * 0.3f;
    else if (anchorEdge == HarkenSearchRegionEdgeTarget::Out)
        center = outPoint - regionW * 0.3f;
    else
        center = (inPoint + outPoint) * 0.5f;
    center = std::clamp(center, 0.0f, 1.0f);

    if (current.zoom >= targetZoom) {
        return current;
    }

    return HarkenSearchTrimRefineWindow{targetZoom, center};
}

inline bool ShouldExitTrimRefineForGlobalView(float zoom)
{
    constexpr float kEpsilon = 1e-4f;
    return zoom <= 1.0f + kEpsilon;
}

inline HarkenSearchRegionEdgeTarget PickRegionEdgeTarget(
    bool nearInEdge,
    bool nearOutEdge,
    float mouseX,
    float inPixelX,
    float outPixelX)
{
    if (nearInEdge && nearOutEdge) {
        return (std::fabs(mouseX - inPixelX) <= std::fabs(mouseX - outPixelX))
            ? HarkenSearchRegionEdgeTarget::In
            : HarkenSearchRegionEdgeTarget::Out;
    }
    if (nearInEdge) return HarkenSearchRegionEdgeTarget::In;
    if (nearOutEdge) return HarkenSearchRegionEdgeTarget::Out;
    return HarkenSearchRegionEdgeTarget::None;
}

inline float ComputeRegionFadeGain(
    float fileNormPos,
    float inPoint,
    float outPoint,
    float fadeInNorm,
    float fadeOutNorm)
{
    float rIn = std::min(inPoint, outPoint);
    float rOut = std::max(inPoint, outPoint);
    float regionLen = rOut - rIn;
    if (regionLen <= 1e-8f) {
        return 1.0f;
    }

    if (fileNormPos < rIn || fileNormPos > rOut) {
        return 1.0f;
    }

    fadeInNorm = std::clamp(fadeInNorm, 0.0f, 1.0f);
    fadeOutNorm = std::clamp(fadeOutNorm, 0.0f, 1.0f);
    HarkenSearchRegionFadeNorms fades = ClampRegionFades({fadeInNorm, fadeOutNorm}, HarkenSearchFadeSide::FadeIn, 1.0f);
    fades = ClampRegionFades(fades, HarkenSearchFadeSide::FadeOut, 1.0f);

    const float rel = std::clamp((fileNormPos - rIn) / regionLen, 0.0f, 1.0f);
    auto smoothStep01 = [](float t) -> float {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    float gain = 1.0f;
    if (fades.fadeInNorm > 1e-6f && rel < fades.fadeInNorm) {
        gain = std::min(gain, smoothStep01(rel / fades.fadeInNorm));
    }
    if (fades.fadeOutNorm > 1e-6f && rel > 1.0f - fades.fadeOutNorm) {
        const float tailT = (1.0f - rel) / fades.fadeOutNorm;
        gain = std::min(gain, smoothStep01(tailT));
    }
    return std::clamp(gain, 0.0f, 1.0f);
}

inline HarkenSearchRegionFadeNorms ComputeAnchoredFadeDrag(
    float mouseDownNormPos,
    float mouseNormPos,
    float inPoint,
    float outPoint,
    const HarkenSearchRegionFadeNorms& startFades,
    HarkenSearchFadeSide editedSide)
{
    const float rIn = std::min(inPoint, outPoint);
    const float rOut = std::max(inPoint, outPoint);
    const float regionLen = rOut - rIn;
    if (regionLen <= 1e-8f) {
        return {};
    }

    HarkenSearchRegionFadeNorms fades = startFades;
    fades.fadeInNorm = std::clamp(fades.fadeInNorm, 0.0f, 1.0f);
    fades.fadeOutNorm = std::clamp(fades.fadeOutNorm, 0.0f, 1.0f);
    fades = ClampRegionFades(fades, HarkenSearchFadeSide::FadeIn, 1.0f);
    fades = ClampRegionFades(fades, HarkenSearchFadeSide::FadeOut, 1.0f);

    const float deltaNorm = mouseNormPos - mouseDownNormPos;
    if (editedSide == HarkenSearchFadeSide::FadeIn) {
        const float startHandlePos = rIn + fades.fadeInNorm * regionLen;
        const float handlePos = std::clamp(startHandlePos + deltaNorm, rIn, rOut);
        fades.fadeInNorm = std::clamp((handlePos - rIn) / regionLen, 0.0f, 1.0f);
        fades = ClampRegionFades(fades, HarkenSearchFadeSide::FadeIn, 1.0f);
    } else {
        const float startHandlePos = rOut - fades.fadeOutNorm * regionLen;
        const float handlePos = std::clamp(startHandlePos + deltaNorm, rIn, rOut);
        fades.fadeOutNorm = std::clamp((rOut - handlePos) / regionLen, 0.0f, 1.0f);
        fades = ClampRegionFades(fades, HarkenSearchFadeSide::FadeOut, 1.0f);
    }

    return fades;
}

inline bool IsFadeHandleCrowded(
    float edgePixelX,
    float fadePixelX,
    float proximityPx)
{
    return std::fabs(edgePixelX - fadePixelX) < proximityPx;
}

inline bool ShouldAllowFadeHandleEdit(
    bool fadeModifierDown,
    bool currentlyDraggingFade,
    float edgePixelX,
    float fadePixelX,
    float proximityPx)
{
    (void)fadeModifierDown; (void)edgePixelX; (void)fadePixelX; (void)proximityPx;
    // Always allow fade editing; ResolveFadeInHitVsInEdge / ResolveFadeOutHitVsOutEdge
    // handle priority disambiguation when fade and edge overlap.
    (void)currentlyDraggingFade;
    return true;
}

inline bool ShouldStartFadeDragFromEdgeBand(
    float mouseY,
    float waveformTopY,
    float waveformHeight)
{
    if (waveformHeight <= 0.0f) {
        return false;
    }
    return mouseY < waveformTopY + waveformHeight * 0.5f;
}

inline bool ShouldCommitRegionDragPlayback(
    bool isDraggingRegion,
    bool mouseDown,
    bool mouseReleased)
{
    return isDraggingRegion && (mouseReleased || !mouseDown);
}

inline int ComputeWaveformTimeLabelCount(
    float widthPx,
    float minLabelSpacingPx,
    bool zoomCleanMode)
{
    const int preferred = zoomCleanMode ? 4 : 10;
    if (widthPx <= 1.0f || minLabelSpacingPx <= 1.0f) {
        return 2;
    }
    const int maxSegments = std::max(2, (int)std::floor(widthPx / minLabelSpacingPx));
    return std::clamp(maxSegments, 2, preferred);
}
