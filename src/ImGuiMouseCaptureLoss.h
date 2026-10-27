#pragma once

#include "imgui.h"

// @stage7-contract: Call on the ImGui UI thread after WM_CAPTURECHANGED.
// Win32 can transfer mouse capture without delivering WM_LBUTTONUP to this window.
// If there is no current ImGui context, there is nothing to release. Otherwise
// queue a left-button release so widgets finish their drag on the next frame.
inline void ReleaseImGuiLeftButtonOnCaptureLoss() {
    if (ImGui::GetCurrentContext()) {
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    }
}
