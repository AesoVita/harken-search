// src/HarkenSearchWidgetsFocus.cpp
// Aris UI v1 - focus ring state tracker (no render dependencies).
// DrawFocusRing lives in HarkenSearchWidgets.cpp so this translation unit stays
// cheap to link into unit tests that don't need Theme/ImGui draw state.
#include "HarkenSearchWidgets.h"
#include "imgui.h"

namespace aura::ui::focus {

// True when the last input event was keyboard/gamepad (not a mouse click).
// Updated per-frame by UpdateFocusTracker(); read by DrawFocusRing().
static bool g_LastInputWasKeyboard = false;

void UpdateFocusTracker() {
    // Mouse clicks dominate: a mouse press immediately hides the focus ring.
    for (int b = 0; b < 3; ++b) {
        if (ImGui::IsMouseDown(b)) {
            g_LastInputWasKeyboard = false;
            return;
        }
    }
    // Any keyboard/nav key press flips us into keyboard mode.
    const ImGuiKey kNavKeys[] = {
        ImGuiKey_Tab, ImGuiKey_LeftArrow, ImGuiKey_RightArrow,
        ImGuiKey_UpArrow, ImGuiKey_DownArrow,
        ImGuiKey_Space, ImGuiKey_Enter, ImGuiKey_KeypadEnter,
        ImGuiKey_Escape,
    };
    for (ImGuiKey k : kNavKeys) {
        if (ImGui::IsKeyDown(k)) {
            g_LastInputWasKeyboard = true;
            return;
        }
    }
}

bool IsKeyboardInputActive() {
    return g_LastInputWasKeyboard;
}

}
