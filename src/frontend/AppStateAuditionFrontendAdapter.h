#pragma once

#include "frontend/AuditionFrontendContract.h"
#include "frontend/AuditionNativeIntentWorkflow.h"

struct AppState;

namespace aris::frontend {

class AppStateAuditionFrontendAdapter final
    : public AuditionApplicationPort,
      private AuditionNativeTransportEffects {
public:
    explicit AppStateAuditionFrontendAdapter(AppState& state) noexcept;

    void captureSnapshot(AuditionStateSnapshot& output) const override;
    void toggleVisibleFooterPlayback() override;
    void setVolume(float value) override;
    void setPitchControlValue(float value) override;
    void changePitchModeAndReset(AuditionPitchMode mode) override;
    void toggleReversePreservingRegion() override;
    void setLoopAndRearmRegion(bool enabled) override;

private:
    bool isPlaying() const override;
    bool isLoaded() const override;
    bool isReversed() const override;
    bool regionLoopEnabled() const override;
    float regionIn() const override;
    float regionOut() const override;
    float regionFadeIn() const override;
    float regionFadeOut() const override;
    float playbackPosition() const override;
    float currentFrequencyShift() const override;
    AuditionPitchMode pitchMode() const override;

    void pause() override;
    void play() override;
    void playRegion(bool loopEnabled, float fadeIn, float fadeOut) override;
    void setPitch(float value) override;
    void setFrequencyShift(float value) override;
    void rememberFrequencyShift(float value) override;
    void setConfiguredPitchMode(AuditionPitchMode mode) override;
    void setEnginePitchMode(AuditionPitchMode mode) override;
    bool setReverse(bool enabled) override;
    void clearRegion() override;
    void setRegionIn(float value) override;
    void setRegionOut(float value) override;
    void seek(float value) override;
    void setRegionLoopEnabled(bool enabled) override;
    void stopLoopKeepCurrentPass() override;

    AppState& state_;
};

}  // namespace aris::frontend
