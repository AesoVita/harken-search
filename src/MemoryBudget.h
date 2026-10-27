#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>

// 内存预算管理器（字节预算，而非个数限制）
// @stage7-contract
// API contract: MemoryBudget owns accounting state and applies byte budgets;
// callers provide stable cache keys and treat returned counters/snapshots as
// values. One instance must be serialized as declared; no cache entry storage
// or caller string reference escapes the budget manager.
class MemoryBudget {
public:
    // 内存预算配置（字节）；totalBudget 硬约束，先满足总预算再按类型
    struct BudgetConfig {
        uint64_t metadataBudget = 200 * 1024 * 1024;   // 200MB
        uint64_t waveformBudget = 150 * 1024 * 1024;   // 150MB - 波形
        uint64_t coverBudget = 250 * 1024 * 1024;       // 250MB - 封面
        uint64_t totalBudget = 600ULL * 1024 * 1024;   // 600MB - 总预算硬约束
    };

    // 缓存条目（用于LRU淘汰）
    struct CacheEntry {
        std::string key;
        uint64_t sizeBytes = 0;  // 占用的字节数
        uint64_t lastUsedFrame = 0;  // 最后使用帧数
        void* userData = nullptr;  // 用户数据指针（用于释放资源）
    };

    enum class CacheType {
        Metadata,  // 元数据缓存
        Waveform,  // 波形缓存
        Cover      // 封面纹理缓存
    };

    MemoryBudget();
    ~MemoryBudget();

    // 设置预算配置
    void setBudgetConfig(const BudgetConfig& config);
    BudgetConfig getBudgetConfig() const { return m_config; }

    // 注册缓存条目
    bool registerEntry(CacheType type, const std::string& key, uint64_t sizeBytes, void* userData = nullptr);
    
    // 更新条目使用时间（lastUsedFrame 使用 setCurrentFrame 的帧号，保证 LRU 可靠）
    void updateUsage(CacheType type, const std::string& key);
    /// 每帧由 UI 调用，用于 LRU 时间戳（如 ImGui::GetFrameCount()）
    void setCurrentFrame(uint64_t frameNumber);
    
    // 注销缓存条目
    void unregisterEntry(CacheType type, const std::string& key);
    
    // 检查是否可以添加新条目（不超出预算）
    bool canAdd(CacheType type, uint64_t sizeBytes) const;
    
    // 强制淘汰（当达到预算时）；返回需由调用方释放的条目（如 glDeleteTextures / 释 buffer）
    std::vector<CacheEntry> evictLRU(CacheType type, uint64_t requiredBytes);
    /// registerEntry 因总预算触发的跨类型淘汰会暂存于此，调用方每帧取回并释放
    std::vector<std::pair<CacheType, CacheEntry>> getAndClearPendingEvicted();
    
    // 获取当前使用量
    uint64_t getCurrentUsage(CacheType type) const;
    uint64_t getTotalUsage() const;
    
    // 获取统计信息
    struct Statistics {
        uint64_t metadataUsage = 0;
        uint64_t waveformUsage = 0;
        uint64_t coverUsage = 0;
        uint64_t totalUsage = 0;
        size_t metadataCount = 0;
        size_t waveformCount = 0;
        size_t coverCount = 0;
    };
    Statistics getStatistics() const;

private:
    BudgetConfig m_config;
    
    // 每个类型的缓存条目映射
    std::unordered_map<std::string, CacheEntry> m_metadataEntries;
    std::unordered_map<std::string, CacheEntry> m_waveformEntries;
    std::unordered_map<std::string, CacheEntry> m_coverEntries;
    
    // 当前使用量（字节）
    uint64_t m_metadataUsage = 0;
    uint64_t m_waveformUsage = 0;
    uint64_t m_coverUsage = 0;
    
    mutable std::mutex m_mutex;
    uint64_t m_currentFrame = 0;
    std::vector<std::pair<CacheType, CacheEntry>> m_pendingEvicted;

    // 获取指定类型的条目映射
    std::unordered_map<std::string, CacheEntry>& getEntries(CacheType type);
    const std::unordered_map<std::string, CacheEntry>& getEntries(CacheType type) const;
    uint64_t& getUsage(CacheType type);
    const uint64_t& getUsage(CacheType type) const;
    uint64_t getBudget(CacheType type) const;
};
