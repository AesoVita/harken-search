#pragma once

struct AppState;
struct SearchFilter;

namespace aura::search {

void ApplyFileTypePolicyToFilter(
    const AppState& state,
    SearchFilter& filter);

}  // namespace aura::search
