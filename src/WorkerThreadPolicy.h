#pragma once

#include <algorithm>
#include <thread>

namespace aura {

// @stage7-contract
// API contract: worker-thread policy helpers are pure synchronous calculations
// over copied hardware/request counts. They allocate or start no threads and
// return values clamped to the established UI/audio contention limits.

// Interactive list waveform workers: leave 2 cores for the UI + audio
// threads, floor at 3 so even small machines beat the old hardcoded 2,
// ceiling at 8 to bound contention on the shared DB.
inline int ClampWaveformWorkerThreads(unsigned hardwareConcurrency) {
    unsigned hc = hardwareConcurrency == 0u ? 4u : hardwareConcurrency;
    return std::clamp((int)hc - 2, 3, 8);
}

// Background backfill workers run at lower priority; use half the cores,
// floor 2, ceiling 6.
inline int ClampBackfillWorkerThreads(unsigned hardwareConcurrency) {
    unsigned hc = hardwareConcurrency == 0u ? 4u : hardwareConcurrency;
    return std::clamp((int)hc / 2, 2, 6);
}

inline int RecommendedWaveformWorkerThreads() {
    return ClampWaveformWorkerThreads(std::thread::hardware_concurrency());
}

inline int RecommendedBackfillWorkerThreads() {
    return ClampBackfillWorkerThreads(std::thread::hardware_concurrency());
}

inline int ResolveScanBackfillWorkerThreads(int requestedThreads,
                                            unsigned hardwareConcurrency) {
    if (requestedThreads > 0) {
        return std::clamp(requestedThreads, 1, 16);
    }
    return ClampBackfillWorkerThreads(hardwareConcurrency);
}

}
