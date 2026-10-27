#include "AsyncSearchWaveformPreloadWorkflow.h"

#include <algorithm>

namespace aura::search {

void HandleAsyncSearchWaveformPreload(
    const AsyncSearchWaveformPreloadInput& input,
    AsyncSearchWaveformPreloadEffects& effects) {
    if (!input.columnVisible || input.settleFramesRemaining > 0) return;
    const std::size_t count = (std::min)(input.resultCount, std::size_t{5});
    for (std::size_t index = 0; index < count; ++index) effects.enqueueResult(index);
}

}  // namespace aura::search
