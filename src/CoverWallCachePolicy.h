#pragma once

#include "AppState.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aura::cover_wall {

bool CanReuseGlobalCoverWallCache(const AppState& state,
                                  const CoverWallState& cw,
                                  const std::string& dbPath,
                                  std::int64_t totalFileCount);
void StoreGlobalCoverWallCache(CoverWallState& cw,
                               const std::string& dbPath,
                               std::int64_t totalFileCount);
std::string LastCoverWallPageCursor(
    const std::vector<CoverModule::CoverItem>& items);
void ReconstructGlobalCoverWallPageState(CoverWallState& cw,
                                         const std::string& dbPath,
                                         std::size_t pageSize);

}  // namespace aura::cover_wall
