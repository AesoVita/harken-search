#include "ui/CoreWorkspaceStyle.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>

namespace aura::ui::workspace {

namespace {

float SurfaceLuma(const ImVec4& color) noexcept {
    return color.x * 0.299f + color.y * 0.587f + color.z * 0.114f;
}

ImVec4 MixSurface(const ImVec4& a, const ImVec4& b, float t) noexcept {
    const float u = 1.0f - t;
    return ImVec4(
        a.x * u + b.x * t,
        a.y * u + b.y * t,
        a.z * u + b.z * t,
        a.w * u + b.w * t);
}

ImVec4 NudgeSurfaceLuma(ImVec4 color, float target) noexcept {
    target = (std::max)(0.0f, (std::min)(1.0f, target));
    const float current = SurfaceLuma(color);
    if (std::fabs(target - current) < 0.0001f) return color;

    if (target > current) {
        const float room = 1.0f - current;
        const float amount = room > 0.0001f ? (target - current) / room : 0.0f;
        color = MixSurface(color, ImVec4(1.0f, 1.0f, 1.0f, color.w), amount);
    } else {
        const float room = current;
        const float amount = room > 0.0001f ? (current - target) / room : 0.0f;
        color = MixSurface(color, ImVec4(0.0f, 0.0f, 0.0f, color.w), amount);
    }
    color.w = 1.0f;
    return color;
}

ImVec4 EnsureSurfaceLumaGap(
    ImVec4 candidate,
    float baseLuma,
    float minimumGap,
    bool shouldBeAbove,
    float maximumTarget = 1.0f) noexcept {
    const float candidateLuma = SurfaceLuma(candidate);
    const float requestedTarget = shouldBeAbove
        ? baseLuma + minimumGap
        : baseLuma - minimumGap;
    const float target = shouldBeAbove
        ? (std::min)(requestedTarget, maximumTarget)
        : requestedTarget;
    const bool satisfies = shouldBeAbove
        ? candidateLuma >= requestedTarget && candidateLuma <= maximumTarget
        : candidateLuma <= target;
    return satisfies ? candidate : NudgeSurfaceLuma(candidate, target);
}

} // namespace

WorkspaceSurfaceLevels ResolveWorkspaceSurfaceLevels(
    const WorkspaceSurfaceBasis& basis) noexcept {
    constexpr float kDarkDockedGap = 0.034f;
    constexpr float kDarkElevatedFromApplicationGap = 0.062f;
    constexpr float kDarkElevatedFromDockedGap = 0.028f;
    constexpr float kLightDockedRecess = 0.040f;
    constexpr float kLightElevatedFromApplicationGap = 0.025f;
    constexpr float kLightElevatedFromDockedGap = 0.060f;
    constexpr float kLightElevatedHeadroom = 0.995f;
    WorkspaceSurfaceLevels levels;
    levels.application = basis.application;
    const float applicationLuma = SurfaceLuma(basis.application);

    if (basis.lightChrome) {
        // Light themes use the toolbar shelf as the recessed anchor and the
        // frame/dialog pair for the raised surface. This preserves the warm or
        // cool hue instead of introducing a generic gray. Near-white themes
        // have very little upward gamut available, so keep a small headroom
        // below pure white and lower the docked shelf only when needed to
        // preserve the visible elevated step.
        levels.docked = MixSurface(basis.toolbar, basis.application, 0.24f);
        levels.elevated = MixSurface(basis.frame, basis.elevated, 0.50f);
        levels.docked = EnsureSurfaceLumaGap(
            levels.docked, applicationLuma, kLightDockedRecess, false);
        levels.elevated = EnsureSurfaceLumaGap(
            levels.elevated, applicationLuma, kLightElevatedFromApplicationGap, true,
            kLightElevatedHeadroom);
        // If the application is already near white, the requested elevated
        // gap cannot be achieved without clipping. Recess the docked shelf
        // just enough to keep the elevated/docked relationship readable.
        levels.docked = EnsureSurfaceLumaGap(
            levels.docked, kLightElevatedHeadroom, kLightElevatedFromDockedGap, false);
        levels.elevated = EnsureSurfaceLumaGap(
            levels.elevated, SurfaceLuma(levels.docked), kLightElevatedFromDockedGap, true,
            kLightElevatedHeadroom);
    } else {
        // Dark themes lift the frame from the application canvas, then use a
        // restrained frame/dialog blend for floating or elevated surfaces.
        levels.docked = MixSurface(basis.application, basis.frame, 0.58f);
        levels.elevated = MixSurface(basis.frame, basis.elevated, 0.35f);
        levels.docked = EnsureSurfaceLumaGap(
            levels.docked, applicationLuma, kDarkDockedGap, true);
        levels.elevated = EnsureSurfaceLumaGap(
            levels.elevated, applicationLuma, kDarkElevatedFromApplicationGap, true);
        levels.elevated = EnsureSurfaceLumaGap(
            levels.elevated, SurfaceLuma(levels.docked), kDarkElevatedFromDockedGap, true);
    }

    return levels;
}

ToolbarFlowLayout ResolveToolbarFlowLayout(
    const ToolbarFlowLayoutInput& input) noexcept {
    ToolbarFlowLayout layout;
    layout.toolbarClip = {
        input.toolbarMinX,
        input.toolbarMaxX,
        input.toolbarMaxX >= input.toolbarMinX,
    };
    if (!layout.toolbarClip.visible) return layout;

    const float contentRight = (std::max)(
        input.toolbarMinX,
        input.toolbarMaxX - (std::max)(0.0f, input.rightInset));
    const float dialWidth = (std::max)(0.0f, input.presetDialWidth);
    if (dialWidth > 0.0f && contentRight - dialWidth >= input.toolbarMinX) {
        layout.presetDial = {contentRight - dialWidth, contentRight, true};
    }

    const float navRight = layout.presetDial.visible
        ? layout.presetDial.minX - (std::max)(0.0f, input.presetDialGap)
        : contentRight;
    const float navMin = (std::max)(input.toolbarMinX, input.navMinX);
    const float availableWidth = navRight - navMin;
    if (availableWidth < 0.0f) return layout;

    const std::size_t minimumVisible = (std::max)(
        std::size_t{1},
        (std::min)(input.minimumVisibleItems, kToolbarFlowItemCount));
    auto measure = [&](const auto& widths, std::size_t visibleCount) {
        float width = (std::max)(0.0f, input.navLeadingInset) +
                      (std::max)(0.0f, input.navTrailingInset);
        for (std::size_t i = 0; i < visibleCount; ++i) {
            width += (std::max)(0.0f, widths[i]);
            if (i + 1 < visibleCount)
                width += (std::max)(0.0f, input.itemGaps[i]);
        }
        return width;
    };

    const auto* selectedWidths = &input.expandedItemWidths;
    std::size_t visibleCount = kToolbarFlowItemCount;
    float navWidth = measure(*selectedWidths, visibleCount);
    if (navWidth > availableWidth) {
        layout.compact = true;
        selectedWidths = &input.compactItemWidths;
        navWidth = measure(*selectedWidths, visibleCount);
        while (visibleCount > minimumVisible && navWidth > availableWidth) {
            --visibleCount;
            navWidth = measure(*selectedWidths, visibleCount);
        }
    }
    if (navWidth > availableWidth) return layout;

    const float maxOrigin = navRight - navWidth;
    const float preferredOrigin = (std::max)(navMin, input.navPreferredX);
    const float navOrigin = (std::min)(preferredOrigin, maxOrigin);
    layout.navCapsule = {navOrigin, navOrigin + navWidth, true};
    layout.visibleItemCount = visibleCount;

    float itemX = navOrigin + (std::max)(0.0f, input.navLeadingInset);
    for (std::size_t i = 0; i < visibleCount; ++i) {
        const float itemWidth = (std::max)(0.0f, (*selectedWidths)[i]);
        layout.itemSlots[i] = {itemX, itemX + itemWidth, true};
        itemX += itemWidth;
        if (i + 1 < visibleCount)
            itemX += (std::max)(0.0f, input.itemGaps[i]);
    }

    layout.valid = layout.navCapsule.minX >= layout.toolbarClip.minX &&
                   layout.navCapsule.maxX <= navRight &&
                   (!layout.presetDial.visible ||
                    (layout.presetDial.minX >= layout.toolbarClip.minX &&
                     layout.presetDial.maxX <= layout.toolbarClip.maxX));
    return layout;
}

bool ShouldShowTransientScrollbar(
    const TransientScrollbarVisibilityInput& input) noexcept {
    if (!input.scrollable) return false;
    if (input.hovered || input.dragging) {
        return true;
    }
    return input.secondsSinceInteraction >= 0.0 &&
           input.secondsSinceInteraction <= kTransientScrollbarIdleSeconds;
}

void TransientScrollbarController::beginFrame(
    ImGuiID regionId,
    const TransientScrollbarFrameObservation& observation) noexcept {
    if (regionId == 0 || !observation.anyScrollable) return;
    if (observation.becameScrollable ||
        observation.wheelInteraction ||
        observation.scrollPositionChanged ||
        observation.hovered ||
        observation.dragging) {
        reportInteraction(regionId, observation.nowSeconds);
    }
}

void TransientScrollbarController::beginImGuiFrame(
    double nowSeconds) noexcept {
    if (!GImGui) {
        // Conservative fallback: a missing runtime observer must never make
        // scroll navigation permanently invisible.
        m_runtimeObservationUnavailable = true;
        m_previousNativeWindows.clear();
        return;
    }
    m_runtimeObservationUnavailable = false;

    ImGuiContext& context = *GImGui;
    const ImGuiIO& io = context.IO;
    std::unordered_map<ImGuiID, TransientScrollbarFrameObservation> observations;
    std::unordered_map<ImGuiID, NativeWindowState> currentWindows;
    currentWindows.reserve(static_cast<std::size_t>(context.Windows.Size));

    const auto resolveRegionId = [&](ImGuiWindow* window) {
        const auto binding = m_nativeWindowRegions.find(window->ID);
        if (binding != m_nativeWindowRegions.end()) return binding->second;
        return window->ChildId != 0 ? window->ChildId : window->ID;
    };

    for (ImGuiWindow* window : context.Windows) {
        if (!window || !window->WasActive) continue;

        NativeWindowState current;
        current.scrollbarX = window->ScrollbarX && window->ScrollMax.x > 0.0f;
        current.scrollbarY = window->ScrollbarY && window->ScrollMax.y > 0.0f;
        current.scroll = window->Scroll;
        current.regionId = resolveRegionId(window);
        auto& observation = observations[current.regionId];
        observation.nowSeconds = nowSeconds;
        observation.anyScrollable |= current.scrollbarX || current.scrollbarY;

        const auto previous = m_previousNativeWindows.find(window->ID);
        if (previous == m_previousNativeWindows.end() ||
            previous->second.regionId != current.regionId) {
            observation.becameScrollable |= current.scrollbarX || current.scrollbarY;
        } else {
            observation.becameScrollable |=
                (current.scrollbarX && !previous->second.scrollbarX) ||
                (current.scrollbarY && !previous->second.scrollbarY);
            const bool xMoved = current.scrollbarX && previous->second.scrollbarX &&
                std::fabs(current.scroll.x - previous->second.scroll.x) > 0.01f;
            const bool yMoved = current.scrollbarY && previous->second.scrollbarY &&
                std::fabs(current.scroll.y - previous->second.scroll.y) > 0.01f;
            observation.scrollPositionChanged |= xMoved || yMoved;
        }

        const auto observeAxis = [&](ImGuiAxis axis, bool scrollable) {
            if (!scrollable) return;
            const ImRect track = ImGui::GetWindowScrollbarRect(window, axis);
            observation.hovered |= track.Contains(io.MousePos);
            observation.dragging |=
                context.ActiveId == ImGui::GetWindowScrollbarID(window, axis);
        };
        observeAxis(ImGuiAxis_X, current.scrollbarX);
        observeAxis(ImGuiAxis_Y, current.scrollbarY);
        currentWindows.emplace(window->ID, current);
    }

    bool hoveredWindowScrollable = false;
    ImGuiID hoveredRegionId = 0;
    for (ImGuiWindow* window = context.HoveredWindow;
         window && !hoveredWindowScrollable;
         window = window->ParentWindow) {
        const auto current = currentWindows.find(window->ID);
        hoveredWindowScrollable = current != currentWindows.end() &&
            (current->second.scrollbarX || current->second.scrollbarY);
        if (hoveredWindowScrollable) hoveredRegionId = current->second.regionId;
    }
    if (hoveredRegionId != 0 &&
        (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)) {
        observations[hoveredRegionId].nowSeconds = nowSeconds;
        observations[hoveredRegionId].anyScrollable = true;
        observations[hoveredRegionId].wheelInteraction = true;
    }

    m_previousNativeWindows.swap(currentWindows);
    for (const auto& [regionId, observation] : observations) {
        beginFrame(regionId, observation);
    }
}

void TransientScrollbarController::bindCurrentWindowToRegion(
    ImGuiID regionId) noexcept {
    if (!GImGui || regionId == 0 || !GImGui->CurrentWindow) return;
    m_nativeWindowRegions[GImGui->CurrentWindow->ID] = regionId;
}

void TransientScrollbarController::reportInteraction(
    ImGuiID regionId,
    double nowSeconds) noexcept {
    if (regionId == 0) return;
    m_lastInteractionAtByRegion[regionId] = nowSeconds;
}

bool TransientScrollbarController::shouldShow(
    ImGuiID regionId,
    double nowSeconds,
    bool scrollable,
    bool hovered,
    bool dragging) const noexcept {
    if (m_runtimeObservationUnavailable && scrollable) return true;
    TransientScrollbarVisibilityInput input;
    input.scrollable = scrollable;
    const auto interaction = m_lastInteractionAtByRegion.find(regionId);
    input.secondsSinceInteraction = interaction != m_lastInteractionAtByRegion.end()
        ? nowSeconds - interaction->second
        : -1.0;
    input.hovered = hovered;
    input.dragging = dragging;
    return ShouldShowTransientScrollbar(input);
}

int PushTransientScrollbarColors(
    const ImVec4& grab,
    const ImVec4& hovered,
    const ImVec4& active) {
    if (!ImGui::GetCurrentContext()) return 0;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, grab);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, active);
    return 4;
}

TransientScrollbarStyleScope::TransientScrollbarStyleScope(bool visible) {
    if (!ImGui::GetCurrentContext()) return;
    const ImVec4* colors = ImGui::GetStyle().Colors;
    ImVec4 grab = colors[ImGuiCol_ScrollbarGrab];
    ImVec4 hovered = colors[ImGuiCol_ScrollbarGrabHovered];
    ImVec4 active = colors[ImGuiCol_ScrollbarGrabActive];
    if (!visible) {
        grab.w = 0.0f;
        hovered.w = 0.0f;
        active.w = 0.0f;
    }
    m_colorCount = PushTransientScrollbarColors(grab, hovered, active);
}

void TransientScrollbarStyleScope::reset() {
    if (m_colorCount > 0 && ImGui::GetCurrentContext()) {
        ImGui::PopStyleColor(m_colorCount);
    }
    m_colorCount = 0;
}

TransientScrollbarStyleScope::~TransientScrollbarStyleScope() {
    reset();
}

CoreWorkspacePalette BuildCoreWorkspacePalette(
    const CoreWorkspacePaletteInput& input) noexcept {
    CoreWorkspacePalette palette;
    palette.applicationSurface = input.applicationSurface;
    palette.dockedSurface = input.dockedSurface;
    palette.elevatedSurface = input.elevatedSurface;
    palette.perimeter = input.perimeter;
    palette.splitter = input.splitter;
    palette.primaryText = input.primaryText;
    palette.mutedText = input.mutedText;
    palette.functionalAccent = input.functionalAccent;
    palette.error = input.error;
    palette.shadow = input.applicationSurface;
    palette.shadow.w *= 0.42f;
    return palette;
}

const ImVec4& ResolveSurfaceColor(
    const CoreWorkspacePalette& palette,
    SurfaceLevel level) noexcept {
    switch (level) {
        case SurfaceLevel::Application:
            return palette.applicationSurface;
        case SurfaceLevel::Docked:
            return palette.dockedSurface;
        case SurfaceLevel::Elevated:
            return palette.elevatedSurface;
    }
    return palette.applicationSurface;
}

const ImVec4& ResolveAccentColor(
    const CoreWorkspacePalette& palette,
    AccentRole role) noexcept {
    return role == AccentRole::None
        ? palette.perimeter
        : palette.functionalAccent;
}

ImVec4 ResolveSplitterColor(
    const CoreWorkspacePalette& palette,
    SplitterInteraction interaction) noexcept {
    switch (interaction) {
        case SplitterInteraction::Idle:
            return palette.splitter;
        case SplitterInteraction::Hovered: {
            ImVec4 hovered = palette.splitter;
            hovered.w = hovered.w < 0.82f ? 0.82f : hovered.w;
            return hovered;
        }
        case SplitterInteraction::Active:
            return palette.functionalAccent;
    }
    return palette.splitter;
}

void DrawWorkspaceSplitter(
    ImDrawList* drawList,
    const ImVec2& start,
    const ImVec2& end,
    const CoreWorkspacePalette& palette,
    SplitterInteraction interaction) {
    if (!drawList) return;
    drawList->AddLine(
        start,
        end,
        ImGui::ColorConvertFloat4ToU32(
        ResolveSplitterColor(palette, interaction)),
        kCoreWorkspaceMetrics.splitterWidth);
}

void DrawThreeDotSplitterGrip(
    ImDrawList* drawList,
    const ImVec2& center,
    const ImVec2& axis,
    float gap,
    float radius,
    ImU32 color) noexcept {
    if (!drawList) return;
    for (int i = -1; i <= 1; ++i) {
        const float offset = gap * static_cast<float>(i);
        drawList->AddCircleFilled(
            ImVec2(center.x + axis.x * offset, center.y + axis.y * offset),
            radius,
            color,
            12);
    }
}

CoreWorkspaceStyleScope::CoreWorkspaceStyleScope(
    const CoreWorkspacePalette& palette,
    SurfaceLevel surface,
    AccentRole accentRole) {
    if (!ImGui::GetCurrentContext()) return;

    const ImVec4& surfaceColor = ResolveSurfaceColor(palette, surface);
    const ImVec4& borderColor = ResolveAccentColor(palette, accentRole);

    const auto pushColor = [this](ImGuiCol slot, const ImVec4& color) {
        ImGui::PushStyleColor(slot, color);
        ++m_colorCount;
    };
    const auto pushFloat = [this](ImGuiStyleVar slot, float value) {
        ImGui::PushStyleVar(slot, value);
        ++m_varCount;
    };
    const auto pushVec2 = [this](ImGuiStyleVar slot, const ImVec2& value) {
        ImGui::PushStyleVar(slot, value);
        ++m_varCount;
    };

    pushColor(ImGuiCol_ChildBg, surfaceColor);
    pushColor(ImGuiCol_Border, borderColor);
    pushColor(ImGuiCol_Separator, palette.splitter);
    pushColor(ImGuiCol_SeparatorHovered,
              ResolveSplitterColor(palette, SplitterInteraction::Hovered));
    pushColor(ImGuiCol_SeparatorActive,
              ResolveSplitterColor(palette, SplitterInteraction::Active));
    pushColor(ImGuiCol_Text, palette.primaryText);
    pushColor(ImGuiCol_TextDisabled, palette.mutedText);

    const float rounding = surface == SurfaceLevel::Elevated
        ? kCoreWorkspaceMetrics.compactControlRadius
        : 0.0f;
    pushVec2(ImGuiStyleVar_WindowPadding,
             ImVec2(kCoreWorkspaceMetrics.contentInset,
                    kCoreWorkspaceMetrics.contentInset));
    pushVec2(ImGuiStyleVar_ItemSpacing,
             ImVec2(kCoreWorkspaceMetrics.space8,
                    kCoreWorkspaceMetrics.space4));
    pushVec2(ImGuiStyleVar_ItemInnerSpacing,
             ImVec2(kCoreWorkspaceMetrics.space4,
                    kCoreWorkspaceMetrics.space4));
    pushFloat(ImGuiStyleVar_ChildBorderSize,
              kCoreWorkspaceMetrics.perimeterWidth);
    pushFloat(ImGuiStyleVar_ChildRounding, rounding);
    pushFloat(ImGuiStyleVar_FrameRounding,
              kCoreWorkspaceMetrics.compactControlRadius);
}

void CoreWorkspaceStyleScope::reset() {
    if (!ImGui::GetCurrentContext()) {
        m_colorCount = 0;
        m_varCount = 0;
        return;
    }
    if (m_varCount > 0) {
        ImGui::PopStyleVar(m_varCount);
        m_varCount = 0;
    }
    if (m_colorCount > 0) {
        ImGui::PopStyleColor(m_colorCount);
        m_colorCount = 0;
    }
}

CoreWorkspaceStyleScope::~CoreWorkspaceStyleScope() {
    reset();
}

} // namespace aura::ui::workspace
