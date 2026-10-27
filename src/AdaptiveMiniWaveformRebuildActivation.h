#pragma once

namespace aura::waveform_rebuild {

struct AdaptiveMiniWaveformRebuildActivationState {
    bool waveformRebuildMiniOnlyMode = false;
    bool incrementalMiniOnly = false;
    bool rebuildPending = false;
    bool stampPending = false;
};

struct AdaptiveMiniWaveformRebuildActivationResult {
    bool activated = false;
    AdaptiveMiniWaveformRebuildActivationState state;
};

AdaptiveMiniWaveformRebuildActivationResult
ActivateAdaptiveMiniWaveformRebuild(
    bool startSucceeded,
    AdaptiveMiniWaveformRebuildActivationState state);

}  // namespace aura::waveform_rebuild
