#include "src/BpmBatchResultWorkflow.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace aura::music_analysis;

int failures = 0;

void Check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

struct FakePorts final : BpmBatchProgressPort,
                         BpmBatchPersistencePort,
                         BpmBatchViewPort {
    std::vector<std::string> trace;
    int done = 0;
    int found = 0;
    std::vector<BpmBatchViewUpdate> updates;

    void incrementDone() override { ++done; trace.push_back("done"); }
    void incrementFound() override { ++found; trace.push_back("found"); }
    void persistBpm(std::int64_t, float bpm, float confidence) override {
        trace.push_back("bpm:" + std::to_string(bpm) + ":" +
                        std::to_string(confidence));
    }
    void persistKey(std::int64_t, const std::string&, const std::string&,
                    float) override {
        trace.push_back("key");
    }
    void persistMood(std::int64_t, float, float) override {
        trace.push_back("mood");
    }
    void applyToFirstMatchingRecord(
        std::int64_t, const BpmBatchViewUpdate& update) override {
        trace.push_back("view");
        updates.push_back(update);
    }
};

BpmBatchResult ValidResult(float bpm = 120.25f) {
    BpmBatchResult result;
    result.isP2 = true;
    result.fileId = 42;
    result.ok = true;
    result.bpm = {true, bpm, 0.8f};
    result.key = {true, "C# minor", "12A", 0.7f};
    result.mood = {true, 0.25f, 0.75f};
    return result;
}

void TestAdmissionHasNoEffects() {
    FakePorts ports;
    auto notP2 = ValidResult();
    notP2.isP2 = false;
    Check(!ProcessBpmBatchResult(notP2, ports, ports, ports),
          "non-P2 result falls through");
    auto invalidId = ValidResult();
    invalidId.fileId = -1;
    Check(!ProcessBpmBatchResult(invalidId, ports, ports, ports),
          "negative file ID falls through");
    Check(ports.trace.empty(), "rejected results have no effects");
}

void TestFailureWritesSentinelBeforeView() {
    FakePorts ports;
    auto result = ValidResult();
    result.ok = false;
    Check(ProcessBpmBatchResult(result, ports, ports, ports),
          "failed P2 result is consumed");
    Check(ports.trace == std::vector<std::string>({
              "done", "bpm:-1.000000:0.000000", "view"}),
          "failure preserves done, sentinel, view order");
    Check(ports.updates.size() == 1 && ports.updates[0].applyBpm,
          "failure updates numeric BPM");
    Check(!ports.updates[0].applyBpmText,
          "failure preserves existing BPM metadata text");
    Check(ports.updates[0].bpm == -1.0f &&
              ports.updates[0].bpmConfidence == 0.0f,
          "failure view uses sentinel values");
}

void TestStrictBpmBoundsAndEmptySuccess() {
    for (float bpm : {40.0f, 240.0f}) {
        FakePorts ports;
        auto result = ValidResult(bpm);
        result.key.ok = false;
        result.mood.ok = false;
        Check(ProcessBpmBatchResult(result, ports, ports, ports),
              "boundary BPM result is consumed");
        Check(ports.trace == std::vector<std::string>({"done", "view"}),
              "strict boundary BPM is neither persisted nor counted");
        Check(!ports.updates[0].applyBpm && !ports.updates[0].applyKey &&
                  !ports.updates[0].applyMood,
              "empty successful fields produce a no-op view update");
    }
}

void TestValidSuccessPreservesEffectOrderAndView() {
    FakePorts ports;
    Check(ProcessBpmBatchResult(ValidResult(), ports, ports, ports),
          "valid P2 result is consumed");
    Check(ports.trace == std::vector<std::string>({
              "done", "bpm:120.250000:0.800000", "found", "key", "mood", "view"}),
          "valid result preserves persistence and view order");
    Check(ports.done == 1 && ports.found == 1,
          "valid result advances both counters once");
    const auto& update = ports.updates[0];
    Check(update.applyBpm && update.applyBpmText && update.bpmText == "120.2",
          "valid BPM view includes one-decimal metadata text");
    Check(update.applyKey && update.key == "C# minor" && update.camelot == "12A",
          "valid key view preserves key fields");
    Check(update.applyMood && std::fabs(update.moodValence - 0.25f) < 0.0001f &&
              std::fabs(update.moodArousal - 0.75f) < 0.0001f,
          "valid mood view preserves mood fields");
}

}  // namespace

int main() {
    TestAdmissionHasNoEffects();
    TestFailureWritesSentinelBeforeView();
    TestStrictBpmBoundsAndEmptySuccess();
    TestValidSuccessPreservesEffectOrderAndView();
    if (failures != 0) return 1;
    std::cout << "PASS: BPM batch result workflow\n";
    return 0;
}
