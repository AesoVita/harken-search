#include "CoverWallLabState.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"

#include <array>
#include <cstdio>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

struct GlWindow {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC glrc = nullptr;
};

bool CreateDeviceWGL(HWND hwnd, GlWindow& out) {
    out.hdc = ::GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int pixelFormat = ::ChoosePixelFormat(out.hdc, &pfd);
    if (pixelFormat == 0 || !::SetPixelFormat(out.hdc, pixelFormat, &pfd)) {
        return false;
    }
    out.glrc = ::wglCreateContext(out.hdc);
    if (!out.glrc || !::wglMakeCurrent(out.hdc, out.glrc)) {
        return false;
    }
    return true;
}

void CleanupDeviceWGL(GlWindow& window) {
    if (window.glrc) {
        ::wglMakeCurrent(nullptr, nullptr);
        ::wglDeleteContext(window.glrc);
        window.glrc = nullptr;
    }
    if (window.hwnd && window.hdc) {
        ::ReleaseDC(window.hwnd, window.hdc);
        window.hdc = nullptr;
    }
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            ::glViewport(0, 0, LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_CLOSE:
        ::PostQuitMessage(0);
        return 0;
    default:
        return ::DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

ImVec4 ColorFromKey(const std::string& key) {
    unsigned int hash = 2166136261u;
    for (unsigned char ch : key) {
        hash ^= ch;
        hash *= 16777619u;
    }
    const float r = 0.20f + static_cast<float>((hash >> 0) & 0x7F) / 255.0f;
    const float g = 0.20f + static_cast<float>((hash >> 8) & 0x7F) / 255.0f;
    const float b = 0.20f + static_cast<float>((hash >> 16) & 0x7F) / 255.0f;
    return ImVec4(r, g, b, 1.0f);
}

void RenderLab(aura::coverwall_lab::CoverWallLabState& state,
               std::array<char, 1024>& folderBuffer,
               std::array<char, 256>& keywordBuffer,
               std::string& status) {
    using aura::coverwall_lab::TileSizePreset;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("CoverWallLab", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextUnformatted("CoverWallLab");
    ImGui::SameLine();
    ImGui::TextDisabled("source: %s", state.sourceLabel().c_str());
    ImGui::Separator();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 190.0f);
    ImGui::InputText("##folder", folderBuffer.data(), folderBuffer.size());
    ImGui::SameLine();
    if (ImGui::Button("Import Folder", ImVec2(120, 0))) {
        std::string error;
        if (state.importFolder(folderBuffer.data(), &error)) {
            status = "Imported folder into lab memory.";
        } else {
            status = error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fixture", ImVec2(60, 0))) {
        state.loadFixture();
        status = "Fixture library loaded.";
    }

    if (ImGui::InputTextWithHint("Keyword", "search label, path, or file", keywordBuffer.data(), keywordBuffer.size())) {
        state.setKeyword(keywordBuffer.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
        keywordBuffer[0] = '\0';
        state.clearAllFilters();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Cover")) {
        state.clearCover();
    }

    int tileSize = state.filters().tileSizePx;
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::SliderInt("Tile Size", &tileSize,
                         aura::coverwall_lab::CoverWallLabState::kMinTileSizePx,
                         aura::coverwall_lab::CoverWallLabState::kMaxTileSizePx)) {
        state.setTileSizePx(tileSize);
    }
    ImGui::SameLine();
    if (ImGui::Button("Compact")) {
        state.applyPreset(TileSizePreset::Compact);
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        state.applyPreset(TileSizePreset::Browse);
    }
    ImGui::SameLine();
    if (ImGui::Button("Inspect")) {
        state.applyPreset(TileSizePreset::Inspect);
    }

    ImGui::Text("Results: %lld  Paths: %zu  Cover filter: %s  Status: %s",
                static_cast<long long>(state.results().totalCount),
                state.pathFacets().size(),
                state.filters().selectedCoverKey.empty() ? "(none)" : state.filters().selectedCoverKey.c_str(),
                status.c_str());
    ImGui::Separator();

    const float leftWidth = 280.0f;
    ImGui::BeginChild("paths", ImVec2(leftWidth, 0), true);
    ImGui::TextUnformatted("Paths");
    if (ImGui::Selectable("(all paths)", state.filters().selectedPath.empty())) {
        state.clearPath();
    }
    for (const std::string& path : state.pathFacets()) {
        if (ImGui::Selectable(path.c_str(), state.filters().selectedPath == path)) {
            state.selectPath(path);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("wall", ImVec2(0, 0), true);
    const int columns = state.calculateColumnCount(ImGui::GetContentRegionAvail().x);
    const float tile = static_cast<float>(state.filters().tileSizePx);
    const float labelHeight = 42.0f;
    const float rowHeight = tile + labelHeight + ImGui::GetStyle().ItemSpacing.y * 2.0f;
    const int totalItems = static_cast<int>(state.results().items.size());
    const int totalRows = (totalItems + columns - 1) / columns;
    ImGuiListClipper clipper;
    clipper.Begin(totalRows, rowHeight);
    while (clipper.Step()) {
        const auto range = aura::coverwall_lab::CoverWallLabState::calculateVisibleRange(
            clipper.DisplayStart, clipper.DisplayEnd, columns, totalItems);
        int column = 0;
        for (int itemIndex = range.startIndex; itemIndex < range.endIndex; ++itemIndex) {
            const auto& item = state.results().items[static_cast<size_t>(itemIndex)];
            ImGui::PushID(itemIndex);
            const ImVec4 color = ColorFromKey(item.coverKey);
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.08f, color.y + 0.08f, color.z + 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x + 0.14f, color.y + 0.14f, color.z + 0.14f, 1.0f));
            if (ImGui::Button(item.displayLabel.c_str(), ImVec2(tile, tile))) {
                state.selectCover(item.coverKey);
            }
            ImGui::PopStyleColor(3);
            ImGui::TextWrapped("%s", item.folderPath.c_str());
            ImGui::TextDisabled("%d files", item.hitCount);
            ImGui::Dummy(ImVec2(tile, labelHeight - ImGui::GetTextLineHeightWithSpacing()));
            ImGui::PopID();
            ++column;
            if (column < columns && itemIndex + 1 < range.endIndex) {
                ImGui::SameLine();
            } else {
                column = 0;
            }
        }
    }
    if (state.results().items.empty()) {
        ImGui::TextDisabled("No covers match the current filters.");
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    WNDCLASSEXW wc = {sizeof(WNDCLASSEXW), CS_OWNDC, WndProc, 0, 0, hInstance, nullptr, nullptr, nullptr, nullptr, L"CoverWallLab", nullptr};
    ::RegisterClassExW(&wc);
    GlWindow window;
    window.hwnd = ::CreateWindowW(wc.lpszClassName, L"CoverWallLab",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1280, 820,
                                  nullptr, nullptr, hInstance, nullptr);
    if (!window.hwnd || !CreateDeviceWGL(window.hwnd, window)) {
        CleanupDeviceWGL(window);
        ::UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ::UpdateWindow(window.hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window.hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    aura::coverwall_lab::CoverWallLabState state;
    state.loadSettings("coverwall_lab_settings.ini");
    std::array<char, 1024> folderBuffer = {};
    std::array<char, 256> keywordBuffer = {};
    std::string status = "Ready.";

    bool running = true;
    while (running) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderLab(state, folderBuffer, keywordBuffer, status);
        ImGui::Render();

        const ImVec4 clearColor(0.08f, 0.09f, 0.10f, 1.0f);
        glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(window.hdc);
    }

    state.saveSettings("coverwall_lab_settings.ini");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceWGL(window);
    ::DestroyWindow(window.hwnd);
    ::UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}

#else
int main() {
    return 0;
}
#endif
