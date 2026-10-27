#include "AsyncSearchRefreshMemoryStateAdapter.h"

#include "AppState.h"
#include "DiagnosticsLogger.h"
#include "MemoryBudget.h"

#include <utility>

namespace aura::search {

AsyncSearchRefreshMemoryAdapter::AsyncSearchRefreshMemoryAdapter(
    AppState& state,
    AppStateReleaseEvicted release)
    : AsyncSearchRefreshMemoryAdapter(
          [&state] {
              const bool hasMemoryBudget = state.memoryBudget != nullptr;
              return AsyncSearchRefreshMemorySnapshot{
                  hasMemoryBudget,
                  hasMemoryBudget
                      ? state.memoryBudget->getCurrentUsage(
                            MemoryBudget::CacheType::Waveform)
                      : 0,
                  hasMemoryBudget
                      ? state.memoryBudget->getBudgetConfig().waveformBudget
                      : 0,
                  static_cast<std::size_t>(AppState::MAX_WAVEFORM_CACHE_SIZE)};
          },
          [&state](std::uint64_t overBudget) {
              const auto evicted = state.memoryBudget->evictLRU(
                  MemoryBudget::CacheType::Waveform, overBudget);
              std::vector<std::string> keys;
              keys.reserve(evicted.size());
              for (const auto& entry : evicted) keys.push_back(entry.key);
              return keys;
          },
          [&state, release = std::move(release)](const std::string& key) {
              release(state, key);
          },
          [&state] { return state.waveformCache.size(); },
          [&state] { state.waveformCache.erase(state.waveformCache.begin()); },
          [](std::size_t size, std::size_t limit) {
              DIAG_ACTION_FMT("MiniWave", "dataCacheTrim size=%zu limit=%zu",
                              size, limit);
          }) {}

AsyncSearchRefreshMemoryStateAdapter::AsyncSearchRefreshMemoryStateAdapter(
    AppState& state,
    AsyncSearchRefreshMemoryAdapter::AppStateReleaseEvicted release)
    : state_(state), release_(std::move(release)) {}

void AsyncSearchRefreshMemoryStateAdapter::apply() {
    AsyncSearchRefreshMemoryAdapter adapter(state_, std::move(release_));
    adapter.apply();
}

}  // namespace aura::search
