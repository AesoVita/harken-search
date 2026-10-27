#pragma once

#include <vector>

struct AppState;
struct AudioRecord;

namespace aura::search {

void ApplyPlaylistScopedFilters(
    AppState& state,
    std::vector<AudioRecord>& batch,
    bool rebuildTree);

void FilterResultsByFileTypePolicy(
    const AppState& state,
    std::vector<AudioRecord>& batch);

}  // namespace aura::search
