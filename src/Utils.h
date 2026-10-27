#pragma once
// SoundManager - 工具函数模块
// Utils.h - UTF-8转换、路径操作、字符串处理

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <filesystem>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Undefine Windows macros that conflict with imgui
#ifdef SetWindowPos
#undef SetWindowPos
#endif
#ifdef GetWindowPos
#undef GetWindowPos
#endif
#endif

namespace fs = std::filesystem;

// @stage7-contract
// API contract: conversion and path helpers return owned values, do not retain
// caller strings, and preserve their established invalid-input behavior. Path
// operations use the platform boundary rather than exposing native handles;
// callers remain responsible for mutation ordering and thread serialization.

// UTF-8 <-> Wide String Conversion

// UTF-8字符串转换为宽字符串 (Windows)
std::wstring Utf8ToWide(const std::string& str);

// 宽字符串转换为UTF-8字符串
std::string WideToUtf8(const std::wstring& wstr);

// UTF-8路径转换为filesystem::path
fs::path Utf8ToPath(const std::string& str);

// filesystem::path转换为UTF-8字符串
std::string PathToUtf8(const fs::path& p);

// String Utilities

// 去除字符串首尾空白字符
std::string TrimString(const std::string& s);

// 去除字符串首尾空白字符（原地修改）
std::string TrimCopy(std::string s);

// 清理Windows文件名中的非法字符
std::string SanitizeWindowsName(std::string name);

// Natural compare for UTF-8 strings with ASCII digit runs.
// Returns <0 when lhs<rhs, 0 when equal, >0 when lhs>rhs.
int CompareNaturalUtf8(std::string_view lhs, std::string_view rhs);

inline bool NaturalLessUtf8(std::string_view lhs, std::string_view rhs) {
    return CompareNaturalUtf8(lhs, rhs) < 0;
}

// Path Utilities

// 检查路径是否存在
bool PathExistsUtf8(const std::string& pathUtf8);

// 获取可执行文件所在目录
std::string GetExeDirUtf8();

// 获取默认数据库路径
std::string GetDefaultDbPathUtf8();

// 获取数据库专属目录（%LOCALAPPDATA%\SoundManager）
std::string GetDefaultDbDirUtf8();
std::wstring GetDefaultDbDirWide();

// 分离文件名和扩展名
void SplitFileNameUtf8(const std::string& pathUtf8, std::string& outStem, std::string& outExtWithDot);

// 从文件夹封面 key 中抽出用于磁盘查找的路径（strip F| 与 \\?\ 前缀，供 CoverModule 异步 worker 使用）
// 若 key 为 F|H|hash|size 等稳定 key 则返回空；若为 F|\\?\H:\path 或 F|H:\path 则返回普通路径
std::string NormalizeFolderPathFromKeyForLookup(const std::string& key);

// 定位后高亮：当前节点是否为「刚定位到的文件夹」且仍在高亮有效期内（供文件夹树绘制用）
bool ShouldHighlightFolderNodeForLocate(const std::string& nodePath, const std::string& highlightPath, int64_t highlightUntilMs, int64_t nowMs);

// 导出/拖入 DAW 防爆音：对 PCM 缓冲首尾施加短线性淡入淡出（in-place）
// samples = interleaved [L R L R ...]，numFrames=帧数，channels=声道数
// globalStartFrame = 本块在整段中的起始帧号，totalFrames = 整段总帧数，fadeLenFrames = 淡入/淡出长度（帧）
void ApplyExportDeclickFade(float* samples, size_t numFrames, size_t channels, size_t globalStartFrame, size_t totalFrames, size_t fadeLenFrames);

// 开发模式切库持久化：仅当当前数据库路径仍存在，且它确实出现在最近数据库列表中时，
// 才认为这是用户显式切过并应在下次启动时继续保留。
inline bool ShouldKeepDevModeCurrentDbPath(const std::string& currentDbPathUtf8,
                                          const std::vector<std::string>& recentDbPathsUtf8) {
    if (currentDbPathUtf8.empty() || !PathExistsUtf8(currentDbPathUtf8)) return false;
    return std::find(recentDbPathsUtf8.begin(), recentDbPathsUtf8.end(), currentDbPathUtf8) != recentDbPathsUtf8.end();
}

// Audio File Utilities

// Check whether a file path has a supported audio extension (.wav .mp3 .flac etc.)
// Case-insensitive. Declared here so it can be used without pulling in AIEngine.h.
bool IsAudioFileSupported(const std::string& path);

// Settings File I/O (Key=Value format)

// 加载键值对配置文件
std::unordered_map<std::string, std::string> LoadKeyValueFile(const std::string& pathUtf8);

// 保存键值对配置文件
void SaveKeyValueFile(const std::string& pathUtf8, const std::vector<std::pair<std::string, std::string>>& items);

// 解析配置值辅助函数
bool ParseBool(const std::string& v, bool defVal);
int ParseInt(const std::string& v, int defVal);
float ParseFloat(const std::string& v, float defVal);

// Time Formatting

// 格式化Unix时间戳为本地日期时间字符串
std::string FormatDateTimeLocal(int64_t unixSeconds);

// 获取文件创建和修改时间 (Unix时间戳)
bool GetFileTimesUtf8(const std::string& filePathUtf8, int64_t& outCreationUnix, int64_t& outModifiedUnix);

// Database Package Helpers

// 将用户选择的数据库文件路径规范化为“独立目录 + main.db”结构。
// 例如：D:\DB\Weapons.db -> D:\DB\Weapons\main.db
std::string BuildDatabasePackageMainPathUtf8(const std::string& requestedPathUtf8);

// 由 main.db 路径得到数据库包根目录；若不是包结构，则回退到文件所在目录/文件本身。
std::string GetDatabasePackageRootUtf8(const std::string& mainDbPathUtf8);

// 用于 UI 展示数据库名称：包结构优先显示目录名，否则显示文件 stem。
std::string GetDatabaseDisplayNameUtf8(const std::string& dbPathUtf8);
std::string RepairUtf8MojibakeFromAnsiIfNeeded(const std::string& valueUtf8);

enum class DatabaseLibraryState {
    Active,
    Missing,
    Disconnected
};

inline DatabaseLibraryState ClassifyDatabaseLibraryPath(const std::string& dbPathUtf8) {
    if (dbPathUtf8.empty()) return DatabaseLibraryState::Disconnected;
    return PathExistsUtf8(dbPathUtf8) ? DatabaseLibraryState::Active : DatabaseLibraryState::Missing;
}

inline bool IsDatabaseLibrarySelectable(DatabaseLibraryState state) {
    return state == DatabaseLibraryState::Active;
}

inline const char* DatabaseLibraryStateNameUtf8(DatabaseLibraryState state) {
    switch (state) {
    case DatabaseLibraryState::Active: return "Active";
    case DatabaseLibraryState::Missing: return "Missing";
    case DatabaseLibraryState::Disconnected: return "Disconnected";
    }
    return "Disconnected";
}

inline std::string BuildDatabaseLibraryDisplayLabelUtf8(const std::string& dbPathUtf8) {
    std::string label = GetDatabaseDisplayNameUtf8(dbPathUtf8);
    switch (ClassifyDatabaseLibraryPath(dbPathUtf8)) {
    case DatabaseLibraryState::Missing:
        label += " [Missing]";
        break;
    case DatabaseLibraryState::Disconnected:
        label += " [Disconnected]";
        break;
    case DatabaseLibraryState::Active:
        break;
    }
    return label;
}

inline bool RemoveRecentDatabasePath(std::vector<std::string>& recentDbPathsUtf8,
                                     const std::string& dbPathUtf8) {
    const auto oldSize = recentDbPathsUtf8.size();
    recentDbPathsUtf8.erase(
        std::remove(recentDbPathsUtf8.begin(), recentDbPathsUtf8.end(), dbPathUtf8),
        recentDbPathsUtf8.end());
    return recentDbPathsUtf8.size() != oldSize;
}

inline bool ReplaceRecentDatabasePath(std::vector<std::string>& recentDbPathsUtf8,
                                      const std::string& oldPathUtf8,
                                      const std::string& newPathUtf8) {
    if (oldPathUtf8.empty() || newPathUtf8.empty()) return false;
    auto it = std::find(recentDbPathsUtf8.begin(), recentDbPathsUtf8.end(), oldPathUtf8);
    if (it == recentDbPathsUtf8.end()) return false;
    *it = newPathUtf8;
    for (auto jt = recentDbPathsUtf8.begin(); jt != recentDbPathsUtf8.end();) {
        if (jt != it && *jt == newPathUtf8) jt = recentDbPathsUtf8.erase(jt);
        else ++jt;
    }
    return true;
}

// Security & Activation Utilities

// 获取经过加密脱敏的机器唯一标识码
std::string GetEncryptedMachineID();

struct SettingsProtectionResult {
    bool success = false;
    std::string encodedValue;
    std::string errorKind;

    static SettingsProtectionResult Success(std::string encoded) {
        return {true, std::move(encoded), {}};
    }
    static SettingsProtectionResult Failure(std::string kind) {
        return {false, {}, std::move(kind)};
    }
};

struct SettingsUnprotectionResult {
    bool success = false;
    std::string plaintext;
    std::string errorKind;

    static SettingsUnprotectionResult Success(std::string value) {
        return {true, std::move(value), {}};
    }
    static SettingsUnprotectionResult Failure(std::string kind) {
        return {false, {}, std::move(kind)};
    }
};

struct SiliconFlowSaveResult {
    bool success = false;
    std::string errorKind;
    std::string retainedInput;
    std::string savedPlaintext;
};

using SettingsProtectionProvider = std::function<SettingsProtectionResult(const std::string&)>;

class ScopedSettingsProtectionProvider {
public:
    explicit ScopedSettingsProtectionProvider(SettingsProtectionProvider provider);
    ~ScopedSettingsProtectionProvider();
    ScopedSettingsProtectionProvider(const ScopedSettingsProtectionProvider&) = delete;
    ScopedSettingsProtectionProvider& operator=(const ScopedSettingsProtectionProvider&) = delete;
private:
    SettingsProtectionProvider previous_;
};

SettingsProtectionResult ProtectSettingsValue(const std::string& plaintext);
SettingsUnprotectionResult UnprotectSettingsValue(const std::string& encodedValue);
SiliconFlowSaveResult SaveSiliconFlowApiKeyTransactional(
    const std::string& settingsPathUtf8,
    const std::string& credentialTarget,
    const std::string& previousSavedPlaintext,
    const std::string& candidateKey);

// Historical machine-bound XOR helpers retained only for ENC: migration reads.
std::string EncryptSettingsValue(const std::string& plaintext);
std::string DecryptSettingsValue(const std::string& ciphertext);
bool IsValidSiliconFlowApiKey(const std::string& value);

// SiliconFlow API key storage backed by the Windows Credential Manager.
bool StoreWindowsCredential(const std::string& targetName, const std::string& value);
bool LoadWindowsCredential(const std::string& targetName, std::string& outValue);
bool DeleteWindowsCredential(const std::string& targetName);
bool StoreSiliconFlowApiKey(const std::string& apiKey);
bool LoadSiliconFlowApiKey(std::string& outApiKey);
bool DeleteSiliconFlowApiKey();

// CJK token spacing and pinyin initials for search normalization.
std::string InjectCJKSpaces(const std::string& utf8_str);
std::string GetPinyinInitials(const std::string& utf8_str);
