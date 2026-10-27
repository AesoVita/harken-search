#include "AsyncSearchResultApplicationWorkflow.h"

#include "AsyncSearchQueryTimingRegistry.h"
#include "LoadAllBatchRuntime.h"

#include <utility>

namespace aura::search {

void Apply(
    AppState& state,
    AsyncSearchResult& result,
    aura::load_all::LoadAllBatchRuntime& loadAllRuntime,
    AsyncSearchQueryTimingRegistry& queryTiming,
    int loadAllThreshold,
    bool miniWaveformColumnVisible,
    AsyncSearchResultApplicationEffects effects) {
    AsyncSearchResultApplicationAdapter adapter(
        state,
        result,
        loadAllRuntime,
        queryTiming,
        loadAllThreshold,
        miniWaveformColumnVisible,
        std::move(effects));
    adapter.apply();
}

}  // namespace aura::search
