#pragma once

#include "AsyncSearchRefreshMemoryAdapter.h"

namespace aura::search {

class AsyncSearchRefreshMemoryStateAdapter final {
public:
    AsyncSearchRefreshMemoryStateAdapter(
        AppState& state,
        AsyncSearchRefreshMemoryAdapter::AppStateReleaseEvicted release);

    void apply();

private:
    AppState& state_;
    AsyncSearchRefreshMemoryAdapter::AppStateReleaseEvicted release_;
};

}  // namespace aura::search
