#include "CoverKeyFilterWorkflow.h"

#include <cassert>

struct Effects final : aura::search::CoverKeyFilterEffects {
    bool fileHasCover(const std::string& path, const std::string&) override { return path == "fallback"; }
};

int main() {
    Effects effects;
    aura::search::CoverKeyFilterInput input;
    input.active = true;
    input.key = "cover";
    input.records = {{"embedded", "cover", ""}, {"folder", "", "other"}, {"fallback", "", ""}};
    const auto kept = aura::search::HandleCoverKeyFilter(input, effects);
    assert((kept == std::vector<std::size_t>{0, 2}));

    aura::search::CoverKeyFilterInput cached;
    cached.active = true;
    cached.key = "cover";
    aura::search::CoverKeyFilterRecord cachedMatch{"cached-match", "", ""};
    cachedMatch.cachedMatchAvailable = true;
    cachedMatch.cachedMatch = true;
    aura::search::CoverKeyFilterRecord cachedMiss{"cached-miss", "", ""};
    cachedMiss.cachedMatchAvailable = true;
    cachedMiss.cachedMatch = false;
    cached.records = {cachedMatch, cachedMiss};
    const auto cachedKept = aura::search::HandleCoverKeyFilter(cached, effects);
    assert((cachedKept == std::vector<std::size_t>{0}));
    return 0;
}
