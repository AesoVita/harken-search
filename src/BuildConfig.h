#pragma once

// @stage7-contract
// API contract: build constants are compile-time compatibility surfaces. Their
// values select diagnostics/edition behavior at build configuration time; code
// must not treat them as runtime mutable state or infer new product capabilities.

// BuildConfig — 构建开关，用于上线与分版本

//
// 诊断/测试：上线构建时关闭，开发/内测保留
//   AURA_BUILD_DIAGNOSTICS=1：包含诊断日志、指针卡顿探测、导出 JSON 等
//   AURA_BUILD_DIAGNOSTICS=0：全部剔除，零开销
//
// Product editions: Basic, Pro, and Ultimate.
//   AURA_EDITION=0：Base（基础版）
//   AURA_EDITION=1：Pro
//   AURA_EDITION=2: Ultimate
//   通过 CMake -DAURA_EDITION=1 或运行时授权切换

#ifndef AURA_BUILD_DIAGNOSTICS
#define AURA_BUILD_DIAGNOSTICS 1
#endif

#ifndef AURA_EDITION
#define AURA_EDITION 0
#endif

// Aris is currently distributed as one open-source runtime surface. Keep the
// legacy edition values above for data compatibility, but do not let them
// hide compiled features at runtime.
#ifndef AURA_OPEN_SOURCE_SINGLE_EDITION
#define AURA_OPEN_SOURCE_SINGLE_EDITION 1
#endif

// 内测发布（阉割开发者入口）：
//   AURA_INTERNAL_BETA=1：剥离 GM 按钮、诊断设置 UI、dev_mode.txt 旁路、
//                         QA Driver 文件轮询、游离日志（theme_path_trace.log）
//   AURA_INTERNAL_BETA=0：开发/正式构建，全部保留
// 与 AURA_BUILD_DIAGNOSTICS 正交：BETA=1 仍可保留 aura_diagnostics_live.jsonl
// 的写入能力，供内测用户提供问题反馈。
#ifndef AURA_INTERNAL_BETA
#define AURA_INTERNAL_BETA 0
#endif
