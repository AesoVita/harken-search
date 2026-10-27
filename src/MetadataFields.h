#pragma once
// SoundManager - 元数据字段体系模块
// MetadataFields.h - 定义完整的元数据字段表、模板和验证器
// Shared metadata field definitions for BWF/iXML/UCS-friendly workflows.

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// Field Data Types

enum class FieldType {
    String,     // 文本字段
    Integer,    // 整数
    Real,       // 浮点数
    DateTime,   // 日期时间
    Blob        // 二进制 (歌词等)
};

// Field Definition

struct FieldDef {
    std::string name;           // 字段名 (英文标识符)
    std::string displayName;    // 显示名称
    std::string displayName_zh; // 中文显示名
    FieldType type = FieldType::String;
    int maxLength = 255;        // 最大长度 (String 类型)
    bool indexed = false;       // 是否建全文索引
    bool filterable = false;    // 是否可在 Filter Browser 中使用
    bool nullable = true;       // 是否可空
    std::string group;          // 所属分组 (Basic/Music/SFX/Production/iXML/ID3)
    std::string description;    // 字段说明
};

// Database Template

enum class DatabaseTemplate {
    Music,          // 音乐模板
    SoundEffects,   // 音效模板
    Dialog,         // 对白模板
    MusicAndFX,     // 音乐+音效综合模板
    Universal       // 通用模板 (所有字段)
};

struct DatabaseTemplateDef {
    DatabaseTemplate type;
    std::string name;
    std::string description;
    std::vector<std::string> fieldNames;      // 此模板包含的字段
    std::vector<std::string> indexedFields;    // 全文索引字段
    std::vector<std::string> filterBrowser;    // Filter Browser 字段
    std::vector<std::string> rdbFields;        // 关系数据库字段 (用于去重/聚合)
};

// ISRC Validator

struct ISRCValidation {
    bool valid = false;
    std::string cleaned;    // 去掉连字符后的 12 位
    std::string error;
};

// MetadataFields Manager

// @stage7-contract
// API contract: MetadataFieldManager owns static field/template registries
// initialized explicitly by the application. Query results either borrow
// registry entries (pointer/reference APIs) or return owned values; validation
// helpers are synchronous and side-effect free. Callers must initialize before
// querying and must not mutate returned registry objects.
class MetadataFieldManager {
public:
    // 初始化 - 注册所有字段定义
    static void initialize();

    // 获取所有字段定义
    static const std::vector<FieldDef>& getAllFields();
    
    // 按名称查找字段
    static const FieldDef* getField(const std::string& name);
    
    // 按分组获取字段列表
    static std::vector<const FieldDef*> getFieldsByGroup(const std::string& group);
    
    // 获取所有可用的字段名列表 (给 UI 下拉框用)
    static std::vector<std::string> getStringFieldNames();
    
    // 获取模板定义
    static const DatabaseTemplateDef* getTemplate(DatabaseTemplate type);
    static std::vector<DatabaseTemplate> getAllTemplateTypes();
    static std::string getTemplateName(DatabaseTemplate type);
    
    // 获取字段的最大长度表 (name -> maxLength)
    static const std::unordered_map<std::string, int>& getFieldLengthTable();
    
    // 验证器
    static ISRCValidation validateISRC(const std::string& isrc);
    static bool validateBPM(const std::string& bpm);
    static bool validateYear(const std::string& year);
    
    // 状态
    static bool isInitialized();
    static int getFieldCount();
    
private:
    static void registerField(const char* name, const char* display, const char* display_zh,
                              FieldType type, int maxLen, bool indexed, bool filterable,
                              const char* group, const char* desc = "");
    static void buildTemplates();
};
