#include "WaveformRebuildFailurePersistence.h"

#include "WaveformExtractor.h"

namespace aura::waveform_rebuild {

DatabaseWaveformRebuildFailureSink::DatabaseWaveformRebuildFailureSink(
    Database& database) noexcept
    : database_(database) {}

void DatabaseWaveformRebuildFailureSink::persistPreviousFailure(
    const AudioRecord& record,
    const char* reason) {
    const std::string cacheKey = WaveformExtractor::BuildCacheKey(
        record.filePath,
        record.fileSize,
        record.fileMtime);
    database_.upsertWaveformCacheStateByPath(
        record.filePath,
        "failed",
        reason,
        1,
        cacheKey);
}

bool PersistPreviousWaveformRebuildFailure(
    WaveformRebuildFailureSink& sink,
    const WaveformRebuildFailureRequest& request) {
    if (request.fullMode || !request.previouslyFailed) {
        return false;
    }

    const char* reason = request.incrementalMiniOnly
        ? "foreground_mini_rebuild_previous_failure"
        : "foreground_waveform_rebuild_previous_failure";
    sink.persistPreviousFailure(request.record, reason);
    return true;
}

}  // namespace aura::waveform_rebuild
