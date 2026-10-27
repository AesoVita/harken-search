#include "WaveformRebuildPumpGate.h"

namespace aura::waveform_rebuild {

WaveformRebuildCapacityGateResult EvaluateWaveformRebuildCapacityGate(
    WaveformRebuildCapacityGateInput input) {
    const std::size_t pending =
        input.urgentPending + input.normalPending + input.workerPending;
    return {pending >= input.queueThreshold, pending};
}

WaveformRebuildBatchGateDecision EvaluateWaveformRebuildBatchGate(
    WaveformRebuildBatchGateInput input) {
    if (input.batchIndex < static_cast<int>(input.batchSize)) {
        return WaveformRebuildBatchGateDecision::ContinueBatch;
    }
    if (ShouldSampleWaveformRebuildDrain(input) && input.workerPending > 0) {
        return WaveformRebuildBatchGateDecision::WaitForDrain;
    }
    return WaveformRebuildBatchGateDecision::LoadBatch;
}

bool ShouldSampleWaveformRebuildDrain(
    WaveformRebuildBatchGateInput input) {
    return input.batchIndex >= static_cast<int>(input.batchSize) &&
           (input.progress > 0 || input.offset > 0);
}

}  // namespace aura::waveform_rebuild
