#include "frontend/AppStateAuditionFrontendAdapter.h"

#include "AppState.h"

#include <algorithm>

namespace aris::frontend {
namespace {

AuditionPitchMode ToFrontendPitchMode(PitchMode mode) noexcept {
    return mode == PitchMode::FreqShift
        ? AuditionPitchMode::FrequencyShift
        : AuditionPitchMode::Semitone;
}

PitchMode ToEnginePitchMode(AuditionPitchMode mode) noexcept {
    return mode == AuditionPitchMode::FrequencyShift
        ? PitchMode::FreqShift
        : PitchMode::Semitone;
}

bool HasVisualRegion(float inPoint, float outPoint) noexcept {
    return inPoint < outPoint && (inPoint > 0.0f || outPoint < 1.0f);
}

}  // namespace

AppStateAuditionFrontendAdapter::AppStateAuditionFrontendAdapter(
    AppState& state) noexcept
    : state_(state) {}

void AppStateAuditionFrontendAdapter::captureSnapshot(
    AuditionStateSnapshot& output) const {
    const float inPoint = regionIn();
    const float outPoint = regionOut();
    const bool loaded = isLoaded();
    const AuditionPitchMode mode = pitchMode();

    output.audioLoaded = loaded;
    output.playing = isPlaying();
    output.footerPlaybackEnabled = loaded;
    output.volumeEnabled = true;
    output.pitchEnabled = true;
    output.reverseEnabled = true;
    output.loopEnabled = loaded;
    output.reversed = isReversed();
    output.regionLoopEnabled = regionLoopEnabled();
    output.hasRegion = HasVisualRegion(inPoint, outPoint);
    output.volume = state_.audioEngine.getVolume();
    output.volumeMinimum = 0.0f;
    output.volumeMaximum = 1.2f;
    output.pitchMode = mode;
    output.pitchControlValue = mode == AuditionPitchMode::FrequencyShift
        ? state_.audioEngine.getFreqShift()
        : state_.audioEngine.getPitch();
    const float pitchRange = std::max(0.0f, state_.settings.pitchRange);
    output.pitchMinimum = -pitchRange;
    output.pitchMaximum = pitchRange;
    output.frequencyShiftMinimum = -5000.0f;
    output.frequencyShiftMaximum = 5000.0f;
    output.regionIn = inPoint;
    output.regionOut = outPoint;
    output.regionFadeIn = regionFadeIn();
    output.regionFadeOut = regionFadeOut();
}

void AppStateAuditionFrontendAdapter::toggleVisibleFooterPlayback() {
    AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback(*this);
}

void AppStateAuditionFrontendAdapter::setVolume(float value) {
    state_.audioEngine.setVolume(value);
}

void AppStateAuditionFrontendAdapter::setPitchControlValue(float value) {
    AuditionNativeIntentWorkflow::setPitchControlValue(*this, value);
}

void AppStateAuditionFrontendAdapter::changePitchModeAndReset(
    AuditionPitchMode mode) {
    AuditionNativeIntentWorkflow::changePitchModeAndReset(*this, mode);
}

void AppStateAuditionFrontendAdapter::toggleReversePreservingRegion() {
    AuditionNativeIntentWorkflow::toggleReversePreservingRegion(*this);
}

void AppStateAuditionFrontendAdapter::setLoopAndRearmRegion(bool enabled) {
    AuditionNativeIntentWorkflow::setLoopAndRearmRegion(*this, enabled);
}

bool AppStateAuditionFrontendAdapter::isPlaying() const {
    return state_.audioEngine.isPlaying();
}

bool AppStateAuditionFrontendAdapter::isLoaded() const {
    return state_.audioEngine.isLoaded();
}

bool AppStateAuditionFrontendAdapter::isReversed() const {
    return state_.audioEngine.isReversed();
}

bool AppStateAuditionFrontendAdapter::regionLoopEnabled() const {
    return state_.uiState.regionLoopEnabled;
}

float AppStateAuditionFrontendAdapter::regionIn() const {
    return state_.audioEngine.getInPoint();
}

float AppStateAuditionFrontendAdapter::regionOut() const {
    return state_.audioEngine.getOutPoint();
}

float AppStateAuditionFrontendAdapter::regionFadeIn() const {
    return state_.uiState.regionFadeInNorm;
}

float AppStateAuditionFrontendAdapter::regionFadeOut() const {
    return state_.uiState.regionFadeOutNorm;
}

float AppStateAuditionFrontendAdapter::playbackPosition() const {
    return state_.audioEngine.getPlaybackPosition();
}

float AppStateAuditionFrontendAdapter::currentFrequencyShift() const {
    return state_.audioEngine.getFreqShift();
}

AuditionPitchMode AppStateAuditionFrontendAdapter::pitchMode() const {
    return ToFrontendPitchMode(state_.settings.pitchMode);
}

void AppStateAuditionFrontendAdapter::pause() {
    state_.audioEngine.pause();
}

void AppStateAuditionFrontendAdapter::play() {
    state_.audioEngine.play();
}

void AppStateAuditionFrontendAdapter::playRegion(
    bool loopEnabled,
    float fadeIn,
    float fadeOut) {
    state_.audioEngine.playRegion(loopEnabled, fadeIn, fadeOut);
}

void AppStateAuditionFrontendAdapter::setPitch(float value) {
    state_.audioEngine.setPitch(value);
}

void AppStateAuditionFrontendAdapter::setFrequencyShift(float value) {
    state_.audioEngine.setFreqShift(value);
}

void AppStateAuditionFrontendAdapter::rememberFrequencyShift(float value) {
    state_.settings.freqShiftHz = value;
}

void AppStateAuditionFrontendAdapter::setConfiguredPitchMode(
    AuditionPitchMode mode) {
    state_.settings.pitchMode = ToEnginePitchMode(mode);
}

void AppStateAuditionFrontendAdapter::setEnginePitchMode(
    AuditionPitchMode mode) {
    state_.audioEngine.setPitchMode(ToEnginePitchMode(mode));
}

bool AppStateAuditionFrontendAdapter::setReverse(bool enabled) {
    return state_.audioEngine.setReverse(enabled);
}

void AppStateAuditionFrontendAdapter::clearRegion() {
    state_.audioEngine.clearInOutPoints();
}

void AppStateAuditionFrontendAdapter::setRegionIn(float value) {
    state_.audioEngine.setInPoint(value);
}

void AppStateAuditionFrontendAdapter::setRegionOut(float value) {
    state_.audioEngine.setOutPoint(value);
}

void AppStateAuditionFrontendAdapter::seek(float value) {
    state_.audioEngine.seekToPosition(value);
}

void AppStateAuditionFrontendAdapter::setRegionLoopEnabled(bool enabled) {
    state_.uiState.regionLoopEnabled = enabled;
    state_.audioEngine.setUserLoopEnabled(enabled);
}

void AppStateAuditionFrontendAdapter::stopLoopKeepCurrentPass() {
    state_.audioEngine.stopLoopKeepCurrentPass();
}

}  // namespace aris::frontend
