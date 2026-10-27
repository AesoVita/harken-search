#pragma once

#include "AdaptiveMiniWaveformFormatFinalization.h"
#include "WaveformRebuildBatchSelector.h"

#include <cstdint>
#include <set>
#include <vector>

namespace aura::waveform_rebuild {

enum class WaveformRebuildBatchLifecycleDecision {
    BatchLoaded,
    RebuildCompleted,
};

struct WaveformRebuildBatchLifecycleRequest {
    WaveformRebuildBatchRequest batch;
    AdaptiveMiniFormatFinalizationState adaptiveState;
};

struct WaveformRebuildBatchLifecycleResult {
    WaveformRebuildBatchLifecycleDecision decision =
        WaveformRebuildBatchLifecycleDecision::BatchLoaded;
    std::vector<AudioRecord> records;
    bool batchMiniOnly = false;
    int batchIndex = 0;
    AdaptiveMiniFormatFinalizationResult finalization;
};

WaveformRebuildBatchLifecycleResult LoadWaveformRebuildBatchLifecycle(
    WaveformRebuildBatchSource& source,
    AdaptiveMiniFormatStampPort& stampPort,
    std::set<std::int64_t>& queuedIds,
    const WaveformRebuildBatchLifecycleRequest& request);

}  // namespace aura::waveform_rebuild
