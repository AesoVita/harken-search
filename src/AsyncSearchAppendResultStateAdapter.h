#pragma once

#include "AsyncSearchAppendResultWorkflow.h"

#include <functional>

struct AppState;
struct AsyncSearchResult;

namespace aura::search {

class AsyncSearchAppendResultStateAdapter final
    : private AsyncSearchAppendEffects {
public:
    using ApplyCoverKeyFilter = std::function<void(AppState&)>;
    using LoadAllObserver = std::function<void(int, bool, bool)>;

    AsyncSearchAppendResultStateAdapter(
        AppState& state,
        AsyncSearchResult& result,
        AsyncSearchAppendKind kind,
        ApplyCoverKeyFilter applyCoverKeyFilter,
        LoadAllObserver observeLoadAll);

    void apply();

private:
    void appendRows() override;
    void updateCounts(std::int64_t totalCount, bool hasMore) override;
    void stopAfterEmptyPage() override;
    void applyCoverKeyFilter() override;
    void loadAllResult(int loadedCount, bool hasMore, bool emptyPage) override;
    void finishLoadingMore() override;

    AppState& state_;
    AsyncSearchResult& result_;
    AsyncSearchAppendKind kind_;
    ApplyCoverKeyFilter applyCoverKeyFilter_;
    LoadAllObserver observeLoadAll_;
};

}  // namespace aura::search
