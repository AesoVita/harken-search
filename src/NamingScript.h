#pragma once
// SoundManager - 文件命名脚本模块
// NamingScript.h - 模板化文件命名系统
// Template-based file naming helpers for delivery workflows.

#include <string>
#include <vector>
#include <unordered_map>

// @stage7-contract
// API contract: naming helpers consume value metadata/templates and return owned
// strings/tokens. They do not perform file I/O or retain caller containers;
// malformed tokens remain explicit validation errors.

// Naming Template Token

struct NamingToken {
    enum class Type {
        Literal,    // 固定文本
        Field,      // 元数据字段引用 <FieldName>
        FieldWords  // 元数据字段取前N个词 <FieldName:N>
    };
    
    Type type = Type::Literal;
    std::string text;           // Literal 内容或字段名
    int wordCount = 0;          // FieldWords 时取几个词 (0=全部)
};

// Naming Preset

struct NamingPreset {
    std::string name;           // 预设名称
    std::string pattern;        // 模板字符串 (如 "<Description:1>_<Source:2>")
    int maxLength = 255;        // 最大文件名长度
    std::string description;    // 说明
};

// Naming Script Engine

class NamingScriptEngine {
public:
    // 初始化 - 加载内置预设
    static void initialize();
    
// 模板解析和执行
    
    // 解析模板字符串为 token 列表
    // 格式: <FieldName> 或 <FieldName:N> 取前 N 个词
    // 其他文字作为 Literal
    static std::vector<NamingToken> parseTemplate(const std::string& pattern);
    
    // 用元数据填充模板，生成文件名
    // metadata: field name -> value
    // maxLength: 最大字符数 (0=不限)
    static std::string applyTemplate(const std::string& pattern,
                                      const std::unordered_map<std::string, std::string>& metadata,
                                      int maxLength = 255);
    
    // 从 token 列表生成文件名
    static std::string applyTokens(const std::vector<NamingToken>& tokens,
                                    const std::unordered_map<std::string, std::string>& metadata,
                                    int maxLength = 255);
    
// 文件名清理
    
    // 移除/替换文件名中的非法字符
    static std::string sanitizeFilename(const std::string& name);
    
    // 移除文件名中的非法路径字符
    static std::string sanitizePath(const std::string& path);
    
    // 截断到指定长度 (Unicode安全)
    static std::string truncateToLength(const std::string& name, int maxLength);
    
// 取前N个词
    
    // 从字段值中取前 N 个词
    static std::string takeFirstWords(const std::string& value, int n);
    
// 内置预设
    
    static const std::vector<NamingPreset>& getPresets();
    static const NamingPreset* getPreset(const std::string& name);
    
// 自定义预设管理
    
    static void addPreset(const NamingPreset& preset);
    static void removePreset(const std::string& name);
    
// 冲突解决 (F-012)

    /// 生成无冲突的文件名：如果 targetDir/baseName.ext 已存在，
    /// 自动追加后缀 _1, _2, ... 直到不冲突（最多尝试 999 次）。
    /// 返回最终文件名（不含目录），空字符串表示无法解决。
    static std::string resolveConflict(const std::string& targetDir,
                                        const std::string& baseName,
                                        const std::string& extension,
                                        int maxAttempts = 999);

    // 状态
    static bool isInitialized();
};
