#include "src/SingleMusicAnalysisResultWorkflow.h"

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

struct FakePorts final : SingleMusicAnalysisStatePort,
                         SingleMusicAnalysisPersistencePort,
                         SingleMusicAnalysisViewPort {
    std::vector<std::string> trace;
    std::vector<SingleMusicPendingUpdate> updates;
    void markAnalysisComplete() override { trace.push_back("complete"); }
    void applyPendingUpdate(const SingleMusicPendingUpdate& update) override {
        trace.push_back("pending"); updates.push_back(update);
    }
    void persistKey(std::int64_t, const std::string&, const std::string&,
                    float) override { trace.push_back("key-db"); }
    void persistMood(std::int64_t, float, float) override {
        trace.push_back("mood-db");
    }
    void applyKeyToFirstMatchingRecord(std::int64_t, const std::string&,
                                       const std::string&, float) override {
        trace.push_back("key-view");
    }
};

SingleMusicAnalysisResult FullResult() {
    SingleMusicAnalysisResult result;
    result.generation = 8;
    result.fileId = 17;
    result.ok = false;
    result.bpm = {true, 123.5f, 0.81f, 0.125f};
    result.key = {true, "D minor", "7A", 0.72f};
    result.mood = {true, 0.2f, 0.9f};
    return result;
}

void TestStaleHasNoEffects() {
    FakePorts ports;
    Check(!ProcessSingleMusicAnalysisResult(FullResult(), 9, ports, ports, ports),
          "stale generation is rejected");
    Check(ports.trace.empty(), "stale result has no effects");
}

void TestSuccessPreservesOrderAndSelectiveState() {
    FakePorts ports;
    Check(ProcessSingleMusicAnalysisResult(FullResult(), 8, ports, ports, ports),
          "current result is processed regardless of aggregate ok");
    Check(ports.trace == std::vector<std::string>({
              "complete", "key-db", "key-view", "mood-db", "pending"}),
          "success preserves completion, key, mood, pending order");
    const auto& update = ports.updates[0];
    Check(update.applyBpm && update.bpm == 123.5f &&
              update.bpmConfidence == 0.81f,
          "success applies BPM values");
    Check(update.applyAiAutoSource && update.applyBeatOffset &&
              update.beatOffset == 0.125f,
          "success applies source and beat offset");
    Check(!update.applyFailed, "success does not clear existing failed state");
    Check(update.applyReady && update.ready && update.resetBubbleTimer,
          "success arms and resets the bubble");
}

void TestNegativeIdStillAppliesBpmButSkipsPersistence() {
    FakePorts ports;
    auto result = FullResult();
    result.fileId = -1;
    Check(ProcessSingleMusicAnalysisResult(result, 8, ports, ports, ports),
          "negative-ID current result is still processed");
    Check(ports.trace == std::vector<std::string>({"complete", "pending"}),
          "negative ID suppresses key and mood effects only");
    Check(ports.updates[0].applyBpm, "negative-ID result still updates BPM bubble");
}

void TestBpmFailurePreservesSourceAndOffset() {
    FakePorts ports;
    auto result = FullResult();
    result.key.ok = false;
    result.mood.ok = false;
    result.bpm.ok = false;
    Check(ProcessSingleMusicAnalysisResult(result, 8, ports, ports, ports),
          "failed BPM result is processed");
    Check(ports.trace == std::vector<std::string>({"complete", "pending"}),
          "failed BPM has only completion and pending effects");
    const auto& update = ports.updates[0];
    Check(update.applyBpm && update.bpm == 0.0f && update.bpmConfidence == 0.0f,
          "failure clears numeric BPM values");
    Check(update.applyFailed && update.failed,
          "failure explicitly marks failed");
    Check(!update.applyAiAutoSource && !update.applyBeatOffset,
          "failure preserves source and beat offset");
    Check(update.applyReady && update.ready && update.resetBubbleTimer,
          "failure arms and resets the bubble");
}

}  // namespace

int main() {
    TestStaleHasNoEffects();
    TestSuccessPreservesOrderAndSelectiveState();
    TestNegativeIdStillAppliesBpmButSkipsPersistence();
    TestBpmFailurePreservesSourceAndOffset();
    if (failures != 0) return 1;
    std::cout << "PASS: single music analysis result workflow\n";
    return 0;
}
