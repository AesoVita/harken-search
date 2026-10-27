#pragma once

// ThemeMascot.h - 主题精灵系统 (Theme Mascot System)

// 独立模块：管理每个主题的专属宠物精灵、音量条特效、动画状态
// 
// 功能列表:
//   - 多个主题精灵: 幽灵(Dark), 美人鱼(Ocean), 蘑菇精(Sunset), 猫咪(RoseCedar), 机器人(Gray)
//   - 4种动画状态: 淡出/待机/过载交互/点击互动
//   - 主题化音量条边框特效: 迷雾/气泡/熔岩
//   - 主题化过载提示颜色
//   - 精灵可拖拽 (最高层级渲染)
//   - 点击宠物互动: 回头看、光效闪烁

#ifndef THEME_MASCOT_H
#define THEME_MASCOT_H

#include "imgui.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>

// 主题枚举 (需与 main.cpp 中的 AppTheme 一致)

enum class ThemeType {
    Dark = 0,
    Ocean = 1,
    Sunset = 2,
    Gray = 6,         // 太空灰 (Pro Tools style) - 机器人精灵
    Custom = 7,
    ElectricBlue = 8,
    RoseCedar = 10    // 玫瑰雪松 - 柔粉猫咪精灵
};

// 精灵动画状态

enum class MascotAnimState {
    FadeOut = 0,     // 音量 < 20%  → 透明度渐变到30%
    Idle = 1,        // 音量 20-100% → 待机动画
    Excited = 2,     // 音量 100-120% → 兴奋交互动画
    Clicked = 3      // 点击宠物 → 回头看 + 光效
};

// 精灵系统核心类

// @stage7-contract
// API contract: ThemeMascot is a process-local UI-thread singleton owning its
// animation/drag state. update/render methods borrow ImGui draw lists and
// coordinates only for each frame; returned colors/state are copied values.
// Callers must use an active ImGui context and must not invoke concurrently.
class ThemeMascot {
public:
    // 单例
    static ThemeMascot& getInstance();

// 主循环接口
    
    // 更新动画状态 (每帧调用)
    // volume: 0-120, isPlaying: 是否有音频播放
    void update(float deltaTime, float volume, bool isPlaying);
    
    // 渲染音量条边框特效 (在音量条绘制之后调用)
    void renderVolumeBarEffects(ImDrawList* dl, ThemeType theme,
                                ImVec2 barPos, float barWidth, float barHeight,
                                float volume);
    
    // 渲染过载火焰/水花特效 (在音量条上方)
    void renderOverdriveEffect(ImDrawList* dl, ThemeType theme,
                               ImVec2 barPos, float barWidth, float barHeight,
                               float volume, float fireSpace);
    
    // 渲染精灵 (最高层级, 在所有UI之上)
    // 返回: 精灵是否被点击
    bool renderMascot(ImDrawList* dl, ThemeType theme,
                      ImVec2 barPos, float barWidth, float barHeight);
    
    // 获取当前主题的过载提示颜色
    ImU32 getOverdriveColor(ThemeType theme, float volume) const;
    
    // 获取当前主题的过载填充色
    ImU32 getOverdriveFillStart(ThemeType theme) const;
    ImU32 getOverdriveFillEnd(ThemeType theme) const;
    
// 状态查询
    bool isMascotDragging() const { return m_isDragging; }
    bool wasMascotClicked() const { return m_wasClicked; }
    MascotAnimState getAnimState() const { return m_animState; }
    float getMascotAlpha() const { return m_currentAlpha; }

private:
    ThemeMascot();
    ~ThemeMascot() = default;
    ThemeMascot(const ThemeMascot&) = delete;
    ThemeMascot& operator=(const ThemeMascot&) = delete;

// 内部绘制函数
    void drawMermaid(ImDrawList* dl, float baseX, float baseY, float px);
    void drawMushroom(ImDrawList* dl, float baseX, float baseY, float px);
    void drawGhost(ImDrawList* dl, float baseX, float baseY, float px);
    void drawKitty(ImDrawList* dl, float baseX, float baseY, float px);   // RoseCedar 主题精灵
    void drawRobot(ImDrawList* dl, float baseX, float baseY, float px);   // Gray 主题精灵
    
    // 绘制点击光效
    void drawClickFlash(ImDrawList* dl, float cx, float cy);
    
    // 绘制ZZZ睡眠文字 (音量极低时)
    void drawSleepZzz(ImDrawList* dl, float baseX, float baseY, float px);
    
// 各主题音量条边框特效
    void drawOceanBubbles(ImDrawList* dl, ImVec2 pos, float w, float h);
    void drawSunsetLava(ImDrawList* dl, ImVec2 pos, float w, float h);
    void drawDarkMist(ImDrawList* dl, ImVec2 pos, float w, float h);
    void drawKittySparkles(ImDrawList* dl, ImVec2 pos, float w, float h); // RoseCedar 音量条特效
    void drawGrayPulse(ImDrawList* dl, ImVec2 pos, float w, float h);     // Gray 主题脉冲光效
    
// 各主题过载特效
    void drawFireOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity, float fireSpace);
    void drawWaterOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity, float fireSpace);
    void drawLavaOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity, float fireSpace);
    void drawMistOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity, float fireSpace);
    void drawHeartOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity, float fireSpace); // RoseCedar 过载特效
    void drawGrayOverdrive(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity, float fireSpace);  // Gray 电流过载

// 辅助函数
    void drawPixel(ImDrawList* dl, float baseX, float baseY, float px,
                   float gx, float gy, ImU32 col) const;
    void updateDragState();
    void updateAnimState(float volume, bool isPlaying);
    
// 状态变量
    float m_time = 0.0f;            // 全局动画时间
    float m_frameTimer = 0.0f;      // 帧计时器
    int   m_frame = 0;              // 当前动画帧 (0-11)
    float m_stateTimer = 0.0f;      // 状态持续时间
    
    MascotAnimState m_animState = MascotAnimState::Idle;
    float m_currentAlpha = 1.0f;    // 当前透明度 (0-1)
    float m_targetAlpha = 1.0f;     // 目标透明度
    
    // 拖拽状态
    bool  m_isDragging = false;
    bool  m_wasClicked = false;
    float m_dragOffsetX = 0.0f;
    float m_dragOffsetY = 0.0f;
    float m_mascotScreenX = 0.0f;   // 精灵实际屏幕坐标
    float m_mascotScreenY = 0.0f;
    bool  m_hasCustomPos = false;    // 是否被用户拖动过
    
    // 点击交互状态
    float m_clickFlashTimer = 0.0f;  // 光效闪烁计时
    float m_headTurnTimer = 0.0f;    // 回头看计时
    bool  m_lookingAtUser = false;   // 是否在看用户
    // 蘑菇精跑步位置
    float m_mushroomRunPos = 0.0f;
    
    // 精灵包围盒 (用于点击检测)
    ImVec2 m_mascotBoundsMin;
    ImVec2 m_mascotBoundsMax;
    
    // 像素大小
    static constexpr float PIXEL_SIZE = 4.5f;    // 更大更清晰
    static constexpr int SPRITE_FRAMES = 12;
    static constexpr float CLICK_FLASH_DURATION = 0.6f;
    static constexpr float HEAD_TURN_DURATION = 1.5f;
    static constexpr float ALPHA_FADE_SPEED = 2.0f;  // 透明度变化速度
    
// 性能优化
    float m_renderAccum = 0.0f;      // 渲染帧率控制累加器
    bool  m_needsRender = true;      // 是否需要重绘
    int   m_lastFrame = -1;          // 上一次绘制的帧号
    float m_cachedSinT2 = 0.0f;     // 缓存 sin(time*2)
    float m_cachedSinT3 = 0.0f;     // 缓存 sin(time*3)
    float m_cachedCosT = 0.0f;      // 缓存 cos(time)
    static constexpr float RENDER_INTERVAL = 1.0f / 30.0f;  // 精灵30FPS足够
};

#endif // THEME_MASCOT_H
