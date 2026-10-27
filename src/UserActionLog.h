#pragma once

// UserActionLog — 用户操作回放日志

//
// 纯用户操作时间线，不记录后台/性能数据。用于 bug 复现：
//   "用户做了什么 → 导致了什么问题"
//
// 输出格式（人类可读）：
//   [00:02.341] +2.341s  SEARCH     "kick drum"
//   [00:03.012] +0.671s  FILTER     category=Foley
//   [00:04.500] +1.488s  SELECT     file#3 "kick_01.wav"
//   [00:04.501] +0.001s  PLAY       "kick_01.wav"
//
// 构建开关：跟随 AURA_BUILD_DIAGNOSTICS。Release 时宏为空操作。

#include "BuildConfig.h"
#include "platform/filesystem.h"
#include <string>
#include <vector>

// @stage7-contract
// API contract: UserActionLog records user-visible actions only; background and
// performance events are excluded. Entries are owned value data, append/export
// preserve chronological order, and disabled release builds remain no-ops.
#include <mutex>
#include <chrono>
#include <cstdint>
#include <atomic>
#include <cstdio>

// Action Categories

// 枚举用于内部存储和过滤；日志输出时转为可读字符串
enum class UACategory : uint8_t {
    Search,      // 搜索输入、清空
    Filter,      // 筛选器增删改
    Sort,        // 排序切换
    Select,      // 文件选中
    Play,        // 播放/暂停/停止
    Seek,        // 波形点击/拖拽跳转
    Navigate,    // 文件浏览器导航、Tab切换
    Edit,        // 元数据编辑、标签修改
    Export,      // 导出、复制路径、拖拽到外部
    Shortcut,    // 键盘快捷键
    UI,          // 窗口操作（resize、dock、右键菜单）
    System,      // 应用启动/关闭/设置变更
};

struct UAEntry {
    UACategory  category;
    char        detail[256];      // 操作描述
    int64_t     timestampMs;      // Unix 时间戳 (ms)
    double      elapsedSinceLastS; // 距上一条操作的秒数
    double      sessionTimeS;     // 会话开始后的秒数
};

// UserActionLog — Singleton

class UserActionLog {
public:
    static UserActionLog& instance();

    // ---- 开关 ----
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled.load(std::memory_order_relaxed); }

    // ---- 输出路径 ----
    void setOutputPath(const std::string& pathUtf8);

    // ---- 日志写入 ----
    void log(UACategory cat, const char* detail);
    void logFmt(UACategory cat, const char* fmt, ...);

    // ---- 导出 ----
    // 导出人类可读的操作时间线到文件
    bool exportTimeline(const std::string& pathUtf8) const;
    // 导出到字符串（用于 UI 显示或剪贴板）
    std::string exportTimelineString() const;
    // 导出最近 N 条到字符串
    std::string exportRecentString(int n = 50) const;

    // ---- 查询 ----
    size_t entryCount() const;
    void   clear();

    // 复制最近 N 条到输出数组，返回实际复制数
    int copyRecent(UAEntry* out, int maxOut) const;

private:
    UserActionLog();
    ~UserActionLog();
    UserActionLog(const UserActionLog&) = delete;
    UserActionLog& operator=(const UserActionLog&) = delete;

    void flushEntry(const UAEntry& entry);
    static const char* categoryName(UACategory cat);
    static int64_t nowMs();

    std::atomic<bool>    m_enabled{false};
    std::string          m_outputPath;

    // 环形缓冲（内存中保留最近 2000 条用户操作）
    static constexpr size_t kMaxEntries = 2000;
    std::vector<UAEntry> m_ring;
    size_t               m_head  = 0;
    size_t               m_count = 0;
    mutable std::mutex   m_mutex;

    // 时间基准
    std::chrono::steady_clock::time_point m_sessionStart;
    std::chrono::steady_clock::time_point m_lastAction;

    // 流式文件输出
    Platform::FileSystem::StreamWriter m_file;
};

// 便捷宏 — Release 时零开销

#if AURA_BUILD_DIAGNOSTICS

#define UA_LOG(category, detail) \
    do { if (UserActionLog::instance().isEnabled()) \
        UserActionLog::instance().log(category, detail); } while(0)

#define UA_LOG_FMT(category, fmt, ...) \
    do { if (UserActionLog::instance().isEnabled()) \
        UserActionLog::instance().logFmt(category, fmt, __VA_ARGS__); } while(0)

#else

#define UA_LOG(category, detail)          do {} while(0)
#define UA_LOG_FMT(category, fmt, ...)    do {} while(0)

#endif
