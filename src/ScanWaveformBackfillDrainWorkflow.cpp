#include "ScanWaveformBackfillDrainWorkflow.h"

#include <unordered_map>
#include <utility>

namespace aura::backfill {
namespace {

constexpr std::size_t kBackfillWorkerQueueThreshold = 32;

void PersistPermanentFailure(
    ScanWaveformBackfillRecordPort& records,
    const aura::BackfillJob& job,
    const char* reason) {
    const bool terminalRecorded =
        job.scanSessionId <= 0 ||
        records.markForegroundMiniFailed(
            job.scanSessionId, job.fileId, reason);
    if (terminalRecorded) {
        records.clearPending(job.fileId, job.kind);
    }
}

}  // namespace

ScanWaveformBackfillDrainResult DrainScanWaveformBackfillQueue(
    ScanWaveformBackfillObservationPort& observations,
    ScanWaveformBackfillQueuePort& queue,
    ScanWaveformBackfillRecordPort& records,
    ScanWaveformBackfillSubmissionPort& submissions,
    const ScanWaveformBackfillDrainRequest& request) {
    ScanWaveformBackfillDrainResult result;
    if (!request.autoBackfillEnabled &&
        !request.foregroundCompletionActive) {
        return result;
    }

    const bool admissionScanning = observations.admissionScanning();
    if (admissionScanning && !request.foregroundCompletionActive) {
        return result;
    }
    if (observations.reconcileScanRunning()) {
        return result;
    }
    if (queue.queuedWaveformCount() <= 0) {
        return result;
    }

    result.pending = observations.urgentPending();
    result.pending += observations.normalPending();
    result.pending += observations.workerPending();
    if (result.pending >= kBackfillWorkerQueueThreshold) {
        return result;
    }

    const int perFrame = observations.batchScanning() ? 2 : 8;
    std::vector<aura::BackfillJob> jobs;
    jobs.reserve(static_cast<std::size_t>(perFrame));
    std::vector<std::int64_t> ids;
    ids.reserve(static_cast<std::size_t>(perFrame));
    for (int i = 0;
         i < perFrame &&
         result.pending + jobs.size() < kBackfillWorkerQueueThreshold;
         ++i) {
        aura::BackfillJob job{};
        if (!queue.tryDequeueWaveform(job)) break;
        jobs.push_back(job);
        ids.push_back(job.fileId);
        ++result.dequeued;
    }
    if (jobs.empty()) return result;

    const auto loadedRecords = records.loadByIds(ids);
    std::unordered_map<std::int64_t, std::string> pathById;
    pathById.reserve(loadedRecords.size());
    for (const auto& record : loadedRecords) {
        if (record.id > 0 && !record.filePath.empty()) {
            pathById.emplace(record.id, record.filePath);
        }
    }

    for (const auto& job : jobs) {
        const auto path = pathById.find(job.fileId);
        if (path == pathById.end()) {
            queue.reportOutcome(
                job, aura::BackfillCompletionOutcome::PermanentFailure);
            ++result.missingRecords;
            ++result.permanentFailures;
            PersistPermanentFailure(records, job, "missing_audio_row");
            continue;
        }

        if (submissions.submit(path->second, job)) {
            ++result.pending;
            ++result.submitted;
            continue;
        }

        queue.reportOutcome(
            job, aura::BackfillCompletionOutcome::TransientFailure);
        ++result.transientFailures;
        if (queue.itemState(job.fileId, job.kind) ==
            aura::BackfillItemState::FailedPermanent) {
            ++result.permanentFailures;
            PersistPermanentFailure(
                records, job, "waveform_submit_failed");
        }
    }

    return result;
}

}  // namespace aura::backfill
