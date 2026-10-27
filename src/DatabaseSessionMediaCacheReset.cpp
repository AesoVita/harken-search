#include "DatabaseSessionMediaCacheReset.h"

#include "AppState.h"

namespace aura::database_session {

void ResetDatabaseSessionMediaCaches(AppState& state) {
    state.coverWallDatabaseInvalidationRequested = true;
    state.invalidateCoverWallAfterDatabaseContentChange();
    state.preloadedCoverWallEntries.clear();
    state.preloadedCoverWallFullyDiscovered = false;
    state.coverWall.reset();
    state.waveformCache.clear();
    state.clearPendingWaveforms();
    state.waveformFailed.clear();
    state.visibleFilePaths.clear();
}

} // namespace aura::database_session
