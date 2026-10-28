#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "ui/dock/DockCaptionWndProc.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
using namespace aura::ui::dock;
namespace {
HWND host = nullptr;
HWND foreign = nullptr;
bool backendReady = false;
int forwardedUps = 0;
int failures = 0;

LRESULT CALLBACK BackendProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_LBUTTONUP) ++forwardedUps;
    if (backendReady && ImGui_ImplWin32_WndProcHandler(hwnd, message, w, l)) return 1;
    return DefWindowProcW(hwnd, message, w, l);
}
LRESULT CALLBACK DetachedProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    DockCaptionRuntime runtime{};
    runtime.hostHwnd = host;
    runtime.originalProc = BackendProc;
    return DispatchDockCaptionWindowMessage(hwnd, message, w, l, runtime);
}
bool NextFrame() {
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImGui::EndFrame();
    return down;
}
void Check(bool result, const char* label) {
    std::printf("%s %s\n", result ? "PASS" : "FAIL", label);
    if (!result) ++failures;
}
void ClickAndRelease(HWND hwnd, const char* label) {
    const int before = forwardedUps;
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 30));
    const bool down = NextFrame();
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(30, 30));
    const bool stuck = NextFrame();
    const bool forwarded = forwardedUps == before + 1;
    std::printf("case=%s down=%d stuck_after_release=%d release_forwarded=%d\n",
        label, down, stuck, forwarded);
    Check(down && !stuck && forwarded, label);
    // Reset only after recording the result, so one failure cannot hide later cases.
    BackendProc(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(30, 30));
    NextFrame();
}
void LoseCaptureWithoutButtonUp(HWND hwnd, const char* label) {
    const int before = forwardedUps;
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 30));
    const bool down = NextFrame();
    const bool captured = GetCapture() == hwnd;
    SetCapture(foreign); // Win32 sends the real WM_CAPTURECHANGED to the old owner.
    const bool stuck = NextFrame();
    const bool noButtonUp = forwardedUps == before;
    std::printf("case=%s down=%d captured=%d stuck_after_capture_loss=%d no_button_up=%d\n",
        label, down, captured, stuck, noButtonUp);
    Check(down && captured && !stuck && noButtonUp && GetCapture() == foreign, label);
    ReleaseCapture();
    // Clear a failed case only after recording it.
    BackendProc(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(30, 30));
    NextFrame();
    ClickAndRelease(hwnd, "fresh_click_after_capture_loss");
}
void PreserveInternalCaptureTransfer(HWND hwnd, bool releaseFirst, const char* label) {
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 30));
    const bool down = NextFrame();
    const bool captured = GetCapture() == hwnd;
    // The viewport backend releases then captures the host when retiring a HWND.
    if (releaseFirst) ReleaseCapture();
    SetCapture(host);
    const bool stillDown = NextFrame();
    SendMessageW(host, WM_LBUTTONUP, 0, MAKELPARAM(30, 30));
    const bool stuck = NextFrame();
    Check(down && captured && stillDown && !stuck, label);
}
}

int main() {
    WNDCLASSW wc{};
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpfnWndProc = BackendProc;
    wc.lpszClassName = L"HarkenInputControlTest";
    if (!RegisterClassW(&wc)) return 2;
    host = CreateWindowExW(0, wc.lpszClassName, L"Input control",
        WS_OVERLAPPEDWINDOW, 100, 100, 640, 480, nullptr, nullptr, wc.hInstance, nullptr);
    wc.lpfnWndProc = DetachedProc;
    wc.lpszClassName = L"HarkenDetachedInputTest";
    if (!RegisterClassW(&wc)) return 3;
    HWND popup = CreateWindowExW(0, wc.lpszClassName, L"Detached popup",
        WS_OVERLAPPEDWINDOW, 700, 200, 320, 200, nullptr, nullptr, wc.hInstance, nullptr);
    HWND panel = CreateWindowExW(0, wc.lpszClassName, L"Dock panel",
        WS_OVERLAPPEDWINDOW, 1100, 200, 320, 200, nullptr, nullptr, wc.hInstance, nullptr);
    foreign = CreateWindowExW(0, L"STATIC", L"External capture owner",
        WS_OVERLAPPEDWINDOW, 1400, 200, 200, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!host || !popup || !panel || !foreign) return 4;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!ImGui_ImplWin32_Init(host)) return 5;
    backendReady = true;
    NextFrame();
    ClickAndRelease(host, "host_control");
    ClickAndRelease(popup, "ordinary_detached_popup");
    ClickAndRelease(popup, "ordinary_detached_popup_second_click");
    SetWindowPos(popup, nullptr, -800, -600, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ClickAndRelease(popup, "negative_monitor_coordinates");
    LoseCaptureWithoutButtonUp(popup, "popup_client_capture_loss");
    PreserveInternalCaptureTransfer(popup, false, "direct_viewport_capture_transfer");
    PreserveInternalCaptureTransfer(popup, true, "viewport_retirement_capture_transfer");
    BindNativeCaptionModule(panel, "filter");
    ClickAndRelease(panel, "dock_panel_idle_widget");
    LoseCaptureWithoutButtonUp(panel, "dock_panel_client_capture_loss");
    // A completed native caption drag leaves its HWND/module remembered.
    // Ordinary client clicks must still reach ImGui after that drag.
    SendMessageW(panel, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    Check(NativeCaptionDragging() && NativeCaptionHwnd() == panel && GetCapture() == panel,
        "native_caption_drag_starts");
    ClickAndRelease(popup, "popup_while_another_panel_owns_drag");
    SendMessageW(panel, WM_LBUTTONUP, 0, 0);
    Check(!NativeCaptionDragging() && GetCapture() != panel, "native_caption_drag_releases");
    ClickAndRelease(panel, "dock_panel_widget_after_caption_drag");
    ClickAndRelease(popup, "popup_after_caption_drag");
    SendMessageW(panel, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    SetCapture(host);
    Check(!NativeCaptionDragging() && GetCapture() == host, "native_drag_capture_loss");
    ReleaseCapture();
    ClickAndRelease(panel, "dock_panel_widget_after_capture_loss");
    if (GetCapture()) ReleaseCapture();
    backendReady = false;
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(popup);
    DestroyWindow(panel);
    DestroyWindow(foreign);
    DestroyWindow(host);
    std::printf("failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
