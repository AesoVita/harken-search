#include "src/BpmBatchScanWorkflow.h"

#include <iostream>
#include <string>
#include <vector>

using namespace aura::music_analysis;

namespace {

int g_failed = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++g_failed;
    }
}

struct FakePort final : BpmBatchScanPort {
    bool open = true;
    std::vector<BpmBatchScanRow> rows;
    std::uint64_t generation = 41;
    std::vector<std::string> trace;
    std::vector<BpmBatchScanRequest> requests;

    bool databaseOpen() const override {
        const_cast<FakePort*>(this)->trace.push_back("open");
        return open;
    }
    std::vector<BpmBatchScanRow> queryMissingBpm(
        int limit,
        bool includeFailures,
        const std::string& prefix) override {
        trace.push_back(
            "query:" + std::to_string(limit) + ":" +
            (includeFailures ? "true:" : "false:") + prefix);
        return rows;
    }
    void cancelP2() override { trace.push_back("cancelP2"); }
    void setBatchTotal(int value) override {
        trace.push_back("total:" + std::to_string(value));
    }
    void setBatchDone(int value) override {
        trace.push_back("done:" + std::to_string(value));
    }
    void setBatchFound(int value) override {
        trace.push_back("found:" + std::to_string(value));
    }
    void setBatchRunning(bool value) override {
        trace.push_back(value ? "running:true" : "running:false");
    }
    void setBatchCancelFlag(bool value) override {
        trace.push_back(value ? "cancel:true" : "cancel:false");
    }
    std::uint64_t nextGeneration() override {
        trace.push_back("generation");
        return ++generation;
    }
    void submitP2(const BpmBatchScanRequest& request) override {
        trace.push_back("submit:" + std::to_string(request.fileId));
        requests.push_back(request);
    }
};

void TestClosedAndEmptyAdmission() {
    FakePort closed;
    closed.open = false;
    Check(!StartBpmBatchScan(closed, "C:/scope"),
          "closed Database rejects BPM batch");
    Check(closed.trace == std::vector<std::string>{"open"},
          "closed Database performs no query or analyzer effect");

    FakePort empty;
    Check(!StartBpmBatchScan(empty, "C:/scope"),
          "empty query rejects BPM batch");
    Check(empty.trace == std::vector<std::string>{
              "open", "query:50000:true:C:/scope"},
          "empty query preserves fixed query policy and suppresses state effects");
    Check(empty.generation == 41,
          "empty query does not advance generation");
}

void TestAdmittedBatchOrderAndRequests() {
    FakePort port;
    port.rows = {{7, "C:/sounds/a.wav"}, {8, ""}};

    Check(StartBpmBatchScan(port, "C:/sounds"),
          "non-empty query starts BPM batch");
    const std::vector<std::string> expected = {
        "open",
        "query:50000:true:C:/sounds",
        "cancelP2",
        "total:2",
        "done:0",
        "found:0",
        "running:true",
        "cancel:false",
        "generation",
        "submit:7",
        "submit:8",
    };
    Check(port.trace == expected,
          "batch cancellation, state, generation, and FIFO submit order are exact");
    Check(port.generation == 42 && port.requests.size() == 2,
          "admitted batch advances generation once and submits every row");

    for (const auto& request : port.requests) {
        Check(request.generation == 42,
              "all requests share the admitted generation");
        Check(request.startSec == 0.0 && request.endSec == 30.0,
              "batch requests analyze the first thirty seconds");
        Check(request.runKey,
              "batch requests preserve default key analysis");
    }
    Check(port.requests[0].fileId == 7 &&
              port.requests[0].filePath == "C:/sounds/a.wav" &&
              port.requests[1].fileId == 8 &&
              port.requests[1].filePath.empty(),
          "row identity, FIFO order, and legacy empty-path submission are preserved");
}

}  // namespace

int main() {
    TestClosedAndEmptyAdmission();
    TestAdmittedBatchOrderAndRequests();
    if (g_failed != 0) {
        std::cerr << "FAIL: " << g_failed
                  << " BPM batch scan workflow checks failed\n";
        return 1;
    }
    std::cout << "PASS: BPM batch scan workflow\n";
    return 0;
}
