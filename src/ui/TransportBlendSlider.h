#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>

namespace TransportBlendSlider {
inline bool Draw(const char* label, float* percent, ImU32 ink, ImU32 background) {
    ImGui::PushStyleColor(ImGuiCol_Text, ink);
    const bool changed = ImGui::SliderFloat(label, percent, 0.f, 100.f, "%.0f%%");
    ImGui::PopStyleColor();
    // Ctrl+click and keyboard activation use ImGui's editor. Its text and caret
    // must remain visible instead of being covered by the ordinary value plate.
    if (ImGui::TempInputIsActive(ImGui::GetItemID())) return changed;
    const ImVec2 minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    char text[16]; std::snprintf(text, sizeof(text), "%.0f%%", *percent);
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 position((minimum.x + maximum.x - size.x) * .5f,
        (minimum.y + maximum.y - size.y) * .5f);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(position.x - 2, position.y),
        ImVec2(position.x + size.x + 2, position.y + size.y), background, 2);
    draw->AddText(position, ink, text);
    return changed;
}
}
