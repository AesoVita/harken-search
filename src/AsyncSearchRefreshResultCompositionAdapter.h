#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct AppState;
struct AsyncSearchResult;
struct SearchFilter;

namespace aura::search {

struct AsyncSearchRefreshResultCompositionEffects final {
    using StartDeferredCoverPriming = std::function<void()>;
    using ApplyCoverKeyFilter = std::function<void(AppState&)>;
    using ReadStatePredicate = std::function<bool(const AppState&)>;
    using ReadFlag = std::function<bool()>;
    using CanonicalizeFolderPath =
        std::function<std::string(const std::string&)>;
    using RequestRefresh = std::function<void(AppState&)>;
    using SubmitCountRepair = std::function<std::uint64_t(
        AppState&,
        const SearchFilter&,
        bool)>;
    using ScheduleWaveformDetail =
        std::function<void(AppState&, const std::string&, int)>;
    using ResetWaveformZoom = std::function<void(AppState&)>;
    using MarkFilePlayed =
        std::function<void(AppState&, const std::string&, std::int64_t)>;
    using ReleaseWaveformEvicted =
        std::function<void(AppState&, const std::string&)>;

    StartDeferredCoverPriming startDeferredCoverPriming;
    ApplyCoverKeyFilter applyCoverKeyFilter;
    ReadStatePredicate readIntegratedMode;
    ReadStatePredicate readActiveDatabaseFilter;
    CanonicalizeFolderPath canonicalizeFolderPath;
    ReadStatePredicate hasActiveDatabaseFilter;
    ReadStatePredicate hasAdvancedFilters;
    ReadFlag isCoverWallPriorityMode;
    SubmitCountRepair submitCountRepair;
    RequestRefresh requestCoverWallRefresh;
    ScheduleWaveformDetail scheduleWaveformDetail;
    ResetWaveformZoom resetWaveformZoom;
    MarkFilePlayed markFilePlayed;
    ReleaseWaveformEvicted releaseWaveformEvicted;
};

class AsyncSearchRefreshResultCompositionAdapter final {
public:
    AsyncSearchRefreshResultCompositionAdapter(
        AppState& state,
        AsyncSearchResult& result,
        bool miniWaveformColumnVisible,
        AsyncSearchRefreshResultCompositionEffects effects);

    void apply();

private:
    AppState& state_;
    AsyncSearchResult& result_;
    bool miniWaveformColumnVisible_ = false;
    AsyncSearchRefreshResultCompositionEffects effects_;
};

}  // namespace aura::search
