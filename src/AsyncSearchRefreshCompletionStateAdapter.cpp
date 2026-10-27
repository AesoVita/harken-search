#include "AsyncSearchRefreshCompletionStateAdapter.h"

#include "AppState.h"

#include <utility>

namespace aura::search {

namespace {

class Effects final : public AsyncSearchRefreshCompletionEffects {
public:
    Effects(AppState& state, AsyncSearchRefreshCompletionStateAdapter::RefreshRequest request)
        : state_(state), requestCoverWallRefresh_(std::move(request)) {}

    void markGenerationApplied() override {
        state_.filterQueryAppliedGeneration = state_.filterQueryGeneration;
    }
    void requestCoverWallRefresh() override {
        requestCoverWallRefresh_(state_);
    }
    void setError(const std::string& error) override {
        state_.filterQueryState = FilterQueryState::Error;
        state_.filterQueryError = error;
    }
    void setReady() override { state_.filterQueryState = FilterQueryState::Ready; }

private:
    AppState& state_;
    AsyncSearchRefreshCompletionStateAdapter::RefreshRequest
        requestCoverWallRefresh_;
};

}  // namespace

AsyncSearchRefreshCompletionStateAdapter::
    AsyncSearchRefreshCompletionStateAdapter(
        AppState& state,
        AdvancedFilterPredicate hasAdvancedFilters,
        RefreshRequest requestCoverWallRefresh)
    : state_(state),
      hasAdvancedFilters_(std::move(hasAdvancedFilters)),
      requestCoverWallRefresh_(std::move(requestCoverWallRefresh)) {}

void AsyncSearchRefreshCompletionStateAdapter::apply() {
    Effects effects(state_, requestCoverWallRefresh_);
    HandleAsyncSearchRefreshCompletion(
        {state_.viewMode == ViewMode::Database,
         state_.viewMode == ViewMode::Playlist,
         state_.viewMode == ViewMode::PlaylistLibrary,
         state_.folderFilterActive,
         hasAdvancedFilters_(state_),
         state_.coverFilterActive,
         state_.findSameFilenameActive,
         state_.sameFolderScanActive,
         state_.sameFolderScanDone,
         state_.searchResults.empty(),
         state_.database.getLastError()},
        effects);
}

}  // namespace aura::search
