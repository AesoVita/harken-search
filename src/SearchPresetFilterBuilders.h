#pragma once

struct AppState;
struct SearchFilter;

namespace aura::search {

SearchFilter BuildCurrentPlaylistSearchFilter(const AppState& state);
SearchFilter BuildCurrentSmartCollectionFilter(const AppState& state);

}  // namespace aura::search
