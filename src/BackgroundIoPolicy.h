#pragma once

#include <algorithm>
#include <chrono>

// @stage7-contract
// API contract: BackgroundIoContext is a value snapshot of user/activity and
// lifecycle state. Policy functions are pure decisions over the snapshot and
// must not mutate UI/database/audio ownership or steal foreground focus.

struct BackgroundIoContext {
    bool contextMenuHeavyFreeze = false;
    bool coverWallPriorityMode = false;
    bool playing = false;
    bool waveformRebuildActive = false;
    bool scanning = false;
    bool dbSwitchInProgress = false;
    bool dbSwitchSettling = false;
    bool loadFilePending = false;
    bool detailPending = false;
    bool detailDeferring = false;
    bool detailJustCompleted = false;
    bool modeSwitchCoolingDown = false;
    bool mouseIdle = true;
    bool userActive = false;
    bool scrollingFast = false;
    bool coverWarmup = false;
    bool hasBrowseAnchor = true;
    double secondsSinceSelectionOrLoad = 999.0;
};

struct CoverWallBackgroundBudget {
    int maxTextureUploads = 0;
    int budgetMicroseconds = 0;
    int maxRequestsPerFrame = 0;
    bool allowBackgroundRequests = false;
    bool requestVisibleOnly = true;
    const char* reason = "blocked";
};

struct DecoderPrewarmDecision {
    bool allowed = false;
    int lookahead = 0;
    const char* reason = "blocked";
};

struct MiniWaveformWorkerDecision {
    int submitListBudget = 0;
    bool throttleWorker = true;
    bool cancelQueuedListWork = false;
    bool allowCurrentFileFastLane = true;
    const char* reason = "blocked";
};

struct AutomaticBackgroundMaintenanceDecision {
    bool allowed = false;
    const char* reason = "blocked";
};

inline bool ShouldThrottleMiniUploadsForPlayback(bool isPlaying)
{
    return isPlaying;
}

inline bool IsIdleAssetBackfillCheckDue(
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point nextCheckAt)
{
    return nextCheckAt.time_since_epoch().count() == 0 || now >= nextCheckAt;
}

inline bool HasIdleAssetBackfillDelayElapsed(
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point lastInputAt,
    int delayMinutes)
{
    const int safeDelayMinutes = std::clamp(delayMinutes, 1, 240);
    return now - lastInputAt >= std::chrono::minutes(safeDelayMinutes);
}

inline bool ShouldBlockIdleBackfillPoolTakeover(bool poolRunning,
                                                int inFlight,
                                                int queued)
{
    return poolRunning && (inFlight > 0 || queued > 0);
}

inline bool IsForegroundCriticalForBackgroundIo(const BackgroundIoContext& ctx)
{
    return ctx.contextMenuHeavyFreeze ||
           ctx.waveformRebuildActive ||
           ctx.scanning ||
           ctx.dbSwitchInProgress ||
           ctx.dbSwitchSettling ||
           ctx.playing ||
           ctx.loadFilePending ||
           ctx.detailPending ||
           ctx.detailDeferring ||
           ctx.detailJustCompleted ||
           ctx.modeSwitchCoolingDown ||
           ctx.secondsSinceSelectionOrLoad < 0.55;
}

// Decides whether the cover-wall first-screen sweep cursor should re-arm to the
// start of the current viewport window.
//
// The cursor obviously re-arms when the viewport scrolls to a new window. The
// subtle case is "at rest": the user is not scrolling, so the window did not
// change, yet visible cells are still black because the previous sweep finished
// (cursor reached the window end) before every cell was cached/uploaded — e.g.
// the per-frame request budget ran out, or a transient texture failure left a
// gap. Without re-arming, those cells stay black until the user drags the
// scrollbar, which is exactly the reported "must drag to load" symptom.
//
// We only re-arm at rest when there is actually eligible work left in view, and
// never while background IO is paused for a foreground-critical job.
inline bool ShouldRearmFirstScreenCursor(bool windowChanged,
                                         bool cursorReachedEnd,
                                         bool hasEligibleVisibleCells,
                                         bool paused)
{
    if (paused) {
        return false;
    }
    if (windowChanged) {
        return true;
    }
    return cursorReachedEnd && hasEligibleVisibleCells;
}

inline bool ShouldDropCoverWallLoadedForDisplayReload(bool useFacetCoverWall)
{
    // Display-level reloads (search/facet/tab sync) must not force the global
    // or filtered/facet cover wall to rebuild. Real DB-content invalidations
    // clear CoverWallState separately before this display flag is consumed.
    // Dropping loaded for a facet-only refresh causes the visible wall to flash
    // through an empty facet surface while the authoritative facet follow-up is
    // still pending.
    (void)useFacetCoverWall;
    return false;
}

inline CoverWallBackgroundBudget ResolveCoverWallBackgroundBudget(const BackgroundIoContext& ctx)
{
    CoverWallBackgroundBudget budget;
    if (ctx.contextMenuHeavyFreeze) {
        budget.reason = "context_menu";
        return budget;
    }
    if (ctx.waveformRebuildActive || ctx.scanning || ctx.dbSwitchInProgress || ctx.dbSwitchSettling) {
        budget.reason = "exclusive_job";
        return budget;
    }
    if (IsForegroundCriticalForBackgroundIo(ctx)) {
        budget.reason = "foreground_visible";
        budget.allowBackgroundRequests = true;
        budget.maxTextureUploads = 1;
        budget.budgetMicroseconds = 120;
        budget.maxRequestsPerFrame = 1;
        budget.requestVisibleOnly = true;
        return budget;
    }

    budget.allowBackgroundRequests = true;
    // Default product semantics: non-cover-wall surfaces only serve visible
    // requests. The cover wall owns an explicit priority mode; when it is open
    // and the user is idle, it may progressively fill off-screen thumbnails.
    budget.requestVisibleOnly = true;
    budget.reason = (!ctx.mouseIdle || ctx.userActive || ctx.scrollingFast)
        ? "interaction"
        : "visible_only";

    if (!ctx.coverWallPriorityMode) {
        budget.maxTextureUploads = ctx.mouseIdle ? 2 : 1;
        budget.budgetMicroseconds = ctx.mouseIdle ? 300 : 150;
        budget.maxRequestsPerFrame = ctx.mouseIdle ? 2 : 1;
        return budget;
    }

    if (ctx.coverWarmup) {
        budget.maxTextureUploads = 2;
        budget.budgetMicroseconds = 300;
        budget.maxRequestsPerFrame = 4;
        budget.requestVisibleOnly = true;
        budget.reason = "warmup_visible_only";
        return budget;
    }

    if (ctx.userActive) {
        budget.maxTextureUploads = 1;
        budget.budgetMicroseconds = 120;
        budget.maxRequestsPerFrame = 1;
        budget.requestVisibleOnly = true;
        return budget;
    }

    if (!ctx.mouseIdle || ctx.scrollingFast) {
        budget.maxTextureUploads = 2;
        budget.budgetMicroseconds = 800;
        budget.maxRequestsPerFrame = 2;
        budget.requestVisibleOnly = true;
        return budget;
    }

    budget.maxTextureUploads = 8;
    budget.budgetMicroseconds = 1800;
    budget.maxRequestsPerFrame = 24;
    budget.requestVisibleOnly = false;
    return budget;
}

inline DecoderPrewarmDecision ResolveDecoderPrewarmDecision(const BackgroundIoContext& ctx)
{
    DecoderPrewarmDecision decision;
    if (ctx.contextMenuHeavyFreeze) {
        decision.reason = "context_menu";
        return decision;
    }
    if (ctx.waveformRebuildActive || ctx.scanning || ctx.dbSwitchInProgress || ctx.dbSwitchSettling) {
        decision.reason = "exclusive_job";
        return decision;
    }
    if (ctx.playing || ctx.loadFilePending || ctx.detailPending || ctx.detailDeferring ||
        ctx.detailJustCompleted || ctx.modeSwitchCoolingDown || ctx.secondsSinceSelectionOrLoad < 0.55) {
        decision.reason = "foreground";
        return decision;
    }
    if (!ctx.hasBrowseAnchor) {
        decision.reason = "no_anchor";
        return decision;
    }

    decision.allowed = true;
    decision.lookahead = ctx.mouseIdle ? 3 : 1;
    decision.reason = ctx.mouseIdle ? "idle" : "interaction";
    return decision;
}

inline MiniWaveformWorkerDecision ResolveMiniWaveformWorkerDecision(const BackgroundIoContext& ctx)
{
    MiniWaveformWorkerDecision decision;
    if (ctx.contextMenuHeavyFreeze) {
        decision.reason = "context_menu";
        return decision;
    }
    if (ctx.waveformRebuildActive || ctx.scanning || ctx.dbSwitchInProgress || ctx.dbSwitchSettling) {
        decision.allowCurrentFileFastLane = false;
        decision.reason = "exclusive_job";
        return decision;
    }
    if (IsForegroundCriticalForBackgroundIo(ctx)) {
        decision.cancelQueuedListWork = true;
        decision.allowCurrentFileFastLane = true;
        // Keep a single visible-only lane alive while playback/detail work owns
        // the foreground. WindowsMain rechecks visibleFilePaths before using
        // this budget, so stale/off-screen list work remains cancelled.
        decision.submitListBudget = 1;
        decision.reason = "foreground";
        return decision;
    }

    decision.throttleWorker = false;
    decision.cancelQueuedListWork = false;
    decision.allowCurrentFileFastLane = true;
    decision.submitListBudget = ctx.mouseIdle ? 16 : 6;
    decision.reason = ctx.mouseIdle ? "idle" : "interaction";
    return decision;
}

inline AutomaticBackgroundMaintenanceDecision
ResolveAutomaticMainDetailPrewarmDecision(const BackgroundIoContext& ctx, bool explicitOptIn)
{
    AutomaticBackgroundMaintenanceDecision decision;
    if (!explicitOptIn) {
        decision.reason = "not_opted_in";
        return decision;
    }
    if (IsForegroundCriticalForBackgroundIo(ctx)) {
        decision.reason = "foreground";
        return decision;
    }
    decision.allowed = true;
    decision.reason = "idle";
    return decision;
}

inline AutomaticBackgroundMaintenanceDecision
ResolveAutomaticCoverMaintenanceDecision(const BackgroundIoContext& ctx, bool explicitOptIn)
{
    AutomaticBackgroundMaintenanceDecision decision;
    if (!explicitOptIn) {
        decision.reason = "not_opted_in";
        return decision;
    }
    if (IsForegroundCriticalForBackgroundIo(ctx)) {
        decision.reason = "foreground";
        return decision;
    }
    decision.allowed = true;
    decision.reason = "idle";
    return decision;
}
