#pragma once

namespace aura::cover_rebuild {

struct CoverRebuildLifecycleInput {
    bool cancelRequested = false;
    bool batchEmpty = false;
    bool coverRegistryStale = false;
};

class CoverRebuildLifecycleStatePort {
public:
    virtual ~CoverRebuildLifecycleStatePort() = default;
    virtual void cancelBatch() = 0;
    virtual void completeBatch() = 0;
    virtual void clearFacets() = 0;
    virtual void setPreferDeferredCount() = 0;
    virtual void switchToCoverTab() = 0;
};

class CoverRebuildLifecycleEffectsPort {
public:
    virtual ~CoverRebuildLifecycleEffectsPort() = default;
    virtual void forceCoverWallReload() = 0;
    virtual void refreshSearchResults() = 0;
    virtual void syncCoverRegistry() = 0;
};

bool ResolveCoverRebuildLifecycle(
    CoverRebuildLifecycleInput input,
    CoverRebuildLifecycleStatePort& state,
    CoverRebuildLifecycleEffectsPort& effects);

}  // namespace aura::cover_rebuild
