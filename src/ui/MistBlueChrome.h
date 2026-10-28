#pragma once
#include "Theme.h"
#include "imgui.h"
namespace MistBlueChrome {
// Applied only to the built-in LapisBlue preset. Keep the dark icon accent
// separate from the light surfaces carrying dark text.
inline void Apply(ImVec4* colors,const ThemeColorScheme& s) {
    colors[ImGuiCol_ButtonActive]=s.historyCapsuleActiveBG;
    colors[ImGuiCol_TabActive]=s.resultsTabActive;
    colors[ImGuiCol_TabHovered]=s.resultsTabHover;
    colors[ImGuiCol_HeaderActive]=s.historyCapsuleActiveBG;
}
inline ImVec4 ResultsTabText(const ThemeColorScheme& s) { return s.resultsText; }
}
