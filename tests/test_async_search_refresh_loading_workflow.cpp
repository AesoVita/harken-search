#include "AsyncSearchRefreshLoadingWorkflow.h"

#include <cassert>

struct Effects final : aura::search::AsyncSearchRefreshLoadingEffects {
    int finished = 0;
    void finishLoadingMore() override { ++finished; }
};

int main() {
    Effects refresh;
    aura::search::HandleAsyncSearchRefreshLoading({true}, refresh);
    assert(refresh.finished == 1);

    Effects append;
    aura::search::HandleAsyncSearchRefreshLoading({false}, append);
    assert(append.finished == 0);
    return 0;
}
