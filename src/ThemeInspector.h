// ThemeInspector.h — lightweight per-frame zone registration and hit-testing for theme editor inspect mode.
#pragma once

#include "imgui.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ThemeInspector {

// @stage7-contract
// API contract: inspector snapshots own annotation strings and frame geometry;
// registration/hit-testing is UI-thread frame state and must not retain ImGui
// pointers beyond the frame. Invalid or absent zones return empty snapshots.

struct AnnotationZoneSnapshot {
    std::string stableId;
    std::string key;
    std::string displayName;
    std::string sourceAnchor;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float area = 0.0f;
    std::uint32_t order = 0;
};

struct AnnotationFrameSnapshot {
    std::uint64_t frameNumber = 0;
    float displayWidth = 0.0f;
    float displayHeight = 0.0f;
    std::vector<AnnotationZoneSnapshot> zones;
};

// Call once at frame start to clear previous frame's zones.
void BeginFrame();

// Register a UI zone with its primary theme token key (typically a string literal from ThemeEditorSchema).
// Call after drawing a region (use GetItemRectMin/Max or explicit rect).
// displayNameZh is optional UI label for overlay; may be nullptr.
void RegisterZone(const ImVec2& min,
                  const ImVec2& max,
                  const char* tokenKey,
                  const char* displayNameZh = nullptr,
                  const char* sourceAnchor = nullptr);
void RegisterLastItem(const char* key,
                      const char* displayName = nullptr,
                      const char* sourceAnchor = nullptr);

void SetActive(bool active);
bool IsActive();
void SetBridgeCollectionEnabled(bool enabled);
bool IsCollecting();
AnnotationFrameSnapshot CopyLatestSnapshot();

// Call once per frame after all zones are registered — performs hit testing and click → scroll request.
void EndFrame();

// Query results (valid after EndFrame() for the current frame).
const char* GetHoveredTokenKey();     // nullptr if no zone hovered
const char* GetHoveredDisplayName();  // nullptr if no zone hovered or no display name
ImVec2        GetHoveredZoneMin();
ImVec2        GetHoveredZoneMax();
bool          HasHoveredZone();

// Set when user clicks a hovered zone while inspect mode is active. Theme editor reads and clears.
const char* ConsumeScrollRequest();

// Highlighted border + token label — call at end of frame (foreground draw list).
void RenderOverlay();

} // namespace ThemeInspector
