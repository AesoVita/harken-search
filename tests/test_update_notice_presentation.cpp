#include "update/HarkenSearchUpdateNoticeUi.h"

#include <cassert>

#if defined(ARIS_UPDATE_NOTICE_NATIVE_QA)
#include "Theme.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"
#include <windows.h>
#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <fstream>

using namespace aris::update;

ImU32 g_waveformColor = 0, g_waveformBgColor = 0, g_waveformRmsColor = 0;
ImU32 g_miniWaveColor = 0, g_miniWaveBgColor = 0;
ImVec4 COLOR_BG_DARK, COLOR_BG_PANEL, COLOR_TEXT, COLOR_TEXT_DIM, COLOR_ACCENT;
ImVec4 COLOR_ACCENT_DIM, COLOR_SELECTED, COLOR_WAVEFORM_BG, COLOR_WAVEFORM;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static float QaLinear(float value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}
static float QaContrast(const ImVec4& a, const ImVec4& b) {
    const float la = 0.2126f * QaLinear(a.x) + 0.7152f * QaLinear(a.y) + 0.0722f * QaLinear(a.z);
    const float lb = 0.2126f * QaLinear(b.x) + 0.7152f * QaLinear(b.y) + 0.0722f * QaLinear(b.z);
    const float lighter = la > lb ? la : lb;
    const float darker = la < lb ? la : lb;
    return (lighter + 0.05f) / (darker + 0.05f);
}
static ImVec4 QaMix(const ImVec4& a, const ImVec4& b, float amount) {
    return ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount,
                  a.z + (b.z - a.z) * amount, a.w + (b.w - a.w) * amount);
}
static LRESULT WINAPI NoticeQaWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    WNDCLASSW wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = NoticeQaWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"HarkenSearchUpdateNoticeNativeQa";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Aris Update Center Native QA",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 980, 780,
        nullptr, nullptr, instance, nullptr);
    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd{sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
        PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0};
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    HGLRC gl = wglCreateContext(dc);
    wglMakeCurrent(dc, gl);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/msyh.ttc", 18.0f, nullptr,
        ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    const char* themeName = std::getenv("ARIS_UPDATE_NOTICE_QA_THEME");
    ThemeColors::current() = themeName && std::string(themeName) == "light"
        ? ThemeColors::getPowderedPastels()
        : (themeName && std::string(themeName) == "lapis"
            ? ThemeColors::getDeepNavy() : ThemeColors::getHarkenSearchProductDark());
    ThemeColors::applyToImGui();

    if (const char* contrastPath = std::getenv("ARIS_UPDATE_NOTICE_QA_CONTRAST_MANIFEST")) {
        std::ofstream out(contrastPath, std::ios::binary);
        out << "{\n  \"themes\": [\n";
        bool contrastOk = true;
        for (int index = 0; index < 8; ++index) {
            const auto preset = ThemeColors::getPresetByIndex(index);
            const float normal = QaContrast(preset.mainWindowBG, preset.resultsText);
            const float hover = QaContrast(
                preset.mainWindowBG, QaMix(preset.resultsText, preset.mainWindowBG, 0.06f));
            const float pressed = QaContrast(
                preset.mainWindowBG, QaMix(preset.resultsText, preset.mainWindowBG, 0.10f));
            const float disabled = QaContrast(preset.resultsText, preset.frameBgBase);
            const float progress = QaContrast(preset.resultsText, preset.dialogPanelBG);
            contrastOk = contrastOk && normal >= 4.5f && hover >= 4.5f &&
                         pressed >= 4.5f && disabled >= 4.5f && progress >= 3.0f;
            out << "    {\"index\":" << index << ",\"normal\":" << normal
                << ",\"hover\":" << hover << ",\"pressed\":" << pressed
                << ",\"disabled\":" << disabled << ",\"progress\":" << progress << "}"
                << (index == 7 ? "\n" : ",\n");
        }
        out << "  ],\n  \"pass\": " << (contrastOk ? "true" : "false") << "\n}\n";
        if (!contrastOk) return 9;
    }

    const std::string stateName = std::getenv("ARIS_UPDATE_NOTICE_QA_STATE")
        ? std::getenv("ARIS_UPDATE_NOTICE_QA_STATE") : "available";
    HarkenSearchUpdateUiState update{};
    update.currentVersion = "1.0.24";
    update.candidateVersion = u8"1.0.25-super-long-version-中文边界-真实表情🎧";
    if (stateName == "checking") update.phase = HarkenSearchUpdateUiPhase::Checking;
    else if (stateName == "latest" || stateName.rfind("history", 0) == 0) update.phase = HarkenSearchUpdateUiPhase::Idle;
    else if (stateName.rfind("available", 0) == 0 || stateName == "ready-popup") update.phase = HarkenSearchUpdateUiPhase::Ready;
    else if (stateName == "downloading") {
        update.phase = HarkenSearchUpdateUiPhase::Downloading;
        update.downloadedBytes = 56U * 1024U * 1024U;
        update.totalBytes = 132U * 1024U * 1024U;
    } else if (stateName == "restarting") update.phase = HarkenSearchUpdateUiPhase::Restarting;
    else {
        update.phase = HarkenSearchUpdateUiPhase::Error;
        update.lastError = u8"runtime pair mismatch: Bootstrap signature rejected; 请重新下载完整安装包后重试。这里继续追加约百分之三十的诊断说明，以验证超长错误详情仍可滚动到全文末尾。";
    }
    ReleaseNotesUiState notes{};
    notes.hasData = true;
    notes.showHistory = stateName.rfind("history", 0) == 0;
    notes.releases = {
        {"v1.0.25", "2026-08-11", u8"自动更新闭环与主题一致性：这是一条接近四百一十像素限制的超长中文历史标题🎧", {"Aris Team"},
         {{"fix", u8"更新中心", u8"七种状态、完整历史内容和约百分之三十扩展文案均可到达。", "Aris Team"}}, {}},
        {"v1.0.24", "2026-08-04", u8"真实发布链路验证", {"Aris Team"},
         {{"fix", u8"安装", u8"失败时保留确切诊断现场。", "Aris Team"}}, {}},
    };
    if (stateName == "history-expanded") notes.expandedReleaseVersion = "v1.0.25";
    bool open = true;
    HarkenSearchUpdateNoticeActions actions{
        [] { return true; },
        [] { return true; },
    };
    HarkenSearchUpdateNoticePresentation presentation{};
    presentation.windowId = "##NativeQaUpdateCenter";
    presentation.downloadedBytes = update.downloadedBytes;
    presentation.totalBytes = update.totalBytes;
    presentation.lastError = update.lastError;

    bool readyMarkerWritten = false;
    while (open) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) open = false;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (stateName == "available-hover" || stateName == "available-pressed") {
            ImGui::GetIO().AddMousePosEvent(405.0f, 557.0f);
            ImGui::GetIO().AddMouseButtonEvent(0, stateName == "available-pressed");
        }
        ImGui::NewFrame();
        if (stateName == "history-expanded" && notes.expandedReleaseVersion.empty()) {
            notes.expandedReleaseVersion = "v1.0.25";
        }
        if (stateName == "ready-popup") {
            ImGui::SetNextWindowSize(ImVec2(520.0f, 210.0f), ImGuiCond_Always);
            ImGui::SetNextWindowPos(
                ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("##RealReadyPopupEvidence", &open,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            const std::string readyCopy = BuildHarkenSearchUpdateReadyCopy(update.candidateVersion);
            ImGui::TextWrapped("%s", readyCopy.c_str());
            ImGui::Separator();
            ImGui::TextWrapped(u8"立即重启 Aris，并应用已经下载完成的更新。");
            ImGui::Button(u8"立即重启", ImVec2(130.0f, 0.0f));
            ImGui::SameLine();
            ImGui::Button(u8"稍后再说", ImVec2(110.0f, 0.0f));
            ImGui::End();
        } else {
            RenderHarkenSearchUpdateNoticeUi(open, notes, update, actions, presentation);
        }
        ImGui::Render();
        RECT rect; GetClientRect(hwnd, &rect);
        glViewport(0, 0, rect.right, rect.bottom);
        const auto& bg = ThemeColors::current().mainWindowBG;
        glClearColor(bg.x, bg.y, bg.z, bg.w); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); SwapBuffers(dc);
        if (!readyMarkerWritten) {
            if (const char* markerPath = std::getenv("ARIS_UPDATE_NOTICE_QA_READY_MARKER")) {
                std::ofstream marker(markerPath, std::ios::binary);
                marker << "frame-ready";
            }
            readyMarkerWritten = true;
        }
        Sleep(10);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(gl); ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, instance);
    return 0;
}

#else

using namespace aris::update;

int main() {
    static_assert(static_cast<int>(HarkenSearchUpdateNoticeVisibleState::History) == 6);
    const std::string rawDisplay = u8"v1.0-中文-✓-🎧✨";
    const std::string rawDisplayBefore = rawDisplay;
    if (SanitizeHarkenSearchUpdateDisplayText(rawDisplay) !=
        u8"v1.0-中文-✓-[emoji]✨") return 20;
    if (rawDisplay != rawDisplayBefore) return 21;
    if (SanitizeHarkenSearchUpdateDisplayText(u8"🎧🎧") != "[emoji][emoji]") return 22;
    const std::string malformedDisplay("A\xF0\x28\x8C\x28Z", 6);
    if (SanitizeHarkenSearchUpdateDisplayText(malformedDisplay) !=
        "A[invalid]([invalid](Z") return 23;
    const std::string truncatedDisplay("B\xE4\xB8", 3);
    if (SanitizeHarkenSearchUpdateDisplayText(truncatedDisplay) != "B[invalid]") return 24;
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Checking, false) ==
           HarkenSearchUpdateNoticeVisibleState::Checking);
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Idle, false) ==
           HarkenSearchUpdateNoticeVisibleState::Latest);
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Ready, false) ==
           HarkenSearchUpdateNoticeVisibleState::Available);
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Downloading, false) ==
           HarkenSearchUpdateNoticeVisibleState::Downloading);
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Restarting, false) ==
           HarkenSearchUpdateNoticeVisibleState::Restarting);
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Error, false) ==
           HarkenSearchUpdateNoticeVisibleState::Error);
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Idle, true) ==
           HarkenSearchUpdateNoticeVisibleState::History);

    const auto unknown = BuildHarkenSearchUpdateProgress(42, 0);
    assert(!unknown.determinate);
    const auto known = BuildHarkenSearchUpdateProgress(50, 100);
    assert(known.determinate && known.fraction == 0.5f);

    assert(ClassifyHarkenSearchUpdateHistory(false, false) == HarkenSearchUpdateHistoryState::Empty);
    assert(ClassifyHarkenSearchUpdateHistory(true, false) == HarkenSearchUpdateHistoryState::LoadFailed);
    assert(ClassifyHarkenSearchUpdateHistory(false, true) == HarkenSearchUpdateHistoryState::Populated);

    HarkenSearchUpdateNoticeActionGate gate;
    gate.Enter("ready:v1.0.25");
    assert(gate.TryConsume());
    assert(!gate.TryConsume());
    gate.Enter("error:network");
    assert(gate.TryConsume());
    gate.Enter("latest", 1);
    assert(gate.TryConsume());
    assert(!gate.TryConsume());
    gate.Enter("checking", 1);
    assert(gate.TryConsume());
    gate.Enter("latest", 2);
    assert(gate.TryConsume());
    gate.Enter("latest", 3);  // Same visible key, a new real operation generation.
    assert(gate.TryConsume());
    gate.Enter("latest", 4);
    if (!gate.TryConsume()) return 25;
    gate.ReleaseRejectedAction();
    if (!gate.TryConsume()) return 26;

    ReleaseNotesUiState closeNotes{};
    closeNotes.showHistory = true;
    closeNotes.expandedReleaseVersion = "v1";
    bool open = true;
    FinalizeHarkenSearchUpdateNoticeClose(open, closeNotes);
    assert(!open && !closeNotes.showHistory && closeNotes.expandedReleaseVersion.empty());
    open = true;
    assert(MapHarkenSearchUpdateNoticeState(HarkenSearchUpdateUiPhase::Ready, closeNotes.showHistory) ==
           HarkenSearchUpdateNoticeVisibleState::Available);

    const auto readyCopy = BuildHarkenSearchUpdateReadyCopy("1.0.25");
    assert(readyCopy.find(u8"已下载") != std::string::npos);
    assert(readyCopy.find(u8"已更新到") == std::string::npos);
    return 0;
}
#endif
