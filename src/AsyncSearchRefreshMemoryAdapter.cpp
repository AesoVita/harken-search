#include "AsyncSearchRefreshMemoryAdapter.h"

#include <utility>

namespace aura::search {

AsyncSearchRefreshMemoryAdapter::AsyncSearchRefreshMemoryAdapter(
    SnapshotProvider snapshot,
    EvictionProvider evict,
    ReleaseEvicted release,
    FallbackSizeProvider fallbackSize,
    FallbackEraser eraseFallbackOldest,
    TrimObserver observeTrim)
    : snapshot_(std::move(snapshot)),
      evict_(std::move(evict)),
      release_(std::move(release)),
      fallbackSize_(std::move(fallbackSize)),
      eraseFallbackOldest_(std::move(eraseFallbackOldest)),
      observeTrim_(std::move(observeTrim)) {}

void AsyncSearchRefreshMemoryAdapter::apply() {
    const auto snapshot = snapshot_();
    fallbackLimit_ = snapshot.fallbackLimit;
    HandleAsyncSearchRefreshMemory(
        {snapshot.hasMemoryBudget,
         snapshot.usage,
         snapshot.budget,
         !snapshot.hasMemoryBudget && fallbackSize_() > fallbackLimit_},
        *this);
}

void AsyncSearchRefreshMemoryAdapter::evictLeastRecentlyUsed(
    std::uint64_t overBudget) {
    for (const auto& key : evict_(overBudget)) {
        release_(key);
    }
}

void AsyncSearchRefreshMemoryAdapter::trimFallbackCache() {
    const auto initialSize = fallbackSize_();
    if (initialSize > fallbackLimit_) {
        observeTrim_(initialSize, fallbackLimit_);
    }
    while (fallbackSize_() > fallbackLimit_) {
        eraseFallbackOldest_();
    }
}

}  // namespace aura::search
