#pragma once

#include "frontend/AuditionFrontendContract.h"

namespace aris::frontend {

class AuditionNativeTransportEffects {
public:
    virtual ~AuditionNativeTransportEffects() = default;

    virtual bool isPlaying() const = 0;
    virtual bool isLoaded() const = 0;
    virtual bool isReversed() const = 0;
    virtual bool regionLoopEnabled() const = 0;
    virtual float regionIn() const = 0;
    virtual float regionOut() const = 0;
    virtual float regionFadeIn() const = 0;
    virtual float regionFadeOut() const = 0;
    virtual float playbackPosition() const = 0;
    virtual float currentFrequencyShift() const = 0;
    virtual AuditionPitchMode pitchMode() const = 0;

    virtual void pause() = 0;
    virtual void play() = 0;
    virtual void playRegion(bool loopEnabled, float fadeIn, float fadeOut) = 0;
    virtual void setPitch(float value) = 0;
    virtual void setFrequencyShift(float value) = 0;
    virtual void rememberFrequencyShift(float value) = 0;
    virtual void setConfiguredPitchMode(AuditionPitchMode mode) = 0;
    virtual void setEnginePitchMode(AuditionPitchMode mode) = 0;
    virtual bool setReverse(bool enabled) = 0;
    virtual void clearRegion() = 0;
    virtual void setRegionIn(float value) = 0;
    virtual void setRegionOut(float value) = 0;
    virtual void seek(float value) = 0;
    virtual void setRegionLoopEnabled(bool enabled) = 0;
    // Disarm an armed region loop immediately without pausing or seeking: the
    // current pass finishes and stops naturally. Used when the user switches
    // loop off while a full sound (no boxed selection) is looping.
    virtual void stopLoopKeepCurrentPass() = 0;
};

class AuditionNativeIntentWorkflow final {
public:
    static void toggleVisibleFooterPlayback(AuditionNativeTransportEffects& effects);
    static void setPitchControlValue(
        AuditionNativeTransportEffects& effects,
        float value);
    static void changePitchModeAndReset(
        AuditionNativeTransportEffects& effects,
        AuditionPitchMode mode);
    static void toggleReversePreservingRegion(AuditionNativeTransportEffects& effects);
    static void setLoopAndRearmRegion(
        AuditionNativeTransportEffects& effects,
        bool enabled);
};

}  // namespace aris::frontend
