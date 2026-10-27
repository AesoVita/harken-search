#pragma once
// SoundManager - UCS 分类系统模块
// UCSCategory.h - Universal Category System (UCS) 分类定义和查询
// UCS category definitions used by tagging helpers.

#include <string>
#include <vector>
#include <unordered_map>
#include "FeatureExtractor.h"

// UCS 分类条目

struct UCSEntry {
    std::string category;       // 主分类 (e.g. "AMBIENCE")
    std::string subCategory;    // 子分类 (e.g. "FOREST")
    std::string catId;          // 分类ID (e.g. "AMBForst")
    std::string catShort;       // 短码 (e.g. "AMB")
    std::string explanation;    // 解释说明
    std::string synonyms;       // 同义词 (逗号分隔)

    // 多语言翻译
    std::string category_zh;    // 中文主分类
    std::string subCategory_zh; // 中文子分类
    std::string category_ja;    // 日文主分类
    std::string subCategory_ja; // 日文子分类
};

// UCS 主分类节点 (树形结构)

struct UCSCategoryNode {
    std::string name;           // 分类名称
    std::string catShort;       // 短码
    std::string name_zh;        // 中文名
    int totalCount = 0;         // 此分类下的条目总数
    std::vector<UCSEntry> entries;       // 子分类条目
    std::vector<UCSCategoryNode> children; // 子节点 (备用)
};

// UCS 分类管理器

// @stage7-contract
// API contract: UCSCategoryManager owns static built-in/custom registries and
// must be initialized before queries. Query vectors contain non-owning pointers
// valid until the next registry mutation; value-returning strings/vectors are
// owned. Custom-entry mutations are synchronous owner-thread operations and
// preserve the built-in dataset.
class UCSCategoryManager {
public:
    // 初始化 - 加载内置分类数据
    static void initialize();
    
    // 查询
    static const std::vector<UCSEntry>& getAllEntries();
    static const std::vector<UCSCategoryNode>& getCategoryTree();
    
    // 搜索分类 (支持模糊匹配, 中英文)
    static std::vector<const UCSEntry*> search(const std::string& query, int maxResults = 50);
    
    // 按主分类获取子分类列表
    static std::vector<const UCSEntry*> getByCategory(const std::string& category);
    
    // 按 CatID 查找
    static const UCSEntry* getByCatId(const std::string& catId);
    
    // 按 CatShort 获取主分类名
    static std::string getCategoryByCatShort(const std::string& catShort);
    
    // 获取所有不重复的主分类列表
    static std::vector<std::string> getMainCategories();
    
    // 根据同义词反查分类
    static std::vector<const UCSEntry*> findBySynonym(const std::string& synonym);
    
    // 自动分类建议 (根据文件名或描述)
    static std::vector<const UCSEntry*> suggestCategory(const std::string& text, int maxResults = 5);

// 基于音频特征的自动 UCS 分类推断 (F-028-ext)
    // 根据 48 维特征向量推断最可能的 UCS 主分类和子分类（返回 Top-3 候选）
    // 算法：启发式规则映射 spectral/energy/pitch 特征到 UCS 类别
    static std::vector<const UCSEntry*> suggestCategoryFromFeatures(
        const AudioFeatureVector& features, int maxResults = 3);

    // 获取分类的中文翻译
    static std::string getCategoryZh(const std::string& category);
    static std::string getSubCategoryZh(const std::string& subCategory);

    // 状态
    static bool isInitialized();
    static int getEntryCount();

// 版本管理 (F-014)
    /// 返回当前加载的 UCS 数据版本号（如 "9.1.0"）
    static std::string getVersion();
    /// 检查是否有新增/废弃的分类，返回变更描述列表
    static std::vector<std::string> checkVersionChanges(const std::string& oldVersion);

// 用户自定义分类扩展 (F-014)
    /// 添加用户私有分类条目（不影响内置 UCS 条目，重启保留需持久化）
    static void addCustomEntry(const UCSEntry& entry);
    /// 移除用户自定义分类
    static bool removeCustomEntry(const std::string& catId);
    /// 获取所有用户自定义分类
    static const std::vector<UCSEntry>& getCustomEntries();

private:
    static void buildCategoryTree();
    static bool containsIgnoreCase(const std::string& haystack, const std::string& needle);
    static std::string toLowerStr(const std::string& s);
};
