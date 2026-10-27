#include "DockNativeCaptionBridge.h"
#include <chrono>
#include <cstdio>

int main() {
    using namespace aura::ui::dock;
    NativeCaptionDragEvent down{};
    down.kind = NativeCaptionEventKind::NonClientLeftDown;
    down.hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x100));
    down.moduleId = "filter";
    if (!ShouldBeginDockDragFromNativeCaption(down)) return 1;

    NativeCaptionDragEvent move{};
    move.kind = NativeCaptionEventKind::NonClientMouseMove;
    move.hwnd = down.hwnd;
    move.moduleId = "filter";
    move.dragging = true;
    if (!ShouldUpdateDockDragFromNativeCaption(move)) return 1;

    NativeCaptionDragEvent imguiOnly{};
    imguiOnly.kind = NativeCaptionEventKind::ImGuiTitleBarRect;
    imguiOnly.moduleId = "filter";
    if (ShouldBeginDockDragFromNativeCaption(imguiOnly)) return 1;

    NativeCaptionDragEvent unknownHwnd{};
    unknownHwnd.kind = NativeCaptionEventKind::NonClientLeftDown;
    unknownHwnd.hwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x200));
    if (ShouldBeginDockDragFromNativeCaption(unknownHwnd)) return 1;

    void* filterHwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x300));
    void* infoHwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x400));
    void* hostHwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000));
    BindNativeCaptionModule(filterHwnd, "filter");
    BindNativeCaptionModule(infoHwnd, "info");
    if (ModuleIdForNativeCaptionHwnd(filterHwnd) != "filter") return 1;
    if (ModuleIdForNativeCaptionHwnd(infoHwnd) != "info") return 1;
    if (!ModuleIdForNativeCaptionHwnd(reinterpret_cast<void*>(static_cast<uintptr_t>(0x500))).empty()) return 1;

    NativeCaptionDragEvent infoDown{};
    infoDown.kind = NativeCaptionEventKind::NonClientLeftDown;
    infoDown.hwnd = infoHwnd;
    infoDown.moduleId = ModuleIdForNativeCaptionHwnd(infoHwnd);
    if (!ShouldBeginDockDragFromNativeCaption(infoDown)) return 1;
    if (infoDown.moduleId != "info") return 1;

    NativeCaptionDragEvent extraViewportCaption{};
    extraViewportCaption.kind = NativeCaptionEventKind::NonClientLeftDown;
    extraViewportCaption.hwnd = filterHwnd;
    extraViewportCaption.moduleId = ModuleIdForNativeCaptionHwnd(filterHwnd);
    extraViewportCaption.hostHwnd = hostHwnd;
    extraViewportCaption.hitTest = NativeCaptionHit::Caption;
    if (!ShouldSubclassExtraViewportCaption(extraViewportCaption)) return 11;
    NativeCaptionProcDecision decision = HandleExtraViewportCaptionMessage(extraViewportCaption);
    if (!decision.beginDockDrag) return 12;
    if (decision.letDefWindowProcMoveWindow) return 13;
    if (!decision.setCapture) return 14;

    NativeCaptionDragEvent hostCaption{};
    hostCaption.kind = NativeCaptionEventKind::NonClientLeftDown;
    hostCaption.hwnd = hostHwnd;
    hostCaption.hostHwnd = hostHwnd;
    hostCaption.hitTest = NativeCaptionHit::Caption;
    NativeCaptionProcDecision hostDecision = HandleExtraViewportCaptionMessage(hostCaption);
    if (hostDecision.beginDockDrag) return 15;

    NativeCaptionDragEvent afterSystemMove{};
    afterSystemMove.kind = NativeCaptionEventKind::NonClientMouseMove;
    afterSystemMove.hwnd = filterHwnd;
    afterSystemMove.moduleId = "filter";
    afterSystemMove.hostHwnd = hostHwnd;
    afterSystemMove.cursorStillOverSourceHwnd = false;
    afterSystemMove.buttonDown = true;
    afterSystemMove.alreadyCapturing = true;
    if (!ShouldKeepExtraViewportDockDrag(afterSystemMove)) return 16;

    NativeCaptionDragging() = false;
    NativeCaptionHwnd() = nullptr;
    NativeCaptionModuleId().clear();
    if (!ApplyNativeCaptionPointerState(infoHwnd, "info", true, true, true)) return 1;
    if (NativeCaptionModuleId() != "info" || !NativeCaptionDragging()) return 1;
    if (!ApplyNativeCaptionPointerState(infoHwnd, "info", false, false, true)) return 1;
    if (NativeCaptionModuleId() != "info") return 1;
    if (ApplyNativeCaptionPointerState(infoHwnd, "info", false, false, false)) return 1;
    if (NativeCaptionDragging()) return 1;
    if (!ShouldMoveExtraHwndWithSetWindowPos(true)) return 21;
    if (ShouldMoveExtraHwndWithSetWindowPos(false)) return 22;

    // A viewport can be replaced while its former HWND is still alive for a
    // frame. Keep the old HWND's original procedure until WM_NCDESTROY so the
    // caption hook never records itself as its own forwarding target.
    void* oldFilterHwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x600));
    void* newFilterHwnd = reinterpret_cast<void*>(static_cast<uintptr_t>(0x700));
    void* oldFilterProc = reinterpret_cast<void*>(static_cast<uintptr_t>(0x800));
    void* captionHookProc = reinterpret_cast<void*>(static_cast<uintptr_t>(0x900));
    void* fallbackProc = reinterpret_cast<void*>(static_cast<uintptr_t>(0xA00));
    BindNativeCaptionModule(oldFilterHwnd, "filter");
    RememberExtraViewportProc(oldFilterHwnd, oldFilterProc);
    BindNativeCaptionModule(newFilterHwnd, "filter");
    if (ModuleIdForNativeCaptionHwnd(oldFilterHwnd) != "filter") return 23;
    if (OriginalExtraViewportProc(oldFilterHwnd) != oldFilterProc) return 24;
    if (ResolveCaptionForwardProc(oldFilterProc, captionHookProc, fallbackProc) != oldFilterProc) return 25;
    if (ResolveCaptionForwardProc(captionHookProc, captionHookProc, fallbackProc) != fallbackProc) return 26;
    if (ResolveCaptionForwardProc(static_cast<void*>(nullptr), captionHookProc, fallbackProc) != fallbackProc) return 27;
    ForgetExtraViewportProc(oldFilterHwnd);
    if (!ModuleIdForNativeCaptionHwnd(oldFilterHwnd).empty()) return 28;
    if (OriginalExtraViewportProc(oldFilterHwnd) != nullptr) return 29;
    // Idle, pointer outside and an owned drag must avoid synchronous hit tests.
    int idleProbeCount = 0;
    int dragProbeCount = 0;
    const auto idleStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; ++i) {
        if (ShouldProbeNativeCaptionHit(false, false, false)) ++idleProbeCount;
        if (ShouldProbeNativeCaptionHit(false, true, false)) ++idleProbeCount;
    }
    const auto idleEnd = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; ++i) {
        if (ShouldProbeNativeCaptionHit(true, true, false)) ++dragProbeCount;
        if (ShouldProbeNativeCaptionHit(true, true, true)) ++dragProbeCount;
    }
    const auto dragEnd = std::chrono::steady_clock::now();
    if (idleProbeCount != 0 || dragProbeCount != 10000) return 30;
    std::printf("dock_caption_probe idle_calls=%d active_calls=%d idle_us=%lld active_us=%lld\n",
        idleProbeCount, dragProbeCount,
        static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(idleEnd - idleStart).count()),
        static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(dragEnd - idleEnd).count()));
    return 0;
}
