#include "src/WaveformRebuildFailurePersistence.h"
#include "src/WaveformExtractor.h"

#include <cstdio>
#include <string>

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

class FakeSink final
    : public aura::waveform_rebuild::WaveformRebuildFailureSink {
public:
    int calls = 0;
    AudioRecord record;
    std::string reason;

    void persistPreviousFailure(
        const AudioRecord& value,
        const char* failureReason) override {
        ++calls;
        record = value;
        reason = failureReason;
    }
};

AudioRecord Record(const char* path) {
    AudioRecord record;
    record.id = 42;
    record.filePath = path;
    record.fileSize = 2048;
    record.fileMtime = 123456;
    return record;
}

}  // namespace

int main() {
    using aura::waveform_rebuild::PersistPreviousWaveformRebuildFailure;
    using aura::waveform_rebuild::WaveformRebuildFailureRequest;

    FakeSink full;
    Check(!PersistPreviousWaveformRebuildFailure(
              full, WaveformRebuildFailureRequest{Record("full.wav"), true, true, false}),
          "full rebuild does not consume a previous failure");
    Check(full.calls == 0, "full rebuild does not persist a previous failure");

    FakeSink fresh;
    Check(!PersistPreviousWaveformRebuildFailure(
              fresh, WaveformRebuildFailureRequest{Record("fresh.wav"), false, false, false}),
          "a record without previous failure remains eligible for submission");
    Check(fresh.calls == 0, "a record without previous failure is not persisted");

    FakeSink mini;
    Check(PersistPreviousWaveformRebuildFailure(
              mini, WaveformRebuildFailureRequest{Record("mini.wav"), false, true, true}),
          "incremental mini rebuild consumes a previous failure");
    Check(mini.calls == 1 && mini.record.filePath == "mini.wav",
          "mini previous failure is persisted exactly once with its record");
    Check(mini.reason == "foreground_mini_rebuild_previous_failure",
          "mini previous failure keeps the legacy reason");

    FakeSink detail;
    Check(PersistPreviousWaveformRebuildFailure(
              detail, WaveformRebuildFailureRequest{Record("detail.wav"), false, true, false}),
          "incremental detail rebuild consumes a previous failure");
    Check(detail.calls == 1 && detail.record.filePath == "detail.wav",
          "detail previous failure is persisted exactly once with its record");
    Check(detail.reason == "foreground_waveform_rebuild_previous_failure",
          "detail previous failure keeps the legacy reason");

    std::printf("\n=== Waveform Rebuild Failure Persistence: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
