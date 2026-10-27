#pragma once

namespace aura::search {

struct AsyncSearchRefreshLoadingInput {
    bool refreshResult = false;
};

class AsyncSearchRefreshLoadingEffects {
public:
    virtual ~AsyncSearchRefreshLoadingEffects() = default;
    virtual void finishLoadingMore() = 0;
};

void HandleAsyncSearchRefreshLoading(
    const AsyncSearchRefreshLoadingInput& input,
    AsyncSearchRefreshLoadingEffects& effects);

}  // namespace aura::search
