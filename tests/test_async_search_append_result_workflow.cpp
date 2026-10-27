#include "AsyncSearchAppendResultWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchAppendEffects {
    std::vector<int> events;
    void appendRows() override { events.push_back(1); }
    void updateCounts(std::int64_t, bool) override { events.push_back(2); }
    void stopAfterEmptyPage() override { events.push_back(3); }
    void applyCoverKeyFilter() override { events.push_back(4); }
    void loadAllResult(int, bool, bool) override { events.push_back(5); }
    void finishLoadingMore() override { events.push_back(6); }
};

int main() {
    Effects loadAll;
    aura::search::HandleAsyncSearchAppendResult(
        {aura::search::AsyncSearchAppendKind::LoadAll, 0, 0, true, true}, loadAll);
    assert((loadAll.events == std::vector<int>{1, 2, 3, 4, 5}));
    Effects loadMore;
    aura::search::HandleAsyncSearchAppendResult(
        {aura::search::AsyncSearchAppendKind::LoadMore, 2, 5, true, false}, loadMore);
    assert((loadMore.events == std::vector<int>{1, 2, 4, 6}));
    return 0;
}
