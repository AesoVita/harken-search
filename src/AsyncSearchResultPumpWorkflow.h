#pragma once

#include <functional>

struct AppState;
struct AsyncSearchResult;
namespace aura::search { class AsyncSearchQueryTimingRegistry; }

namespace aura::search {

using AsyncSearchResultApply =
    std::function<void(AppState&, AsyncSearchResult&)>;
using AsyncSearchResultPerfLogger = std::function<void(const char*)>;

bool Pump(
    AppState& state,
    const AsyncSearchResultApply& applyResult,
    const AsyncSearchResultPerfLogger& logPerformance);

bool PumpOne(
    AppState& state,
    const AsyncSearchResultApply& applyResult);

void CancelPending(AppState& state, AsyncSearchQueryTimingRegistry& queryTiming);

}  // namespace aura::search
