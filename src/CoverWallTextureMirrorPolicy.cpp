#include "CoverWallTextureMirrorPolicy.h"

namespace aura::cover_wall {

void MarkTextureMirrorsStaleFromNonCoverTexture(CoverWallState& state) {
    ++state.nonCoverTextureEpoch;
}

void CullTextureMirrorsMatchingNonCoverTexture(CoverWallState& state,
                                               std::uint32_t textureId) {
    if (textureId == 0) return;
    for (auto it = state.texCache.begin(); it != state.texCache.end();) {
        if (it->second.texId == textureId) {
            it = state.texCache.erase(it);
        } else {
            ++it;
        }
    }
}

bool HasCurrentTextureMirror(const CoverWallState& state,
                             const CoverModule::CoverItem& item) {
    const auto it = state.texCache.find(item.key);
    return it != state.texCache.end() &&
           it->second.texId != 0 &&
           it->second.nonCoverTextureEpoch == state.nonCoverTextureEpoch;
}

}  // namespace aura::cover_wall
