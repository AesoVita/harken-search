#include "AsyncSearchPlaylistResultPreparationStateAdapter.h"

#include "AppState.h"
#include "AsyncSearchPlaylistResultPreparationWorkflow.h"
#include "AsyncWorker.h"
#include "DiagnosticsLogger.h"

namespace aura::search {

namespace {

class Effects final : public AsyncSearchPlaylistResultPreparationEffects {
public:
    Effects(
        AppState& state,
        AsyncSearchResult& result,
        AsyncSearchPlaylistResultPreparationStateAdapter::PlaylistFilter
            applyPlaylistFilters,
        AsyncSearchPlaylistResultPreparationStateAdapter::LibraryFilter
            applyLibraryFilters)
        : state_(state),
          result_(result),
          applyPlaylistFilters_(applyPlaylistFilters),
          applyLibraryFilters_(applyLibraryFilters) {}

    void clearPlaylistFolderSearch() override {
        state_.setPlaylistFolderHierarchy(FolderInfo{}, state_.selectedPlaylistId);
        state_.folderSearchHits.clear();
        state_.folderSearchDirectHits.clear();
        state_.folderSearchExpandPaths.clear();
        state_.lastFolderSearch.clear();
    }

    int applyPlaylistFilters(bool rebuildFolderTree) override {
        applyPlaylistFilters_(state_, result_.results, rebuildFolderTree);
        return static_cast<int>(result_.results.size());
    }

    int applyLibraryFilters() override {
        applyLibraryFilters_(state_, result_.results);
        return static_cast<int>(result_.results.size());
    }

    void reportPlaylistRefresh(
        int loadedCount,
        bool rebuildFolderTree) override {
        DIAG_ACTION_FMT(
            "Playlist",
            "ApplyAsyncRefresh req=%llu playlist=%lld finalCount=%d total=%lld hasMore=%d folderTree=%d",
            static_cast<unsigned long long>(result_.requestId),
            static_cast<long long>(state_.selectedPlaylistId),
            loadedCount,
            static_cast<long long>(result_.totalCount),
            result_.hasMore ? 1 : 0,
            rebuildFolderTree ? 1 : 0);
    }

    void reportLibraryRefresh(std::size_t rawCount, int loadedCount) override {
        DIAG_ACTION_FMT(
            "Playlist",
            "ApplyAsyncLibraryRefresh req=%llu raw=%zu finalCount=%d total=%lld hasMore=%d",
            static_cast<unsigned long long>(result_.requestId),
            rawCount,
            loadedCount,
            static_cast<long long>(result_.totalCount),
            result_.hasMore ? 1 : 0);
    }

private:
    AppState& state_;
    AsyncSearchResult& result_;
    AsyncSearchPlaylistResultPreparationStateAdapter::PlaylistFilter
        applyPlaylistFilters_;
    AsyncSearchPlaylistResultPreparationStateAdapter::LibraryFilter
        applyLibraryFilters_;
};

}  // namespace

AsyncSearchPlaylistResultPreparationStateAdapter::
    AsyncSearchPlaylistResultPreparationStateAdapter(
        AppState& state,
        AsyncSearchResult& result,
        AsyncSearchResultDispatch dispatch,
        int loadAllThreshold,
        PlaylistFilter applyPlaylistFilters,
        LibraryFilter applyLibraryFilters)
    : state_(state),
      result_(result),
      dispatch_(dispatch),
      loadAllThreshold_(loadAllThreshold),
      applyPlaylistFilters_(applyPlaylistFilters),
      applyLibraryFilters_(applyLibraryFilters) {}

void AsyncSearchPlaylistResultPreparationStateAdapter::apply() {
    Effects effects(
        state_, result_, applyPlaylistFilters_, applyLibraryFilters_);
    const auto preparation = HandleAsyncSearchPlaylistResultPreparation(
        {state_.viewMode == ViewMode::Playlist,
         state_.viewMode == ViewMode::PlaylistLibrary,
         dispatch_ == AsyncSearchResultDispatch::Refresh,
         dispatch_ == AsyncSearchResultDispatch::LoadAll,
         result_.totalCount,
         loadAllThreshold_,
         result_.results.size()},
        effects);
    if (preparation.prepared) {
        result_.loadedCount = preparation.loadedCount;
    }
}

}  // namespace aura::search
