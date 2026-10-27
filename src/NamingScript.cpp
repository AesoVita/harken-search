// SoundManager - 文件命名脚本模块
// NamingScript.cpp - 模板解析、文件名生成实现
// File naming preset definitions for delivery workflows.

#include "NamingScript.h"
#include "platform/filesystem.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <string>

// Static Data

static bool s_namingInitialized = false;
static std::vector<NamingPreset> s_presets;

// Initialize

void NamingScriptEngine::initialize() {
    if (s_namingInitialized) return;
    
    // Built-in presets for delivery-oriented naming templates.
    s_presets.push_back({
        "Default",
        "<Description:1>_<Source:2>",
        31,
        "First word of description + first two words of source"
    });
    s_presets.push_back({
        "Full Description",
        "<Description>",
        255,
        "Full description as filename"
    });
    s_presets.push_back({
        "Category + Description",
        "<Category>_<Description:3>",
        64,
        "Category followed by first three words of description"
    });
    s_presets.push_back({
        "Library - FXName",
        "<Library> - <FXName>",
        128,
        "Library name dash FX name"
    });
    s_presets.push_back({
        "CatID_Description",
        "<CatID>_<Description:2>",
        31,
        "UCS Category ID + first two words of description"
    });
    s_presets.push_back({
        "Show_Scene_Take",
        "<Show>_<Scene>_<Take>",
        64,
        "Production naming: Show Scene Take"
    });
    s_presets.push_back({
        "Artist - Title",
        "<Artist> - <TrackTitle>",
        128,
        "Music naming: Artist dash Title"
    });
    s_presets.push_back({
        "Library_Category_SubCategory_Description",
        "<Library>_<Category>_<SubCategory>_<Description:2>",
        128,
        "Full library path style naming"
    });
    s_presets.push_back({
        "Filename Only",
        "<Filename>",
        255,
        "Keep original filename"
    });
    s_presets.push_back({
        "ShortID_Description",
        "<ShortID>_<Description:3>",
        64,
        "Short ID code + description words"
    });
    // UCS Standard: Category_SubCategory_Description_Library_Designer
    // Follows UCS v9 naming convention for game audio delivery
    s_presets.push_back({
        "UCS Standard",
        "<Category>_<SubCategory>_<Description:3>_<Library>_<Designer>",
        200,
        "UCS v9 standard: Category_SubCategory_Description_Library_Designer"
    });
    s_presets.push_back({
        "UCS Compact",
        "<CatID>_<Description:3>_<Designer>",
        100,
        "UCS compact: CatID_Description_Designer (shorter, DAW-friendly)"
    });

    s_namingInitialized = true;
}

// Template Parsing

std::vector<NamingToken> NamingScriptEngine::parseTemplate(const std::string& pattern) {
    std::vector<NamingToken> tokens;
    
    size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == '<') {
            // Find closing >
            size_t close = pattern.find('>', i);
            if (close == std::string::npos) {
                // No closing bracket, treat as literal
                NamingToken t;
                t.type = NamingToken::Type::Literal;
                t.text = pattern.substr(i);
                tokens.push_back(t);
                break;
            }
            
            std::string fieldSpec = pattern.substr(i + 1, close - i - 1);
            NamingToken t;
            
            // Check for :N suffix
            size_t colonPos = fieldSpec.find(':');
            if (colonPos != std::string::npos) {
                t.type = NamingToken::Type::FieldWords;
                t.text = fieldSpec.substr(0, colonPos);
                try {
                    t.wordCount = std::stoi(fieldSpec.substr(colonPos + 1));
                } catch (...) {
                    t.wordCount = 0;
                }
            } else {
                t.type = NamingToken::Type::Field;
                t.text = fieldSpec;
            }
            
            tokens.push_back(t);
            i = close + 1;
        } else {
            // Literal text until next <
            size_t next = pattern.find('<', i);
            if (next == std::string::npos) next = pattern.size();
            
            NamingToken t;
            t.type = NamingToken::Type::Literal;
            t.text = pattern.substr(i, next - i);
            tokens.push_back(t);
            i = next;
        }
    }
    
    return tokens;
}

// Take First N Words

std::string NamingScriptEngine::takeFirstWords(const std::string& value, int n) {
    if (n <= 0 || value.empty()) return value;
    
    std::istringstream iss(value);
    std::string result;
    std::string word;
    int count = 0;
    
    while (iss >> word && count < n) {
        if (!result.empty()) result += " ";
        result += word;
        count++;
    }
    
    return result;
}

// Apply Template

std::string NamingScriptEngine::applyTokens(const std::vector<NamingToken>& tokens,
                                              const std::unordered_map<std::string, std::string>& metadata,
                                              int maxLength) {
    std::string result;
    
    for (const auto& token : tokens) {
        switch (token.type) {
            case NamingToken::Type::Literal:
                result += token.text;
                break;
                
            case NamingToken::Type::Field: {
                auto it = metadata.find(token.text);
                if (it != metadata.end() && !it->second.empty()) {
                    result += it->second;
                }
                break;
            }
            
            case NamingToken::Type::FieldWords: {
                auto it = metadata.find(token.text);
                if (it != metadata.end() && !it->second.empty()) {
                    result += takeFirstWords(it->second, token.wordCount);
                }
                break;
            }
        }
    }
    
    // Sanitize and truncate
    result = sanitizeFilename(result);
    if (maxLength > 0) {
        result = truncateToLength(result, maxLength);
    }
    
    return result;
}

std::string NamingScriptEngine::applyTemplate(const std::string& pattern,
                                               const std::unordered_map<std::string, std::string>& metadata,
                                               int maxLength) {
    auto tokens = parseTemplate(pattern);
    return applyTokens(tokens, metadata, maxLength);
}

// Filename Sanitization

std::string NamingScriptEngine::sanitizeFilename(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    
    for (size_t i = 0; i < name.size(); i++) {
        unsigned char c = (unsigned char)name[i];
        
        // Skip illegal filename characters
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            result += '_';
        } else if (c < 32) {
            // Skip control characters
        } else {
            result += (char)c;
        }
    }
    
    // Trim trailing spaces and dots (Windows restriction)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    
    // Trim leading spaces
    size_t start = result.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    if (start > 0) result = result.substr(start);
    
    return result;
}

std::string NamingScriptEngine::sanitizePath(const std::string& path) {
    std::string result;
    result.reserve(path.size());
    
    for (size_t i = 0; i < path.size(); i++) {
        unsigned char c = (unsigned char)path[i];
        
        // Allow path separators but clean other illegals
        if (c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            result += '_';
        } else if (c < 32) {
            // Skip control characters
        } else {
            result += (char)c;
        }
    }
    
    return result;
}

std::string NamingScriptEngine::truncateToLength(const std::string& name, int maxLength) {
    if (maxLength <= 0 || (int)name.size() <= maxLength) return name;
    
    // UTF-8 safe truncation: don't cut in the middle of multi-byte sequences
    int len = 0;
    int bytePos = 0;
    
    while (bytePos < (int)name.size() && len < maxLength) {
        unsigned char c = (unsigned char)name[bytePos];
        int charBytes = 1;
        
        if (c >= 0xF0)      charBytes = 4;
        else if (c >= 0xE0) charBytes = 3;
        else if (c >= 0xC0) charBytes = 2;
        
        if (bytePos + charBytes > (int)name.size()) break;
        bytePos += charBytes;
        len++;
    }
    
    return name.substr(0, bytePos);
}

// Presets

const std::vector<NamingPreset>& NamingScriptEngine::getPresets() {
    return s_presets;
}

const NamingPreset* NamingScriptEngine::getPreset(const std::string& name) {
    for (const auto& p : s_presets) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

void NamingScriptEngine::addPreset(const NamingPreset& preset) {
    // Replace if exists
    for (auto& p : s_presets) {
        if (p.name == preset.name) { p = preset; return; }
    }
    s_presets.push_back(preset);
}

void NamingScriptEngine::removePreset(const std::string& name) {
    s_presets.erase(
        std::remove_if(s_presets.begin(), s_presets.end(),
                        [&](const NamingPreset& p) { return p.name == name; }),
        s_presets.end());
}

bool NamingScriptEngine::isInitialized() {
    return s_namingInitialized;
}

// 冲突解决 (F-012)
std::string NamingScriptEngine::resolveConflict(const std::string& targetDir,
                                                 const std::string& baseName,
                                                 const std::string& extension,
                                                 int maxAttempts) {
    // First check: no conflict
    std::string candidate = baseName + extension;
    std::string fullPath = targetDir.empty() ? candidate : (targetDir + "/" + candidate);

    auto pathExists = [](const std::string& utf8Path) {
        return Platform::FileSystem::ProbePath(utf8Path) ==
               Platform::FileSystem::PathStatus::Present;
    };

    if (!pathExists(fullPath)) return candidate;

    // Conflict: try _1, _2, ...
    for (int i = 1; i <= maxAttempts; ++i) {
        candidate = baseName + "_" + std::to_string(i) + extension;
        fullPath = targetDir.empty() ? candidate : (targetDir + "/" + candidate);
        if (!pathExists(fullPath)) return candidate;
    }

    return {}; // Could not resolve
}
