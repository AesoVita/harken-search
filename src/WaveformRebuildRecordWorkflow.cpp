#include "WaveformRebuildRecordWorkflow.h"

namespace aura::waveform_rebuild {

WaveformRebuildRecordResult ProcessNextWaveformRebuildRecord(
    WaveformRebuildFailureSink& failureSink,
    WaveformRebuildSubmissionPort& submissionPort,
    std::set<std::int64_t>& queuedIds,
    const std::set<std::string>& previouslyFailedPaths,
    const WaveformRebuildRecordRequest& request) {
    WaveformRebuildRecordResult result{
        request.state,
        WaveformRebuildRecordOutcome::NoRecord};
    if (result.state.batchIndex < 0 ||
        result.state.batchIndex >= static_cast<int>(request.batch.size())) {
        return result;
    }

    const AudioRecord& record = request.batch[result.state.batchIndex];
    ++result.state.batchIndex;
    ++result.state.offset;

    if (!queuedIds.insert(record.id).second) {
        result.outcome = WaveformRebuildRecordOutcome::Duplicate;
        return result;
    }
    ++result.state.progress;

    if (PersistPreviousWaveformRebuildFailure(
            failureSink,
            {record,
             request.fullMode,
             previouslyFailedPaths.count(record.filePath) > 0,
             request.incrementalMiniOnly})) {
        ++result.state.failed;
        result.outcome =
            WaveformRebuildRecordOutcome::PreviousFailurePersisted;
        return result;
    }

    const auto submission = SubmitWaveformRebuildRequest(
        submissionPort,
        {record.filePath, record.id, request.batchMiniOnly});
    if (submission == WaveformRebuildSubmissionResult::Submitted) {
        ++result.state.pending;
        result.outcome = WaveformRebuildRecordOutcome::Submitted;
    } else {
        ++result.state.skipped;
        result.outcome = WaveformRebuildRecordOutcome::Rejected;
    }
    return result;
}

}  // namespace aura::waveform_rebuild
