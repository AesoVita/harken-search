#pragma once

#include "AsyncSearchPlaylistContextWorkflow.h"

struct AppState;
struct AsyncSearchResult;

namespace aura::search {

class AsyncSearchPlaylistContextStateAdapter final {
public:
    AsyncSearchPlaylistContextStateAdapter(
        AppState& state,
        AsyncSearchResult& result);

    void apply();

private:
    AppState& state_;
    AsyncSearchResult& result_;
};

}  // namespace aura::search
