#pragma once

#include "AsyncSearchResultDispatchWorkflow.h"

#include <vector>

struct AppState;
struct AsyncSearchResult;
struct AudioRecord;

namespace aura::search {

class AsyncSearchPlaylistResultPreparationStateAdapter final {
public:
    using PlaylistFilter =
        void (*)(AppState&, std::vector<AudioRecord>&, bool);
    using LibraryFilter =
        void (*)(const AppState&, std::vector<AudioRecord>&);

    AsyncSearchPlaylistResultPreparationStateAdapter(
        AppState& state,
        AsyncSearchResult& result,
        AsyncSearchResultDispatch dispatch,
        int loadAllThreshold,
        PlaylistFilter applyPlaylistFilters,
        LibraryFilter applyLibraryFilters);

    void apply();

private:
    AppState& state_;
    AsyncSearchResult& result_;
    AsyncSearchResultDispatch dispatch_;
    int loadAllThreshold_;
    PlaylistFilter applyPlaylistFilters_;
    LibraryFilter applyLibraryFilters_;
};

}  // namespace aura::search
