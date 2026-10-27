#pragma once

#include <string>

// Looks up folder artwork in an optional external library database.
// 配置：AppState::externalLibraryDbPathUtf8 / settings.ini externalLibraryDbPath
namespace ExternalLibraryCover {

// 设置外部库 SQLite 根目录（空则禁用）
void setDbPath(const std::string& dbRootPathUtf8);

// 按文件夹路径查封面图；命中则写 outCoverPathUtf8 并返回 true
// @stage7-contract
// API contract: setDbPath copies the optional UTF-8 library root for the
// process, while tryGetCoverForFolder synchronously reads caller-owned folder
// input and writes an owned output path on success. Empty/false indicates the
// disabled, missing, or unreadable-library cases; no caller buffer is retained.
bool tryGetCoverForFolder(const std::string& folderPathUtf8, std::string& outCoverPathUtf8);
}
