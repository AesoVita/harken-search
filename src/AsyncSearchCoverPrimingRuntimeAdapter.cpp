#include "AsyncSearchCoverPrimingRuntimeAdapter.h"

#include "AppState.h"
#include "CoverModule.h"
#include "DevToolsModule.h"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace aura::search {
namespace {

struct CoverPrimeJob final {
    bool active = false;
    std::size_t cursor = 0;
};

CoverPrimeJob g_coverPrimeJob;

}  // namespace

void StartDeferredCoverPriming() {
    g_coverPrimeJob.active = true;
    g_coverPrimeJob.cursor = 0;
}

void PumpDeferredCoverPriming(AppState& state, int maxPerFrame) {
    if (!g_coverPrimeJob.active) return;
    if (state.audioEngine.isPlaying()) {
        maxPerFrame = (std::min)(maxPerFrame, 2);
    }
    const bool readyToWorkPending = [&]() {
        std::lock_guard<std::recursive_mutex> lock(
            state.scanReadyToWorkMutex);
        return state.scanReadyToWorkPending;
    }();
    if (state.warmupFramesRemaining > 0 ||
        state.dbSwitchSettleFramesRemaining > 0 ||
        readyToWorkPending) {
        return;
    }

    const auto startedAt = std::chrono::steady_clock::now();
    const std::size_t total = (std::min)(
        state.searchResults.size(),
        static_cast<std::size_t>(AppState::PAGE_SIZE));
    const std::size_t end = (std::min)(
        g_coverPrimeJob.cursor + static_cast<std::size_t>(maxPerFrame),
        total);
    for (std::size_t i = g_coverPrimeJob.cursor; i < end; ++i) {
        const auto& record = state.searchResults[i];
        if (record.filePath.empty()) continue;
        CoverModule::PrimeFileCoverKeysFromDb(
            record.filePath,
            record.embeddedCoverKey,
            record.embeddedCoverKeyMtime,
            record.folderCoverKey,
            record.folderCoverSrcPath,
            record.folderCoverSrcMtime);
    }
    g_coverPrimeJob.cursor = end;
    if (g_coverPrimeJob.cursor >= total) {
        g_coverPrimeJob.active = false;
    }
    DevTools_LogCoverPrimingMs(
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startedAt)
            .count());
}

}  // namespace aura::search
