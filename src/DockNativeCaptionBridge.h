#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace aura::ui::dock {

#if AURA_BUILD_DIAGNOSTICS && ARIS_PREVIEW_ISOLATION && !AURA_INTERNAL_BETA
// Enabled only after normal legal consent in an isolated preview runtime.
struct DockFrameTrace {
    bool enabled = false;
    bool dragActive = false;
    int geometryCalls = 0;
    int hitCalls = 0;
    int previewCalls = 0;
    int captionMessages = 0;
    int hostNcHitTests = 0;
    int extraNcHitTests = 0;
    int zoneKind = -1, zoneSide = -1;
    int previewKind = -1, previewSide = -1;
    int commitKind = -1, commitSide = -1, commitRootRegion = -1;
    int leafCountBefore = -1, leafCountAfter = -1;
    double geometryMs = 0.0;
    double hitMs = 0.0;
    double previewMs = 0.0;
    double captionMs = 0.0;
    void beginFrame() {
        dragActive = false;
        geometryCalls = hitCalls = previewCalls = captionMessages = 0;
        hostNcHitTests = extraNcHitTests = 0;
        zoneKind = zoneSide = previewKind = previewSide = -1;
        commitKind = commitSide = commitRootRegion = -1;
        leafCountBefore = leafCountAfter = -1;
        geometryMs = hitMs = previewMs = captionMs = 0.0;
    }
};
inline DockFrameTrace& ActiveDockFrameTrace() {
    static DockFrameTrace trace;
    return trace;
}
#endif


enum class NativeCaptionEventKind {
    None = 0,
    NonClientLeftDown,
    NonClientMouseMove,
    NonClientLeftUp,
    ImGuiTitleBarRect
};

enum class NativeCaptionHit { None, Caption, Client, Other };

struct NativeCaptionDragEvent {
    NativeCaptionEventKind kind = NativeCaptionEventKind::None;
    void* hwnd = nullptr;
    void* hostHwnd = nullptr;
    std::string moduleId;
    NativeCaptionHit hitTest = NativeCaptionHit::None;
    bool dragging = false;
    bool cursorStillOverSourceHwnd = true;
    bool buttonDown = false;
    bool alreadyCapturing = false;
};

struct NativeCaptionProcDecision {
    bool beginDockDrag = false;
    bool letDefWindowProcMoveWindow = true;
    bool setCapture = false;
    bool swallowMessage = false;
};

inline bool ShouldBeginDockDragFromNativeCaption(const NativeCaptionDragEvent& event) {
    return event.kind == NativeCaptionEventKind::NonClientLeftDown &&
        event.hwnd != nullptr &&
        !event.moduleId.empty();
}

inline bool ShouldUpdateDockDragFromNativeCaption(const NativeCaptionDragEvent& event) {
    return event.kind == NativeCaptionEventKind::NonClientMouseMove &&
        event.hwnd != nullptr &&
        event.dragging &&
        !event.moduleId.empty();
}

inline bool ShouldSubclassExtraViewportCaption(const NativeCaptionDragEvent& event) {
    return event.kind == NativeCaptionEventKind::NonClientLeftDown &&
        event.hwnd != nullptr &&
        event.hostHwnd != nullptr &&
        event.hwnd != event.hostHwnd &&
        !event.moduleId.empty() &&
        event.hitTest == NativeCaptionHit::Caption;
}

inline NativeCaptionProcDecision HandleExtraViewportCaptionMessage(const NativeCaptionDragEvent& event) {
    NativeCaptionProcDecision decision;
    if (event.hostHwnd != nullptr && event.hwnd == event.hostHwnd) {
        decision.letDefWindowProcMoveWindow = true;
        decision.beginDockDrag = false;
        decision.setCapture = false;
        return decision;
    }
    if (ShouldSubclassExtraViewportCaption(event) ||
        (event.kind == NativeCaptionEventKind::NonClientLeftDown &&
            event.hwnd != nullptr &&
            event.hwnd != event.hostHwnd &&
            !event.moduleId.empty() &&
            event.hitTest == NativeCaptionHit::Caption)) {
        decision.beginDockDrag = true;
        decision.setCapture = true;
        decision.letDefWindowProcMoveWindow = false;
        decision.swallowMessage = true;
        return decision;
    }
    return decision;
}

inline bool ShouldKeepExtraViewportDockDrag(const NativeCaptionDragEvent& event) {
    return event.buttonDown &&
        event.alreadyCapturing &&
        event.hwnd != nullptr &&
        event.hwnd != event.hostHwnd &&
        !event.moduleId.empty();
}

inline std::string& NativeCaptionModuleId() {
    static std::string id;
    return id;
}

inline bool& NativeCaptionDragging() {
    static bool dragging = false;
    return dragging;
}

inline void*& NativeCaptionHwnd() {
    static void* hwnd = nullptr;
    return hwnd;
}

inline int& NativeCaptionOriginCursorX() {
    static int value = 0;
    return value;
}
inline int& NativeCaptionOriginCursorY() {
    static int value = 0;
    return value;
}
inline int& NativeCaptionOriginWindowX() {
    static int value = 0;
    return value;
}
inline int& NativeCaptionOriginWindowY() {
    static int value = 0;
    return value;
}

struct NativeCaptionBinding {
    void* hwnd = nullptr;
    std::string moduleId;
    void* originalProc = nullptr;
};

inline constexpr std::size_t kNativeCaptionBindingCapacity = 16;

inline std::array<NativeCaptionBinding, kNativeCaptionBindingCapacity>& NativeCaptionBindings() {
    static std::array<NativeCaptionBinding, kNativeCaptionBindingCapacity> bindings{};
    return bindings;
}

inline std::string ModuleIdForNativeCaptionHwnd(void* hwnd) {
    if (!hwnd) return {};
    for (const auto& binding : NativeCaptionBindings()) {
        if (binding.hwnd == hwnd) return binding.moduleId;
    }
    return {};
}

inline void* OriginalExtraViewportProc(void* hwnd) {
    if (!hwnd) return nullptr;
    for (const auto& binding : NativeCaptionBindings()) {
        if (binding.hwnd == hwnd) return binding.originalProc;
    }
    return nullptr;
}

inline bool RememberExtraViewportProc(void* hwnd, void* originalProc) {
    if (!hwnd || !originalProc) return false;
    for (auto& binding : NativeCaptionBindings()) {
        if (binding.hwnd == hwnd) {
            if (!binding.originalProc) binding.originalProc = originalProc;
            return true;
        }
    }
    for (auto& binding : NativeCaptionBindings()) {
        if (!binding.hwnd) {
            binding.hwnd = hwnd;
            binding.originalProc = originalProc;
            return true;
        }
    }
    return false;
}

inline void ForgetExtraViewportProc(void* hwnd) {
    if (!hwnd) return;
    for (auto& binding : NativeCaptionBindings()) {
        if (binding.hwnd == hwnd) {
            binding = {};
            return;
        }
    }
}

inline void BindNativeCaptionModule(void* hwnd, std::string_view moduleId) {
    if (!hwnd || moduleId.empty()) return;
    auto& bindings = NativeCaptionBindings();
    for (auto& binding : bindings) {
        if (binding.hwnd == hwnd) {
            binding.moduleId = std::string(moduleId);
            return;
        }
    }
    for (auto& binding : bindings) {
        if (!binding.hwnd) {
            binding.hwnd = hwnd;
            binding.moduleId = std::string(moduleId);
            return;
        }
    }
}

template <typename Proc>
inline Proc ResolveCaptionForwardProc(Proc originalProc, Proc hookProc, Proc fallbackProc) {
    return originalProc && originalProc != hookProc ? originalProc : fallbackProc;
}

inline bool ShouldRequestStandaloneMoveFromNativeDrag(bool nativeDragging, bool floating) {
    return nativeDragging && floating;
}

inline bool ShouldMoveExtraHwndWithSetWindowPos(bool extraViewport) {
    return extraViewport;
}

// A synchronous WM_NCHITTEST is only useful while an unowned left-button
// press is over this viewport. An owned native drag already knows its module.
inline bool ShouldProbeNativeCaptionHit(bool buttonDown, bool cursorOverThisHwnd,
                                        bool alreadyDraggingThis) {
    return buttonDown && cursorOverThisHwnd && !alreadyDraggingThis;
}

inline bool ApplyNativeCaptionPointerState(
    void* hwnd,
    std::string_view moduleId,
    bool cursorOverThisHwnd,
    bool hitCaption,
    bool buttonDown) {
    if (!hwnd || moduleId.empty()) return false;
    const bool already = NativeCaptionDragging() &&
        NativeCaptionHwnd() == hwnd &&
        NativeCaptionModuleId() == moduleId;
    NativeCaptionDragEvent begin;
    begin.kind = NativeCaptionEventKind::NonClientLeftDown;
    begin.hwnd = hwnd;
    begin.moduleId = std::string(moduleId);
    if (buttonDown && (already || (cursorOverThisHwnd && hitCaption && ShouldBeginDockDragFromNativeCaption(begin)))) {
        NativeCaptionHwnd() = hwnd;
        NativeCaptionModuleId() = std::string(moduleId);
        NativeCaptionDragging() = true;
        return true;
    }
    if (!buttonDown && already) {
        NativeCaptionDragging() = false;
        NativeCaptionHwnd() = nullptr;
        NativeCaptionModuleId().clear();
    }
    return false;
}

} // namespace aura::ui::dock
