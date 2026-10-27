#include "src/CoverRebuildLifecycleWorkflow.h"

#include <iostream>
#include <string>
#include <vector>

using namespace aura::cover_rebuild;

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
struct FakeState final : CoverRebuildLifecycleStatePort {
    std::vector<std::string> trace;
    void cancelBatch() override { trace.push_back("cancel"); }
    void completeBatch() override { trace.push_back("complete"); }
    void clearFacets() override { trace.push_back("facets"); }
    void setPreferDeferredCount() override { trace.push_back("deferred"); }
    void switchToCoverTab() override { trace.push_back("tab"); }
};
struct FakeEffects final : CoverRebuildLifecycleEffectsPort {
    std::vector<std::string> trace;
    void forceCoverWallReload() override { trace.push_back("wall"); }
    void refreshSearchResults() override { trace.push_back("refresh"); }
    void syncCoverRegistry() override { trace.push_back("sync"); }
};
void CheckTrace(const std::vector<std::string>& got,
                std::initializer_list<const char*> expected,
                const char* message) {
    std::vector<std::string> want;
    for (const char* item : expected) want.emplace_back(item);
    Check(got == want, message);
}
}

int main() {
    {
        FakeState state; FakeEffects effects;
        Check(!ResolveCoverRebuildLifecycle({false, false, false}, state, effects),
              "inactive lifecycle is not consumed");
        Check(state.trace.empty() && effects.trace.empty(), "inactive has no effects");
    }
    {
        FakeState state; FakeEffects effects;
        Check(ResolveCoverRebuildLifecycle({true, true, true}, state, effects),
              "cancel is consumed");
        CheckTrace(state.trace, {"cancel", "facets", "deferred"},
                   "cancel state order and precedence");
        CheckTrace(effects.trace, {"wall", "refresh"}, "cancel effects order");
    }
    {
        FakeState state; FakeEffects effects;
        ResolveCoverRebuildLifecycle({false, true, true}, state, effects);
        CheckTrace(state.trace, {"complete", "tab", "facets", "deferred"},
                   "completion state order");
        CheckTrace(effects.trace, {"sync", "wall", "refresh"},
                   "stale completion effect order");
    }
    {
        FakeState state; FakeEffects effects;
        ResolveCoverRebuildLifecycle({false, true, false}, state, effects);
        CheckTrace(effects.trace, {"wall", "refresh"},
                   "fresh completion skips registry sync");
    }
    if (failures) return 1;
    std::cout << "PASS: cover rebuild lifecycle workflow\n";
    return 0;
}
