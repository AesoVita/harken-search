#include "src/BpmBatchCompletionWorkflow.h"

#include <iostream>
#include <string>
#include <vector>

using namespace aura::music_analysis;

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (value) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}
struct FakePort final : BpmBatchCompletionPort {
    std::vector<std::string> trace;
    void markBatchStopped() override { trace.push_back("stop"); }
    void invalidateBrowserMetadataCache() override { trace.push_back("invalidate"); }
};
}

int main() {
    for (const BpmBatchCompletionSnapshot snapshot : {
             BpmBatchCompletionSnapshot{false, 0, false},
             BpmBatchCompletionSnapshot{true, 1, false},
             BpmBatchCompletionSnapshot{true, 0, true}}) {
        FakePort port;
        Check(!CompleteBpmBatchIfDrained(snapshot, port),
              "incomplete snapshot is rejected");
        Check(port.trace.empty(), "rejected snapshot has no effects");
    }
    FakePort completed;
    Check(CompleteBpmBatchIfDrained({true, 0, false}, completed),
          "drained batch completes");
    Check(completed.trace == std::vector<std::string>({"stop", "invalidate"}),
          "completion preserves stop then invalidate order");
    if (failures) return 1;
    std::cout << "PASS: BPM batch completion workflow\n";
    return 0;
}
