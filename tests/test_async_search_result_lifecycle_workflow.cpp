#include "AsyncSearchResultLifecycleWorkflow.h"

#include <cassert>

struct Effects final : aura::search::AsyncSearchResultLifecycleEffects {
    int idleCalls = 0;
    int durationCalls = 0;
    double durationMs = -1.0;
    int failureCalls = 0;
    int rejectCalls = 0;

    void clearPrimarySearchProgress() override { ++idleCalls; }
    void recordQueryDuration(double durationMsValue) override {
        ++durationCalls;
        durationMs = durationMsValue;
    }
    void applyFailureState() override { ++failureCalls; }
    void rejectStaleLoadAll() override { ++rejectCalls; }
};

int main() {
    Effects effects;
    assert(!aura::search::HandleAsyncSearchResultLifecycle(
        {false, true, false, true, 17.5}, effects));
    assert(effects.idleCalls == 1);
    assert(effects.durationCalls == 1);
    assert(effects.durationMs == 17.5);
    assert(effects.failureCalls == 0);

    effects = Effects{};
    assert(!aura::search::HandleAsyncSearchResultLifecycle(
        {true, true, false, true, 2.0}, effects));
    assert(effects.idleCalls == 0);

    effects = Effects{};
    assert(aura::search::HandleAsyncSearchResultLifecycle(
        {false, false, false, true, 4.0}, effects));
    assert(effects.idleCalls == 1);
    assert(effects.durationCalls == 1);
    assert(effects.failureCalls == 1);

    effects = Effects{};
    assert(aura::search::HandleAsyncSearchResultLifecycle(
        {false, true, true, false, 1.0}, effects));
    assert(effects.rejectCalls == 1);
    return 0;
}
