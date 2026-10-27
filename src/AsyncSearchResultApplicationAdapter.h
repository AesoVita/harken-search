#pragma once

#include "AsyncSearchRefreshResultCompositionAdapter.h"

#include <functional>
#include <vector>

struct AppState;
struct AsyncSearchResult;
struct AudioRecord;

namespace aura::load_all {
class LoadAllBatchRuntime;
}

namespace aura::search {

class AsyncSearchQueryTimingRegistry;

struct AsyncSearchResultApplicationEffects final {
    using PlaylistFilter =
        void (*)(AppState&, std::vector<AudioRecord>&, bool);
    using LibraryFilter =
        void (*)(const AppState&, std::vector<AudioRecord>&);
    using FacetWarmStartPredicate =
        std::function<bool(const AppState&, const AsyncSearchResult&)>;
    using StateAction = std::function<void(AppState&)>;

    PlaylistFilter applyPlaylistFilters;
    LibraryFilter applyLibraryFilters;
    FacetWarmStartPredicate inconsistentFacetWarmStart;
    StateAction requestCoverWallRefresh;
    StateAction applyCoverKeyFilter;
    AsyncSearchRefreshResultCompositionEffects refreshEffects;
};

class AsyncSearchResultApplicationAdapter final {
public:
    AsyncSearchResultApplicationAdapter(
        AppState& state,
        AsyncSearchResult& result,
        aura::load_all::LoadAllBatchRuntime& loadAllRuntime,
        AsyncSearchQueryTimingRegistry& queryTiming,
        int loadAllThreshold,
        bool miniWaveformColumnVisible,
        AsyncSearchResultApplicationEffects effects);

    void apply();

private:
    AppState& state_;
    AsyncSearchResult& result_;
    aura::load_all::LoadAllBatchRuntime& loadAllRuntime_;
    AsyncSearchQueryTimingRegistry& queryTiming_;
    int loadAllThreshold_ = 0;
    bool miniWaveformColumnVisible_ = false;
    AsyncSearchResultApplicationEffects effects_;
};

}  // namespace aura::search
