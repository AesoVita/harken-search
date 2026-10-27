#include "AsyncSearchEmptyFallbackWorkflow.h"

#include <cassert>

struct Effects final : aura::search::AsyncSearchEmptyFallbackEffects {
    int clears = 0;
    void clearFallback() override { ++clears; }
};

int main() {
    Effects empty;
    aura::search::HandleAsyncSearchEmptyFallback({true, true, 0}, empty);
    assert(empty.clears == 1);

    Effects nonEmpty;
    aura::search::HandleAsyncSearchEmptyFallback({true, false, 0}, nonEmpty);
    assert(nonEmpty.clears == 0);

    Effects nonDatabase;
    aura::search::HandleAsyncSearchEmptyFallback({false, true, 0}, nonDatabase);
    assert(nonDatabase.clears == 0);
    return 0;
}
