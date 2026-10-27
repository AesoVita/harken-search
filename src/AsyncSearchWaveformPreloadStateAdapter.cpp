#include "AsyncSearchWaveformPreloadStateAdapter.h"

#include "AppState.h"

namespace aura::search {

AsyncSearchWaveformPreloadStateAdapter::
    AsyncSearchWaveformPreloadStateAdapter(
        AppState& state,
        bool columnVisible)
    : state_(state),
      columnVisible_(columnVisible) {}

void AsyncSearchWaveformPreloadStateAdapter::apply() {
    HandleAsyncSearchWaveformPreload(
        {columnVisible_,
         state_.dbSwitchSettleFramesRemaining,
         state_.searchResults.size()},
        *this);
}

void AsyncSearchWaveformPreloadStateAdapter::enqueueResult(std::size_t index) {
    if (index >= state_.searchResults.size()) return;
    const auto& record = state_.searchResults[index];
    if (!record.filePath.empty()) {
        state_.addPendingWaveform(record.filePath, true);
    }
}

}  // namespace aura::search
