#pragma once
// HarkenSearchThemeManager.h — 次世代音频管理站视觉系统
// 整合 ThemeColorScheme 配色、ImGui 样式参数、波形渲染逻辑
// 统一管理从登录界面到主界面的视觉一致性

#include "imgui.h"
#include "Theme.h"
#include <string>

// Waveform Style — 波形渲染风格配置

struct WaveformStyle {
    ImU32 primaryColor;       // 主波形色 (蓝)
    ImU32 secondaryColor;     // 辅助渐变色 (青)
    ImU32 rmsColor;           // RMS 波形色
    ImU32 backgroundColor;    // 波形背景色
    ImU32 playheadColor;      // 播放头主色
    ImU32 playheadGlowColor;  // 播放头外发光色
    ImU32 playheadDotColor;   // 播放头顶部发光点色
    ImU32 borderColor;        // 边框色
    ImU32 emptyLineColor;     // 无波形时中线色
    ImU32 emptyTextColor;     // 无波形时文字色
    float cornerRadius;       // 圆角半径
    float barGap;             // 波形条间距
    float playheadWidth;      // 播放头宽度
    float playheadGlowWidth;  // 播放头外发光宽度
    float playheadDotRadius;  // 播放头顶部点半径
    bool  useGradient;        // 是否使用双色垂直渐变
};

// ImGui Style Config — ImGui 样式配置

struct ImGuiStyleConfig {
    // 圆角
    float windowRounding;
    float childRounding;
    float frameRounding;
    float popupRounding;
    float scrollbarRounding;
    float grabRounding;
    float tabRounding;

    // 间距
    ImVec2 windowPadding;
    ImVec2 framePadding;
    ImVec2 itemSpacing;
    ImVec2 itemInnerSpacing;
    ImVec2 cellPadding;

    // 边框
    float windowBorderSize;
    float childBorderSize;
    float frameBorderSize;
    float popupBorderSize;
};

// HarkenSearchThemeManager — 统一的主题管理器

// @stage7-contract
// API contract: HarkenSearchThemeManager is a process-local UI-thread singleton.
// initialize copies the supplied theme scheme; apply/draw methods must run
// with an active ImGui context and borrow draw-list/pixel buffers only for the
// call. Accessors return references owned by the singleton; callers must not
// retain them across reinitialization or use the manager concurrently.
class HarkenSearchThemeManager {
public:
// 单例
    static HarkenSearchThemeManager& instance();

// 初始化/应用
    // 从 ThemeColorScheme 初始化所有视觉参数 (颜色 + 样式 + 波形)
    void initialize(const ThemeColorScheme& scheme);

    // 应用 ImGui 样式 (颜色 + 尺寸)
    void applyImGuiStyle();

    // 获取当前波形样式
    const WaveformStyle& waveformStyle() const { return m_waveformStyle; }

    // 获取当前样式配置
    const ImGuiStyleConfig& styleConfig() const { return m_styleConfig; }

    // 获取当前颜色方案 (只读)
    const ThemeColorScheme& colorScheme() const { return m_colorScheme; }

// 波形渲染
    // 现代化波形绘制 — 支持双色渐变 + Neon 播放头
    // 在文件列表中使用的迷你波形
    void drawMiniWaveform(
        ImDrawList* drawList,
        ImVec2 pos, float width, float height,
        const float* peaks, int peakCount,
        float playbackPos = -1.0f  // <0 表示不绘制播放头
    );

    // 大波形面板绘制 (含 RMS 层)
    void drawWaveformPanel(
        ImDrawList* drawList,
        ImVec2 pos, float width, float height,
        const float* peaks, const float* rms, int peakCount,
        float playbackPos = -1.0f
    );

// 辅助工具
    // ImVec4 → ImU32 转换
    static ImU32 toImU32(const ImVec4& color);
    // ImU32 with alpha override
    static ImU32 withAlpha(ImU32 color, int alpha);

// 预设
    // Electric Blue 预设 (匹配登录界面)
    static ImGuiStyleConfig getElectricBlueStyle();
    static WaveformStyle    getElectricBlueWaveform();

private:
    HarkenSearchThemeManager() = default;

    ThemeColorScheme m_colorScheme;
    ImGuiStyleConfig m_styleConfig;
    WaveformStyle    m_waveformStyle;

    // 内部：从 ThemeColorScheme 派生全量 ImGui 颜色映射
    void applyColorMapping(ImVec4* colors);

    // 内部：绘制 Neon 播放头
    void drawNeonPlayhead(ImDrawList* drawList, float x, float yTop, float yBottom);
};
