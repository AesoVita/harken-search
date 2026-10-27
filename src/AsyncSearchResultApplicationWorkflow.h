#pragma once

#include "AsyncSearchResultApplicationAdapter.h"

struct AppState;
struct AsyncSearchResult;

namespace aura::load_all {
class LoadAllBatchRuntime;
}

namespace aura::search {

class AsyncSearchQueryTimingRegistry;

void Apply(
    AppState& state,
    AsyncSearchResult& result,
    aura::load_all::LoadAllBatchRuntime& loadAllRuntime,
    AsyncSearchQueryTimingRegistry& queryTiming,
    int loadAllThreshold,
    bool miniWaveformColumnVisible,
    AsyncSearchResultApplicationEffects effects);

}  // namespace aura::search
