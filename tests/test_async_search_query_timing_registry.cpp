#include "AsyncSearchQueryTimingRegistry.h"

#include <cassert>
#include <chrono>

int main() {
    using Registry = aura::search::AsyncSearchQueryTimingRegistry;
    Registry::TimePoint now{};
    Registry registry([&now] { return now; });

    registry.begin(42);
    now += std::chrono::milliseconds(125);
    assert(registry.finishMilliseconds(42) == 125.0);
    assert(registry.finishMilliseconds(42) == 0.0);

    registry.begin(1);
    registry.begin(2);
    registry.clear();
    assert(registry.finishMilliseconds(1) == 0.0);
    assert(registry.finishMilliseconds(2) == 0.0);
    return 0;
}
