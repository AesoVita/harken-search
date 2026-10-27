#pragma once

namespace aura::ui::gl {

struct ViewportGlState {
    void* hdc = nullptr;
    void* hrc = nullptr;
    void* sharedRc = nullptr;
    bool presentable = false;
    bool ownsRc = false;
    bool destroyed = false;
};

struct ViewportGlBindResult {
    void* hrc = nullptr;
    bool reuseSharedRc = false;
    bool presentable = false;
    bool ownsRc = false;
};

inline ViewportGlBindResult ResolveExtraViewportContext(const ViewportGlState& state, bool shareListsSucceeded) {
    ViewportGlBindResult result;
    if (!state.hdc || !state.sharedRc) return result;
    if (shareListsSucceeded) {
        result.hrc = state.sharedRc;
        result.reuseSharedRc = true;
        result.presentable = true;
        result.ownsRc = false;
        return result;
    }
    result.hrc = nullptr;
    result.reuseSharedRc = false;
    result.presentable = false;
    result.ownsRc = false;
    return result;
}

inline bool CanSwapViewportBuffers(const ViewportGlState& state) {
    return state.presentable && !state.destroyed && state.hdc != nullptr && state.hrc != nullptr;
}

inline ViewportGlBindResult BindExtraViewportContext(const ViewportGlState& state, bool shareListsSucceeded) {
    return ResolveExtraViewportContext(state, shareListsSucceeded);
}

inline bool ShouldClearCurrentGlOnExtraCleanup(const ViewportGlState& state) {
    return state.ownsRc && state.hrc != nullptr && state.hrc != state.sharedRc;
}

} // namespace aura::ui::gl
