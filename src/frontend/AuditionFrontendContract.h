#pragma once

#include <cstdint>

namespace aris::frontend {

enum class AuditionPitchMode : std::uint8_t {
    Semitone = 0,
    FrequencyShift = 1,
};

enum class AuditionActionType : std::uint8_t {
    ToggleVisibleFooterPlayback = 0,
    SetVolume,
    SetPitchControlValue,
    ChangePitchModeAndReset,
    ToggleReversePreservingRegion,
    SetLoopAndRearmRegion,
};

enum class AuditionDispatchStatus : std::uint8_t {
    Applied = 0,
    Rejected,
    Unavailable,
    WrongThread,
    Failed,
};

enum class AuditionDiagnosticCode : std::uint8_t {
    None = 0,
    NonFinitePayload,
    InvalidControlRange,
    InvalidPitchMode,
    ActionUnavailable,
    WrongThread,
    AdapterFailure,
    UnknownAction,
};

struct AuditionStateSnapshot final {
    bool audioLoaded = false;
    bool playing = false;
    bool footerPlaybackEnabled = false;
    bool volumeEnabled = false;
    bool pitchEnabled = false;
    bool reverseEnabled = false;
    bool loopEnabled = false;
    bool reversed = false;
    bool regionLoopEnabled = false;
    bool hasRegion = false;

    float volume = 1.0f;
    float volumeMinimum = 0.0f;
    float volumeMaximum = 1.2f;
    float pitchControlValue = 0.0f;
    float pitchMinimum = -24.0f;
    float pitchMaximum = 24.0f;
    float frequencyShiftMinimum = -5000.0f;
    float frequencyShiftMaximum = 5000.0f;
    float regionIn = 0.0f;
    float regionOut = 1.0f;
    float regionFadeIn = 0.0f;
    float regionFadeOut = 0.0f;
    AuditionPitchMode pitchMode = AuditionPitchMode::Semitone;
};

struct AuditionAction final {
    AuditionActionType type = AuditionActionType::ToggleVisibleFooterPlayback;
    float numericValue = 0.0f;
    bool enabled = false;
    AuditionPitchMode pitchMode = AuditionPitchMode::Semitone;

    static constexpr AuditionAction toggleVisibleFooterPlayback() noexcept {
        return {AuditionActionType::ToggleVisibleFooterPlayback};
    }

    static constexpr AuditionAction setVolume(float value) noexcept {
        return {AuditionActionType::SetVolume, value};
    }

    static constexpr AuditionAction setPitchControlValue(float value) noexcept {
        return {AuditionActionType::SetPitchControlValue, value};
    }

    static constexpr AuditionAction changePitchModeAndReset(
        AuditionPitchMode mode) noexcept {
        AuditionAction action{};
        action.type = AuditionActionType::ChangePitchModeAndReset;
        action.pitchMode = mode;
        return action;
    }

    static constexpr AuditionAction toggleReversePreservingRegion() noexcept {
        return {AuditionActionType::ToggleReversePreservingRegion};
    }

    static constexpr AuditionAction setLoopAndRearmRegion(bool value) noexcept {
        AuditionAction action{};
        action.type = AuditionActionType::SetLoopAndRearmRegion;
        action.enabled = value;
        return action;
    }
};

struct AuditionDispatchResult final {
    AuditionDispatchStatus status = AuditionDispatchStatus::Failed;
    AuditionDiagnosticCode diagnostic = AuditionDiagnosticCode::AdapterFailure;

    constexpr bool applied() const noexcept {
        return status == AuditionDispatchStatus::Applied;
    }
};

class AuditionApplicationPort {
public:
    virtual ~AuditionApplicationPort() = default;

    virtual void captureSnapshot(AuditionStateSnapshot& output) const = 0;
    virtual void toggleVisibleFooterPlayback() = 0;
    virtual void setVolume(float value) = 0;
    virtual void setPitchControlValue(float value) = 0;
    virtual void changePitchModeAndReset(AuditionPitchMode mode) = 0;
    virtual void toggleReversePreservingRegion() = 0;
    virtual void setLoopAndRearmRegion(bool enabled) = 0;
};

}  // namespace aris::frontend
