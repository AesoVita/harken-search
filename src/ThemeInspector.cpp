// ThemeInspector.cpp — per-frame zone buffer, smallest-rect hit test, inspect overlay.

#include "ThemeInspector.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace ThemeInspector {

namespace {

struct Zone {
    ImVec2        min{};
    ImVec2        max{};
    std::string   key;
    std::string   displayNameZh;
    std::string   sourceAnchor;
    float         area          = 0.f;
};

static std::vector<Zone> g_zones;
static bool              g_active = false;
static bool              g_bridgeCollectionEnabled = false;
static AnnotationFrameSnapshot g_latestSnapshot;
static std::uint64_t     g_frameNumber = 0;
static ImVec2            g_displaySize{};

static bool              g_hasHovered = false;
static const char*       g_hoveredKey = nullptr;
static const char*       g_hoveredDisplay = nullptr;
static ImVec2            g_hoveredMin{};
static ImVec2            g_hoveredMax{};

static std::string       g_scrollPending;
static std::string       g_scrollReturnBuffer; // stable pointer for last ConsumeScrollRequest

static void NormalizeRect(ImVec2& outMin, ImVec2& outMax, ImVec2 a, ImVec2 b) {
    outMin.x = (std::min)(a.x, b.x);
    outMin.y = (std::min)(a.y, b.y);
    outMax.x = (std::max)(a.x, b.x);
    outMax.y = (std::max)(a.y, b.y);
}

static float RectArea(const ImVec2& mn, const ImVec2& mx) {
    const float w = mx.x - mn.x;
    const float h = mx.y - mn.y;
    if (w <= 0.f || h <= 0.f)
        return 0.f;
    return w * h;
}

// ImGui-style: min inclusive, max exclusive (matches ImRect::Contains for points).
static bool PointInRect(const ImVec2& p, const ImVec2& mn, const ImVec2& mx) {
    return p.x >= mn.x && p.y >= mn.y && p.x < mx.x && p.y < mx.y;
}

static std::string SanitizeStableIdPart(const std::string& value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_')
            sanitized.push_back(static_cast<char>(c));
        else
            sanitized.push_back('_');
    }
    return sanitized.empty() ? std::string("zone") : sanitized;
}

static void FinalizeAnnotationSnapshot() {
    if (!IsCollecting())
        return;

    AnnotationFrameSnapshot snapshot;
    snapshot.frameNumber = g_frameNumber;
    snapshot.displayWidth = g_displaySize.x;
    snapshot.displayHeight = g_displaySize.y;
    snapshot.zones.reserve(g_zones.size());

    std::unordered_map<std::string, std::uint32_t> occurrences;
    for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(g_zones.size()); ++index) {
        const Zone& zone = g_zones[index];
        AnnotationZoneSnapshot output;
        output.key = zone.key;
        output.displayName = zone.displayNameZh;
        output.sourceAnchor = zone.sourceAnchor;
        output.minX = zone.min.x;
        output.minY = zone.min.y;
        output.maxX = zone.max.x;
        output.maxY = zone.max.y;
        output.area = zone.area;
        output.order = index;
        const std::uint32_t occurrence = occurrences[zone.key]++;
        output.stableId = SanitizeStableIdPart(zone.key) + "--" + std::to_string(occurrence);
        snapshot.zones.push_back(std::move(output));
    }
    g_latestSnapshot = std::move(snapshot);
}

} // namespace

void BeginFrame() {
    g_zones.clear();
    if (ImGui::GetCurrentContext()) {
        g_displaySize = ImGui::GetIO().DisplaySize;
        g_frameNumber = static_cast<std::uint64_t>(ImGui::GetFrameCount());
    } else {
        g_displaySize = ImVec2(0.0f, 0.0f);
        g_frameNumber = 0;
    }
    // Invalidate hover until EndFrame() runs this frame (avoid stale reads).
    g_hasHovered     = false;
    g_hoveredKey     = nullptr;
    g_hoveredDisplay = nullptr;
    g_hoveredMin     = ImVec2(0.f, 0.f);
    g_hoveredMax     = ImVec2(0.f, 0.f);
}

void RegisterZone(const ImVec2& min,
                  const ImVec2& max,
                  const char* tokenKey,
                  const char* displayNameZh,
                  const char* sourceAnchor) {
    if (!IsCollecting())
        return;
    if (!tokenKey || !tokenKey[0])
        return;

    Zone z;
    NormalizeRect(z.min, z.max, min, max);
    z.key           = tokenKey;
    z.displayNameZh = displayNameZh ? displayNameZh : "";
    z.sourceAnchor  = sourceAnchor ? sourceAnchor : "";
    z.area          = RectArea(z.min, z.max);
    if (z.area <= 0.f)
        return;

    g_zones.push_back(z);
}

void RegisterLastItem(const char* key,
                      const char* displayName,
                      const char* sourceAnchor) {
    if (!IsCollecting() || !ImGui::GetCurrentContext())
        return;
    RegisterZone(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        key,
        displayName,
        sourceAnchor);
}

void SetActive(bool active) {
    g_active = active;
}

bool IsActive() {
    return g_active;
}

void SetBridgeCollectionEnabled(bool enabled) {
    g_bridgeCollectionEnabled = enabled;
}

bool IsCollecting() {
    return g_active || g_bridgeCollectionEnabled;
}

AnnotationFrameSnapshot CopyLatestSnapshot() {
    return g_latestSnapshot;
}

void EndFrame() {
    g_hasHovered     = false;
    g_hoveredKey     = nullptr;
    g_hoveredDisplay = nullptr;
    g_hoveredMin     = ImVec2(0.f, 0.f);
    g_hoveredMax     = ImVec2(0.f, 0.f);

    FinalizeAnnotationSnapshot();

    if (!g_active || g_zones.empty()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse(io.MousePos.x, io.MousePos.y);

    float    bestArea = FLT_MAX;
    const Zone* best  = nullptr;

    for (const Zone& z : g_zones) {
        if (!PointInRect(mouse, z.min, z.max))
            continue;
        if (z.area < bestArea) {
            bestArea = z.area;
            best     = &z;
        }
    }

    if (best) {
        g_hasHovered     = true;
        g_hoveredKey     = best->key.c_str();
        g_hoveredDisplay = best->displayNameZh.empty() ? nullptr : best->displayNameZh.c_str();
        g_hoveredMin     = best->min;
        g_hoveredMax     = best->max;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            g_scrollPending = best->key;
            io.MouseClicked[ImGuiMouseButton_Left] = false;
            io.MouseClickedCount[ImGuiMouseButton_Left] = 0;
        }
    }
}

const char* GetHoveredTokenKey() {
    return g_hasHovered ? g_hoveredKey : nullptr;
}

const char* GetHoveredDisplayName() {
    return g_hasHovered ? g_hoveredDisplay : nullptr;
}

ImVec2 GetHoveredZoneMin() {
    return g_hoveredMin;
}

ImVec2 GetHoveredZoneMax() {
    return g_hoveredMax;
}

bool HasHoveredZone() {
    return g_hasHovered;
}

const char* ConsumeScrollRequest() {
    if (g_scrollPending.empty())
        return nullptr;
    g_scrollReturnBuffer = g_scrollPending;
    g_scrollPending.clear();
    return g_scrollReturnBuffer.c_str();
}

void RenderOverlay() {
    if (!g_active || !g_hasHovered)
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl)
        return;

    const ImU32 colBorder = IM_COL32(36, 158, 152, 255);
    const ImU32 colFill   = IM_COL32(36, 158, 152, 24);
    const ImU32 colBg     = IM_COL32(20, 40, 45, 230);
    const ImU32 colText   = IM_COL32(255, 255, 255, 255);
    const ImU32 colDim    = IM_COL32(180, 210, 210, 200);

    dl->AddRectFilled(g_hoveredMin, g_hoveredMax, colFill);
    dl->AddRect(g_hoveredMin, g_hoveredMax, colBorder, 0.f, ImDrawFlags_None, 2.f);

    const char* keyLabel = g_hoveredKey ? g_hoveredKey : "";
    const char* zhLabel  = g_hoveredDisplay;

    char combinedLabel[256];
    if (zhLabel && zhLabel[0]) {
        std::snprintf(combinedLabel, sizeof(combinedLabel), "%s  (%s)", zhLabel, keyLabel);
    } else {
        std::snprintf(combinedLabel, sizeof(combinedLabel), "%s", keyLabel);
    }

    const ImVec2 textSize = ImGui::CalcTextSize(combinedLabel);
    const float pad = 5.f;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 labelPos(g_hoveredMin.x, g_hoveredMin.y - textSize.y - pad * 2.f);
    if (labelPos.y < 0.f)
        labelPos = ImVec2(g_hoveredMin.x + pad, g_hoveredMin.y + pad);
    if (labelPos.x + textSize.x + pad * 2.f > displaySize.x)
        labelPos.x = displaySize.x - textSize.x - pad * 2.f;

    const ImVec2 bgMin(labelPos.x - pad, labelPos.y - pad);
    const ImVec2 bgMax(labelPos.x + textSize.x + pad, labelPos.y + textSize.y + pad);
    dl->AddRectFilled(bgMin, bgMax, colBg, 4.f);
    dl->AddRect(bgMin, bgMax, colBorder, 4.f, ImDrawFlags_None, 1.f);

    if (zhLabel && zhLabel[0]) {
        const ImVec2 zhSize = ImGui::CalcTextSize(zhLabel);
        dl->AddText(labelPos, colText, zhLabel);
        ImVec2 keyPos(labelPos.x + zhSize.x, labelPos.y);
        char keyPart[128];
        std::snprintf(keyPart, sizeof(keyPart), "  (%s)", keyLabel);
        dl->AddText(keyPos, colDim, keyPart);
    } else {
        dl->AddText(labelPos, colText, keyLabel);
    }
}

} // namespace ThemeInspector
