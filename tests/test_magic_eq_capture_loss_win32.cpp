#include <windows.h>
#include <cstdio>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "ImGuiMouseCaptureLoss.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static bool NextFrame() {
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImGui::EndFrame();
    return down;
}

int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"Magic EQ capture test",
                                  WS_OVERLAPPEDWINDOW, 0, 0, 300, 100,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 2;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(300, 100);
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!ImGui_ImplWin32_Init(window)) return 3;
    NextFrame();

    ImGui_ImplWin32_WndProcHandler(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 20));
    const bool afterDown = NextFrame();
    ReleaseImGuiLeftButtonOnCaptureLoss(); // Application's WM_CAPTURECHANGED action.
    ImGui_ImplWin32_WndProcHandler(window, WM_CAPTURECHANGED, 0, 0);
    const bool afterCaptureChanged = NextFrame();
    // The old click never sends WM_LBUTTONUP; a fresh click must work anyway.
    ImGui_ImplWin32_WndProcHandler(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 20));
    const bool afterSecondDown = NextFrame();
    ImGui_ImplWin32_WndProcHandler(window, WM_LBUTTONUP, 0, MAKELPARAM(20, 20));
    const bool afterUp = NextFrame();

    std::printf("down=%d capture_changed=%d second_down=%d up=%d\n",
                afterDown, afterCaptureChanged, afterSecondDown, afterUp);
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(window);
    return (afterDown && !afterCaptureChanged && afterSecondDown && !afterUp) ? 0 : 1;
}
