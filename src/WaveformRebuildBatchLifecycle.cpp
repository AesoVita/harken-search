#include "WaveformRebuildBatchLifecycle.h"

#include <utility>

namespace aura::waveform_rebuild {

WaveformRebuildBatchLifecycleResult LoadWaveformRebuildBatchLifecycle(
    WaveformRebuildBatchSource& source,
    AdaptiveMiniFormatStampPort& stampPort,
    std::set<std::int64_t>& queuedIds,
    const WaveformRebuildBatchLifecycleRequest& request) {
    queuedIds.clear();
    auto selection = SelectWaveformRebuildBatch(source, request.batch);

    WaveformRebuildBatchLifecycleResult result;
    result.records = std::move(selection.records);
    result.batchMiniOnly = selection.miniOnly;
    result.finalization.state = request.adaptiveState;
    if (!result.records.empty()) {
        return result;
    }

    result.decision =
        WaveformRebuildBatchLifecycleDecision::RebuildCompleted;
    result.finalization = FinalizeAdaptiveMiniWaveformFormat(
        stampPort,
        AdaptiveMiniFormatFinalizationReason::RebuildCompleted,
        request.adaptiveState);
    return result;
}

}  // namespace aura::waveform_rebuild
