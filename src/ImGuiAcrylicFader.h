#pragma once
#include "imgui.h"

namespace ui::synth {

// @stage7-contract
// API contract: acrylic fader helpers are UI-thread ImGui draw/interaction
// functions. They borrow the active context, id, and value pointer for the
// call; AcrySliderFloat mutates only *value within the supplied range and
// reports change through bool. No pointer or draw state is retained.

// Themed version: LED color passed as parameter (replaces hardcoded Cyan-400).
// size: arbitrary float (not enum-constrained).
void DrawArtAcrylicFaderThemed(ImVec2 center, float size,
                                bool is_hovered, ImVec4 ledColor,
                                bool isLightTheme = false);

// Interactive horizontal slider: renders track groove + acrylic cap + handles mouse input.
// Returns true when value changed.
// trackWidth/trackHeight: pixel area the slider occupies.
// capSize: acrylic cap side length in pixels.
bool AcrySliderFloat(const char* id, float* value, float min, float max,
                     float trackWidth, float trackHeight, float capSize,
                     ImVec4 ledColor, bool isLightTheme = false);

} // namespace ui::synth
