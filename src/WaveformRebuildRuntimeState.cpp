#include "WaveformRebuildRuntimeState.h"

#include "AppState.h"

namespace aura::waveform_rebuild {

void WaveformRebuildRuntimeState::Reset(
    AppState& state,
    bool cancelWorkerQueue) {
    state.waveformRebuildActive = false;
    state.waveformRebuildFullMode = false;
    state.waveformRebuildTotal = 0;
    state.waveformRebuildMiniTotal = 0;
    state.waveformRebuildDetailTotal = 0;
    state.waveformRebuildProgress = 0;
    state.waveformRebuildSkipped = 0;
    state.waveformRebuildFailed = 0;
    state.waveformRebuildCancel = false;
    state.waveformRebuildOffset = 0;
    state.waveformRebuildBatch.clear();
    state.waveformRebuildQueuedIds.clear();
    state.waveformRebuildBatchIdx = 0;
    state.clearPendingWaveforms(cancelWorkerQueue);
    state.waveformRebuildMiniOnlyMode = false;
    batchMiniOnly_ = false;
    incrementalMiniOnly_ = false;
}

bool WaveformRebuildRuntimeState::batchMiniOnly() const noexcept {
    return batchMiniOnly_;
}

void WaveformRebuildRuntimeState::setBatchMiniOnly(bool value) noexcept {
    batchMiniOnly_ = value;
}

bool WaveformRebuildRuntimeState::incrementalMiniOnly() const noexcept {
    return incrementalMiniOnly_;
}

void WaveformRebuildRuntimeState::setIncrementalMiniOnly(bool value) noexcept {
    incrementalMiniOnly_ = value;
}

}  // namespace aura::waveform_rebuild
