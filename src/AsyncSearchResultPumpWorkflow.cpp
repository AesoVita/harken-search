#include "AsyncSearchResultPumpWorkflow.h"

#include "AppState.h"
#include "AsyncWorker.h"
#include "AsyncSearchQueryTimingRegistry.h"

#include <chrono>
#include <cstdio>

namespace aura::search {

namespace {

uint64_t ExpectedRequestId(const AppState& state) {
    return state.pendingSearchRequestId != 0
        ? state.pendingSearchRequestId
        : state.pendingFacetRequestId;
}

}  // namespace

bool Pump(
    AppState& state,
    const AsyncSearchResultApply& applyResult,
    const AsyncSearchResultPerfLogger& logPerformance) {
    const uint64_t expectedId = ExpectedRequestId(state);
    if (expectedId == 0) return false;

    AsyncSearchResult result;
    const auto beforePoll = std::chrono::steady_clock::now();
    if (!state.searchWorker.pollResult(result, expectedId)) return false;
    const auto afterPoll = std::chrono::steady_clock::now();

    const auto beforeApply = std::chrono::steady_clock::now();
    applyResult(state, result);
    const auto afterApply = std::chrono::steady_clock::now();

    char perfLine[256] = {};
    const auto pollMs = std::chrono::duration<double, std::milli>(
        afterPoll - beforePoll).count();
    const auto applyMs = std::chrono::duration<double, std::milli>(
        afterApply - beforeApply).count();
    std::snprintf(
        perfLine,
        sizeof(perfLine),
        "[AsyncComplete] pollResult: %.2f ms, ApplyAsyncSearchResult: %.2f ms, items=%zu\n",
        pollMs,
        applyMs,
        result.results.size());
    if (logPerformance) logPerformance(perfLine);
    return true;
}

bool PumpOne(
    AppState& state,
    const AsyncSearchResultApply& applyResult) {
    const uint64_t expectedId = ExpectedRequestId(state);
    if (expectedId == 0) return false;

    AsyncSearchResult result;
    if (!state.searchWorker.pollResult(result, expectedId)) return false;
    applyResult(state, result);
    return true;
}

void CancelPending(AppState& state, AsyncSearchQueryTimingRegistry& queryTiming) {
    state.searchWorker.cancelActiveSearch();
    state.pendingSearchRequestId = 0;
    state.pendingFacetRequestId = 0;
    state.asyncSearchInProgress = false;
    queryTiming.clear();
}

}  // namespace aura::search
