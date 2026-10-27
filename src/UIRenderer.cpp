// SoundManager - UI渲染模块
// UIRenderer.cpp - 主UI渲染、面板系统、文件列表
//
// 模块化重构说明：

// 这个文件是 main.cpp 模块化重构的一部分。
// 目前，大部分 UI 渲染代码仍在 main.cpp 中实现。
// 
// 重构计划：
// Phase 1: 创建头文件和模块结构（已完成）
// Phase 2: 移动独立的工具函数
// Phase 3: 移动面板系统代码
// Phase 4: 移动主 RenderUI 函数
// Phase 5: 移动其余渲染函数
//
// 此文件将在后续重构中逐步填充实现代码。

#include "UIRenderer.h"
#include "AppState.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

PlaybackAction GetPlaybackAction(
    const AppSettings& settings,
    bool wasPlaying,
    bool selectionChanged,
    bool isDoubleClick) {
    if (isDoubleClick) {
        return (settings.playbackMode == PlaybackMode::DoubleClick ||
                settings.playbackMode == PlaybackMode::Toggle)
            ? PlaybackAction::Play
            : PlaybackAction::None;
    }
    switch (settings.playbackMode) {
        case PlaybackMode::SingleClick:
            return settings.autoplayOnSelect
                ? PlaybackAction::Play
                : PlaybackAction::None;
        case PlaybackMode::Toggle:
            return (!selectionChanged && wasPlaying)
                ? PlaybackAction::Pause
                : PlaybackAction::Play;
        case PlaybackMode::DoubleClick:
            return PlaybackAction::None;
    }
    return PlaybackAction::None;
}

std::string FormatTime(float seconds, bool showMs) {
    int hours = (int)(seconds / 3600);
    int mins = (int)(seconds / 60) % 60;
    int secs = (int)seconds % 60;
    char buffer[32];
    if (showMs) {
        int frames = (int)((seconds - (int)seconds) * 100);
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%02d", hours, mins, secs, frames);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, mins, secs);
    }
    return buffer;
}

float GetSmoothedPlaybackPosition(AppState& state) {
    const int frame = ImGui::GetFrameCount();
    static int s_lastFrame = -1;
    static float s_lastBackend = 0.0f;
    static float s_prevBackend = -1.0f;
    static std::chrono::steady_clock::time_point s_lastSyncWall;
    static bool s_first = true;
    static bool s_lastPlaying = false;
    static float s_lastDur = 0.0f;
    static std::string s_lastFilePath;

    const auto& fileInfo = state.audioEngine.getFileInfo();
    const std::string currentFilePath = fileInfo.filePath;

    if (currentFilePath != s_lastFilePath) {
        s_lastFilePath = currentFilePath;
        s_lastFrame = -1;
        s_lastBackend = 0.0f;
        s_prevBackend = -1.0f;
        s_lastSyncWall = std::chrono::steady_clock::now();
        s_first = true;
        s_lastPlaying = false;
        s_lastDur = fileInfo.durationSeconds;
    }

    if (frame != s_lastFrame) {
        s_lastFrame = frame;
        s_lastPlaying = state.audioEngine.isPlaying();
        s_lastDur = fileInfo.durationSeconds;
        const float backend = state.audioEngine.getPlaybackPosition();
        auto now = std::chrono::steady_clock::now();

        if (!s_lastPlaying || s_lastDur <= 0.0f) {
            s_lastBackend = backend;
            s_prevBackend = backend;
            if (s_first) s_first = false;
            s_lastSyncWall = now;
        } else {
            if (s_lastPlaying && !s_first) {
                float pitch = state.audioEngine.getPitch();
                float speed = std::pow(2.0f, pitch / 12.0f);
                double elapsed =
                    std::chrono::duration<double>(now - s_lastSyncWall).count();
                float predicted =
                    s_lastBackend + (float)(elapsed * speed / (double)s_lastDur);

                float diff = std::abs(backend - predicted);
                float maxSmoothDiff = 0.02f;
                if (elapsed > 0.033 && diff < maxSmoothDiff) {
                    float alpha = std::min(1.0f, (float)(elapsed * 3.0));
                    s_lastBackend = predicted + (backend - predicted) * alpha;
                } else {
                    s_lastBackend = backend;
                }
            } else {
                s_lastBackend = backend;
            }
            s_lastSyncWall = now;
            s_first = false;
        }
    }

    if (s_lastPlaying && s_lastDur > 0.0f) {
        float pitch = state.audioEngine.getPitch();
        float speed = std::pow(2.0f, pitch / 12.0f);
        double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - s_lastSyncWall).count();
        float predicted =
            s_lastBackend + (float)(elapsed * speed / (double)s_lastDur);
        return std::clamp(predicted, 0.0f, 1.0f);
    }
    return std::clamp(s_lastBackend, 0.0f, 1.0f);
}

// 当前所有函数实现都在 main.cpp 中
// 它们将在后续重构中移到这里
