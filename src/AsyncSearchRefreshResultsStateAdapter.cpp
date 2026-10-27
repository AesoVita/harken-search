#include "AsyncSearchRefreshResultsStateAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"

#include <utility>

namespace aura::search {

namespace {

class Effects final : public AsyncSearchRefreshResultsEffects {
public:
    Effects(AppState& state, AsyncSearchResult& result)
        : state_(state), result_(result) {}

    void replaceRows() override {
        state_.searchResults = std::move(result_.results);
    }
    void throttleMiniWaveforms(int frames) override {
        state_.postSearchMiniWaveThrottleFrames =
            (std::max)(state_.postSearchMiniWaveThrottleFrames, frames);
    }
    void resultsChanged() override { state_.bumpSearchResultsGen(); }
    void setBuiltRevision(std::uint64_t revision) override {
        state_.setSearchResultsBuiltRevision(revision);
    }
    void setLoadedState(int loadedCount, bool hasMore) override {
        state_.loadedCount = loadedCount;
        state_.hasMoreResults = hasMore;
    }
    void clearSelection() override {
        state_.selectedResultIndex = -1;
        state_.selectedIndicesSet.clear();
        state_.lastClickedIndex = -1;
    }

private:
    AppState& state_;
    AsyncSearchResult& result_;
};

}  // namespace

AsyncSearchRefreshResultsStateAdapter::
    AsyncSearchRefreshResultsStateAdapter(
        AppState& state,
        AsyncSearchResult& result)
    : state_(state), result_(result) {}

void AsyncSearchRefreshResultsStateAdapter::apply() {
    Effects effects(state_, result_);
    HandleAsyncSearchRefreshResults(
        {result_.loadedCount, result_.hasMore, result_.requestedAtDbRevision},
        effects);
}

}  // namespace aura::search
