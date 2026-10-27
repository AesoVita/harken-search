#pragma once

#include <cstddef>
#include <cstdint>

namespace aura::waveform_rebuild {

enum class WaveformRebuildBatchGateDecision {
    ContinueBatch,
    WaitForDrain,
    LoadBatch,
};

struct WaveformRebuildCapacityGateInput {
    std::size_t urgentPending = 0;
    std::size_t normalPending = 0;
    std::size_t workerPending = 0;
    std::size_t queueThreshold = 0;
};

struct WaveformRebuildCapacityGateResult {
    bool wait = false;
    std::size_t pending = 0;
};

struct WaveformRebuildBatchGateInput {
    int batchIndex = 0;
    std::size_t batchSize = 0;
    std::int64_t progress = 0;
    std::int64_t offset = 0;
    std::size_t workerPending = 0;
};

WaveformRebuildCapacityGateResult EvaluateWaveformRebuildCapacityGate(
    WaveformRebuildCapacityGateInput input);

bool ShouldSampleWaveformRebuildDrain(
    WaveformRebuildBatchGateInput input);

WaveformRebuildBatchGateDecision EvaluateWaveformRebuildBatchGate(
    WaveformRebuildBatchGateInput input);

}  // namespace aura::waveform_rebuild
