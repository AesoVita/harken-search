#pragma once

#include "AppState.h"

#include <cstddef>
#include <vector>

namespace aura::cover_wall {

struct VisibleMirrorAuditResult {
    std::size_t checked = 0;
    std::size_t culled = 0;
};

VisibleMirrorAuditResult CullStaleVisibleTextureMirrors(
    CoverWallState& cw,
    const std::vector<CoverModule::CoverItem>& items,
    std::size_t startIdx,
    std::size_t endIdx);

}  // namespace aura::cover_wall
