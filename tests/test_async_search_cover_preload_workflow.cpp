#include "AsyncSearchCoverPreloadWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchCoverPreloadEffects {
    std::vector<std::size_t> indices;
    void preloadResult(std::size_t index) override { indices.push_back(index); }
};

int main() {
    Effects active;
    aura::search::AsyncSearchCoverPreloadInput input;
    input.databaseView = true;
    input.resultCount = 32;
    aura::search::HandleAsyncSearchCoverPreload(input, active);
    assert(active.indices.size() == 30 && active.indices.front() == 0 && active.indices.back() == 29);

    Effects deferred;
    input.coverWallPriorityMode = true;
    aura::search::HandleAsyncSearchCoverPreload(input, deferred);
    assert(deferred.indices.empty());

    Effects filtered;
    input.folderFilterActive = true;
    aura::search::HandleAsyncSearchCoverPreload(input, filtered);
    assert(filtered.indices.size() == 30);

    Effects settling;
    input.folderFilterActive = false;
    input.coverWallPriorityMode = false;
    input.settleFramesRemaining = 1;
    aura::search::HandleAsyncSearchCoverPreload(input, settling);
    assert(settling.indices.empty());
    return 0;
}
