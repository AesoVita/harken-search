#pragma once

#include "frontend/AuditionFrontendController.h"

namespace aris::ui {

class AuditionTransportView final {
public:
    AuditionTransportView(
        const frontend::AuditionStateSnapshot& state,
        frontend::AuditionUiDispatcher& dispatcher) noexcept;

    const frontend::AuditionStateSnapshot& state() const noexcept;
    bool footerPlaybackEnabled() const noexcept;
    bool volumeEnabled() const noexcept;
    bool pitchEnabled() const noexcept;
    bool reverseEnabled() const noexcept;
    bool loopEnabled() const noexcept;

    frontend::AuditionDispatchResult toggleVisibleFooterPlayback() noexcept;
    frontend::AuditionDispatchResult setVolume(float value) noexcept;
    frontend::AuditionDispatchResult setPitchControlValue(float value) noexcept;
    frontend::AuditionDispatchResult changePitchModeAndReset(
        frontend::AuditionPitchMode mode) noexcept;
    frontend::AuditionDispatchResult toggleReversePreservingRegion() noexcept;
    frontend::AuditionDispatchResult setLoopAndRearmRegion(bool enabled) noexcept;

private:
    const frontend::AuditionStateSnapshot& state_;
    frontend::AuditionUiDispatcher& dispatcher_;
};

}  // namespace aris::ui
