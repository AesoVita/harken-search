#include "src/WaveformRebuildBatchLifecycle.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

std::vector<AudioRecord> Database::getAllFiles(int, int) { return {}; }
WaveformCachePolicy::Mode Database::getWaveformCacheMode() {
    return WaveformCachePolicy::Mode::MiniOnly;
}
std::vector<int64_t> Database::getFilesNeedingWaveformCache(int) { return {}; }
std::vector<int64_t> Database::getFilesNeedingDetailCache(int) { return {}; }
std::vector<AudioRecord> Database::getFilesByIds(const std::vector<int64_t>&) {
    return {};
}
bool Database::markAdaptiveMiniWaveformFormatUpgradeComplete() { return true; }

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) std::printf("PASS: %s\n", message);
    else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

AudioRecord Record(int64_t id, const char* path) {
    AudioRecord record;
    record.id = id;
    record.filePath = path;
    return record;
}

class FakeSource final
    : public aura::waveform_rebuild::WaveformRebuildBatchSource {
public:
    std::vector<AudioRecord> allRecords;
    std::vector<int64_t> miniIds;
    std::vector<int64_t> detailIds;
    std::vector<AudioRecord> resolvedRecords;
    bool miniMode = false;
    mutable std::vector<std::string> calls;

    std::vector<AudioRecord> getAllFiles(int limit, int offset) override {
        calls.push_back("all:" + std::to_string(limit) + ":" + std::to_string(offset));
        return allRecords;
    }
    bool usesMiniOnlyCacheMode() const override {
        calls.push_back("mode");
        return miniMode;
    }
    std::vector<int64_t> getFilesNeedingWaveformCache(int limit) override {
        calls.push_back("mini:" + std::to_string(limit));
        return miniIds;
    }
    std::vector<int64_t> getFilesNeedingDetailCache(int limit) override {
        calls.push_back("detail:" + std::to_string(limit));
        return detailIds;
    }
    std::vector<AudioRecord> getFilesByIds(const std::vector<int64_t>&) override {
        calls.push_back("by_ids");
        return resolvedRecords;
    }
};

class FakeStamp final
    : public aura::waveform_rebuild::AdaptiveMiniFormatStampPort {
public:
    bool returnValue = false;
    int calls = 0;

    bool markUpgradeComplete() override {
        ++calls;
        return returnValue;
    }
};

}  // namespace

int main() {
    using namespace aura::waveform_rebuild;

    {
        FakeSource source;
        source.allRecords = {Record(7, "full.wav")};
        source.miniMode = true;
        FakeStamp stamp;
        std::set<int64_t> queued{1, 2};
        const auto result = LoadWaveformRebuildBatchLifecycle(
            source, stamp, queued,
            {{true, false, 64, 12}, {true, true}});
        Check(result.decision == WaveformRebuildBatchLifecycleDecision::BatchLoaded &&
                  result.records.size() == 1 && result.batchMiniOnly &&
                  result.batchIndex == 0,
              "loaded full batch returns initialized batch state");
        Check(queued.empty(), "loaded batch clears prior queued IDs");
        Check(source.calls == std::vector<std::string>{"all:64:12", "mode"} &&
                  stamp.calls == 0,
              "loaded batch selects once and suppresses terminal stamp");
        Check(result.finalization.state.rebuildPending &&
                  result.finalization.state.stampPending &&
                  !result.finalization.stampAttempted,
              "loaded batch preserves adaptive state without finalizing");
    }

    {
        FakeSource source;
        source.miniIds = {8};
        source.resolvedRecords = {Record(8, "mini.wav")};
        FakeStamp stamp;
        std::set<int64_t> queued{8};
        const auto result = LoadWaveformRebuildBatchLifecycle(
            source, stamp, queued,
            {{false, true, 32, 0}, {false, false}});
        Check(result.decision == WaveformRebuildBatchLifecycleDecision::BatchLoaded &&
                  result.batchMiniOnly && result.records[0].id == 8,
              "incremental mini batch preserves selector classification");
        Check(source.calls == std::vector<std::string>{"mini:32", "by_ids"} &&
                  stamp.calls == 0,
              "incremental loaded batch suppresses stamp");
    }

    {
        FakeSource source;
        FakeStamp stamp;
        std::set<int64_t> queued{9};
        const auto result = LoadWaveformRebuildBatchLifecycle(
            source, stamp, queued,
            {{false, false, 16, 0}, {false, false}});
        Check(result.decision == WaveformRebuildBatchLifecycleDecision::RebuildCompleted &&
                  result.records.empty() && !result.finalization.stampAttempted,
              "exhausted batch without pending stamp completes without write");
        Check(queued.empty() && stamp.calls == 0,
              "exhausted no-op still clears prior queued IDs");
    }

    {
        FakeSource source;
        FakeStamp stamp;
        stamp.returnValue = true;
        std::set<int64_t> queued;
        const auto result = LoadWaveformRebuildBatchLifecycle(
            source, stamp, queued,
            {{false, false, 16, 0}, {true, true}});
        Check(result.decision == WaveformRebuildBatchLifecycleDecision::RebuildCompleted &&
                  result.finalization.stampAttempted &&
                  result.finalization.stampSucceeded && stamp.calls == 1,
              "exhausted pending stamp succeeds exactly once");
        Check(result.finalization.state.rebuildPending &&
                  !result.finalization.state.stampPending,
              "completion success consumes stamp pending only");
    }

    {
        FakeSource source;
        FakeStamp stamp;
        std::set<int64_t> queued;
        const auto result = LoadWaveformRebuildBatchLifecycle(
            source, stamp, queued,
            {{false, false, 16, 0}, {false, true}});
        Check(result.finalization.stampAttempted &&
                  !result.finalization.stampSucceeded && stamp.calls == 1,
              "exhausted stamp failure is reported once");
        Check(result.finalization.state.rebuildPending &&
                  !result.finalization.state.stampPending,
              "stamp failure requests rebuild retry and consumes stamp pending");
    }

    std::printf("\n=== Waveform Rebuild Batch Lifecycle: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
