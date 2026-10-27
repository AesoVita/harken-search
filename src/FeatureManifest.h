#pragma once

// FeatureManifest — 运行时功能裁剪（打包系统配套）

// 优先加载 harken_features.json + HMAC-SHA256 签名验证；旧文件名仅兼容读取。
// 用于在 ProductEdition 授权天花板以下进一步裁剪打包时排除的功能。
// 篡改或缺失 → 回退 Base 默认配置（最保守）。

#include <string>

namespace FeatureManifest {

// @stage7-contract
// API contract: manifest loading consumes an executable-directory path, verifies
// the signed feature file, and falls back conservatively on failure. Queries
// return value booleans and do not expose signature/storage ownership.

/// 启动时调用。加载 {exeDir}/harken_features.json + harken_features.sig。
/// 验证 HMAC-SHA256 签名。失败/缺失/篡改 → 内部标记为 fallback 模式。
/// @return true if loaded and verified; false if fallback.
bool loadAndVerify(const std::string& exeDir);

/// 查询某功能是否在本次打包中启用。
/// - manifest 未加载或验证失败 → 返回 true（不阻断，由 Edition 兜底）
/// - manifest 验证通过且功能不在白名单 → 返回 false
bool isEnabled(const char* featureId);

/// 是否已成功加载并验证 manifest。
bool isLoaded();

} // namespace FeatureManifest
