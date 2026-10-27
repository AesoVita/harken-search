#pragma once

#include "imgui.h"

struct AppState;

// @stage7-contract
// API contract: UI-thread drawing of the left sidebar activity bar. AppState is
// borrowed for one frame and not retained. Sidebar panel content, filter trees,
// and tag clouds remain owned by the composition root.

void DrawSidebarActivityBar(AppState& state);
float SidebarRailWidth(const AppState& state);
const char* GetSidebarSectionLabel(int sectionId);
void DrawSidebarIcon(ImDrawList* dl, ImVec2 center, float size, int sectionId, ImU32 color);
