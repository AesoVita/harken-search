#pragma once

#include "ui/CoreWorkspaceStyle.h"

namespace aura::ui::workspace {

enum class ModuleWindowMode {
    Docked,
    Floating,
    Detached,
};

enum class ModuleShellActivity {
    Inactive,
    Active,
    Focused,
};

enum class ModuleShellContentState {
    Ready,
    Empty,
    Loading,
    Error,
    Unavailable,
};

struct ModuleWindowShellState {
    ModuleWindowMode mode = ModuleWindowMode::Docked;
    ModuleShellActivity activity = ModuleShellActivity::Inactive;
    ModuleShellContentState content = ModuleShellContentState::Ready;
};

struct ModuleWindowShellVisual {
    SurfaceLevel surface = SurfaceLevel::Docked;
    ImVec4 borderColor{};
    ImVec4 statusColor{};
    bool drawShadow = false;
    bool contentVisible = true;
};

struct ModuleWindowShellOptions {
    const char* title = nullptr;
    const char* stateMessage = nullptr;
    const char* footerText = nullptr;
    ImVec2 size{0.0f, 0.0f};
    ModuleWindowShellState state{};
    bool showFooter = false;
    bool resizable = false;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
};

ModuleWindowShellVisual ResolveModuleWindowShellVisual(
    const CoreWorkspacePalette& palette,
    const ModuleWindowShellState& state) noexcept;

const char* DefaultModuleShellStatusText(
    ModuleShellContentState state) noexcept;

// Visual/API shell only: it creates a styled child surface but does not own a
// docking tree, tab group, persistence model, or cross-window behavior.
class ModuleWindowShellScope {
public:
    ModuleWindowShellScope(
        const char* stableId,
        const CoreWorkspacePalette& palette,
        const ModuleWindowShellOptions& options = {});
    ~ModuleWindowShellScope();

    ModuleWindowShellScope(const ModuleWindowShellScope&) = delete;
    ModuleWindowShellScope& operator=(const ModuleWindowShellScope&) = delete;

    bool visible() const noexcept { return m_visible; }
    bool shouldRenderContent() const noexcept {
        return m_visible && m_visual.contentVisible;
    }
    const ModuleWindowShellVisual& visual() const noexcept { return m_visual; }

private:
    void drawHeader();
    void drawContentState();
    void drawFooter();
    void drawResizeAffordance();

    CoreWorkspacePalette m_palette{};
    ModuleWindowShellOptions m_options{};
    ModuleWindowShellVisual m_visual{};
    CoreWorkspaceStyleScope m_styleScope;
    bool m_childBegun = false;
    bool m_visible = false;
};

} // namespace aura::ui::workspace
