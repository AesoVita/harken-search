#include "AdaptiveMiniWaveformRebuildActivation.h"

namespace aura::waveform_rebuild {

AdaptiveMiniWaveformRebuildActivationResult
ActivateAdaptiveMiniWaveformRebuild(
    bool startSucceeded,
    AdaptiveMiniWaveformRebuildActivationState state) {
    if (!startSucceeded) {
        return {false, state};
    }

    state.waveformRebuildMiniOnlyMode = true;
    state.incrementalMiniOnly = true;
    state.rebuildPending = false;
    state.stampPending = true;
    return {true, state};
}

}  // namespace aura::waveform_rebuild
