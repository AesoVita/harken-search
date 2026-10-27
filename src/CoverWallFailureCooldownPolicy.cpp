#include "CoverWallFailureCooldownPolicy.h"

#include "CoverWallTextureMirrorPolicy.h"

namespace aura::cover_wall {

bool FailureCoolingDown(const CoverWallState& cw,
                        const std::string& key,
                        std::uint64_t frame,
                        double nowSeconds) {
    constexpr std::uint64_t kCooldownFrames = 600;
    constexpr double kCooldownSeconds = 10.0;
    const auto it = cw.failedTexCache.find(key);
    return it != cw.failedTexCache.end() &&
           ((frame - it->second.lastAttemptFrame) < kCooldownFrames ||
            (nowSeconds - it->second.lastAttemptTimeSec) < kCooldownSeconds);
}

void RecordFailure(CoverWallState& cw,
                   const std::string& key,
                   std::uint64_t frame,
                   double nowSeconds) {
    auto& fail = cw.failedTexCache[key];
    fail.lastAttemptFrame = frame;
    fail.lastAttemptTimeSec = nowSeconds;
    fail.failures++;
}

void RecordSuccess(CoverWallState& cw, const std::string& key) {
    cw.failedTexCache.erase(key);
}

bool ShouldRecordConfirmedFailure(const CoverModule::CoverItem& item) {
    return CoverModule::IsCoverKeyFailed(item);
}

bool ReadyToWorkCandidateSatisfied(const CoverWallState& cw,
                                   const CoverModule::CoverItem& item,
                                   std::uint64_t frame,
                                   double nowSeconds) {
    return HasCurrentTextureMirror(cw, item) ||
           FailureCoolingDown(cw, item.key, frame, nowSeconds) ||
           ShouldRecordConfirmedFailure(item);
}

}  // namespace aura::cover_wall
