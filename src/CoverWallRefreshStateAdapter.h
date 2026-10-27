#pragma once

struct AppState;

void ForceCoverWallReload(AppState& state);

namespace aura::cover_wall {

void RequestDisplayRefresh(AppState& state);
void RequestDisplayRefreshForExplicitRetry(AppState& state);
void RequestDatabaseRefreshForExplicitRetry(AppState& state);
void RequestDatabaseInvalidation(AppState& state);

}  // namespace aura::cover_wall
