#include "ui/AuditionTransportView.h"

namespace aris::ui {

AuditionTransportView::AuditionTransportView(
    const frontend::AuditionStateSnapshot& state,
    frontend::AuditionUiDispatcher& dispatcher) noexcept
    : state_(state),
      dispatcher_(dispatcher) {}

const frontend::AuditionStateSnapshot& AuditionTransportView::state() const noexcept {
    return state_;
}

bool AuditionTransportView::footerPlaybackEnabled() const noexcept {
    return state_.footerPlaybackEnabled;
}

bool AuditionTransportView::volumeEnabled() const noexcept {
    return state_.volumeEnabled;
}

bool AuditionTransportView::pitchEnabled() const noexcept {
    return state_.pitchEnabled;
}

bool AuditionTransportView::reverseEnabled() const noexcept {
    return state_.reverseEnabled;
}

bool AuditionTransportView::loopEnabled() const noexcept {
    return state_.loopEnabled;
}

frontend::AuditionDispatchResult
AuditionTransportView::toggleVisibleFooterPlayback() noexcept {
    return dispatcher_.dispatch(
        frontend::AuditionAction::toggleVisibleFooterPlayback());
}

frontend::AuditionDispatchResult AuditionTransportView::setVolume(float value) noexcept {
    return dispatcher_.dispatch(frontend::AuditionAction::setVolume(value));
}

frontend::AuditionDispatchResult AuditionTransportView::setPitchControlValue(
    float value) noexcept {
    return dispatcher_.dispatch(
        frontend::AuditionAction::setPitchControlValue(value));
}

frontend::AuditionDispatchResult AuditionTransportView::changePitchModeAndReset(
    frontend::AuditionPitchMode mode) noexcept {
    return dispatcher_.dispatch(
        frontend::AuditionAction::changePitchModeAndReset(mode));
}

frontend::AuditionDispatchResult
AuditionTransportView::toggleReversePreservingRegion() noexcept {
    return dispatcher_.dispatch(
        frontend::AuditionAction::toggleReversePreservingRegion());
}

frontend::AuditionDispatchResult AuditionTransportView::setLoopAndRearmRegion(
    bool enabled) noexcept {
    return dispatcher_.dispatch(
        frontend::AuditionAction::setLoopAndRearmRegion(enabled));
}

}  // namespace aris::ui
