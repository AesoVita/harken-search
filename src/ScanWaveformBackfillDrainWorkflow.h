#pragma once

#include "BackfillQueue.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aura::backfill {

struct ScanWaveformBackfillDrainRequest {
    bool foregroundCompletionActive = false;
    bool autoBackfillEnabled = false;
};

struct ScanWaveformBackfillRecordRef {
    std::int64_t id = 0;
    std::string filePath;
};

struct ScanWaveformBackfillDrainResult {
    std::size_t pending = 0;
    int dequeued = 0;
    int submitted = 0;
    int missingRecords = 0;
    int transientFailures = 0;
    int permanentFailures = 0;
};

class ScanWaveformBackfillObservationPort {
public:
    virtual ~ScanWaveformBackfillObservationPort() = default;
    virtual bool admissionScanning() const = 0;
    virtual bool batchScanning() const = 0;
    virtual bool reconcileScanRunning() const = 0;
    virtual std::size_t urgentPending() const = 0;
    virtual std::size_t normalPending() const = 0;
    virtual std::size_t workerPending() const = 0;
};

class ScanWaveformBackfillQueuePort {
public:
    virtual ~ScanWaveformBackfillQueuePort() = default;
    virtual int queuedWaveformCount() const = 0;
    virtual bool tryDequeueWaveform(aura::BackfillJob& job) = 0;
    virtual void reportOutcome(
        const aura::BackfillJob& job,
        aura::BackfillCompletionOutcome outcome) = 0;
    virtual aura::BackfillItemState itemState(
        std::int64_t fileId,
        aura::BackfillJobKind kind) const = 0;
};

class ScanWaveformBackfillRecordPort {
public:
    virtual ~ScanWaveformBackfillRecordPort() = default;
    virtual std::vector<ScanWaveformBackfillRecordRef> loadByIds(
        const std::vector<std::int64_t>& ids) = 0;
    virtual bool markForegroundMiniFailed(
        std::int64_t sessionId,
        std::int64_t fileId,
        const char* reason) = 0;
    virtual void clearPending(
        std::int64_t fileId,
        aura::BackfillJobKind kind) = 0;
};

class ScanWaveformBackfillSubmissionPort {
public:
    virtual ~ScanWaveformBackfillSubmissionPort() = default;
    virtual bool submit(
        const std::string& filePath,
        const aura::BackfillJob& job) = 0;
};

ScanWaveformBackfillDrainResult DrainScanWaveformBackfillQueue(
    ScanWaveformBackfillObservationPort& observations,
    ScanWaveformBackfillQueuePort& queue,
    ScanWaveformBackfillRecordPort& records,
    ScanWaveformBackfillSubmissionPort& submissions,
    const ScanWaveformBackfillDrainRequest& request);

}  // namespace aura::backfill
