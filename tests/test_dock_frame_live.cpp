#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include "ui/dock/DockCaptionWndProc.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstdio>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstdlib>

using namespace aura::ui::dock;

static POINT g_testCursor{};
static int g_captionMessages = 0;
static int g_positionMessages = 0;
static int g_positionAttempts = 0;
static HWND g_hostHwnd = nullptr;
static HGLRC g_sharedRc = nullptr;
static int g_viewportCreates = 0;
struct LiveViewportData { HDC dc = nullptr; };

static void CreateViewportRenderer(ImGuiViewport* viewport) {
    ++g_viewportCreates;
    HWND hwnd = static_cast<HWND>(viewport->PlatformHandle);
    auto* data = new LiveViewportData{};
    data->dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    const int format = data->dc ? ChoosePixelFormat(data->dc, &pfd) : 0;
    if (!format || !SetPixelFormat(data->dc, format, &pfd)) {
        if (data->dc) ReleaseDC(hwnd, data->dc);
        data->dc = nullptr;
    }
    viewport->RendererUserData = data;
}
static void DestroyViewportRenderer(ImGuiViewport* viewport) {
    auto* data = static_cast<LiveViewportData*>(viewport->RendererUserData);
    if (!data) return;
    if (data->dc) ReleaseDC(static_cast<HWND>(viewport->PlatformHandle), data->dc);
    delete data;
    viewport->RendererUserData = nullptr;
}
static void RenderViewport(ImGuiViewport* viewport, void*) {
    auto* data = static_cast<LiveViewportData*>(viewport->RendererUserData);
    if (data && data->dc) wglMakeCurrent(data->dc, g_sharedRc);
}
static void PresentViewport(ImGuiViewport* viewport, void*) {
    auto* data = static_cast<LiveViewportData*>(viewport->RendererUserData);
    if (data && data->dc) SwapBuffers(data->dc);
}
static bool TestCursor(POINT* out) { *out = g_testCursor; return true; }

static LRESULT CALLBACK LiveWindowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_NCLBUTTONDOWN || message == WM_MOUSEMOVE) ++g_captionMessages;
    if (message == WM_WINDOWPOSCHANGED) ++g_positionMessages;
    if (message == WM_WINDOWPOSCHANGING) ++g_positionAttempts;
    DockCaptionRuntime runtime{};
    runtime.hostHwnd = g_hostHwnd;
    runtime.originalProc = DefWindowProcW;
    runtime.getCursorPos = TestCursor;
    return DispatchDockCaptionWindowMessage(hwnd, message, w, l, runtime);
}

int main(int argc, char** argv) {
    const int panelCount = argc > 1 ? std::atoi(argv[1]) : 5;
    const bool drag = argc > 2 ? std::atoi(argv[2]) != 0 : true;
    const int duplicateCount = argc > 3 ? std::atoi(argv[3]) : 10;
    if (panelCount != 1 && panelCount != 3 && panelCount != 5) return 8;
    WNDCLASSW wc{};
    wc.lpfnWndProc = LiveWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HarkenSearchDockFrameLiveTest";
    if (!RegisterClassW(&wc)) return 2;
    HWND host = CreateWindowExW(0, wc.lpszClassName, L"Dock host test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 960, 600,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!host) return 3;
    g_hostHwnd = host;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Dock floating panel test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 1120, 100, 320, 240,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) return 3;
    HDC dc = GetDC(host);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    int format = ChoosePixelFormat(dc, &pfd);
    if (!format || !SetPixelFormat(dc, format, &pfd)) return 4;
    HGLRC rc = wglCreateContext(dc);
    if (!rc || !wglMakeCurrent(dc, rc)) return 5;
    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    if (!ImGui_ImplWin32_InitForOpenGL(host) ||
        !ImGui_ImplOpenGL3_Init("#version 130")) return 7;
    g_sharedRc = rc;
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = CreateViewportRenderer;
    platform.Renderer_DestroyWindow = DestroyViewportRenderer;
    platform.Platform_RenderWindow = RenderViewport;
    platform.Renderer_SwapBuffers = PresentViewport;
    BindNativeCaptionModule(hwnd, "similar-sounds");
    const int basePositionAttempts = g_positionAttempts;
    const int basePositionMessages = g_positionMessages;
    RECT before{};
    GetWindowRect(hwnd, &before);
    g_testCursor = {before.left + 40, before.top + 12};
    if (drag) SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    std::vector<double> frameMs;
    std::vector<double> moveMs, uiMs, extraMs, presentMs;
    frameMs.reserve(120);
    BOOL presented = FALSE;
    int visibleRows = 0;
    for (int frame = 0; frame < 120; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        if (drag) {
            ++g_testCursor.x;
            SendMessageW(hwnd, WM_MOUSEMOVE, 0, 0);
            for (int duplicate = 0; duplicate < duplicateCount; ++duplicate)
                SendMessageW(hwnd, WM_MOUSEMOVE, 0, 0);
        }
        const auto movedAt = std::chrono::steady_clock::now();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(4.0f, 4.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(280.0f, 180.0f), ImGuiCond_Always);
        ImGui::Begin("Synthetic Search Results");
        ImGuiListClipper clipper;
        clipper.Begin(82000);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                ImGui::Text("Synthetic row %d", row);
                ++visibleRows;
            }
        }
        ImGui::End();
        const ImVec2 viewportPos = ImGui::GetMainViewport()->Pos;
        const ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
        for (int panel = 0; panel < panelCount; ++panel) {
            ImGui::SetNextWindowPos(ImVec2(
                viewportPos.x + viewportSize.x + 80.0f + 340.0f * panel,
                viewportPos.y + 100.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(320.0f, 240.0f), ImGuiCond_Always);
            char title[80]{};
            std::snprintf(title, sizeof(title), "Floating Dock panel %d", panel);
            ImGui::Begin(title);
            ImGui::TextUnformatted(panel == 0 ? "Similar Sounds" : "Dock panel");
            ImGui::End();
        }
        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        const auto renderedAt = std::chrono::steady_clock::now();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        wglMakeCurrent(dc, rc);
        const auto extraAt = std::chrono::steady_clock::now();
        presented = SwapBuffers(dc);
        const auto doneAt = std::chrono::steady_clock::now();
        auto ms = [](auto begin, auto end) {
            return std::chrono::duration<double, std::milli>(end - begin).count();
        };
        moveMs.push_back(ms(start, movedAt));
        uiMs.push_back(ms(movedAt, renderedAt));
        extraMs.push_back(ms(renderedAt, extraAt));
        presentMs.push_back(ms(extraAt, doneAt));
        frameMs.push_back(ms(start, doneAt));
    }
    RECT after{};
    GetWindowRect(hwnd, &after);
    std::sort(frameMs.begin(), frameMs.end());
    std::sort(moveMs.begin(), moveMs.end());
    std::sort(uiMs.begin(), uiMs.end());
    std::sort(extraMs.begin(), extraMs.end());
    std::sort(presentMs.begin(), presentMs.end());
    std::printf("drag=%d duplicates=%d captionMessages=%d positionAttempts=%d positionMessages=%d movedX=%ld movedY=%ld "
                "panels=%d extraViewports=%d viewportCreates=%d frames=%zu rows=82000 visibleRows=%d "
                "p50Ms=%.4f p95Ms=%.4f p99Ms=%.4f moveP95Ms=%.4f uiP95Ms=%.4f "
                "extraP95Ms=%.4f presentP95Ms=%.4f presented=%d\n",
        drag, duplicateCount, g_captionMessages, g_positionAttempts - basePositionAttempts,
        g_positionMessages - basePositionMessages, after.left - before.left,
        after.top - before.top, panelCount, ImGui::GetPlatformIO().Viewports.Size - 1,
        g_viewportCreates, frameMs.size(), visibleRows, frameMs[59], frameMs[113],
        frameMs[118], moveMs[113], uiMs[113], extraMs[113], presentMs[113], presented);
    const bool movedAsExpected = drag
        ? (g_captionMessages == 1 + 120 * (1 + duplicateCount) &&
           g_positionAttempts - basePositionAttempts <= 121 &&
           g_positionMessages - basePositionMessages == 120 &&
           after.left - before.left == 120)
        : (g_captionMessages == 0 && after.left == before.left);
    const bool passed = movedAsExpected && after.top - before.top == 0 &&
        presented && visibleRows > 0 &&
        ImGui::GetPlatformIO().Viewports.Size - 1 >= panelCount;
    if (drag) SendMessageW(hwnd, WM_LBUTTONUP, 0, 0);
    ImGui::DestroyPlatformWindows();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(context);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(rc);
    ReleaseDC(host, dc);
    DestroyWindow(hwnd);
    DestroyWindow(host);
    if (!passed) return 6;
    return 0;
}
