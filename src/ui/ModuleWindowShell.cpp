#include "ui/ModuleWindowShell.h"

namespace aura::ui::workspace {
namespace {

AccentRole AccentForActivity(ModuleShellActivity activity) noexcept {
    switch (activity) {
        case ModuleShellActivity::Inactive:
            return AccentRole::None;
        case ModuleShellActivity::Active:
            return AccentRole::Active;
        case ModuleShellActivity::Focused:
            return AccentRole::Focused;
    }
    return AccentRole::None;
}

} // namespace

ModuleWindowShellVisual ResolveModuleWindowShellVisual(
    const CoreWorkspacePalette& palette,
    const ModuleWindowShellState& state) noexcept {
    ModuleWindowShellVisual visual;
    visual.surface = state.mode == ModuleWindowMode::Docked
        ? SurfaceLevel::Docked
        : SurfaceLevel::Elevated;
    visual.borderColor = ResolveAccentColor(
        palette, AccentForActivity(state.activity));
    visual.statusColor = state.content == ModuleShellContentState::Error
        ? palette.error
        : palette.mutedText;
    visual.drawShadow = state.mode != ModuleWindowMode::Docked;
    visual.contentVisible = state.content == ModuleShellContentState::Ready;
    return visual;
}

const char* DefaultModuleShellStatusText(
    ModuleShellContentState state) noexcept {
    switch (state) {
        case ModuleShellContentState::Ready:
            return "";
        case ModuleShellContentState::Empty:
            return "No content";
        case ModuleShellContentState::Loading:
            return "Loading...";
        case ModuleShellContentState::Error:
            return "Unable to load module";
        case ModuleShellContentState::Unavailable:
            return "Module unavailable";
    }
    return "";
}

ModuleWindowShellScope::ModuleWindowShellScope(
    const char* stableId,
    const CoreWorkspacePalette& palette,
    const ModuleWindowShellOptions& options)
    : m_palette(palette),
      m_options(options),
      m_visual(ResolveModuleWindowShellVisual(palette, options.state)),
      m_styleScope(
          palette,
          m_visual.surface,
          AccentForActivity(options.state.activity)) {
    if (!ImGui::GetCurrentContext()) return;

    m_childBegun = true;
    m_visible = ImGui::BeginChild(
        stableId ? stableId : "##ModuleWindowShell",
        m_options.size,
        ImGuiChildFlags_Borders,
        m_options.windowFlags);
    if (!m_visible) return;

    drawHeader();
    if (!m_visual.contentVisible) drawContentState();
}

void ModuleWindowShellScope::drawHeader() {
    if (!m_options.title || m_options.title[0] == '\0') return;

    const float startY = ImGui::GetCursorPosY();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(m_options.title);
    const float used = ImGui::GetCursorPosY() - startY;
    const float remaining = kCoreWorkspaceMetrics.headerHeight - used;
    if (remaining > 0.0f) ImGui::Dummy(ImVec2(0.0f, remaining));
    ImGui::Separator();
}

void ModuleWindowShellScope::drawContentState() {
    const char* text = m_options.stateMessage;
    if (!text || text[0] == '\0') {
        text = DefaultModuleShellStatusText(m_options.state.content);
    }
    ImGui::Dummy(ImVec2(0.0f, kCoreWorkspaceMetrics.space8));
    ImGui::PushStyleColor(ImGuiCol_Text, m_visual.statusColor);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void ModuleWindowShellScope::drawFooter() {
    if (!m_options.showFooter) return;
    ImGui::Separator();
    if (m_options.footerText && m_options.footerText[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, m_palette.mutedText);
        ImGui::TextUnformatted(m_options.footerText);
        ImGui::PopStyleColor();
    } else {
        ImGui::Dummy(ImVec2(0.0f, kCoreWorkspaceMetrics.footerHeight));
    }
}

void ModuleWindowShellScope::drawResizeAffordance() {
    if (!m_options.resizable) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 corner(windowPos.x + windowSize.x - kCoreWorkspaceMetrics.space4,
                        windowPos.y + windowSize.y - kCoreWorkspaceMetrics.space4);
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(m_palette.splitter);
    DrawThreeDotSplitterGrip(
        drawList,
        corner,
        ImVec2(-0.7071f, 0.7071f),
        kCoreWorkspaceMetrics.space4,
        kCoreWorkspaceMetrics.splitterWidth + 0.5f,
        color);
}

ModuleWindowShellScope::~ModuleWindowShellScope() {
    if (m_childBegun && ImGui::GetCurrentContext()) {
        if (m_visible) {
            drawFooter();
            drawResizeAffordance();
        }
        ImGui::EndChild();
    }
    m_childBegun = false;
    m_visible = false;
    m_styleScope.reset();
}

} // namespace aura::ui::workspace
