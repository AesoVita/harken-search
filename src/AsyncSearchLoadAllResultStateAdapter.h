#pragma once

struct AppState;

namespace aura::load_all {
class LoadAllBatchRuntime;
}

namespace aura::search {

class AsyncSearchLoadAllResultStateAdapter final {
public:
    AsyncSearchLoadAllResultStateAdapter(
        AppState& state,
        aura::load_all::LoadAllBatchRuntime& runtime);

    bool acceptsResult() const;
    void observeResult(int loadedCount, bool hasMore, bool emptyPage);

private:
    AppState& state_;
    aura::load_all::LoadAllBatchRuntime& runtime_;
};

}  // namespace aura::search
