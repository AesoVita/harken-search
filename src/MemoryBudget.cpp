#include "MemoryBudget.h"
#include <algorithm>

MemoryBudget::MemoryBudget() {
    // 默认配置已在头文件中定义
}

MemoryBudget::~MemoryBudget() {
    // 清理所有条目
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metadataEntries.clear();
    m_waveformEntries.clear();
    m_coverEntries.clear();
}

void MemoryBudget::setBudgetConfig(const BudgetConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

bool MemoryBudget::registerEntry(CacheType type, const std::string& key, uint64_t sizeBytes, void* userData) {
    if (key.empty() || sizeBytes == 0) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto& entries = getEntries(type);
    auto& usage = getUsage(type);
    uint64_t budget = getBudget(type);
    
    // 检查是否已存在
    if (entries.find(key) != entries.end()) {
        // 更新现有条目
        CacheEntry& entry = entries[key];
        usage -= entry.sizeBytes;  // 减去旧大小
        entry.sizeBytes = sizeBytes;
        entry.userData = userData;
        entry.lastUsedFrame = 0;  // 将在updateUsage中设置
        usage += sizeBytes;
        return true;
    }
    
     // 检查单类型预算
    if (usage + sizeBytes > budget) {
        uint64_t requiredBytes = (usage + sizeBytes) - budget;
        auto toEvict = evictLRU(type, requiredBytes);
        for (auto& e : toEvict) m_pendingEvicted.push_back({type, std::move(e)});
        if (usage + sizeBytes > budget) return false;
    }
    
    // 总预算硬约束：先按总预算跨类型淘汰再注册
    uint64_t totalUsage = m_metadataUsage + m_waveformUsage + m_coverUsage;
    while (totalUsage + sizeBytes > m_config.totalBudget) {
        uint64_t need = (totalUsage + sizeBytes) - m_config.totalBudget;
        bool freed = false;
        for (CacheType t : {CacheType::Waveform, CacheType::Cover, CacheType::Metadata}) {
            if (getUsage(t) == 0) continue;
            uint64_t toFree = (std::min)(need, getUsage(t));
            auto ev = evictLRU(t, toFree);
            for (auto& e : ev) { m_pendingEvicted.push_back({t, std::move(e)}); freed = true; }
            totalUsage = m_metadataUsage + m_waveformUsage + m_coverUsage;
            if (totalUsage + sizeBytes <= m_config.totalBudget) break;
        }
        if (!freed) return false;
    }
    
    // 添加新条目
    CacheEntry entry;
    entry.key = key;
    entry.sizeBytes = sizeBytes;
    entry.lastUsedFrame = 0;
    entry.userData = userData;
    entries[key] = entry;
    usage += sizeBytes;
    
    return true;
}

void MemoryBudget::setCurrentFrame(uint64_t frameNumber) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFrame = frameNumber;
}

void MemoryBudget::updateUsage(CacheType type, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto& entries = getEntries(type);
    auto it = entries.find(key);
    if (it != entries.end())
        it->second.lastUsedFrame = m_currentFrame;
}

void MemoryBudget::unregisterEntry(CacheType type, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto& entries = getEntries(type);
    auto& usage = getUsage(type);
    
    auto it = entries.find(key);
    if (it != entries.end()) {
        usage -= it->second.sizeBytes;
        entries.erase(it);
    }
}

bool MemoryBudget::canAdd(CacheType type, uint64_t sizeBytes) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const auto& usage = getUsage(type);
    uint64_t budget = getBudget(type);
    
    return (usage + sizeBytes) <= budget;
}

std::vector<std::pair<MemoryBudget::CacheType, MemoryBudget::CacheEntry>> MemoryBudget::getAndClearPendingEvicted() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<CacheType, CacheEntry>> out = std::move(m_pendingEvicted);
    m_pendingEvicted.clear();
    return out;
}

std::vector<MemoryBudget::CacheEntry> MemoryBudget::evictLRU(CacheType type, uint64_t requiredBytes) {
    std::vector<CacheEntry> evicted;

    auto& entries = getEntries(type);
    auto& usage = getUsage(type);

    if (entries.empty() || requiredBytes == 0) return evicted;

    std::vector<std::pair<std::string, CacheEntry*>> sortedEntries;
    sortedEntries.reserve(entries.size());
    for (auto& [key, entry] : entries) {
        sortedEntries.push_back({key, &entry});
    }

    // Reserve once, then sort by last use for deterministic LRU eviction.
    std::sort(sortedEntries.begin(), sortedEntries.end(),
        [](const auto& a, const auto& b) {
            return a.second->lastUsedFrame < b.second->lastUsedFrame;
        });

    // 淘汰最久未使用的条目，直到释放足够的空间
    uint64_t freedBytes = 0;
    for (const auto& [key, entryPtr] : sortedEntries) {
        if (freedBytes >= requiredBytes) break;

        evicted.push_back(*entryPtr);
        freedBytes += entryPtr->sizeBytes;
        usage -= entryPtr->sizeBytes;
        entries.erase(key);
    }

    return evicted;
}

uint64_t MemoryBudget::getCurrentUsage(CacheType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getUsage(type);
}

uint64_t MemoryBudget::getTotalUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataUsage + m_waveformUsage + m_coverUsage;
}

MemoryBudget::Statistics MemoryBudget::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Statistics stats;
    stats.metadataUsage = m_metadataUsage;
    stats.waveformUsage = m_waveformUsage;
    stats.coverUsage = m_coverUsage;
    stats.totalUsage = stats.metadataUsage + stats.waveformUsage + stats.coverUsage;
    stats.metadataCount = m_metadataEntries.size();
    stats.waveformCount = m_waveformEntries.size();
    stats.coverCount = m_coverEntries.size();
    
    return stats;
}

std::unordered_map<std::string, MemoryBudget::CacheEntry>& MemoryBudget::getEntries(CacheType type) {
    switch (type) {
        case CacheType::Metadata: return m_metadataEntries;
        case CacheType::Waveform: return m_waveformEntries;
        case CacheType::Cover: return m_coverEntries;
        default: return m_metadataEntries;
    }
}

const std::unordered_map<std::string, MemoryBudget::CacheEntry>& MemoryBudget::getEntries(CacheType type) const {
    switch (type) {
        case CacheType::Metadata: return m_metadataEntries;
        case CacheType::Waveform: return m_waveformEntries;
        case CacheType::Cover: return m_coverEntries;
        default: return m_metadataEntries;
    }
}

uint64_t& MemoryBudget::getUsage(CacheType type) {
    switch (type) {
        case CacheType::Metadata: return m_metadataUsage;
        case CacheType::Waveform: return m_waveformUsage;
        case CacheType::Cover: return m_coverUsage;
        default: return m_metadataUsage;
    }
}

const uint64_t& MemoryBudget::getUsage(CacheType type) const {
    switch (type) {
        case CacheType::Metadata: return m_metadataUsage;
        case CacheType::Waveform: return m_waveformUsage;
        case CacheType::Cover: return m_coverUsage;
        default: return m_metadataUsage;
    }
}

uint64_t MemoryBudget::getBudget(CacheType type) const {
    switch (type) {
        case CacheType::Metadata: return m_config.metadataBudget;
        case CacheType::Waveform: return m_config.waveformBudget;
        case CacheType::Cover: return m_config.coverBudget;
        default: return m_config.metadataBudget;
    }
}
