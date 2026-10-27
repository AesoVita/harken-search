#include "src/WaveformRebuildRecordWorkflow.h"

#include "src/WaveformExtractor.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

bool Database::upsertWaveformCacheStateByPath(
    const std::string&,
    const std::string&,
    const std::string&,
    int,
    const std::string&) {
    return true;
}

std::string WaveformExtractor::BuildCacheKey(
    const std::string& filePath,
    uint64_t fileSize,
    int64_t fileMtime,
    int,
    int) {
    return filePath + ":" + std::to_string(fileSize) + ":" +
        std::to_string(fileMtime);
}

bool WaveformWorker::submitVisibleRequest(const std::string&) {
    return true;
}

bool WaveformWorker::submitDetailRequest(const std::string&, int64_t) {
    return true;
}

namespace {

using aura::waveform_rebuild::WaveformRebuildRecordOutcome;
using aura::waveform_rebuild::WaveformRebuildRecordRequest;
using aura::waveform_rebuild::WaveformRebuildRecordState;

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

class FakeFailureSink final
    : public aura::waveform_rebuild::WaveformRebuildFailureSink {
public:
    std::vector<std::string>& trace;

    explicit FakeFailureSink(std::vector<std::string>& value)
        : trace(value) {}

    void persistPreviousFailure(
        const AudioRecord& record,
        const char* reason) override {
        trace.push_back(
            "failure:" + std::to_string(record.id) + ":" + reason);
    }
};

class FakeSubmissionPort final
    : public aura::waveform_rebuild::WaveformRebuildSubmissionPort {
public:
    std::vector<std::string>& trace;
    bool accepted = false;

    explicit FakeSubmissionPort(std::vector<std::string>& value)
        : trace(value) {}

    bool submitMini(const std::string& filePath) override {
        trace.push_back("mini:" + filePath);
        return accepted;
    }

    bool submitDetail(
        const std::string& filePath,
        int64_t fileId) override {
        trace.push_back(
            "detail:" + filePath + ":" + std::to_string(fileId));
        return accepted;
    }
};

AudioRecord Record(int64_t id, const char* path) {
    AudioRecord record;
    record.id = id;
    record.filePath = path;
    record.fileSize = 2048;
    record.fileMtime = 123456;
    return record;
}

WaveformRebuildRecordState InitialState(int index = 1) {
    return {index, 10, 20, 2, 3, 4};
}

WaveformRebuildRecordRequest Request(
    const std::vector<AudioRecord>& batch,
    WaveformRebuildRecordState state,
    bool fullMode = false,
    bool incrementalMiniOnly = false,
    bool batchMiniOnly = false) {
    return {batch,
            state,
            fullMode,
            incrementalMiniOnly,
            batchMiniOnly};
}

}  // namespace

int main() {
    using aura::waveform_rebuild::ProcessNextWaveformRebuildRecord;

    const std::vector<AudioRecord> batch{
        Record(11, "first.wav"),
        Record(42, "path.wav")};

    {
        std::vector<std::string> trace;
        FakeFailureSink failure(trace);
        FakeSubmissionPort submission(trace);
        std::set<int64_t> queued;
        std::set<std::string> previous;
        const auto result = ProcessNextWaveformRebuildRecord(
            failure,
            submission,
            queued,
            previous,
            Request(batch, InitialState(2)));
        Check(result.outcome == WaveformRebuildRecordOutcome::NoRecord,
              "out-of-range batch index reports no record");
        Check(result.state.batchIndex == 2 && result.state.offset == 10 &&
                  result.state.progress == 20 && result.state.failed == 2 &&
                  result.state.skipped == 3 && result.state.pending == 4,
              "no-record preserves the complete input state");
        Check(queued.empty() && trace.empty(),
              "no-record suppresses queued-ID mutation and both ports");
    }

    {
        std::vector<std::string> trace;
        FakeFailureSink failure(trace);
        FakeSubmissionPort submission(trace);
        std::set<int64_t> queued{42};
        std::set<std::string> previous;
        const auto result = ProcessNextWaveformRebuildRecord(
            failure,
            submission,
            queued,
            previous,
            Request(batch, InitialState()));
        Check(result.outcome == WaveformRebuildRecordOutcome::Duplicate,
              "queued ID reports duplicate");
        Check(result.state.batchIndex == 2 && result.state.offset == 11 &&
                  result.state.progress == 20 && result.state.failed == 2 &&
                  result.state.skipped == 3 && result.state.pending == 4,
              "duplicate advances position but no accounting counter");
        Check(queued.size() == 1 && trace.empty(),
              "duplicate suppresses both ports and retains the queued set");
    }

    {
        std::vector<std::string> trace;
        FakeFailureSink failure(trace);
        FakeSubmissionPort submission(trace);
        std::set<int64_t> queued;
        std::set<std::string> previous{"path.wav"};
        const auto result = ProcessNextWaveformRebuildRecord(
            failure,
            submission,
            queued,
            previous,
            Request(batch, InitialState(), false, true, false));
        Check(result.outcome ==
                  WaveformRebuildRecordOutcome::PreviousFailurePersisted,
              "known incremental failure reports persistence");
        Check(result.state.batchIndex == 2 && result.state.offset == 11 &&
                  result.state.progress == 21 && result.state.failed == 3 &&
                  result.state.skipped == 3 && result.state.pending == 4,
              "known failure advances progress and failed only");
        Check(queued.count(42) == 1 && trace.size() == 1 &&
                  trace[0] ==
                      "failure:42:foreground_mini_rebuild_previous_failure",
              "known failure queues ID, keeps reason, and suppresses submission");
    }

    {
        std::vector<std::string> trace;
        FakeFailureSink failure(trace);
        FakeSubmissionPort submission(trace);
        submission.accepted = true;
        std::set<int64_t> queued;
        std::set<std::string> previous;
        const auto result = ProcessNextWaveformRebuildRecord(
            failure,
            submission,
            queued,
            previous,
            Request(batch, InitialState()));
        Check(result.outcome == WaveformRebuildRecordOutcome::Submitted,
              "accepted detail request reports submitted");
        Check(result.state.progress == 21 && result.state.pending == 5 &&
                  result.state.failed == 2 && result.state.skipped == 3,
              "accepted detail increments progress and pending only");
        Check(trace == std::vector<std::string>{"detail:path.wav:42"},
              "detail request preserves path and file ID");
    }

    {
        std::vector<std::string> trace;
        FakeFailureSink failure(trace);
        FakeSubmissionPort submission(trace);
        submission.accepted = true;
        std::set<int64_t> queued;
        std::set<std::string> previous;
        const auto result = ProcessNextWaveformRebuildRecord(
            failure,
            submission,
            queued,
            previous,
            Request(batch, InitialState(), false, false, true));
        Check(result.outcome == WaveformRebuildRecordOutcome::Submitted &&
                  result.state.pending == 5,
              "accepted mini request increments pending");
        Check(trace == std::vector<std::string>{"mini:path.wav"},
              "mini batch preserves the path-only route");
    }

    {
        std::vector<std::string> trace;
        FakeFailureSink failure(trace);
        FakeSubmissionPort submission(trace);
        std::set<int64_t> queued;
        std::set<std::string> previous;
        const auto result = ProcessNextWaveformRebuildRecord(
            failure,
            submission,
            queued,
            previous,
            Request(batch, InitialState()));
        Check(result.outcome == WaveformRebuildRecordOutcome::Rejected,
              "rejected worker request reports rejected");
        Check(result.state.progress == 21 && result.state.skipped == 4 &&
                  result.state.pending == 4 && result.state.failed == 2,
              "rejection increments progress and skipped only");
        Check(trace == std::vector<std::string>{"detail:path.wav:42"},
              "rejection still calls exactly one selected route");
    }

    std::printf(
        "\n=== Waveform Rebuild Record Workflow: %d failure(s) ===\n",
        g_failures);
    return g_failures == 0 ? 0 : 1;
}
