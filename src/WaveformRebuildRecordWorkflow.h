#pragma once

#include "WaveformRebuildFailurePersistence.h"
#include "WaveformRebuildRequestSubmission.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace aura::waveform_rebuild {

enum class WaveformRebuildRecordOutcome {
    NoRecord,
    Duplicate,
    PreviousFailurePersisted,
    Submitted,
    Rejected,
};

struct WaveformRebuildRecordState {
    int batchIndex = 0;
    int offset = 0;
    std::int64_t progress = 0;
    int failed = 0;
    int skipped = 0;
    std::size_t pending = 0;
};

struct WaveformRebuildRecordRequest {
    const std::vector<AudioRecord>& batch;
    WaveformRebuildRecordState state;
    bool fullMode = false;
    bool incrementalMiniOnly = false;
    bool batchMiniOnly = false;
};

struct WaveformRebuildRecordResult {
    WaveformRebuildRecordState state;
    WaveformRebuildRecordOutcome outcome =
        WaveformRebuildRecordOutcome::NoRecord;
};

WaveformRebuildRecordResult ProcessNextWaveformRebuildRecord(
    WaveformRebuildFailureSink& failureSink,
    WaveformRebuildSubmissionPort& submissionPort,
    std::set<std::int64_t>& queuedIds,
    const std::set<std::string>& previouslyFailedPaths,
    const WaveformRebuildRecordRequest& request);

}  // namespace aura::waveform_rebuild
