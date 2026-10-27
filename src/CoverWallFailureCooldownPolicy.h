#pragma once

#include "AppState.h"
#include "CoverModule.h"

#include <cstdint>
#include <string>

namespace aura::cover_wall {

bool FailureCoolingDown(const CoverWallState& cw,
                        const std::string& key,
                        std::uint64_t frame,
                        double nowSeconds);
void RecordFailure(CoverWallState& cw,
                   const std::string& key,
                   std::uint64_t frame,
                   double nowSeconds);
void RecordSuccess(CoverWallState& cw, const std::string& key);
bool ShouldRecordConfirmedFailure(const CoverModule::CoverItem& item);
bool ReadyToWorkCandidateSatisfied(const CoverWallState& cw,
                                   const CoverModule::CoverItem& item,
                                   std::uint64_t frame,
                                   double nowSeconds);

}  // namespace aura::cover_wall
