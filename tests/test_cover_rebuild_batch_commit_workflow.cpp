#include "src/CoverRebuildBatchCommitWorkflow.h"

#include <iostream>
#include <string>
#include <vector>

using namespace aura::cover_rebuild;

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
struct FakePersistence final : CoverRebuildBatchPersistencePort {
    std::vector<std::string> trace;
    void setHasCover(const std::vector<std::string>& paths) override {
        trace.push_back("set:" + std::to_string(paths.size()));
    }
    void clearHasCover(const std::vector<std::string>& paths) override {
        trace.push_back("clear:" + std::to_string(paths.size()));
    }
    void updateCoverKeyCache(const std::vector<CoverRebuildBatchCacheUpdate>& updates) override {
        trace.push_back("cache:" + std::to_string(updates.size()));
    }
};
}

int main() {
    FakePersistence persistence;
    const std::vector<std::string> set{"a.wav", "b.wav"};
    const std::vector<std::string> clear{"c.wav"};
    const std::vector<CoverRebuildBatchCacheUpdate> cache{{"a.wav", "key", "cover.png", 7}};
    CommitCoverRebuildBatch({set, clear, cache}, persistence);
    Check(persistence.trace == std::vector<std::string>({"set:2", "clear:1", "cache:1"}),
          "batch writes preserve set, clear, cache order");

    persistence.trace.clear();
    CommitCoverRebuildBatch({{}, {}, {}}, persistence);
    Check(persistence.trace.empty(), "empty batch skips all writes");
    if (failures) return 1;
    std::cout << "PASS: cover rebuild batch commit workflow\n";
    return 0;
}
