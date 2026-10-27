#include "frontend/AuditionFrontendController.h"

#include <algorithm>
#include <cmath>

namespace aris::frontend {
namespace {

constexpr AuditionDispatchResult Applied() noexcept {
    return {AuditionDispatchStatus::Applied, AuditionDiagnosticCode::None};
}

constexpr AuditionDispatchResult Rejected(AuditionDiagnosticCode diagnostic) noexcept {
    return {AuditionDispatchStatus::Rejected, diagnostic};
}

constexpr AuditionDispatchResult Unavailable() noexcept {
    return {
        AuditionDispatchStatus::Unavailable,
        AuditionDiagnosticCode::ActionUnavailable,
    };
}

bool IsValidPitchMode(AuditionPitchMode mode) noexcept {
    return mode == AuditionPitchMode::Semitone ||
           mode == AuditionPitchMode::FrequencyShift;
}

bool IsValidAction(AuditionActionType type) noexcept {
    switch (type) {
    case AuditionActionType::ToggleVisibleFooterPlayback:
    case AuditionActionType::SetVolume:
    case AuditionActionType::SetPitchControlValue:
    case AuditionActionType::ChangePitchModeAndReset:
    case AuditionActionType::ToggleReversePreservingRegion:
    case AuditionActionType::SetLoopAndRearmRegion:
        return true;
    }
    return false;
}

bool IsActionAvailable(
    const AuditionStateSnapshot& snapshot,
    AuditionActionType type) noexcept {
    switch (type) {
    case AuditionActionType::ToggleVisibleFooterPlayback:
        return snapshot.footerPlaybackEnabled;
    case AuditionActionType::SetVolume:
        return snapshot.volumeEnabled;
    case AuditionActionType::SetPitchControlValue:
    case AuditionActionType::ChangePitchModeAndReset:
        return snapshot.pitchEnabled;
    case AuditionActionType::ToggleReversePreservingRegion:
        return snapshot.reverseEnabled;
    case AuditionActionType::SetLoopAndRearmRegion:
        return snapshot.loopEnabled;
    }
    return false;
}

bool IsFiniteRange(float minimum, float maximum) noexcept {
    return std::isfinite(minimum) && std::isfinite(maximum) && minimum <= maximum;
}

}  // namespace

AuditionFrontendController::AuditionFrontendController(
    AuditionApplicationPort& port) noexcept
    : port_(port) {}

AuditionDispatchResult AuditionFrontendController::captureSnapshot(
    AuditionStateSnapshot& output) const noexcept {
    try {
        port_.captureSnapshot(output);
        return Applied();
    } catch (...) {
        return {
            AuditionDispatchStatus::Failed,
            AuditionDiagnosticCode::AdapterFailure,
        };
    }
}

AuditionDispatchResult AuditionFrontendController::dispatch(
    const AuditionAction& action) noexcept {
    AuditionStateSnapshot snapshot{};
    const AuditionDispatchResult captureResult = captureSnapshot(snapshot);
    if (!captureResult.applied()) {
        return captureResult;
    }
    if (!IsValidAction(action.type)) {
        return Rejected(AuditionDiagnosticCode::UnknownAction);
    }
    if (!IsActionAvailable(snapshot, action.type)) {
        return Unavailable();
    }

    try {
        switch (action.type) {
        case AuditionActionType::ToggleVisibleFooterPlayback:
            port_.toggleVisibleFooterPlayback();
            break;

        case AuditionActionType::SetVolume:
            if (!std::isfinite(action.numericValue)) {
                return Rejected(AuditionDiagnosticCode::NonFinitePayload);
            }
            if (!IsFiniteRange(snapshot.volumeMinimum, snapshot.volumeMaximum)) {
                return Rejected(AuditionDiagnosticCode::InvalidControlRange);
            }
            port_.setVolume(std::clamp(
                action.numericValue,
                snapshot.volumeMinimum,
                snapshot.volumeMaximum));
            break;

        case AuditionActionType::SetPitchControlValue: {
            if (!std::isfinite(action.numericValue)) {
                return Rejected(AuditionDiagnosticCode::NonFinitePayload);
            }
            const bool frequencyMode =
                snapshot.pitchMode == AuditionPitchMode::FrequencyShift;
            const float minimum = frequencyMode
                ? snapshot.frequencyShiftMinimum
                : snapshot.pitchMinimum;
            const float maximum = frequencyMode
                ? snapshot.frequencyShiftMaximum
                : snapshot.pitchMaximum;
            if (!IsFiniteRange(minimum, maximum)) {
                return Rejected(AuditionDiagnosticCode::InvalidControlRange);
            }
            port_.setPitchControlValue(
                std::clamp(action.numericValue, minimum, maximum));
            break;
        }

        case AuditionActionType::ChangePitchModeAndReset:
            if (!IsValidPitchMode(action.pitchMode)) {
                return Rejected(AuditionDiagnosticCode::InvalidPitchMode);
            }
            port_.changePitchModeAndReset(action.pitchMode);
            break;

        case AuditionActionType::ToggleReversePreservingRegion:
            port_.toggleReversePreservingRegion();
            break;

        case AuditionActionType::SetLoopAndRearmRegion:
            port_.setLoopAndRearmRegion(action.enabled);
            break;

        default:
            return Rejected(AuditionDiagnosticCode::UnknownAction);
        }
    } catch (...) {
        return {
            AuditionDispatchStatus::Failed,
            AuditionDiagnosticCode::AdapterFailure,
        };
    }

    return Applied();
}

AuditionUiDispatcher::AuditionUiDispatcher(
    AuditionFrontendController& controller) noexcept
    : controller_(controller),
      ownerThread_(std::this_thread::get_id()) {}

AuditionDispatchResult AuditionUiDispatcher::captureSnapshot(
    AuditionStateSnapshot& output) const noexcept {
    if (!isOwnerThread()) {
        return {
            AuditionDispatchStatus::WrongThread,
            AuditionDiagnosticCode::WrongThread,
        };
    }
    return controller_.captureSnapshot(output);
}

AuditionDispatchResult AuditionUiDispatcher::dispatch(
    const AuditionAction& action) noexcept {
    if (!isOwnerThread()) {
        return {
            AuditionDispatchStatus::WrongThread,
            AuditionDiagnosticCode::WrongThread,
        };
    }
    return controller_.dispatch(action);
}

bool AuditionUiDispatcher::isOwnerThread() const noexcept {
    return std::this_thread::get_id() == ownerThread_;
}

}  // namespace aris::frontend
