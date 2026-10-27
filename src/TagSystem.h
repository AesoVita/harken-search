#pragma once
// SoundManager - 标签/Facet 过滤系统模块
// TagSystem.h - 标签定义、自动生成、Facet 过滤
// Tag extraction and facet generation helpers for library workflows.

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>

// @stage7-contract
// API contract: tag/facet helpers own returned strings and containers, preserve
// TagDimension meanings, and do not perform database or UI mutation. Callers
// provide value metadata and handle empty/unknown tags explicitly.

// Tag Dimension (分面维度)

enum class TagDimension {
    Category,           // 音乐风格
    Mood,               // 情绪
    FeaturedInstrument, // 乐器
    Tempo,              // 速度
    Usage,              // 用途
    Version,            // 版本
    Era,                // 年代
    Duration,           // 时长 (自动生成)
    BPMRange,           // BPM范围 (自动生成)
    COUNT
};

// Tag Entry

struct TagEntry {
    std::string value;          // 标签值 (英文)
    std::string value_zh;       // 中文翻译
    TagDimension dimension;     // 所属维度
    int fileCount = 0;          // 关联文件数 (运行时统计)
};

// Facet Filter State

struct FacetFilterState {
    TagDimension dimension;
    std::set<std::string> selectedValues;  // 选中的标签值
    bool expanded = false;                  // UI 是否展开
    
    bool hasFilter() const { return !selectedValues.empty(); }
    void clear() { selectedValues.clear(); }
    void toggle(const std::string& val) {
        if (selectedValues.count(val)) selectedValues.erase(val);
        else selectedValues.insert(val);
    }
    bool isSelected(const std::string& val) const { return selectedValues.count(val) > 0; }
};

// Auto-Tag Results

struct AutoTagResult {
    std::string field;      // 写入哪个标签字段
    std::string value;      // 标签值
};

// Smart tag suggestion with confidence score
struct TagSuggestion {
    std::string value;          // Tag value (English)
    std::string value_zh;       // Chinese translation
    TagDimension dimension;     // Dimension
    float confidence;           // 0.0-1.0 confidence score
};

// Tag System Manager

class TagSystemManager {
public:
    // 初始化 - 加载内置标签数据
    static void initialize();
    
    // 获取所有标签
    static const std::vector<TagEntry>& getAllTags();
    
    // 获取某个维度的所有标签
    static std::vector<const TagEntry*> getTagsByDimension(TagDimension dim);
    
    // 获取维度的显示名称
    static const char* getDimensionName(TagDimension dim);
    static const char* getDimensionNameZh(TagDimension dim);
    
    // 获取所有维度
    static std::vector<TagDimension> getAllDimensions();
    
    // Facet 过滤
    static FacetFilterState& getFacetState(TagDimension dim);
    static bool hasAnyFacetFilter();
    static void clearAllFacets();
    
    // 检查一条记录是否满足当前 Facet 过滤条件
    // fields: {"Category" -> "Rock", "Mood" -> "Epic,Dark", ...}
    static bool matchesFacetFilter(const std::unordered_map<std::string, std::string>& fields);
    
// 自动标签生成 (参考 maketags.lua)
    
    // 根据 BPM 值自动生成 Tempo 标签
    static std::string autoTempoFromBPM(const std::string& bpmStr);
    
    // 根据时长自动生成 Duration 标签
    static std::string autoDurationTag(float durationSeconds);
    
    // 根据 BPM 值自动生成 BPM Range 标签
    static std::string autoBPMRangeTag(const std::string& bpmStr);
    
    // 从逗号分隔的字段值拆分为标签列表
    static std::vector<std::string> splitTags(const std::string& value, char delimiter = ',');
    
    // 合并标签列表为逗号分隔的字符串 (去重)
    static std::string joinTags(const std::vector<std::string>& tags, const std::string& separator = ", ");
    
    // 根据元数据自动生成所有可能的标签
    // metadata: field name -> value
    static std::vector<AutoTagResult> autoGenerateTags(const std::unordered_map<std::string, std::string>& metadata);
    
    // 标签搜索
    static std::vector<const TagEntry*> searchTags(const std::string& query, int maxResults = 20);

// 智能标签推荐 (F-007 + F-015)
    // Suggest tags based on text analysis of filename, description, keywords, transcription.
    // Returns ranked suggestions with confidence scores.
    static std::vector<TagSuggestion> suggestTagsFromText(
        const std::string& filename,
        const std::string& description,
        const std::string& keywords,
        int maxResults = 10);

    // 状态
    static bool isInitialized();
    
private:
    static void addTag(const char* value, const char* value_zh, TagDimension dim);
};
