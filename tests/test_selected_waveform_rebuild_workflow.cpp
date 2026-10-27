#include "SelectedWaveformRebuildWorkflow.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

bool WaveformWorker::submitVisibleRequest(const std::string&) {
    return true;
}

bool WaveformWorker::submitDetailRequest(const std::string&, std::int64_t) {
    return true;
}

namespace {

int g_failed = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++g_failed;
    }
}

using aura::waveform_rebuild::SelectedWaveformRebuildEffectsPort;
using ::SelectedWaveformRebuildItem;
using aura::waveform_rebuild::SelectedWaveformRebuildPumpDecision;
using aura::waveform_rebuild::SelectedWaveformRebuildStats;
using aura::waveform_rebuild::SelectedWaveformRebuildStoragePort;
using aura::waveform_rebuild::SelectedWaveformRebuildWorkflow;
using aura::waveform_rebuild::WaveformRebuildSubmissionPort;

struct FakePorts final : SelectedWaveformRebuildStoragePort,
                         SelectedWaveformRebuildEffectsPort,
                         WaveformRebuildSubmissionPort {
    bool beginResult = true;
    bool commitResult = true;
    bool miniOnly = false;
    std::string loadedPath;
    std::map<std::string, bool> clearResults;
    std::map<std::string, bool> submitResults;
    std::set<std::string> matchedPaths;
    std::vector<std::string> trace;

    bool beginTransaction() override {
        trace.push_back("begin");
        return beginResult;
    }

    bool clearWaveformCacheByPath(const std::string& filePath) override {
        trace.push_back("clear:" + filePath);
        const auto it = clearResults.find(filePath);
        return it == clearResults.end() || it->second;
    }

    bool commitTransaction() override {
        trace.push_back("commit");
        return commitResult;
    }

    void rollbackTransaction() override { trace.push_back("rollback"); }

    bool miniOnlyPolicy() const override {
        const_cast<FakePorts*>(this)->trace.push_back("miniOnly");
        return miniOnly;
    }

    std::string loadedFilePath() const override {
        const_cast<FakePorts*>(this)->trace.push_back("loadedPath");
        return loadedPath;
    }

    void clearWaveformUiCachesForPath(const std::string& filePath) override {
        trace.push_back("ui:" + filePath);
    }

    void addPendingWaveform(const std::string& filePath, bool detail) override {
        trace.push_back("pending:" + filePath + ":" + (detail ? "detail" : "mini"));
    }

    bool isLoadedItem(
        const SelectedWaveformRebuildItem& item,
        const std::string& observedLoadedPath) const override {
        const_cast<FakePorts*>(this)->trace.push_back(
            "match:" + item.filePath + ":" + observedLoadedPath);
        return matchedPaths.count(item.filePath) != 0;
    }

    void forceLoadedItem(const SelectedWaveformRebuildItem& item) override {
        trace.push_back("force:" + item.filePath);
    }

    void reportTransactionCommitFailure() override {
        trace.push_back("commitFailure");
    }

    void reportForcedCurrent(const SelectedWaveformRebuildItem& item) override {
        trace.push_back("forcedReport:" + item.filePath);
    }

    void reportCompleted(const SelectedWaveformRebuildStats& stats) override {
        trace.push_back(
            "completed:" + std::to_string(stats.cleared) + ":" +
            std::to_string(stats.queued) + ":" +
            std::to_string(stats.pending));
    }

    bool submitMini(const std::string& filePath) override {
        trace.push_back("submitMini:" + filePath);
        const auto it = submitResults.find(filePath);
        return it == submitResults.end() || it->second;
    }

    bool submitDetail(const std::string& filePath, std::int64_t fileId) override {
        trace.push_back(
            "submitDetail:" + filePath + ":" + std::to_string(fileId));
        const auto it = submitResults.find(filePath);
        return it == submitResults.end() || it->second;
    }
};

std::vector<SelectedWaveformRebuildItem> Items(int count) {
    std::vector<SelectedWaveformRebuildItem> items;
    for (int i = 0; i < count; ++i) {
        items.push_back({i + 1, "file_" + std::to_string(i + 1) + ".wav"});
    }
    return items;
}

void TestAdmissionAndClosedDatabaseClear() {
    SelectedWaveformRebuildWorkflow workflow;
    FakePorts ports;

    const auto inactive = workflow.pump(ports, ports, ports, {false, true});
    Check(inactive.decision == SelectedWaveformRebuildPumpDecision::NoWork,
          "inactive workflow reports no work");
    Check(ports.trace.empty(), "inactive workflow performs no port call");

    workflow.start(Items(1));
    const auto blocked = workflow.pump(ports, ports, ports, {true, true});
    Check(blocked.decision ==
              SelectedWaveformRebuildPumpDecision::BlockedByExplicitRebuild,
          "explicit rebuild blocks selected rebuild");
    Check(workflow.active() && workflow.remaining() == 1,
          "blocked workflow preserves queued work");
    Check(ports.trace.empty(),
          "explicit rebuild blocks without observation or mutation");

    const auto closed = workflow.pump(ports, ports, ports, {false, false});
    Check(closed.decision ==
              SelectedWaveformRebuildPumpDecision::ClearedClosedDatabase,
          "closed Database reports queue clear");
    Check(!workflow.active() && workflow.remaining() == 0,
          "closed Database clears the selected queue");
    Check(ports.trace.empty(), "closed Database performs no port call");
}

void TestFrameBudgetOrderAndCumulativeStats() {
    SelectedWaveformRebuildWorkflow workflow;
    auto items = Items(5);
    items.push_back({99, ""});
    items.push_back(items[1]);
    workflow.start(items);

    FakePorts ports;
    ports.loadedPath = "loaded.wav";
    const auto first = workflow.pump(ports, ports, ports, {false, true});
    Check(first.decision == SelectedWaveformRebuildPumpDecision::Pumped,
          "partial frame reports pumped");
    Check(first.stats.cleared == 4 && first.stats.queued == 4 &&
              first.stats.pending == 0 && first.stats.remaining == 1,
          "five items honor four-item frame budget and cumulative stats");
    Check(ports.trace.front() == "begin" && ports.trace[5] == "commit" &&
              ports.trace[6] == "loadedPath" && ports.trace[7] == "miniOnly",
          "transaction precedes loaded-path and mini-only observations");
    Check(ports.trace[8] == "ui:file_1.wav" &&
              ports.trace[9] == "submitDetail:file_1.wav:1" &&
              ports.trace[10] == "match:file_1.wav:loaded.wav",
          "per-item effects preserve UI, submit, match order");

    ports.trace.clear();
    const auto second = workflow.pump(ports, ports, ports, {false, true});
    Check(second.decision == SelectedWaveformRebuildPumpDecision::Completed,
          "last frame reports completed");
    Check(second.stats.cleared == 5 && second.stats.queued == 5 &&
              second.stats.remaining == 0,
          "stats remain cumulative across frames");
    Check(ports.trace.back() == "completed:5:5:0",
          "completion notification follows the last per-item effect");
}

void TestCommitFailureContinuesAndSubmissionFallback() {
    SelectedWaveformRebuildWorkflow workflow;
    workflow.start(Items(2));

    FakePorts ports;
    ports.commitResult = false;
    ports.miniOnly = true;
    ports.clearResults["file_2.wav"] = false;
    ports.submitResults["file_1.wav"] = false;

    const auto result = workflow.pump(ports, ports, ports, {false, true});
    const std::vector<std::string> expectedPrefix = {
        "begin", "clear:file_1.wav", "clear:file_2.wav", "commit",
        "rollback", "commitFailure", "loadedPath", "miniOnly",
        "ui:file_1.wav", "submitMini:file_1.wav",
        "pending:file_1.wav:mini", "ui:file_2.wav", "submitMini:file_2.wav"};
    Check(ports.trace.size() == expectedPrefix.size() + 1,
          "commit failure path records all effects plus completion");
    Check(std::equal(expectedPrefix.begin(), expectedPrefix.end(), ports.trace.begin()),
          "commit failure rolls back and reports before continuing submissions");
    Check(result.stats.cleared == 1 && result.stats.queued == 1 &&
              result.stats.pending == 1,
          "clear and submission outcomes update exact cumulative counters");
    Check(ports.trace.back() == "completed:1:1:1",
          "commit failure still completes drained work");
}

void TestCurrentFileLifecycleDoesNotBlockAndConverges() {
    SelectedWaveformRebuildWorkflow workflow;

    const auto missing = workflow.queueCurrentFile({7, "missing.wav"}, false);
    Check(missing ==
              aura::waveform_rebuild::SelectedWaveformRebuildQueueDecision::MissingFile,
          "missing current file is rejected before any queue work");
    Check(!workflow.active() && !workflow.currentFileTaskActive(),
          "missing current file leaves no background task");
    Check(workflow.lifecycle().state ==
              aura::waveform_rebuild::SelectedWaveformRebuildLifecycle::Failed,
          "missing current file is visible as failed");

    const auto queued = workflow.queueCurrentFile({8, "current.wav"}, true);
    Check(queued ==
              aura::waveform_rebuild::SelectedWaveformRebuildQueueDecision::Queued,
          "existing current file is queued without doing storage work");
    Check(workflow.currentFileTaskActive(),
          "queued current file remains visible as in progress");
    Check(workflow.lifecycle().state ==
              aura::waveform_rebuild::SelectedWaveformRebuildLifecycle::Running,
          "queued current file reports running");

    const auto duplicate = workflow.queueCurrentFile({8, "current.wav"}, true);
    Check(duplicate ==
              aura::waveform_rebuild::SelectedWaveformRebuildQueueDecision::AlreadyRunning,
          "repeat click does not start a second current-file task");
    Check(workflow.remaining() == 1, "repeat click keeps a single queued file");

    FakePorts ports;
    const auto pumped = workflow.pump(ports, ports, ports, {false, true});
    Check(pumped.decision == SelectedWaveformRebuildPumpDecision::Completed,
          "current-file pump still uses the existing selected rebuild path");
    Check(workflow.currentFileTaskActive(),
          "lifecycle stays running until the worker result arrives");
    Check(!workflow.observeWorkerResult("other.wav", true, ""),
          "unrelated worker results do not close the current-file task");
    Check(workflow.observeWorkerResult("current.wav", true, ""),
          "matching worker success closes the current-file task");
    Check(!workflow.currentFileTaskActive(),
          "successful current-file rebuild leaves no leftover task");
    Check(workflow.lifecycle().state ==
              aura::waveform_rebuild::SelectedWaveformRebuildLifecycle::Completed,
          "successful current-file rebuild is visible as completed");

    workflow.queueCurrentFile({9, "cancel.wav"}, true);
    workflow.cancel();
    Check(!workflow.active() && !workflow.currentFileTaskActive(),
          "cancel clears queued current-file work");
    Check(workflow.lifecycle().state ==
              aura::waveform_rebuild::SelectedWaveformRebuildLifecycle::Cancelled,
          "cancel is visible as cancelled");
    Check(!workflow.observeWorkerResult("cancel.wav", true, ""),
          "stale success after cancel does not resurrect the task");

    workflow.queueCurrentFile({10, "closing.wav"}, true);
    workflow.shutdown();
    Check(!workflow.active() && !workflow.currentFileTaskActive(),
          "window close clears current-file work");
    Check(workflow.lifecycle().state ==
              aura::waveform_rebuild::SelectedWaveformRebuildLifecycle::Idle,
          "window close returns the lifecycle to idle");
}

void TestCurrentFileAdmissionPreservesExistingQueue() {
    SelectedWaveformRebuildWorkflow workflow;
    workflow.start(Items(2));
    const auto queued = workflow.queueCurrentFile({9, "current.wav"}, true);
    Check(queued == aura::waveform_rebuild::SelectedWaveformRebuildQueueDecision::Queued,
          "current file can join an existing rebuild queue");
    Check(workflow.remaining() == 3,
          "current file admission preserves previously queued files");
}

void TestBeginFailureAndForceAtMostOncePerFrame() {
    SelectedWaveformRebuildWorkflow workflow;
    workflow.start(Items(2));

    FakePorts ports;
    ports.beginResult = false;
    ports.loadedPath = "current.wav";
    ports.matchedPaths.insert("file_1.wav");
    ports.matchedPaths.insert("file_2.wav");

    workflow.pump(ports, ports, ports, {false, true});
    Check(std::find(ports.trace.begin(), ports.trace.end(), "commit") ==
              ports.trace.end(),
          "failed begin suppresses commit");
    Check(std::find(ports.trace.begin(), ports.trace.end(), "rollback") ==
              ports.trace.end(),
          "failed begin suppresses rollback");
    Check(std::count(ports.trace.begin(), ports.trace.end(), "force:file_1.wav") == 1,
          "first loaded match is forced once");
    Check(std::find(ports.trace.begin(), ports.trace.end(), "force:file_2.wav") ==
              ports.trace.end(),
          "later loaded match is not forced again in the same frame");
    Check(std::count_if(
              ports.trace.begin(), ports.trace.end(),
              [](const std::string& value) { return value.rfind("match:", 0) == 0; }) == 2,
          "loaded match remains a per-item observation after first force");
}

}  // namespace

int main() {
    TestAdmissionAndClosedDatabaseClear();
    TestFrameBudgetOrderAndCumulativeStats();
    TestCommitFailureContinuesAndSubmissionFallback();
    TestCurrentFileLifecycleDoesNotBlockAndConverges();
    TestCurrentFileAdmissionPreservesExistingQueue();
    TestBeginFailureAndForceAtMostOncePerFrame();

    if (g_failed != 0) {
        std::cerr << "FAIL: " << g_failed
                  << " selected waveform rebuild workflow checks failed\n";
        return 1;
    }
    std::cout << "PASS: selected waveform rebuild workflow\n";
    return 0;
}
