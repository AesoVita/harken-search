#pragma once

// GMPanel — 集中式 debug 数据面板（替代主界面内联 debug 文字）

// AURA_BUILD_DIAGNOSTICS=0 时编译为空操作。
// 激活路径：Ctrl+Alt+D 开启诊断模式 → 状态栏 GM 按钮 → 浮动面板

#include "BuildConfig.h"

struct AppState;

// @stage7-contract
// API contract: GMPanel_Render is a diagnostics-gated UI-thread render pass
// borrowing AppState and the active ImGui context. In non-diagnostics builds
// it remains the existing no-op; no state ownership or background work is
// introduced at this boundary.

#if AURA_BUILD_DIAGNOSTICS
void GMPanel_Render(AppState& state);
#else
inline void GMPanel_Render(AppState&) {}
#endif
