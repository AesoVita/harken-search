#include "Theme.h"
#include "ThemeEditorSchema.h"
#include "DiagnosticsLogger.h"

#include "imgui.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

ImU32 g_waveformColor = 0;
ImU32 g_waveformBgColor = 0;
ImU32 g_waveformRmsColor = 0;
ImU32 g_miniWaveColor = 0;
ImU32 g_miniWaveBgColor = 0;
ImVec4 COLOR_BG_DARK = ImVec4(0, 0, 0, 1);
ImVec4 COLOR_BG_PANEL = ImVec4(0, 0, 0, 1);
ImVec4 COLOR_TEXT = ImVec4(1, 1, 1, 1);
ImVec4 COLOR_TEXT_DIM = ImVec4(0.5f, 0.5f, 0.5f, 1);
ImVec4 COLOR_ACCENT = ImVec4(0.6f, 0.8f, 0.2f, 1);
ImVec4 COLOR_ACCENT_DIM = ImVec4(0.4f, 0.5f, 0.15f, 1);
ImVec4 COLOR_SELECTED = ImVec4(0.8f, 0.6f, 0.1f, 1);
ImVec4 COLOR_WAVEFORM_BG = ImVec4(0, 0, 0, 1);
ImVec4 COLOR_WAVEFORM = ImVec4(1, 1, 1, 1);

DiagnosticsLogger& DiagnosticsLogger::instance() {
    static DiagnosticsLogger s_instance;
    return s_instance;
}

DiagnosticsLogger::DiagnosticsLogger() = default;
DiagnosticsLogger::~DiagnosticsLogger() = default;
void DiagnosticsLogger::setEnabled(bool) {}
void DiagnosticsLogger::logSystem(const char*, const char*) {}

namespace Platform {
bool IsSystemDarkMode() {
    return false;
}
}

std::filesystem::path Utf8ToPath(const std::string& pathUtf8) {
    return std::filesystem::path(pathUtf8);
}

std::string GetExeDirUtf8() {
    return std::filesystem::temp_directory_path().string();
}

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << (msg) << "\n"; \
        ++g_failed; \
    } else { \
        std::cout << "OK: " << (msg) << "\n"; \
        ++g_passed; \
    } \
} while (0)

static bool approxEqual(float a, float b, float eps = 0.0050f) {
    return std::fabs(a - b) <= eps;
}

static bool approxEqual(const ImVec4& a, const ImVec4& b, float eps = 0.0050f) {
    return approxEqual(a.x, b.x, eps)
        && approxEqual(a.y, b.y, eps)
        && approxEqual(a.z, b.z, eps)
        && approxEqual(a.w, b.w, eps);
}

static ImVec4 mixColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t);
}

static ImVec4 withAlpha(ImVec4 c, float alpha) {
    c.w = alpha;
    return c;
}

static const ThemeEditorItem* findEditorItem(const char* key) {
    size_t count = 0;
    const ThemeEditorItem* items = GetThemeEditorItems(count);
    for (size_t i = 0; i < count; ++i) {
        if (std::string(items[i].key) == key) {
            return &items[i];
        }
    }
    return nullptr;
}

int main() {
    std::cout << "=== Theme Playlist/Waveform Contract Tests ===\n\n";

    {
        const ThemeEditorItem* frameBgBase = findEditorItem("frameBgBase");
        CHECK(frameBgBase != nullptr, "editor exposes frameBgBase");
        if (frameBgBase) {
            CHECK(frameBgBase->group == ThemeEditorGroup::Foundation,
                  "frameBgBase is grouped under Foundation");
        }

        const ThemeEditorItem* chromeBorder = findEditorItem("chromeBorder");
        CHECK(chromeBorder != nullptr, "editor exposes chromeBorder");
        if (chromeBorder) {
            CHECK(chromeBorder->group == ThemeEditorGroup::Foundation,
                  "chromeBorder is grouped under Foundation");
        }

        const ThemeEditorItem* popupBg = findEditorItem("popupBG");
        CHECK(popupBg != nullptr, "editor exposes popupBG");
        if (popupBg) {
            CHECK(popupBg->group == ThemeEditorGroup::TopBar,
                  "popupBG is grouped under TopBar");
        }

        const ThemeEditorItem* menuPopupBg = findEditorItem("menuPopupBG");
        CHECK(menuPopupBg != nullptr, "editor exposes menuPopupBG");
        if (menuPopupBg) {
            CHECK(menuPopupBg->group == ThemeEditorGroup::TopBar,
                  "menuPopupBG is grouped under TopBar");
        }

        const ThemeEditorItem* dialogBg = findEditorItem("dialogBG");
        CHECK(dialogBg != nullptr, "editor exposes dialogBG");
        if (dialogBg) {
            CHECK(dialogBg->group == ThemeEditorGroup::TopBar,
                  "dialogBG is grouped under TopBar");
        }

        const ThemeEditorItem* dialogPanelBg = findEditorItem("dialogPanelBG");
        CHECK(dialogPanelBg != nullptr, "editor exposes dialogPanelBG");
        if (dialogPanelBg) {
            CHECK(dialogPanelBg->group == ThemeEditorGroup::TopBar,
                  "dialogPanelBG is grouped under TopBar");
        }

        const ThemeEditorItem* historyCapsuleBg = findEditorItem("historyCapsuleBG");
        CHECK(historyCapsuleBg != nullptr, "editor exposes historyCapsuleBG");
        if (historyCapsuleBg) {
            CHECK(historyCapsuleBg->group == ThemeEditorGroup::TopBar,
                  "historyCapsuleBG is grouped under TopBar");
        }

        const ThemeEditorItem* historyCapsuleActiveBg = findEditorItem("historyCapsuleActiveBG");
        CHECK(historyCapsuleActiveBg != nullptr, "editor exposes historyCapsuleActiveBG");
        if (historyCapsuleActiveBg) {
            CHECK(historyCapsuleActiveBg->group == ThemeEditorGroup::TopBar,
                  "historyCapsuleActiveBG is grouped under TopBar");
        }

        const ThemeEditorItem* modeCapsuleBg = findEditorItem("modeCapsuleBG");
        CHECK(modeCapsuleBg != nullptr, "editor exposes modeCapsuleBG");
        if (modeCapsuleBg) {
            CHECK(modeCapsuleBg->group == ThemeEditorGroup::TopBar,
                  "modeCapsuleBG is grouped under TopBar");
        }

        const ThemeEditorItem* modeCapsuleActiveBg = findEditorItem("modeCapsuleActiveBG");
        CHECK(modeCapsuleActiveBg != nullptr, "editor exposes modeCapsuleActiveBG");
        if (modeCapsuleActiveBg) {
            CHECK(modeCapsuleActiveBg->group == ThemeEditorGroup::TopBar,
                  "modeCapsuleActiveBG is grouped under TopBar");
        }

        const ThemeEditorItem* controlAccent = findEditorItem("controlAccent");
        CHECK(controlAccent != nullptr, "editor exposes controlAccent");
        if (controlAccent) {
            CHECK(controlAccent->group == ThemeEditorGroup::TopBar,
                  "controlAccent is grouped under TopBar");
        }

        const ThemeEditorItem* resultsGradientStart = findEditorItem("resultsGradientStart");
        CHECK(resultsGradientStart != nullptr, "editor exposes resultsGradientStart");
        if (resultsGradientStart) {
            CHECK(resultsGradientStart->group == ThemeEditorGroup::Results,
                  "resultsGradientStart is grouped under Results");
        }

        const ThemeEditorItem* resultsGradientEnd = findEditorItem("resultsGradientEnd");
        CHECK(resultsGradientEnd != nullptr, "editor exposes resultsGradientEnd");
        if (resultsGradientEnd) {
            CHECK(resultsGradientEnd->group == ThemeEditorGroup::Results,
                  "resultsGradientEnd is grouped under Results");
        }

        const ThemeEditorItem* playlistStart = findEditorItem("playlistGradientStart");
        CHECK(playlistStart != nullptr, "editor exposes playlistGradientStart");
        if (playlistStart) {
            CHECK(playlistStart->group == ThemeEditorGroup::Results,
                  "playlistGradientStart is grouped under Results");
        }

        const ThemeEditorItem* playlistEnd = findEditorItem("playlistGradientEnd");
        CHECK(playlistEnd != nullptr, "editor exposes playlistGradientEnd");
        if (playlistEnd) {
            CHECK(playlistEnd->group == ThemeEditorGroup::Results,
                  "playlistGradientEnd is grouped under Results");
        }

        const ThemeEditorItem* playlistHighlight = findEditorItem("playlistGradientHighlight");
        CHECK(playlistHighlight != nullptr, "editor exposes playlistGradientHighlight");
        if (playlistHighlight) {
            CHECK(playlistHighlight->group == ThemeEditorGroup::Results,
                  "playlistGradientHighlight is grouped under Results");
        }

        const ThemeEditorItem* playlistShadow = findEditorItem("playlistGradientShadow");
        CHECK(playlistShadow != nullptr, "editor exposes playlistGradientShadow");
        if (playlistShadow) {
            CHECK(playlistShadow->group == ThemeEditorGroup::Results,
                  "playlistGradientShadow is grouped under Results");
        }

        const ThemeEditorItem* coverWallBg = findEditorItem("coverWallBG");
        CHECK(coverWallBg != nullptr, "editor exposes coverWallBG");
        if (coverWallBg) {
            CHECK(coverWallBg->group == ThemeEditorGroup::Metadata,
                  "coverWallBG is grouped under Metadata");
        }

        const ThemeEditorItem* waveformFooterBg = findEditorItem("waveformFooterBG");
        CHECK(waveformFooterBg != nullptr, "editor exposes waveformFooterBG");
        if (waveformFooterBg) {
            CHECK(waveformFooterBg->group == ThemeEditorGroup::Waveform,
                  "waveformFooterBG is grouped under Waveform");
        }

        const ThemeEditorItem* waveformPanelBorder = findEditorItem("waveformPanelBorder");
        CHECK(waveformPanelBorder != nullptr, "editor exposes waveformPanelBorder");
        if (waveformPanelBorder) {
            CHECK(waveformPanelBorder->group == ThemeEditorGroup::Waveform,
                  "waveformPanelBorder is grouped under Waveform");
        }

        const ThemeEditorItem* waveformSplitterBg = findEditorItem("waveformSplitterBG");
        CHECK(waveformSplitterBg != nullptr, "editor exposes waveformSplitterBG");
        if (waveformSplitterBg) {
            CHECK(waveformSplitterBg->group == ThemeEditorGroup::Waveform,
                  "waveformSplitterBG is grouped under Waveform");
        }

        const ThemeEditorItem* waveformSplitterLine = findEditorItem("waveformSplitterLine");
        CHECK(waveformSplitterLine != nullptr, "editor exposes waveformSplitterLine");
        if (waveformSplitterLine) {
            CHECK(waveformSplitterLine->group == ThemeEditorGroup::Waveform,
                  "waveformSplitterLine is grouped under Waveform");
        }
    }

    {
        const std::string legacyJson = R"JSON({
  "mainWindowBG": "fff7f2ec",
  "menuBarBG": "fff2ece6",
  "menuText": "ff565a62",
  "toolbarBG": "ffede6e0",
  "transportBarBG": "ffe9e2dc",
  "timecodeText": "ff8fa4b3",
  "timecodeTextHover": "ff7f99ab",
  "interfaceText": "ff666a73",
  "resultsBG": "fffbf7f2",
  "resultsHeaderBG": "fff2ece6",
  "resultsText": "ff555962",
  "resultsOutline": "ffd8d0c9",
  "resultsSelection": "ffc7d7e8",
  "resultsScrollbar": "ffc9c2c3",
  "resultsTabBG": "ffebe4de",
  "resultsTabHover": "ffe3edf4",
  "resultsTabActive": "ffd9e5ef",
  "resultsSecondaryText": "ff93949c",
  "resultsPlayedText": "ffc3a0a6",
  "playlistGradientStart": "ffdde7f0",
  "playlistGradientEnd": "fff1ddd1",
  "metadataPanelBG": "fff5efe9",
  "metadataPanelText": "ff545861",
  "metadataPanelLabel": "ffb5c4d3",
  "metadataPanelScrollbar": "ffc8c1c2",
  "waveformBG": "fff8f3ee",
  "waveformColor": "ffb7c9dd",
  "waveformSecondaryColor": "ffdccedf",
  "waveformPositionLine": "ffe8d58d",
  "filterBrowserBG": "fff0e9e2",
  "filterBrowserText": "ff5a5e67",
  "filterBrowserHeading": "ff6d6870",
  "filterBrowserOutline": "ffd6cec7",
  "filterBrowserSelection": "ffc5d7e8",
  "filterBrowserScrollbar": "ffc7c0c2",
  "filterBrowserSectionBG": "ffe7dfd9",
  "filterBrowserButtonBG": "ffded6d0",
  "filterBrowserTreeText": "ff565a63",
  "filterBrowserTreeSelection": "ffc5d7e8",
  "transferButtonBG": "ffc7d8c5",
  "searchButtonBG": "ffe9d1c4",
  "transferHistoryBG": "ffe9e1db",
  "transferHistoryText": "ff7b7e86",
  "spottingBG": "ffefe8e2",
  "spottingText": "ff72767e",
  "tabBG": "ffe5ddd8",
  "volumeColor": "ffbfd1b0",
  "volumeClipColor": "ffd8aaa7"
})JSON";

        ThemeColorScheme imported = ThemeColors::getClassicDark();
        const bool ok = ThemeColors::importFromJSON(legacyJson, imported);
        CHECK(ok, "legacy schema JSON imports successfully");
        CHECK(approxEqual(imported.mainWindowBG, ThemeColors::fromARGBHex("fff7f2ec")),
              "legacy schema preserves mainWindowBG");
        CHECK(approxEqual(imported.playlistGradientStart, ThemeColors::fromARGBHex("ffdde7f0")),
              "legacy schema preserves playlistGradientStart");
        CHECK(approxEqual(imported.frameBgBase, imported.toolbarBG),
              "legacy schema backfills frameBgBase from toolbarBG");
        CHECK(approxEqual(imported.chromeBorder, imported.resultsOutline),
              "legacy schema backfills chromeBorder from resultsOutline");
        CHECK(approxEqual(imported.popupBG, ImVec4(imported.toolbarBG.x, imported.toolbarBG.y, imported.toolbarBG.z, 0.98f)),
              "legacy schema backfills popupBG from toolbarBG");
        CHECK(approxEqual(imported.menuPopupBG, imported.popupBG),
              "legacy schema backfills menuPopupBG from popupBG");
        CHECK(approxEqual(imported.dialogBG, ImVec4(
                  imported.popupBG.x + (imported.mainWindowBG.x - imported.popupBG.x) * 0.08f,
                  imported.popupBG.y + (imported.mainWindowBG.y - imported.popupBG.y) * 0.08f,
                  imported.popupBG.z + (imported.mainWindowBG.z - imported.popupBG.z) * 0.08f,
                  imported.popupBG.w + (imported.mainWindowBG.w - imported.popupBG.w) * 0.08f)),
              "legacy schema backfills dialogBG from popupBG and mainWindowBG");
        CHECK(approxEqual(imported.dialogPanelBG, ImVec4(
                  imported.dialogBG.x + (imported.frameBgBase.x - imported.dialogBG.x) * 0.16f,
                  imported.dialogBG.y + (imported.frameBgBase.y - imported.dialogBG.y) * 0.16f,
                  imported.dialogBG.z + (imported.frameBgBase.z - imported.dialogBG.z) * 0.16f,
                  imported.dialogBG.w + (imported.frameBgBase.w - imported.dialogBG.w) * 0.16f)),
              "legacy schema backfills dialogPanelBG from dialogBG and frameBgBase");
        CHECK(approxEqual(imported.historyCapsuleBG, ImVec4(
                  imported.toolbarBG.x + (imported.mainWindowBG.x - imported.toolbarBG.x) * 0.10f,
                  imported.toolbarBG.y + (imported.mainWindowBG.y - imported.toolbarBG.y) * 0.10f,
                  imported.toolbarBG.z + (imported.mainWindowBG.z - imported.toolbarBG.z) * 0.10f,
                  imported.toolbarBG.w + (imported.mainWindowBG.w - imported.toolbarBG.w) * 0.10f)),
              "legacy schema backfills historyCapsuleBG from toolbar and window");
        CHECK(approxEqual(imported.historyCapsuleActiveBG, ImVec4(
                  imported.frameBgBase.x + (1.0f - imported.frameBgBase.x) * 0.72f,
                  imported.frameBgBase.y + (1.0f - imported.frameBgBase.y) * 0.72f,
                  imported.frameBgBase.z + (1.0f - imported.frameBgBase.z) * 0.72f,
                  imported.frameBgBase.w + (1.0f - imported.frameBgBase.w) * 0.72f)),
              "legacy schema backfills historyCapsuleActiveBG from frameBgBase");
        CHECK(approxEqual(imported.modeCapsuleBG, ImVec4(
                  imported.toolbarBG.x + (imported.mainWindowBG.x - imported.toolbarBG.x) * 0.08f,
                  imported.toolbarBG.y + (imported.mainWindowBG.y - imported.toolbarBG.y) * 0.08f,
                  imported.toolbarBG.z + (imported.mainWindowBG.z - imported.toolbarBG.z) * 0.08f,
                  imported.toolbarBG.w + (imported.mainWindowBG.w - imported.toolbarBG.w) * 0.08f)),
              "legacy schema backfills modeCapsuleBG from toolbar and window");
        CHECK(approxEqual(imported.modeCapsuleActiveBG, ImVec4(
                  imported.frameBgBase.x + (1.0f - imported.frameBgBase.x) * 0.72f,
                  imported.frameBgBase.y + (1.0f - imported.frameBgBase.y) * 0.72f,
                  imported.frameBgBase.z + (1.0f - imported.frameBgBase.z) * 0.72f,
                  imported.frameBgBase.w + (1.0f - imported.frameBgBase.w) * 0.72f)),
              "legacy schema backfills modeCapsuleActiveBG from frameBgBase");
        CHECK(approxEqual(
                  imported.waveformPanelBorder,
                  mixColor(
                      imported.waveformBG,
                      mixColor(imported.chromeBorder, imported.controlAccent, 0.18f),
                      0.44f)),
              "legacy schema backfills waveformPanelBorder from waveformBG and chrome/control accent");
        CHECK(approxEqual(imported.waveformSplitterBG, ImVec4(
                  imported.resultsBG.x + (imported.waveformBG.x - imported.resultsBG.x) * 0.18f,
                  imported.resultsBG.y + (imported.waveformBG.y - imported.resultsBG.y) * 0.18f,
                  imported.resultsBG.z + (imported.waveformBG.z - imported.resultsBG.z) * 0.18f,
                  imported.resultsBG.w + (imported.waveformBG.w - imported.resultsBG.w) * 0.18f)),
              "legacy schema backfills waveformSplitterBG from resultsBG and waveformBG");
        CHECK(approxEqual(
                  imported.waveformSplitterLine,
                  mixColor(
                      imported.waveformSplitterBG,
                      mixColor(imported.waveformPanelBorder, imported.controlAccent, 0.14f),
                      0.24f)),
              "legacy schema backfills waveformSplitterLine from splitter, border, and control accent");
        CHECK(approxEqual(imported.toolbarAccent, imported.resultsSelection),
              "legacy schema backfills toolbarAccent from resultsSelection");
        CHECK(approxEqual(imported.controlAccent, imported.toolbarAccent),
              "legacy schema backfills controlAccent from toolbarAccent");
        CHECK(approxEqual(imported.resultsGradientStart, imported.resultsBG),
              "legacy schema backfills resultsGradientStart from resultsBG");
        CHECK(approxEqual(imported.resultsGradientEnd, ImVec4(
                  imported.resultsBG.x + (imported.resultsSelection.x - imported.resultsBG.x) * 0.10f,
                  imported.resultsBG.y + (imported.resultsSelection.y - imported.resultsBG.y) * 0.10f,
                  imported.resultsBG.z + (imported.resultsSelection.z - imported.resultsBG.z) * 0.10f,
                  imported.resultsBG.w + (imported.resultsSelection.w - imported.resultsBG.w) * 0.10f)),
              "legacy schema backfills resultsGradientEnd from results colors");
        CHECK(approxEqual(imported.playlistGradientHighlight, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)),
              "legacy schema backfills playlistGradientHighlight");
        CHECK(approxEqual(imported.playlistGradientShadow, imported.resultsBG),
              "legacy schema backfills playlistGradientShadow from resultsBG");
        CHECK(approxEqual(imported.coverWallBG, imported.metadataPanelBG),
              "legacy schema backfills coverWallBG from metadataPanelBG");
        CHECK(approxEqual(imported.waveformFooterBG, ImVec4(
                  imported.waveformBG.x + (imported.mainWindowBG.x - imported.waveformBG.x) * 0.28f,
                  imported.waveformBG.y + (imported.mainWindowBG.y - imported.waveformBG.y) * 0.28f,
                  imported.waveformBG.z + (imported.mainWindowBG.z - imported.waveformBG.z) * 0.28f,
                  imported.waveformBG.w + (imported.mainWindowBG.w - imported.waveformBG.w) * 0.28f)),
              "legacy schema backfills waveformFooterBG from waveform and window");
        CHECK(approxEqual(imported.filterBrowserLocateTint, imported.transferButtonBG),
              "legacy schema backfills filterBrowserLocateTint from transferButtonBG");
    }

    {
        ThemeColorScheme scheme = ThemeColors::getElectricBlue();
        scheme.menuBarBG = ImVec4(0.22f, 0.07f, 0.19f, 1.0f);
        scheme.toolbarBG = ImVec4(0.10f, 0.18f, 0.27f, 1.0f);
        scheme.metadataPanelBG = ImVec4(0.18f, 0.12f, 0.09f, 1.0f);
        scheme.metadataPanelScrollbar = ImVec4(0.64f, 0.48f, 0.22f, 1.0f);
        scheme.filterBrowserBG = ImVec4(0.04f, 0.08f, 0.12f, 1.0f);
        scheme.filterBrowserScrollbar = ImVec4(0.12f, 0.24f, 0.36f, 1.0f);
        scheme.resultsOutline = ImVec4(0.52f, 0.30f, 0.18f, 1.0f);
        scheme.waveformBG = ImVec4(0.07f, 0.08f, 0.09f, 1.0f);

        CHECK(approxEqual(ThemeColors::menuBarFillBG(scheme), scheme.menuBarBG),
              "menu bar fill uses menuBarBG");
        CHECK(!approxEqual(ThemeColors::menuBarFillBG(scheme), scheme.toolbarBG),
              "menu bar fill no longer follows toolbarBG");
        CHECK(approxEqual(ThemeColors::fileInfoPanelBG(scheme), scheme.metadataPanelBG),
              "file info panel background uses metadataPanelBG");
        CHECK(approxEqual(ThemeColors::fileInfoPanelScrollbar(scheme), scheme.metadataPanelScrollbar),
              "file info panel scrollbar uses metadataPanelScrollbar");
        CHECK(approxEqual(ThemeColors::peerPanelBG(scheme), scheme.filterBrowserBG),
              "peer panel background still uses filterBrowserBG");
        CHECK(approxEqual(ThemeColors::peerPanelScrollbar(scheme), scheme.filterBrowserScrollbar),
              "peer panel scrollbar still uses filterBrowserScrollbar");
        CHECK(approxEqual(ThemeColors::waveformPanelBG(scheme), scheme.waveformBG),
              "waveform panel background uses waveformBG");
        CHECK(!approxEqual(ThemeColors::waveformPanelBG(scheme), scheme.filterBrowserBG),
              "waveform panel background no longer follows filterBrowserBG");
        CHECK(approxEqual(ThemeColors::waveformFrameBorder(scheme),
                          ImVec4(0.1816f, 0.1904f, 0.1992f, 0.68f)),
              "waveform frame border is derived from waveformBG");
    }

    {
        ThemeColorScheme scheme = ThemeColors::getPowderedPastels();
        scheme.waveformBG = ImVec4(0.97f, 0.95f, 0.92f, 1.0f);
        scheme.waveformPanelBorder = ImVec4(0.82f, 0.78f, 0.74f, 1.0f);
        scheme.chromeBorder = ImVec4(0.74f, 0.69f, 0.66f, 1.0f);

        const ImVec4 expectedLightBorder = withAlpha(
            mixColor(
                scheme.waveformBG,
                mixColor(scheme.waveformPanelBorder, scheme.chromeBorder, 0.30f),
                0.16f),
            0.24f);

        CHECK(approxEqual(ThemeColors::waveformFrameBorder(scheme), expectedLightBorder),
              "light waveform frame border stays close to theme border instead of dark edge");
    }

    {
        ThemeColorScheme scheme = ThemeColors::getElectricBlue();
        scheme.menuBarBG = ImVec4(0.24f, 0.11f, 0.35f, 1.0f);
        scheme.transportBarBG = ImVec4(0.10f, 0.18f, 0.27f, 1.0f);
        scheme.resultsHeaderBG = ImVec4(0.31f, 0.19f, 0.12f, 1.0f);
        scheme.resultsPlayedText = ImVec4(0.92f, 0.58f, 0.24f, 1.0f);
        scheme.playlistGradientStart = ImVec4(0.16f, 0.40f, 0.90f, 1.0f);
        scheme.playlistGradientEnd = ImVec4(0.08f, 0.22f, 0.54f, 1.0f);
        scheme.playlistGradientHighlight = ImVec4(0.97f, 0.93f, 0.76f, 1.0f);
        scheme.playlistGradientShadow = ImVec4(0.15f, 0.18f, 0.24f, 1.0f);
        scheme.waveformBG = ImVec4(0.12f, 0.16f, 0.22f, 1.0f);

        const std::string json = ThemeColors::exportToJSON(scheme);
        CHECK(json.find("\"menuBarBG\"") != std::string::npos,
              "JSON export includes menuBarBG");
        CHECK(json.find("\"transportBarBG\"") != std::string::npos,
              "JSON export includes transportBarBG");
        CHECK(json.find("\"resultsHeaderBG\"") != std::string::npos,
              "JSON export includes resultsHeaderBG");
        CHECK(json.find("\"resultsPlayedText\"") != std::string::npos,
              "JSON export includes resultsPlayedText");
        CHECK(json.find("\"playlistGradientStart\"") != std::string::npos,
              "JSON export includes playlistGradientStart");
        CHECK(json.find("\"playlistGradientEnd\"") != std::string::npos,
              "JSON export includes playlistGradientEnd");
        CHECK(json.find("\"playlistGradientHighlight\"") != std::string::npos,
              "JSON export includes playlistGradientHighlight");
        CHECK(json.find("\"playlistGradientShadow\"") != std::string::npos,
              "JSON export includes playlistGradientShadow");

        ThemeColorScheme roundTrip = ThemeColors::getClassicDark();
        const bool imported = ThemeColors::importFromJSON(json, roundTrip);
        CHECK(imported, "JSON import succeeds");
        CHECK(approxEqual(roundTrip.menuBarBG, scheme.menuBarBG),
              "menuBarBG survives round-trip");
        CHECK(approxEqual(roundTrip.transportBarBG, scheme.transportBarBG),
              "transportBarBG survives round-trip");
        CHECK(approxEqual(roundTrip.resultsHeaderBG, scheme.resultsHeaderBG),
              "resultsHeaderBG survives round-trip");
        CHECK(approxEqual(roundTrip.resultsPlayedText, scheme.resultsPlayedText),
              "resultsPlayedText survives round-trip");
        CHECK(approxEqual(roundTrip.playlistGradientStart, scheme.playlistGradientStart),
              "playlistGradientStart survives round-trip");
        CHECK(approxEqual(roundTrip.playlistGradientEnd, scheme.playlistGradientEnd),
              "playlistGradientEnd survives round-trip");
        CHECK(approxEqual(roundTrip.playlistGradientHighlight, scheme.playlistGradientHighlight),
              "playlistGradientHighlight survives round-trip");
        CHECK(approxEqual(roundTrip.playlistGradientShadow, scheme.playlistGradientShadow),
              "playlistGradientShadow survives round-trip");
        CHECK(approxEqual(roundTrip.waveformBG, scheme.waveformBG),
              "waveformBG survives round-trip");
    }

    {
        namespace fs = std::filesystem;
        const fs::path tmpPath =
            fs::temp_directory_path() / fs::u8path(u8"aura_theme_𠮷😀_roundtrip.json");
        const std::string tmpPathUtf8 = tmpPath.u8string();

        ThemeColorScheme scheme = ThemeColors::getOceanBlue();
        scheme.waveformBG = ImVec4(0.22f, 0.14f, 0.10f, 1.0f);
        scheme.playlistGradientStart = ImVec4(0.72f, 0.42f, 0.18f, 1.0f);
        scheme.playlistGradientEnd = ImVec4(0.24f, 0.12f, 0.08f, 1.0f);
        scheme.playlistGradientHighlight = ImVec4(0.95f, 0.90f, 0.72f, 1.0f);
        scheme.playlistGradientShadow = ImVec4(0.20f, 0.16f, 0.14f, 1.0f);

        const bool saved = ThemeColors::saveThemeToFile(tmpPathUtf8, scheme);
        CHECK(saved, "theme file saves to custom UTF-8 path");

        ThemeColorScheme loaded = ThemeColors::getClassicDark();
        const bool loadedOk = ThemeColors::loadThemeFromFile(tmpPathUtf8, loaded);
        CHECK(loadedOk, "theme file loads from custom UTF-8 path");
        if (loadedOk) {
            CHECK(approxEqual(loaded.waveformBG, scheme.waveformBG),
                  "custom-path save/load preserves waveformBG");
            CHECK(approxEqual(loaded.playlistGradientStart, scheme.playlistGradientStart),
                  "custom-path save/load preserves playlistGradientStart");
            CHECK(approxEqual(loaded.playlistGradientEnd, scheme.playlistGradientEnd),
                  "custom-path save/load preserves playlistGradientEnd");
            CHECK(approxEqual(loaded.playlistGradientHighlight, scheme.playlistGradientHighlight),
                  "custom-path save/load preserves playlistGradientHighlight");
            CHECK(approxEqual(loaded.playlistGradientShadow, scheme.playlistGradientShadow),
                  "custom-path save/load preserves playlistGradientShadow");
        }

        std::error_code ec;
        fs::remove(tmpPath, ec);
    }

    std::cout << "\nPassed: " << g_passed << "\n";
    if (g_failed > 0) {
        std::cerr << "Failed: " << g_failed << "\n";
        return 1;
    }
    std::cout << "All theme playlist/waveform contract tests passed.\n";
    return 0;
}
