#pragma once

#include "HarkenSearchMetrics.h"
#include "imgui.h"

#include <array>
#include <cstddef>
#include <unordered_map>

namespace aura::ui::workspace {

// Exactly three shallow surface levels are allowed in the core workspace.
enum class SurfaceLevel {
    Application = 0,
    Docked = 1,
    Elevated = 2,
};

inline constexpr std::size_t kSurfaceLevelCount = 3;

// The functional accent is reserved for these product states. None resolves
// to the quiet perimeter color rather than consuming the accent decoratively.
enum class AccentRole {
    None,
    Selected,
    Focused,
    Playback,
    Active,
};

enum class SplitterInteraction {
    Idle,
    Hovered,
    Active,
};

struct CoreWorkspaceMetrics {
    float space4 = metrics::kGap4;
    float space8 = metrics::kGap8;
    float space12 = metrics::kGap12;
    float space16 = metrics::kGap16;
    float perimeterWidth = 1.0f;
    float splitterWidth = 1.0f;
    float compactControlHeight = metrics::kHeightMd;
    float compactControlRadius = metrics::kRadiusDefault;
    float headerHeight = metrics::kHeightMd;
    float footerHeight = metrics::kHeightSm + metrics::kGap4;
    float contentInset = metrics::kGap8;
};

inline constexpr CoreWorkspaceMetrics kCoreWorkspaceMetrics{};

// The composition layer maps the current Aris theme into this value. The
// workspace foundation deliberately owns no global theme state.
struct CoreWorkspacePaletteInput {
    ImVec4 applicationSurface{};
    ImVec4 dockedSurface{};
    ImVec4 elevatedSurface{};
    ImVec4 perimeter{};
    ImVec4 splitter{};
    ImVec4 primaryText{};
    ImVec4 mutedText{};
    ImVec4 functionalAccent{};
    ImVec4 error{};
};

// Theme composition supplies the semantic anchors for the workspace shell.
// This resolver keeps the anchors' hue while guaranteeing a shallow but
// readable surface order for both dark and light chrome themes.
struct WorkspaceSurfaceBasis {
    ImVec4 application{};
    ImVec4 frame{};
    ImVec4 toolbar{};
    ImVec4 elevated{};
    bool lightChrome = false;
};

struct WorkspaceSurfaceLevels {
    ImVec4 application{};
    ImVec4 docked{};
    ImVec4 elevated{};
};

WorkspaceSurfaceLevels ResolveWorkspaceSurfaceLevels(
    const WorkspaceSurfaceBasis& basis) noexcept;

inline constexpr std::size_t kToolbarFlowItemCount = 6;

struct ToolbarFlowSlot {
    float minX = 0.0f;
    float maxX = 0.0f;
    bool visible = false;
};

// Horizontal toolbar geometry is resolved before any item is submitted to
// ImGui. This keeps navigation actions and the preset dial in one flow instead
// of allowing independently positioned draw calls to overlap.
struct ToolbarFlowLayoutInput {
    float toolbarMinX = 0.0f;
    float toolbarMaxX = 0.0f;
    float navMinX = 0.0f;
    float navPreferredX = 0.0f;
    float rightInset = 0.0f;
    float presetDialWidth = 0.0f;
    float presetDialGap = 0.0f;
    float navLeadingInset = 0.0f;
    float navTrailingInset = 0.0f;
    std::array<float, kToolbarFlowItemCount> expandedItemWidths{};
    std::array<float, kToolbarFlowItemCount> compactItemWidths{};
    std::array<float, kToolbarFlowItemCount - 1> itemGaps{};
    std::size_t minimumVisibleItems = 1;
};

struct ToolbarFlowLayout {
    ToolbarFlowSlot toolbarClip{};
    ToolbarFlowSlot navCapsule{};
    ToolbarFlowSlot presetDial{};
    std::array<ToolbarFlowSlot, kToolbarFlowItemCount> itemSlots{};
    std::size_t visibleItemCount = 0;
    bool compact = false;
    bool valid = false;
};

ToolbarFlowLayout ResolveToolbarFlowLayout(
    const ToolbarFlowLayoutInput& input) noexcept;

inline constexpr float kTransientWaveformScrollbarZoomThreshold = 1.01f;
inline constexpr double kTransientScrollbarIdleSeconds = 3.0;

struct TransientScrollbarVisibilityInput {
    bool scrollable = false;
    double secondsSinceInteraction = -1.0;
    bool hovered = false;
    bool dragging = false;
};

bool ShouldShowTransientScrollbar(
    const TransientScrollbarVisibilityInput& input) noexcept;

struct TransientScrollbarFrameObservation {
    double nowSeconds = 0.0;
    bool anyScrollable = false;
    bool becameScrollable = false;
    bool wheelInteraction = false;
    bool scrollPositionChanged = false;
    bool hovered = false;
    bool dragging = false;
};

class TransientScrollbarController {
public:
    void beginFrame(
        ImGuiID regionId,
        const TransientScrollbarFrameObservation& observation) noexcept;
    void beginImGuiFrame(double nowSeconds) noexcept;
    void bindCurrentWindowToRegion(ImGuiID regionId) noexcept;
    void reportInteraction(ImGuiID regionId, double nowSeconds) noexcept;
    bool shouldShow(
        ImGuiID regionId,
        double nowSeconds,
        bool scrollable,
        bool hovered = false,
        bool dragging = false) const noexcept;

private:
    struct NativeWindowState {
        bool scrollbarX = false;
        bool scrollbarY = false;
        ImVec2 scroll{};
        ImGuiID regionId = 0;
    };

    std::unordered_map<ImGuiID, double> m_lastInteractionAtByRegion;
    std::unordered_map<ImGuiID, NativeWindowState> m_previousNativeWindows;
    std::unordered_map<ImGuiID, ImGuiID> m_nativeWindowRegions;
    bool m_runtimeObservationUnavailable = false;
};

// Supplies the caller's semantic scrollbar RGB. A short-lived region scope
// gates alpha only while ImGui renders that region; the track stays transparent.
int PushTransientScrollbarColors(
    const ImVec4& grab,
    const ImVec4& hovered,
    const ImVec4& active);

class TransientScrollbarStyleScope {
public:
    explicit TransientScrollbarStyleScope(bool visible);
    ~TransientScrollbarStyleScope();

    TransientScrollbarStyleScope(const TransientScrollbarStyleScope&) = delete;
    TransientScrollbarStyleScope& operator=(const TransientScrollbarStyleScope&) = delete;

    void reset();

private:
    int m_colorCount = 0;
};

struct CoreWorkspacePalette {
    ImVec4 applicationSurface{};
    ImVec4 dockedSurface{};
    ImVec4 elevatedSurface{};
    ImVec4 perimeter{};
    ImVec4 splitter{};
    ImVec4 primaryText{};
    ImVec4 mutedText{};
    ImVec4 functionalAccent{};
    ImVec4 error{};
    ImVec4 shadow{};
};

CoreWorkspacePalette BuildCoreWorkspacePalette(const CoreWorkspacePaletteInput& input) noexcept;

const ImVec4& ResolveSurfaceColor(
    const CoreWorkspacePalette& palette,
    SurfaceLevel level) noexcept;

const ImVec4& ResolveAccentColor(
    const CoreWorkspacePalette& palette,
    AccentRole role) noexcept;

ImVec4 ResolveSplitterColor(
    const CoreWorkspacePalette& palette,
    SplitterInteraction interaction) noexcept;

void DrawWorkspaceSplitter(
    ImDrawList* drawList,
    const ImVec2& start,
    const ImVec2& end,
    const CoreWorkspacePalette& palette,
    SplitterInteraction interaction = SplitterInteraction::Idle);

// Draws the shared three-dot resize affordance along the supplied axis. The
// axis is normally {0, 1} for vertical seams or {1, 0} for horizontal seams;
// diagonal axes are allowed for floating-shell corner affordances.
void DrawThreeDotSplitterGrip(
    ImDrawList* drawList,
    const ImVec2& center,
    const ImVec2& axis,
    float gap,
    float radius,
    ImU32 color) noexcept;

// Scoped ImGui styling for one workspace surface. It only pops values that it
// successfully pushed and is safe to reset more than once.
class CoreWorkspaceStyleScope {
public:
    CoreWorkspaceStyleScope(
        const CoreWorkspacePalette& palette,
        SurfaceLevel surface,
        AccentRole accentRole = AccentRole::None);
    ~CoreWorkspaceStyleScope();

    CoreWorkspaceStyleScope(const CoreWorkspaceStyleScope&) = delete;
    CoreWorkspaceStyleScope& operator=(const CoreWorkspaceStyleScope&) = delete;

    void reset();
    bool active() const noexcept { return m_colorCount > 0 || m_varCount > 0; }

private:
    int m_colorCount = 0;
    int m_varCount = 0;
};

} // namespace aura::ui::workspace
