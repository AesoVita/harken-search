#include "CoverWallRefreshStateAdapter.h"

#include "AppState.h"
#include "CoverModule.h"
#include "DiagnosticsLogger.h"

void ForceCoverWallReload(AppState& state) {
    state.coverWallDatabaseInvalidationRequested = true;
    state.invalidateCoverWallAfterDatabaseContentChange();
}

namespace aura::cover_wall {

void RequestDisplayRefresh(AppState& state) {
    state.coverWallDisplayRefreshRequested = true;
    state.needsCoverWallReload = true;
    state.coverWall.preRequestCursor = 0;
    state.coverWall.lastPreRequestTotalItems = -1;
    state.coverWall.lastFirstScreenStart = static_cast<std::size_t>(-1);
    state.coverWall.lastFirstScreenEnd = 0;
}

void RequestDisplayRefreshForExplicitRetry(AppState& state) {
    RequestDisplayRefresh(state);
    state.coverWall.failedTexCache.clear();
    state.coverWall.failCountPeriod = 0;
    CoverModule::ResetFailedCache();
#if AURA_BUILD_DIAGNOSTICS
    DIAG_ACTION("CoverWall", "ExplicitRetryRefreshRequested");
#endif
}

void RequestDatabaseRefreshForExplicitRetry(AppState& state) {
    RequestDatabaseInvalidation(state);
    state.coverWall.failedTexCache.clear();
    state.coverWall.failCountPeriod = 0;
    CoverModule::ResetFailedCache();
#if AURA_BUILD_DIAGNOSTICS
    DIAG_ACTION("CoverWall", "ExplicitDatabaseRetryRefreshRequested");
#endif
}

void RequestDatabaseInvalidation(AppState& state) {
    ForceCoverWallReload(state);
}

}  // namespace aura::cover_wall
