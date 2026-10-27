#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace aura::search {

class AsyncSearchQueryTimingRegistry {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using NowProvider = std::function<TimePoint()>;

    AsyncSearchQueryTimingRegistry();
    explicit AsyncSearchQueryTimingRegistry(NowProvider now);

    void begin(std::uint64_t requestId);
    double finishMilliseconds(std::uint64_t requestId);
    void clear();

private:
    NowProvider now_;
    std::unordered_map<std::uint64_t, TimePoint> starts_;
};

}  // namespace aura::search
