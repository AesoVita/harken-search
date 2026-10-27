#include "src/ScanWaveformBackfillDrainWorkflow.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

aura::BackfillJob Job(std::int64_t id, std::int64_t session = 0) {
    aura::BackfillJob job{};
    job.fileId = id;
    job.scanSessionId = session;
    job.kind = aura::BackfillJobKind::WaveformL1Overview;
    job.priority = aura::BackfillPriority::RecentScan;
    return job;
}

class FakePorts final
    : public aura::backfill::ScanWaveformBackfillObservationPort,
      public aura::backfill::ScanWaveformBackfillQueuePort,
      public aura::backfill::ScanWaveformBackfillRecordPort,
      public aura::backfill::ScanWaveformBackfillSubmissionPort {
public:
    bool admissionScanningValue = false;
    bool batchScanningValue = false;
    bool reconcileValue = false;
    std::size_t urgentValue = 0;
    std::size_t normalValue = 0;
    std::size_t workerValue = 0;
    int queuedCount = -1;
    std::vector<aura::BackfillJob> jobs;
    std::size_t nextJob = 0;
    std::vector<aura::backfill::ScanWaveformBackfillRecordRef> loadedRecords;
    std::vector<std::int64_t> loadedIds;
    int loadCalls = 0;
    std::vector<bool> submitResults;
    std::size_t nextSubmit = 0;
    aura::BackfillItemState postFailureState = aura::BackfillItemState::Pending;
    bool terminalResult = true;
    std::vector<std::pair<std::int64_t, aura::BackfillCompletionOutcome>> outcomes;
    std::vector<std::string> terminalReasons;
    std::vector<std::int64_t> clearedIds;
    mutable std::vector<std::string> trace;

    bool admissionScanning() const override {
        trace.push_back("admissionScanning");
        return admissionScanningValue;
    }
    bool batchScanning() const override {
        trace.push_back("batchScanning");
        return batchScanningValue;
    }
    bool reconcileScanRunning() const override {
        trace.push_back("reconcileScanRunning");
        return reconcileValue;
    }
    std::size_t urgentPending() const override {
        trace.push_back("urgentPending");
        return urgentValue;
    }
    std::size_t normalPending() const override {
        trace.push_back("normalPending");
        return normalValue;
    }
    std::size_t workerPending() const override {
        trace.push_back("workerPending");
        return workerValue;
    }
    int queuedWaveformCount() const override {
        trace.push_back("queuedWaveformCount");
        return queuedCount >= 0 ? queuedCount
                                : static_cast<int>(jobs.size() - nextJob);
    }
    bool tryDequeueWaveform(aura::BackfillJob& job) override {
        trace.push_back("tryDequeueWaveform");
        if (nextJob >= jobs.size()) return false;
        job = jobs[nextJob++];
        return true;
    }
    void reportOutcome(
        const aura::BackfillJob& job,
        aura::BackfillCompletionOutcome outcome) override {
        trace.push_back("reportOutcome:" + std::to_string(job.fileId) + ":" +
                        std::to_string(static_cast<int>(outcome)));
        outcomes.emplace_back(job.fileId, outcome);
    }
    aura::BackfillItemState itemState(
        std::int64_t fileId,
        aura::BackfillJobKind) const override {
        trace.push_back("itemState:" + std::to_string(fileId));
        return postFailureState;
    }
    std::vector<aura::backfill::ScanWaveformBackfillRecordRef> loadByIds(
        const std::vector<std::int64_t>& ids) override {
        trace.push_back("loadByIds");
        ++loadCalls;
        loadedIds = ids;
        return loadedRecords;
    }
    bool markForegroundMiniFailed(
        std::int64_t sessionId,
        std::int64_t fileId,
        const char* reason) override {
        trace.push_back("markTerminal:" + std::to_string(sessionId) + ":" +
                        std::to_string(fileId));
        terminalReasons.emplace_back(reason);
        return terminalResult;
    }
    void clearPending(
        std::int64_t fileId,
        aura::BackfillJobKind) override {
        trace.push_back("clearPending:" + std::to_string(fileId));
        clearedIds.push_back(fileId);
    }
    bool submit(
        const std::string&,
        const aura::BackfillJob& job) override {
        trace.push_back("submit:" + std::to_string(job.fileId));
        const bool result = nextSubmit < submitResults.size()
                                ? submitResults[nextSubmit]
                                : true;
        ++nextSubmit;
        return result;
    }
};

aura::backfill::ScanWaveformBackfillDrainResult Run(
    FakePorts& ports,
    bool foreground = false,
    bool automatic = true) {
    return aura::backfill::DrainScanWaveformBackfillQueue(
        ports, ports, ports, ports, {foreground, automatic});
}

}  // namespace

int main() {
    FakePorts disabled;
    Run(disabled, false, false);
    Check(disabled.trace.empty(),
          "disabled drain performs no observation or port call");

    FakePorts scanning;
    scanning.admissionScanningValue = true;
    Run(scanning);
    Check(scanning.trace == std::vector<std::string>{"admissionScanning"},
          "ordinary scanning blocks before later observations");

    FakePorts empty;
    empty.queuedCount = 0;
    Run(empty);
    Check(empty.trace == std::vector<std::string>{
              "admissionScanning", "reconcileScanRunning", "queuedWaveformCount"},
          "empty queue suppresses pending samples");

    FakePorts threshold;
    threshold.queuedCount = 1;
    threshold.urgentValue = 10;
    threshold.normalValue = 10;
    threshold.workerValue = 12;
    const auto thresholdResult = Run(threshold);
    Check(thresholdResult.pending == 32 &&
              threshold.trace == std::vector<std::string>{
                  "admissionScanning", "reconcileScanRunning",
                  "queuedWaveformCount", "urgentPending", "normalPending",
                  "workerPending"},
          "threshold equality samples pressure and suppresses dequeue");

    FakePorts bounded;
    bounded.batchScanningValue = true;
    bounded.jobs = {Job(1), Job(2), Job(3)};
    bounded.loadedRecords = {{1, "one.wav"}, {2, "two.wav"}, {3, "three.wav"}};
    const auto boundedResult = Run(bounded);
    Check(boundedResult.dequeued == 2 && boundedResult.submitted == 2 &&
              bounded.nextJob == 2 && bounded.loadCalls == 1 &&
              bounded.loadedIds == std::vector<std::int64_t>{1, 2},
          "second scanning sample enforces two-job budget and one hydration");
    Check(bounded.trace[6] == "batchScanning" &&
              bounded.trace[7] == "tryDequeueWaveform",
          "batch scan sample occurs after all pressure samples");

    FakePorts missing;
    missing.jobs = {Job(7, 42)};
    const auto missingResult = Run(missing);
    Check(missingResult.missingRecords == 1 &&
              missingResult.permanentFailures == 1 &&
              missing.outcomes.size() == 1 &&
              missing.outcomes[0].second ==
                  aura::BackfillCompletionOutcome::PermanentFailure,
          "missing row reports permanent failure");
    Check(missing.terminalReasons == std::vector<std::string>{"missing_audio_row"} &&
              missing.clearedIds == std::vector<std::int64_t>{7},
          "missing row persists terminal reason before clearing pending");

    FakePorts terminalFailure;
    terminalFailure.jobs = {Job(8, 43)};
    terminalFailure.terminalResult = false;
    Run(terminalFailure);
    Check(terminalFailure.clearedIds.empty(),
          "failed terminal persistence suppresses pending clear");

    FakePorts transient;
    transient.jobs = {Job(9, 44)};
    transient.loadedRecords = {{9, "nine.wav"}};
    transient.submitResults = {false};
    const auto transientResult = Run(transient);
    Check(transientResult.transientFailures == 1 &&
              transientResult.permanentFailures == 0 &&
              transient.terminalReasons.empty() && transient.clearedIds.empty(),
          "transient rejection requeues without terminal persistence");

    FakePorts permanent;
    permanent.jobs = {Job(10, 45)};
    permanent.loadedRecords = {{10, "ten.wav"}};
    permanent.submitResults = {false};
    permanent.postFailureState = aura::BackfillItemState::FailedPermanent;
    const auto permanentResult = Run(permanent);
    Check(permanentResult.transientFailures == 1 &&
              permanentResult.permanentFailures == 1 &&
              permanent.terminalReasons ==
                  std::vector<std::string>{"waveform_submit_failed"} &&
              permanent.clearedIds == std::vector<std::int64_t>{10},
          "permanent submit failure persists exact reason and clears pending");

    std::printf("\n=== Scan Waveform Backfill Drain Workflow: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
