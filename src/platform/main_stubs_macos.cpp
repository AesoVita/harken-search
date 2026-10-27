// main_stubs_macos.cpp — No-op stubs for symbols defined in main.cpp / WindowsMain.cpp
// that are referenced by shared modules. macOS does not compile main.cpp
// (Windows-only 13K+ LOC), so these stubs satisfy the linker.

#ifdef __APPLE__

#include "../AppState.h"
#include "../DAWPanel.h"

// ---------------------------------------------------------------------------
// Global colour variables (Theme.cpp / various UI modules read these)
// ---------------------------------------------------------------------------
ImVec4 COLOR_BG_DARK      = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
ImVec4 COLOR_BG_PANEL     = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
ImVec4 COLOR_ACCENT       = ImVec4(0.6f, 0.8f, 0.2f, 1.0f);
ImVec4 COLOR_ACCENT_DIM   = ImVec4(0.4f, 0.5f, 0.15f, 1.0f);
ImVec4 COLOR_TEXT          = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
ImVec4 COLOR_TEXT_DIM      = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
ImVec4 COLOR_SELECTED      = ImVec4(0.8f, 0.6f, 0.1f, 1.0f);
ImVec4 COLOR_WAVEFORM_BG   = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
ImVec4 COLOR_WAVEFORM      = ImVec4(0.3f, 0.6f, 0.2f, 1.0f);

ImU32 g_waveformColor    = IM_COL32(59, 130, 246, 255);
ImU32 g_waveformBgColor  = IM_COL32(8, 9, 11, 255);
ImU32 g_waveformRmsColor = IM_COL32(30, 80, 180, 200);
ImU32 g_miniWaveColor    = IM_COL32(59, 130, 246, 255);
ImU32 g_miniWaveBgColor  = IM_COL32(0, 0, 0, 0);

// ---------------------------------------------------------------------------
// Functions from main.cpp used by shared modules
// ---------------------------------------------------------------------------
void RefreshSearchResults(AppState& /*state*/, bool /*resetPagination*/) {
    // Search refresh remains unsupported in the macOS stub.
}

// DAW capability helpers (DAWPanel.cpp calls these; defined in main.cpp)
DawCapability GetDawCapability(const DawTarget& /*dt*/) {
    return DawCapability::Unconfigured;
}
const char* GetCapabilityTag(const DawTarget& /*dt*/) {
    return "N/A";
}

#endif // __APPLE__
