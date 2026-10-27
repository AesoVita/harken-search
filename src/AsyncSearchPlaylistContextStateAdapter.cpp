#include "AsyncSearchPlaylistContextStateAdapter.h"

#include "AppState.h"
#include "AsyncWorker.h"
#include "DiagnosticsLogger.h"

namespace aura::search {

namespace {

class Effects final : public AsyncSearchPlaylistContextEffects {
public:
    Effects(AppState& state) : state_(state) {}

    void applyPlaylistContext(
        std::int64_t playlistId,
        bool aggregatedView,
        bool folderFilterActive,
        const std::string& selectedFolderPath) override {
        state_.viewMode = ViewMode::Playlist;
        state_.selectedPlaylistId = playlistId;
        state_.playlistAggregatedView = aggregatedView;
        state_.folderFilterActive = folderFilterActive;
        state_.selectedFolderPath = selectedFolderPath;
    }
    void reportPlaylistContext(
        const AsyncSearchPlaylistContextInput& input) override {
        DIAG_ACTION_FMT(
            "Playlist",
            "ApplyAsyncContext req=%llu playlist=%lld aggregated=%d folderFilter=%d selectedFolder=%s kind=%d",
            static_cast<unsigned long long>(input.requestId),
            static_cast<long long>(input.playlistId),
            input.aggregatedView ? 1 : 0,
            input.folderFilterActive ? 1 : 0,
            input.selectedFolderPath.empty() ? "<none>"
                                               : input.selectedFolderPath.c_str(),
            input.resultKind);
    }
    void applyLibraryContext() override {
        state_.viewMode = ViewMode::PlaylistLibrary;
        state_.selectedPlaylistId = -1;
        state_.playlistAggregatedView = false;
    }
    void reportLibraryContext(
        const AsyncSearchPlaylistContextInput& input) override {
        DIAG_ACTION_FMT(
            "Playlist",
            "ApplyAsyncLibraryContext req=%llu kind=%d",
            static_cast<unsigned long long>(input.requestId),
            input.resultKind);
    }

private:
    AppState& state_;
};

}  // namespace

AsyncSearchPlaylistContextStateAdapter::AsyncSearchPlaylistContextStateAdapter(
    AppState& state,
    AsyncSearchResult& result)
    : state_(state), result_(result) {}

void AsyncSearchPlaylistContextStateAdapter::apply() {
    AsyncSearchPlaylistContextKind kind =
        AsyncSearchPlaylistContextKind::None;
    if (result_.viewMode == static_cast<int>(ViewMode::Playlist) &&
        result_.playlistId > 0) {
        kind = AsyncSearchPlaylistContextKind::Playlist;
    } else if (result_.viewMode == static_cast<int>(ViewMode::PlaylistLibrary)) {
        kind = AsyncSearchPlaylistContextKind::Library;
    }

    Effects effects(state_);
    HandleAsyncSearchPlaylistContext(
        {kind,
         result_.requestId,
         result_.playlistId,
         result_.playlistAggregatedView,
         result_.folderFilterActive,
         result_.selectedFolderPath,
         static_cast<int>(result_.kind)},
        effects);
}

}  // namespace aura::search
