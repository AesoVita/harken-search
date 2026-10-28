#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "DockNativeCaptionBridge.h"
#include "ImGuiMouseCaptureLoss.h"

namespace aura::ui::dock {

struct DockCaptionRuntime {
    HWND hostHwnd = nullptr;
    WNDPROC originalProc = ::DefWindowProcW;
    bool (*getCursorPos)(POINT*) = nullptr;
};

inline LRESULT DispatchDockCaptionWindowMessage(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, const DockCaptionRuntime& runtime) {
    const WNDPROC original = ResolveCaptionForwardProc(
        runtime.originalProc, static_cast<WNDPROC>(nullptr), ::DefWindowProcW);
    auto readCursor = [&](POINT* point) {
        return runtime.getCursorPos ? runtime.getCursorPos(point) : ::GetCursorPos(point) != 0;
    };
    // A foreign window can take capture without delivering button-up. Preserve
    // transfers within this ImGui context, including the backend's temporary
    // null capture while retiring a viewport (ReleaseCapture -> SetCapture(host)).
    if (msg == WM_CAPTURECHANGED && lParam != 0 && ImGui::GetCurrentContext()) {
        const HWND nextCapture = reinterpret_cast<HWND>(lParam);
        bool internalTransfer = nextCapture == runtime.hostHwnd;
        for (const auto* viewport : ImGui::GetPlatformIO().Viewports) {
            if (viewport->PlatformHandle == nextCapture) internalTransfer = true;
        }
        if (!internalTransfer) ReleaseImGuiLeftButtonOnCaptureLoss();
    }
    if (msg == WM_NCDESTROY) {
        const LRESULT result = original(hwnd, msg, wParam, lParam);
        ForgetExtraViewportProc(hwnd);
        if (NativeCaptionHwnd() == hwnd) {
            NativeCaptionHwnd() = nullptr;
            NativeCaptionDragging() = false;
            NativeCaptionModuleId().clear();
        }
        return result;
    }
    if (msg == WM_NCLBUTTONDOWN && wParam == HTCAPTION) {
        NativeCaptionDragEvent event;
        event.kind = NativeCaptionEventKind::NonClientLeftDown;
        event.hwnd = hwnd;
        event.hostHwnd = runtime.hostHwnd;
        event.moduleId = ModuleIdForNativeCaptionHwnd(hwnd);
        event.hitTest = NativeCaptionHit::Caption;
        const NativeCaptionProcDecision decision = HandleExtraViewportCaptionMessage(event);
        if (decision.beginDockDrag) {
            POINT cursor{};
            readCursor(&cursor);
            RECT windowRect{};
            ::GetWindowRect(hwnd, &windowRect);
            NativeCaptionHwnd() = hwnd;
            NativeCaptionModuleId() = event.moduleId;
            NativeCaptionDragging() = true;
            NativeCaptionOriginCursorX() = cursor.x;
            NativeCaptionOriginCursorY() = cursor.y;
            NativeCaptionOriginWindowX() = windowRect.left;
            NativeCaptionOriginWindowY() = windowRect.top;
            if (decision.setCapture) ::SetCapture(hwnd);
            if (decision.swallowMessage || !decision.letDefWindowProcMoveWindow) return 0;
        }
        if (decision.letDefWindowProcMoveWindow)
            return original(hwnd, msg, wParam, lParam);
        return 0;
    }
    // Only an active, owned caption drag may intercept client input. Ordinary
    // detached dialogs have no module ID, and completed drags may retain their
    // HWND/module until the next frame. Both must forward button-up to ImGui.
    if (NativeCaptionDragging() &&
        (hwnd == NativeCaptionHwnd() ||
         (!NativeCaptionModuleId().empty() &&
          ModuleIdForNativeCaptionHwnd(hwnd) == NativeCaptionModuleId()))) {
        NativeCaptionDragEvent keep;
        keep.kind = NativeCaptionEventKind::NonClientMouseMove;
        keep.hwnd = hwnd;
        keep.hostHwnd = runtime.hostHwnd;
        keep.moduleId = ModuleIdForNativeCaptionHwnd(hwnd);
        keep.buttonDown = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        keep.alreadyCapturing = ::GetCapture() == hwnd || NativeCaptionDragging();
        keep.cursorStillOverSourceHwnd = false;
        if (msg == WM_MOUSEMOVE || msg == WM_NCMOUSEMOVE) {
            if (!ShouldKeepExtraViewportDockDrag(keep) && !NativeCaptionDragging())
                return original(hwnd, msg, wParam, lParam);
            POINT cursor{};
            readCursor(&cursor);
            const int dx = cursor.x - NativeCaptionOriginCursorX();
            const int dy = cursor.y - NativeCaptionOriginCursorY();
            if (ShouldMoveExtraHwndWithSetWindowPos(hwnd != runtime.hostHwnd)) {
                const int targetX = NativeCaptionOriginWindowX() + dx;
                const int targetY = NativeCaptionOriginWindowY() + dy;
                RECT current{};
                if (::GetWindowRect(hwnd, &current) &&
                    (current.left != targetX || current.top != targetY)) {
                    ::SetWindowPos(hwnd, nullptr, targetX, targetY, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                }
            }
            return 0;
        }
        if (msg == WM_LBUTTONUP || msg == WM_NCLBUTTONUP) {
            if (::GetCapture() == hwnd) ::ReleaseCapture();
            NativeCaptionDragging() = false;
            return 0;
        }
        if (msg == WM_CAPTURECHANGED) {
            NativeCaptionDragging() = false;
            return 0;
        }
    }
    return original(hwnd, msg, wParam, lParam);
}
} // namespace aura::ui::dock
