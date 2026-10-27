#include "AsyncSearchQueryTimingRegistry.h"

#include <utility>

namespace aura::search {

AsyncSearchQueryTimingRegistry::AsyncSearchQueryTimingRegistry()
    : AsyncSearchQueryTimingRegistry([] { return Clock::now(); }) {}

AsyncSearchQueryTimingRegistry::AsyncSearchQueryTimingRegistry(NowProvider now)
    : now_(std::move(now)) {}

void AsyncSearchQueryTimingRegistry::begin(std::uint64_t requestId) {
    starts_[requestId] = now_();
}

double AsyncSearchQueryTimingRegistry::finishMilliseconds(
    std::uint64_t requestId) {
    const auto it = starts_.find(requestId);
    if (it == starts_.end()) return 0.0;
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now_() - it->second);
    starts_.erase(it);
    return static_cast<double>(elapsed.count());
}

void AsyncSearchQueryTimingRegistry::clear() {
    starts_.clear();
}

}  // namespace aura::search
