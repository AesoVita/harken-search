#pragma once

#include "AsyncSearchRefreshMemoryWorkflow.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct AppState;

namespace aura::search {

struct AsyncSearchRefreshMemorySnapshot {
    bool hasMemoryBudget = false;
    std::uint64_t usage = 0;
    std::uint64_t budget = 0;
    std::size_t fallbackLimit = 0;
};

class AsyncSearchRefreshMemoryAdapter final
    : public AsyncSearchRefreshMemoryEffects {
public:
    using SnapshotProvider =
        std::function<AsyncSearchRefreshMemorySnapshot()>;
    using EvictionProvider =
        std::function<std::vector<std::string>(std::uint64_t)>;
    using ReleaseEvicted = std::function<void(const std::string&)>;
    using FallbackSizeProvider = std::function<std::size_t()>;
    using FallbackEraser = std::function<void()>;
    using TrimObserver = std::function<void(std::size_t, std::size_t)>;
    using AppStateReleaseEvicted =
        std::function<void(AppState&, const std::string&)>;

    AsyncSearchRefreshMemoryAdapter(
        AppState& state,
        AppStateReleaseEvicted release);
    AsyncSearchRefreshMemoryAdapter(
        SnapshotProvider snapshot,
        EvictionProvider evict,
        ReleaseEvicted release,
        FallbackSizeProvider fallbackSize,
        FallbackEraser eraseFallbackOldest,
        TrimObserver observeTrim);

    void apply();
    void evictLeastRecentlyUsed(std::uint64_t overBudget) override;
    void trimFallbackCache() override;

private:
    SnapshotProvider snapshot_;
    EvictionProvider evict_;
    ReleaseEvicted release_;
    FallbackSizeProvider fallbackSize_;
    FallbackEraser eraseFallbackOldest_;
    TrimObserver observeTrim_;
    std::size_t fallbackLimit_ = 0;
};

}  // namespace aura::search
