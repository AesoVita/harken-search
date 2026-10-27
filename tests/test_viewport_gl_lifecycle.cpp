#include "ViewportGlLifecycle.h"

#include <cstdint>

int main() {
    using namespace aura::ui::gl;
    ViewportGlState extra{};
    extra.sharedRc = reinterpret_cast<void*>(static_cast<uintptr_t>(0x10));
    extra.hdc = reinterpret_cast<void*>(static_cast<uintptr_t>(0x20));
    extra.hrc = reinterpret_cast<void*>(static_cast<uintptr_t>(0x30));

    ViewportGlBindResult failedShare = ResolveExtraViewportContext(extra, false);
    if (failedShare.reuseSharedRc) return 1;
    if (failedShare.presentable) return 2;
    if (failedShare.hrc == extra.sharedRc) return 3;

    ViewportGlState swap{};
    swap.hdc = extra.hdc;
    swap.hrc = nullptr;
    swap.presentable = true;
    if (CanSwapViewportBuffers(swap)) return 4;

    swap.hrc = extra.hrc;
    swap.destroyed = true;
    if (CanSwapViewportBuffers(swap)) return 5;

    ViewportGlState ok{};
    ok.hdc = extra.hdc;
    ok.hrc = extra.sharedRc;
    ok.presentable = true;
    ok.ownsRc = false;
    if (!CanSwapViewportBuffers(ok)) return 6;

    ViewportGlBindResult shared = ResolveExtraViewportContext(extra, true);
    if (!shared.reuseSharedRc) return 7;
    if (!shared.presentable) return 8;
    if (shared.hrc != extra.sharedRc) return 9;

    ViewportGlState extraShared = ok;
    extraShared.ownsRc = false;
    extraShared.hrc = extra.sharedRc;
    extraShared.sharedRc = extra.sharedRc;
    if (ShouldClearCurrentGlOnExtraCleanup(extraShared)) return 10;

    ViewportGlState extraOwned = extraShared;
    extraOwned.ownsRc = true;
    extraOwned.hrc = extra.hrc;
    if (!ShouldClearCurrentGlOnExtraCleanup(extraOwned)) return 11;

    ViewportGlBindResult assumed = BindExtraViewportContext(extra, false);
    if (assumed.presentable) return 12;
    if (assumed.reuseSharedRc) return 13;
    ViewportGlBindResult proven = BindExtraViewportContext(extra, true);
    if (!proven.presentable || proven.hrc != extra.sharedRc) return 14;
    return 0;
}
