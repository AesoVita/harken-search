#include "frontend/AuditionNativeIntentWorkflow.h"

namespace aris::frontend {
namespace {

bool HasVisualRegion(float inPoint, float outPoint) noexcept {
    return inPoint < outPoint && (inPoint > 0.0f || outPoint < 1.0f);
}

}  // namespace

void AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(
    AuditionNativeTransportEffects& effects) {
    if (effects.isPlaying()) {
        effects.pause();
        return;
    }

    const float inPoint = effects.regionIn();
    const float outPoint = effects.regionOut();
    const bool hasRegion = HasVisualRegion(inPoint, outPoint);
    const bool hasFade = effects.regionFadeIn() > 1.0e-6f ||
                         effects.regionFadeOut() > 1.0e-6f;
    if ((hasRegion && hasFade) || effects.regionLoopEnabled()) {
        effects.playRegion(
            effects.regionLoopEnabled(),
            effects.regionFadeIn(),
            effects.regionFadeOut());
        return;
    }
    effects.play();
}

void AuditionNativeIntentWorkflow::setPitchControlValue(
    AuditionNativeTransportEffects& effects,
    float value) {
    if (effects.pitchMode() == AuditionPitchMode::FrequencyShift) {
        effects.setFrequencyShift(value);
        effects.rememberFrequencyShift(effects.currentFrequencyShift());
        return;
    }
    effects.setPitch(value);
}

void AuditionNativeIntentWorkflow::changePitchModeAndReset(
    AuditionNativeTransportEffects& effects,
    AuditionPitchMode mode) {
    if (effects.pitchMode() == mode) {
        return;
    }
    effects.setPitch(0.0f);
    effects.setFrequencyShift(0.0f);
    effects.rememberFrequencyShift(0.0f);
    effects.setConfiguredPitchMode(mode);
    effects.setEnginePitchMode(mode);
}

void AuditionNativeIntentWorkflow::toggleReversePreservingRegion(
    AuditionNativeTransportEffects& effects) {
    const bool wantedReversed = !effects.isReversed();
    if (!effects.setReverse(wantedReversed)) {
        return;
    }
    if (!effects.isLoaded()) {
        return;
    }

    const float inPoint = effects.regionIn();
    const float outPoint = effects.regionOut();
    if (HasVisualRegion(inPoint, outPoint)) {
        const float newIn = 1.0f - outPoint;
        const float newOut = 1.0f - inPoint;
        effects.clearRegion();
        effects.setRegionIn(newIn);
        effects.setRegionOut(newOut);
        effects.seek(newIn);
        if (!effects.isPlaying()) {
            effects.play();
        }
        return;
    }

    effects.seek(1.0f - effects.playbackPosition());
    if (!effects.isPlaying()) {
        effects.play();
    }
}

void AuditionNativeIntentWorkflow::setLoopAndRearmRegion(
    AuditionNativeTransportEffects& effects,
    bool enabled) {
    effects.setRegionLoopEnabled(enabled);
    if (!enabled) {
        // Closing the loop toggle must cancel the loop immediately. A boxed
        // pass re-arms once through the existing region path: playRegion(false)
        // keeps the current selection playing to its end and stops without
        // re-seeking into a loop. A full-sound (unselected) pass must not
        // restart from the top, so disarm in place instead.
        if (effects.isLoaded()) {
            if (HasVisualRegion(effects.regionIn(), effects.regionOut())) {
                effects.playRegion(
                    false, effects.regionFadeIn(), effects.regionFadeOut());
            } else {
                effects.stopLoopKeepCurrentPass();
            }
        }
        return;
    }
    if (!effects.isLoaded()) {
        return;
    }
    if (!HasVisualRegion(effects.regionIn(), effects.regionOut())) {
        effects.setRegionIn(0.0f);
        effects.setRegionOut(1.0f);
    }
    effects.playRegion(true, effects.regionFadeIn(), effects.regionFadeOut());
}

}  // namespace aris::frontend
