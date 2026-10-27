#pragma once

#include "AppState.h"

#include <cstdint>

namespace aura::cover_wall {

void MarkTextureMirrorsStaleFromNonCoverTexture(CoverWallState& state);
void CullTextureMirrorsMatchingNonCoverTexture(CoverWallState& state,
                                               std::uint32_t textureId);
bool HasCurrentTextureMirror(const CoverWallState& state,
                             const CoverModule::CoverItem& item);

}  // namespace aura::cover_wall
