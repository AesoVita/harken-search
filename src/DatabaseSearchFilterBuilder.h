#pragma once

struct AppState;
struct SearchFilter;

namespace aura::search {

SearchFilter BuildCurrentDbFilter(const AppState& state);

}  // namespace aura::search
