#pragma once

#include <cstdint>

namespace aura::search {

enum class AsyncSearchAppendKind { LoadMore, LoadAll };

struct AsyncSearchAppendInput {
    AsyncSearchAppendKind kind = AsyncSearchAppendKind::LoadMore;
    int loadedCount = 0;
    std::int64_t totalCount = 0;
    bool hasMore = false;
    bool emptyPage = false;
};

class AsyncSearchAppendEffects {
public:
    virtual ~AsyncSearchAppendEffects() = default;
    virtual void appendRows() = 0;
    virtual void updateCounts(std::int64_t totalCount, bool hasMore) = 0;
    virtual void stopAfterEmptyPage() = 0;
    virtual void applyCoverKeyFilter() = 0;
    virtual void loadAllResult(int loadedCount, bool hasMore, bool emptyPage) = 0;
    virtual void finishLoadingMore() = 0;
};

void HandleAsyncSearchAppendResult(
    const AsyncSearchAppendInput& input,
    AsyncSearchAppendEffects& effects);

}  // namespace aura::search
