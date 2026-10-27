#include "AsyncSearchRefreshCompletionWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchRefreshCompletionEffects {
    std::vector<int> events;
    void markGenerationApplied() override { events.push_back(1); }
    void requestCoverWallRefresh() override { events.push_back(2); }
    void setError(const std::string&) override { events.push_back(3); }
    void setReady() override { events.push_back(4); }
};

int main() {
    Effects error;
    aura::search::AsyncSearchRefreshCompletionInput errorInput;
    errorInput.databaseView = true;
    errorInput.resultsEmpty = true;
    errorInput.databaseError = "db";
    errorInput.advancedDbFilters = true;
    aura::search::HandleAsyncSearchRefreshCompletion(errorInput, error);
    assert((error.events == std::vector<int>{1, 2, 3}));
    Effects ready;
    aura::search::AsyncSearchRefreshCompletionInput readyInput;
    readyInput.databaseView = true;
    aura::search::HandleAsyncSearchRefreshCompletion(readyInput, ready);
    assert((ready.events == std::vector<int>{1, 4}));
    return 0;
}
