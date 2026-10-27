#include "CoverWallVisibleMirrorAudit.h"

#include "CoverModule.h"

#include <algorithm>

namespace aura::cover_wall {

VisibleMirrorAuditResult CullStaleVisibleTextureMirrors(
    CoverWallState& cw,
    const std::vector<CoverModule::CoverItem>& items,
    std::size_t startIdx,
    std::size_t endIdx) {
    VisibleMirrorAuditResult result;
    if (startIdx >= endIdx || startIdx >= items.size()) return result;
    endIdx = (std::min)(endIdx, items.size());
    for (std::size_t idx = startIdx; idx < endIdx; ++idx) {
        const auto& item = items[idx];
        auto it = cw.texCache.find(item.key);
        if (it == cw.texCache.end() || it->second.texId == 0 ||
            it->second.nonCoverTextureEpoch != cw.nonCoverTextureEpoch) {
            continue;
        }
        result.checked++;
        if (!CoverModule::IsCoverTextureHandleCurrent(item, it->second.texId)) {
            cw.texCache.erase(it);
            result.culled++;
        }
    }
    return result;
}

}  // namespace aura::cover_wall
