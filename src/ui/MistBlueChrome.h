#pragma once
#include "Theme.h"
#include "imgui.h"
namespace MistBlueChrome {
struct ControlSurfaces { ImVec4 normal, hovered, active, title, muted; };
inline ControlSurfaces Controls(const ThemeColorScheme& s) {
    return {s.frameBgBase, s.resultsTabHover, s.modeCapsuleActiveBG,
            s.toolbarBG, s.resultsSecondaryText};
}
// Applied only to the built-in LapisBlue preset. Keep the light icon accent
// separate from the navy surfaces carrying light text.
inline void Apply(ImVec4* colors,const ThemeColorScheme& s) {
    const auto c = Controls(s);
    colors[ImGuiCol_FrameBg]=c.normal;
    colors[ImGuiCol_FrameBgHovered]=c.hovered;
    colors[ImGuiCol_FrameBgActive]=c.active;
    colors[ImGuiCol_Button]=c.normal;
    colors[ImGuiCol_ButtonHovered]=c.hovered;
    // Muted labels are still information, not disabled controls. Actual disabled
    // widgets retain ImGui DisabledAlpha on top of this readable base.
    colors[ImGuiCol_TextDisabled]=s.resultsSecondaryText;
    colors[ImGuiCol_ButtonActive]=c.active;
    colors[ImGuiCol_TabActive]=s.resultsTabActive;
    colors[ImGuiCol_TabHovered]=s.resultsTabHover;
    colors[ImGuiCol_HeaderActive]=s.historyCapsuleActiveBG;
}
inline ImVec4 ResultsTabText(const ThemeColorScheme& s) { return s.resultsText; }
}
