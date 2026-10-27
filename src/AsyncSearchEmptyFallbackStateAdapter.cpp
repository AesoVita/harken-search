#include "AsyncSearchEmptyFallbackStateAdapter.h"

#include "AppState.h"

namespace aura::search {

namespace {

class Effects final : public AsyncSearchEmptyFallbackEffects {
public:
    explicit Effects(AppState& state) : state_(state) {}

    void clearFallback() override {
        state_.coverWall.fallbackItems.clear();
        state_.coverWall.fallbackResultsGen = 0;
        state_.coverWall.fallbackFacetsGen = 0;
    }

private:
    AppState& state_;
};

}  // namespace

AsyncSearchEmptyFallbackStateAdapter::
    AsyncSearchEmptyFallbackStateAdapter(AppState& state)
    : state_(state) {}

void AsyncSearchEmptyFallbackStateAdapter::apply() {
    Effects effects(state_);
    HandleAsyncSearchEmptyFallback(
        {state_.viewMode == ViewMode::Database,
         state_.searchResults.empty(),
         state_.totalFileCount},
        effects);
}

}  // namespace aura::search
