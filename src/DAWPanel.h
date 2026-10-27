#pragma once

// DAWPanel — "Send to DAW" capsule widget + custom dropdown

#include "imgui.h"
#include <string>

struct AppState;
struct DawTarget;

// @stage7-contract
// API contract: DAWPanel consumes capability/state snapshots on the UI thread
// and does not own bridge processes or clipboard data. It preserves the
// capability classification domain for callers and adapters.

// Capability classification for a DAW target (based on runtime state)
enum class DawCapability {
    CanDirect,      // Running + direct connection (HttpApi/NamedPipe)
    Experimental,   // Real but still limited / validation-only host route
    CanLaunch,      // Not running + exePath configured
    PasteOnly,      // Running but no direct link (clipboard + paste in DAW)
    ClipboardOnly,  // Explicit clipboard mode
    AdapterMissing, // Dedicated adapter/helper is required
    HostNotReady,   // Host session is live but not ready
    BridgeOffline,  // Adapter/helper exists but endpoint is offline
    Unavailable,    // Explicitly blocked / not implemented
    Unconfigured,   // Missing exePath or processName
};

DawCapability GetDawCapability(const DawTarget& dt);
const char* GetCapabilityTag(const DawTarget& dt);

// Per-DAW vector icon (optical-normalized to ~14px)
void DrawDAWIcon(ImDrawList* dl, ImVec2 center, const std::string& dawName, ImU32 color);

// Draw the capsule + dropdown. Returns true when Send button clicked.
bool DrawSendToDAWWidget(AppState& state);
