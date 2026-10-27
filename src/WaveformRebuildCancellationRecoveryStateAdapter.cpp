#include "WaveformRebuildCancellationRecoveryStateAdapter.h"

#include "AppState.h"
#include "WaveformRebuildRuntimeState.h"

namespace aura::waveform_rebuild {

WaveformRebuildCancellationRecoveryStateAdapter::
    WaveformRebuildCancellationRecoveryStateAdapter(
        AppState& state,
        WaveformRebuildRuntimeState& runtimeState)
    : state_(state), runtimeState_(runtimeState) {}

AdaptiveMiniFormatFinalizationResult
WaveformRebuildCancellationRecoveryStateAdapter::recover(
    AdaptiveMiniFormatStampPort& stampPort,
    AdaptiveMiniFormatFinalizationState adaptiveState) {
    return RecoverCancelledWaveformRebuild(
        stampPort, *this, adaptiveState);
}

void WaveformRebuildCancellationRecoveryStateAdapter::applyAdaptiveState(
    const AdaptiveMiniFormatFinalizationState& adaptiveState) {
    state_.adaptiveMiniWaveformFormatRebuildPending =
        adaptiveState.rebuildPending;
    state_.adaptiveMiniWaveformFormatStampPending =
        adaptiveState.stampPending;
}

void WaveformRebuildCancellationRecoveryStateAdapter::
    restartForegroundWorker() {
    state_.waveformWorker.stop();
    if (state_.database.isOpen()) {
        state_.waveformWorker.start(&state_.database, 1);
    }
}

void WaveformRebuildCancellationRecoveryStateAdapter::resetRuntimeState() {
    runtimeState_.Reset(state_, false);
}

}  // namespace aura::waveform_rebuild
