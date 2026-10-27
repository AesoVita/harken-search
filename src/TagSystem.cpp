// SoundManager - 标签/Facet 过滤系统模块
// TagSystem.cpp - 标签数据、自动标签生成、Facet 过滤实现
// Tag extraction and facet generation helpers for library workflows.

#include "TagSystem.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

// Static Data

static bool s_tagInitialized = false;
static std::vector<TagEntry> s_allTags;
static FacetFilterState s_facetStates[(int)TagDimension::COUNT];

// Helper

void TagSystemManager::addTag(const char* value, const char* value_zh, TagDimension dim) {
    TagEntry t;
    t.value = value;
    t.value_zh = value_zh;
    t.dimension = dim;
    s_allTags.push_back(std::move(t));
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return r;
}

static std::string trimStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Initialize

void TagSystemManager::initialize() {
    if (s_tagInitialized) return;
    s_allTags.reserve(300);
    
    // Initialize facet states
    for (int i = 0; i < (int)TagDimension::COUNT; i++) {
        s_facetStates[i].dimension = (TagDimension)i;
    }

// Category (音乐风格)
    addTag("Ambient",      "氛围",     TagDimension::Category);
    addTag("Ballad",       "民谣",     TagDimension::Category);
    addTag("Blues",        "蓝调",     TagDimension::Category);
    addTag("Classical",    "古典",     TagDimension::Category);
    addTag("Contemporary", "当代",     TagDimension::Category);
    addTag("Country",      "乡村",     TagDimension::Category);
    addTag("Dance",        "舞曲",     TagDimension::Category);
    addTag("Dance_Club",   "俱乐部舞曲", TagDimension::Category);
    addTag("Dance_Disco",  "迪斯科",   TagDimension::Category);
    addTag("Dance_Techno", "电子舞曲", TagDimension::Category);
    addTag("Dance_Trance", "恍惚电子", TagDimension::Category);
    addTag("Easy Listening","轻音乐",  TagDimension::Category);
    addTag("Electronica",  "电子",     TagDimension::Category);
    addTag("Ethnic",       "民族",     TagDimension::Category);
    addTag("Experimental", "实验",     TagDimension::Category);
    addTag("Fanfare",      "号角",     TagDimension::Category);
    addTag("Folk",         "民间",     TagDimension::Category);
    addTag("Funk",         "放克",     TagDimension::Category);
    addTag("Gospel",       "福音",     TagDimension::Category);
    addTag("Inspirational","励志",     TagDimension::Category);
    addTag("Jazz",         "爵士",     TagDimension::Category);
    addTag("Latin",        "拉丁",     TagDimension::Category);
    addTag("New Age",      "新世纪",   TagDimension::Category);
    addTag("Orchestral",   "管弦乐",   TagDimension::Category);
    addTag("Opera",        "歌剧",     TagDimension::Category);
    addTag("Pop",          "流行",     TagDimension::Category);
    addTag("R&B",          "节奏蓝调", TagDimension::Category);
    addTag("Reggae",       "雷鬼",     TagDimension::Category);
    addTag("Retro",        "复古",     TagDimension::Category);
    addTag("Rock",         "摇滚",     TagDimension::Category);
    addTag("Rock_Alternative","另类摇滚", TagDimension::Category);
    addTag("Rock_Metal",   "金属",     TagDimension::Category);
    addTag("Romantic",     "浪漫",     TagDimension::Category);
    addTag("Specialty_Children","儿童", TagDimension::Category);
    addTag("Specialty_Comedy","喜剧",  TagDimension::Category);
    addTag("Specialty_Holiday","节日", TagDimension::Category);
    addTag("Specialty_Horror","恐怖",  TagDimension::Category);
    addTag("Specialty_News","新闻",    TagDimension::Category);
    addTag("Sports",       "体育",     TagDimension::Category);
    addTag("Surf",         "冲浪",     TagDimension::Category);
    addTag("Underscore",   "配乐",     TagDimension::Category);
    addTag("Urban",        "都市",     TagDimension::Category);
    addTag("Urban_Hip Hop","嘻哈",     TagDimension::Category);
    addTag("World",        "世界",     TagDimension::Category);

// Mood (情绪)
    addTag("Action",       "动作",     TagDimension::Mood);
    addTag("Adventure",    "冒险",     TagDimension::Mood);
    addTag("Aggressive",   "激进",     TagDimension::Mood);
    addTag("Ambient",      "环境",     TagDimension::Mood);
    addTag("Angry",        "愤怒",     TagDimension::Mood);
    addTag("Anticipation", "期待",     TagDimension::Mood);
    addTag("Anxious",      "焦虑",     TagDimension::Mood);
    addTag("Atmospheric",  "氛围的",   TagDimension::Mood);
    addTag("Bouncy",       "欢快",     TagDimension::Mood);
    addTag("Bright",       "明亮",     TagDimension::Mood);
    addTag("Building",     "渐强",     TagDimension::Mood);
    addTag("Calm",         "平静",     TagDimension::Mood);
    addTag("Celebratory",  "庆祝",     TagDimension::Mood);
    addTag("Cheerful",     "欢乐",     TagDimension::Mood);
    addTag("Comedy",       "喜剧",     TagDimension::Mood);
    addTag("Confident",    "自信",     TagDimension::Mood);
    addTag("Cool",         "酷",       TagDimension::Mood);
    addTag("Dark",         "黑暗",     TagDimension::Mood);
    addTag("Determined",   "坚定",     TagDimension::Mood);
    addTag("Dramatic",     "戏剧性",   TagDimension::Mood);
    addTag("Driving",      "驱动",     TagDimension::Mood);
    addTag("Dynamic",      "动态",     TagDimension::Mood);
    addTag("Edgy",         "前卫",     TagDimension::Mood);
    addTag("Elegant",      "优雅",     TagDimension::Mood);
    addTag("Emotional",    "情感的",   TagDimension::Mood);
    addTag("Energetic",    "活力",     TagDimension::Mood);
    addTag("Epic",         "史诗",     TagDimension::Mood);
    addTag("Ethereal",     "空灵",     TagDimension::Mood);
    addTag("Fantasy",      "幻想",     TagDimension::Mood);
    addTag("Feelgood",     "感觉良好", TagDimension::Mood);
    addTag("Flowing",      "流畅",     TagDimension::Mood);
    addTag("Fun",          "有趣",     TagDimension::Mood);
    addTag("Funky",        "放克的",   TagDimension::Mood);
    addTag("Futuristic",   "未来",     TagDimension::Mood);
    addTag("Gentle",       "温柔",     TagDimension::Mood);
    addTag("Groovy",       "律动",     TagDimension::Mood);
    addTag("Happy",        "快乐",     TagDimension::Mood);
    addTag("Heavy",        "沉重",     TagDimension::Mood);
    addTag("Hopeful",      "希望",     TagDimension::Mood);
    addTag("Intense",      "强烈",     TagDimension::Mood);
    addTag("Intimate",     "亲密",     TagDimension::Mood);
    addTag("Laid-back",    "悠闲",     TagDimension::Mood);
    addTag("Light",        "轻快",     TagDimension::Mood);
    addTag("Melodic",      "旋律的",   TagDimension::Mood);
    addTag("Mellow",       "柔和",     TagDimension::Mood);
    addTag("Moody",        "情绪化",   TagDimension::Mood);
    addTag("Mysterious",   "神秘",     TagDimension::Mood);
    addTag("Positive",     "积极",     TagDimension::Mood);
    addTag("Powerful",     "强大",     TagDimension::Mood);
    addTag("Relaxed",      "放松",     TagDimension::Mood);
    addTag("Rhythmic",     "节奏感",   TagDimension::Mood);
    addTag("Sad",          "悲伤",     TagDimension::Mood);
    addTag("Smooth",       "顺滑",     TagDimension::Mood);
    addTag("Soft",         "柔软",     TagDimension::Mood);
    addTag("Strong",       "强壮",     TagDimension::Mood);
    addTag("Suspense",     "悬疑",     TagDimension::Mood);
    addTag("Sweet",        "甜美",     TagDimension::Mood);
    addTag("Tense",        "紧张",     TagDimension::Mood);
    addTag("Tranquil",     "宁静",     TagDimension::Mood);
    addTag("Upbeat",       "乐观",     TagDimension::Mood);
    addTag("Uplifting",    "振奋",     TagDimension::Mood);
    addTag("Wild",         "狂野",     TagDimension::Mood);

// Featured Instrument (乐器)
    addTag("Acoustic",     "原声",     TagDimension::FeaturedInstrument);
    addTag("Bass",         "贝斯",     TagDimension::FeaturedInstrument);
    addTag("Brass",        "铜管",     TagDimension::FeaturedInstrument);
    addTag("Choir",        "合唱",     TagDimension::FeaturedInstrument);
    addTag("Drums",        "鼓",       TagDimension::FeaturedInstrument);
    addTag("Drums_Acoustic","原声鼓",  TagDimension::FeaturedInstrument);
    addTag("Drums_Electronic","电子鼓",TagDimension::FeaturedInstrument);
    addTag("Flute",        "长笛",     TagDimension::FeaturedInstrument);
    addTag("Guitar",       "吉他",     TagDimension::FeaturedInstrument);
    addTag("Guitar_Acoustic","木吉他", TagDimension::FeaturedInstrument);
    addTag("Guitar_Electric","电吉他", TagDimension::FeaturedInstrument);
    addTag("Harmonica",    "口琴",     TagDimension::FeaturedInstrument);
    addTag("Horn",         "号",       TagDimension::FeaturedInstrument);
    addTag("Keyboard",     "键盘",     TagDimension::FeaturedInstrument);
    addTag("Organ",        "管风琴",   TagDimension::FeaturedInstrument);
    addTag("Percussion",   "打击乐",   TagDimension::FeaturedInstrument);
    addTag("Piano",        "钢琴",     TagDimension::FeaturedInstrument);
    addTag("Sax",          "萨克斯",   TagDimension::FeaturedInstrument);
    addTag("Strings",      "弦乐",     TagDimension::FeaturedInstrument);
    addTag("Synth",        "合成器",   TagDimension::FeaturedInstrument);
    addTag("Vocals",       "人声",     TagDimension::FeaturedInstrument);
    addTag("Wind",         "木管",     TagDimension::FeaturedInstrument);

// Tempo (速度)
    addTag("Very Fast",    "非常快",   TagDimension::Tempo);
    addTag("Fast",         "快",       TagDimension::Tempo);
    addTag("Medium Fast",  "中快",     TagDimension::Tempo);
    addTag("Medium",       "中等",     TagDimension::Tempo);
    addTag("Medium Slow",  "中慢",     TagDimension::Tempo);
    addTag("Slow",         "慢",       TagDimension::Tempo);
    addTag("Very Slow",    "非常慢",   TagDimension::Tempo);

// Usage (用途)
    addTag("Cooking",      "烹饪",     TagDimension::Usage);
    addTag("Corporate",    "企业",     TagDimension::Usage);
    addTag("Children",     "儿童",     TagDimension::Usage);
    addTag("Crime",        "犯罪",     TagDimension::Usage);
    addTag("Dancing",      "舞蹈",     TagDimension::Usage);
    addTag("Documentary",  "纪录片",   TagDimension::Usage);
    addTag("Driving",      "驾驶",     TagDimension::Usage);
    addTag("Drama",        "剧情",     TagDimension::Usage);
    addTag("Fashion",      "时尚",     TagDimension::Usage);
    addTag("Human Interest","人文",    TagDimension::Usage);
    addTag("Light Activity","轻活动",  TagDimension::Usage);
    addTag("Outdoors",     "户外",     TagDimension::Usage);
    addTag("Party",        "派对",     TagDimension::Usage);
    addTag("Reality",      "真人秀",   TagDimension::Usage);
    addTag("Relaxing",     "放松",     TagDimension::Usage);
    addTag("Sports",       "运动",     TagDimension::Usage);
    addTag("Thriller",     "惊悚",     TagDimension::Usage);
    addTag("Trailers",     "预告片",   TagDimension::Usage);
    addTag("Western",      "西部",     TagDimension::Usage);
    addTag("Working Out",  "锻炼",     TagDimension::Usage);
    addTag("Yoga & Meditation","瑜伽冥想", TagDimension::Usage);

    s_tagInitialized = true;
}

// Query

const std::vector<TagEntry>& TagSystemManager::getAllTags() {
    return s_allTags;
}

std::vector<const TagEntry*> TagSystemManager::getTagsByDimension(TagDimension dim) {
    std::vector<const TagEntry*> result;
    for (const auto& t : s_allTags) {
        if (t.dimension == dim) result.push_back(&t);
    }
    return result;
}

const char* TagSystemManager::getDimensionName(TagDimension dim) {
    switch (dim) {
        case TagDimension::Category:           return "Category";
        case TagDimension::Mood:               return "Mood";
        case TagDimension::FeaturedInstrument: return "Instrument";
        case TagDimension::Tempo:              return "Tempo";
        case TagDimension::Usage:              return "Usage";
        case TagDimension::Version:            return "Version";
        case TagDimension::Era:                return "Era";
        case TagDimension::Duration:           return "Duration";
        case TagDimension::BPMRange:           return "BPM Range";
        default: return "Unknown";
    }
}

const char* TagSystemManager::getDimensionNameZh(TagDimension dim) {
    switch (dim) {
        case TagDimension::Category:           return "\xe5\x88\x86\xe7\xb1\xbb";  // 分类
        case TagDimension::Mood:               return "\xe6\x83\x85\xe7\xbb\xaa";  // 情绪
        case TagDimension::FeaturedInstrument: return "\xe4\xb9\x90\xe5\x99\xa8";  // 乐器
        case TagDimension::Tempo:              return "\xe9\x80\x9f\xe5\xba\xa6";  // 速度
        case TagDimension::Usage:              return "\xe7\x94\xa8\xe9\x80\x94";  // 用途
        case TagDimension::Version:            return "\xe7\x89\x88\xe6\x9c\xac";  // 版本
        case TagDimension::Era:                return "\xe5\xb9\xb4\xe4\xbb\xa3";  // 年代
        case TagDimension::Duration:           return "\xe6\x97\xb6\xe9\x95\xbf";  // 时长
        case TagDimension::BPMRange:           return "BPM\xe8\x8c\x83\xe5\x9b\xb4"; // BPM范围
        default: return "Unknown";
    }
}

std::vector<TagDimension> TagSystemManager::getAllDimensions() {
    return {TagDimension::Category, TagDimension::Mood, TagDimension::FeaturedInstrument,
            TagDimension::Tempo, TagDimension::Usage, TagDimension::Version,
            TagDimension::Era, TagDimension::Duration, TagDimension::BPMRange};
}

// Facet Filter

FacetFilterState& TagSystemManager::getFacetState(TagDimension dim) {
    return s_facetStates[(int)dim];
}

bool TagSystemManager::hasAnyFacetFilter() {
    for (int i = 0; i < (int)TagDimension::COUNT; i++) {
        if (s_facetStates[i].hasFilter()) return true;
    }
    return false;
}

void TagSystemManager::clearAllFacets() {
    for (int i = 0; i < (int)TagDimension::COUNT; i++) {
        s_facetStates[i].clear();
    }
}

// Map dimension to metadata field name
static const char* dimensionToFieldName(TagDimension dim) {
    switch (dim) {
        case TagDimension::Category:           return "Category";
        case TagDimension::Mood:               return "Mood";
        case TagDimension::FeaturedInstrument: return "FeaturedInstrument";
        case TagDimension::Tempo:              return "Tempo";
        case TagDimension::Usage:              return "Usage";
        case TagDimension::Version:            return "Version";
        case TagDimension::Era:                return "Era";
        default: return "";
    }
}

bool TagSystemManager::matchesFacetFilter(const std::unordered_map<std::string, std::string>& fields) {
    for (int i = 0; i < (int)TagDimension::COUNT; i++) {
        auto& facet = s_facetStates[i];
        if (!facet.hasFilter()) continue;
        
        const char* fieldName = dimensionToFieldName((TagDimension)i);
        if (!fieldName[0]) continue;
        
        auto it = fields.find(fieldName);
        if (it == fields.end() || it->second.empty()) return false;
        
        // Check if any selected value is present in the field
        std::vector<std::string> fieldTags = splitTags(it->second);
        bool matched = false;
        for (const auto& ft : fieldTags) {
            std::string trimmed = trimStr(ft);
            if (facet.isSelected(trimmed)) { matched = true; break; }
        }
        if (!matched) return false;
    }
    return true;
}

// Auto-Tag Generation (参考 maketags.lua)

std::string TagSystemManager::autoTempoFromBPM(const std::string& bpmStr) {
    if (bpmStr.empty()) return "";
    
    // Extract numeric value
    std::string cleaned;
    for (char c : bpmStr) {
        if (std::isdigit((unsigned char)c) || c == '.') cleaned += c;
    }
    if (cleaned.empty()) return "";
    
    float bpm = 0;
    try { bpm = std::stof(cleaned); } catch (...) { return ""; }
    if (bpm <= 5) return "";
    
    if (bpm < 40)       return "Very Slow";
    else if (bpm <= 72)  return "Slow";
    else if (bpm <= 90)  return "Medium Slow";
    else if (bpm <= 110) return "Medium";
    else if (bpm <= 130) return "Medium Fast";
    else if (bpm <= 160) return "Fast";
    else                 return "Very Fast";
}

std::string TagSystemManager::autoDurationTag(float durationSeconds) {
    if (durationSeconds <= 0) return "";
    
    if (durationSeconds <= 15)       return ":01 - :15";
    else if (durationSeconds <= 30)  return ":16 - :30";
    else if (durationSeconds <= 60)  return ":31 - 1:00";
    else if (durationSeconds <= 180) return "1:01 - 2:59";
    else                             return "3:00+";
}

std::string TagSystemManager::autoBPMRangeTag(const std::string& bpmStr) {
    if (bpmStr.empty()) return "";
    
    std::string cleaned;
    for (char c : bpmStr) {
        if (std::isdigit((unsigned char)c) || c == '.') cleaned += c;
    }
    if (cleaned.empty()) return "";
    
    float bpm = 0;
    try { bpm = std::stof(cleaned); } catch (...) { return ""; }
    if (bpm <= 5) return "";
    
    // Round down to nearest 5
    int rounded = ((int)bpm / 5) * 5;
    
    if (rounded < 40)       return "Very, Very Slow (Less than 40 bpm)";
    else if (rounded <= 60) return "Very Slow (41-60 bpm)";
    else if (rounded <= 75) return "Slow (61-75 bpm)";
    else if (rounded <= 110)return "Medium (76-110 bpm)";
    else if (rounded <= 130)return "Fast (111-130 bpm)";
    else if (rounded <= 160)return "Very Fast (131-160 bpm)";
    else                    return "Very, Very Fast (More than 160 bpm)";
}

std::vector<std::string> TagSystemManager::splitTags(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::istringstream iss(value);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        std::string trimmed = trimStr(token);
        if (!trimmed.empty()) result.push_back(trimmed);
    }
    return result;
}

std::string TagSystemManager::joinTags(const std::vector<std::string>& tags, const std::string& separator) {
    // Deduplicate
    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;
    for (const auto& t : tags) {
        std::string lower = toLower(t);
        if (seen.find(lower) == seen.end()) {
            seen.insert(lower);
            unique.push_back(t);
        }
    }
    
    std::string result;
    for (size_t i = 0; i < unique.size(); i++) {
        if (i > 0) result += separator;
        result += unique[i];
    }
    return result;
}

std::vector<AutoTagResult> TagSystemManager::autoGenerateTags(const std::unordered_map<std::string, std::string>& metadata) {
    std::vector<AutoTagResult> results;
    
    // Auto-generate Tempo from BPM
    auto bpmIt = metadata.find("BPM");
    if (bpmIt != metadata.end() && !bpmIt->second.empty()) {
        std::string tempo = autoTempoFromBPM(bpmIt->second);
        if (!tempo.empty()) {
            results.push_back({"_TagsTempo", tempo});
        }
        std::string bpmRange = autoBPMRangeTag(bpmIt->second);
        if (!bpmRange.empty()) {
            results.push_back({"_TagsBPM", bpmRange});
        }
    }
    
    // Auto-generate Duration tag
    auto durIt = metadata.find("DurationSeconds");
    if (durIt != metadata.end()) {
        try {
            float dur = std::stof(durIt->second);
            std::string durTag = autoDurationTag(dur);
            if (!durTag.empty()) {
                results.push_back({"_TagsTime", durTag});
            }
        } catch (...) {}
    }
    
    // Process comma-separated fields into tags
    const char* tagFields[] = {"Mood", "Microphone", "RecMedium", "Usage", "FeaturedInstrument"};
    for (const char* field : tagFields) {
        auto it = metadata.find(field);
        if (it != metadata.end() && !it->second.empty()) {
            // Check if tags already exist
            std::string tagKey = std::string("_Tags") + field;
            auto existIt = metadata.find(tagKey);
            if (existIt == metadata.end() || existIt->second.empty()) {
                // Split and rejoin with tag separator
                auto parts = splitTags(it->second);
                if (!parts.empty()) {
                    results.push_back({tagKey, joinTags(parts, " > ")});
                }
            }
        }
    }
    
    // Stuff fields (copy as-is to tag)
    const char* stuffFields[] = {"Manufacturer", "Library", "Version"};
    for (const char* field : stuffFields) {
        auto it = metadata.find(field);
        if (it != metadata.end() && !it->second.empty()) {
            std::string tagKey = std::string("_Tags") + field;
            auto existIt = metadata.find(tagKey);
            if (existIt == metadata.end() || existIt->second.empty()) {
                results.push_back({tagKey, it->second});
            }
        }
    }
    
    return results;
}

std::vector<const TagEntry*> TagSystemManager::searchTags(const std::string& query, int maxResults) {
    std::vector<const TagEntry*> results;
    if (query.empty()) return results;
    
    std::string q = toLower(query);
    for (const auto& t : s_allTags) {
        if ((int)results.size() >= maxResults) break;
        std::string lv = toLower(t.value);
        if (lv.find(q) != std::string::npos || t.value_zh.find(query) != std::string::npos) {
            results.push_back(&t);
        }
    }
    return results;
}

// Smart Tag Recommendation (F-007 + F-015)

// Keyword synonyms/aliases that map to tag values for better matching
struct KeywordAlias {
    const char* keyword;
    const char* tagValue;
    TagDimension dim;
};

static const KeywordAlias s_keywordAliases[] = {
    // Category aliases
    {"ambience",    "Ambient",      TagDimension::Category},
    {"ambient",     "Ambient",      TagDimension::Category},
    {"orchestra",   "Orchestral",   TagDimension::Category},
    {"orchestral",  "Orchestral",   TagDimension::Category},
    {"hiphop",      "Urban_Hip Hop",TagDimension::Category},
    {"hip-hop",     "Urban_Hip Hop",TagDimension::Category},
    {"hip hop",     "Urban_Hip Hop",TagDimension::Category},
    {"techno",      "Dance_Techno", TagDimension::Category},
    {"trance",      "Dance_Trance", TagDimension::Category},
    {"disco",       "Dance_Disco",  TagDimension::Category},
    {"electronic",  "Electronica",  TagDimension::Category},
    {"metal",       "Rock_Metal",   TagDimension::Category},
    {"cinematic",   "Orchestral",   TagDimension::Category},
    {"trailer",     "Orchestral",   TagDimension::Category},
    // Mood aliases
    {"scary",       "Dark",         TagDimension::Mood},
    {"horror",      "Dark",         TagDimension::Mood},
    {"creepy",      "Dark",         TagDimension::Mood},
    {"spooky",      "Mysterious",   TagDimension::Mood},
    {"peaceful",    "Calm",         TagDimension::Mood},
    {"quiet",       "Calm",         TagDimension::Mood},
    {"chill",       "Relaxed",      TagDimension::Mood},
    {"cheerful",    "Happy",        TagDimension::Mood},
    {"joyful",      "Happy",        TagDimension::Mood},
    {"melancholy",  "Sad",          TagDimension::Mood},
    {"sorrow",      "Sad",          TagDimension::Mood},
    {"excitement",  "Energetic",    TagDimension::Mood},
    {"exciting",    "Energetic",    TagDimension::Mood},
    {"battle",      "Epic",         TagDimension::Mood},
    {"war",         "Epic",         TagDimension::Mood},
    {"heroic",      "Epic",         TagDimension::Mood},
    {"tension",     "Tense",        TagDimension::Mood},
    {"thriller",    "Suspense",     TagDimension::Mood},
    {"dreamy",      "Ethereal",     TagDimension::Mood},
    {"magic",       "Fantasy",      TagDimension::Mood},
    // Instrument aliases
    {"violin",      "Strings",      TagDimension::FeaturedInstrument},
    {"cello",       "Strings",      TagDimension::FeaturedInstrument},
    {"viola",       "Strings",      TagDimension::FeaturedInstrument},
    {"trumpet",     "Brass",        TagDimension::FeaturedInstrument},
    {"trombone",    "Brass",        TagDimension::FeaturedInstrument},
    {"tuba",        "Brass",        TagDimension::FeaturedInstrument},
    {"clarinet",    "Wind",         TagDimension::FeaturedInstrument},
    {"oboe",        "Wind",         TagDimension::FeaturedInstrument},
    {"bassoon",     "Wind",         TagDimension::FeaturedInstrument},
    {"drum",        "Drums",        TagDimension::FeaturedInstrument},
    {"cymbal",      "Percussion",   TagDimension::FeaturedInstrument},
    {"snare",       "Drums",        TagDimension::FeaturedInstrument},
    {"kick",        "Drums",        TagDimension::FeaturedInstrument},
    {"hihat",       "Drums",        TagDimension::FeaturedInstrument},
    {"hi-hat",      "Drums",        TagDimension::FeaturedInstrument},
    {"synth",       "Synth",        TagDimension::FeaturedInstrument},
    {"synthesizer", "Synth",        TagDimension::FeaturedInstrument},
    {"voice",       "Vocals",       TagDimension::FeaturedInstrument},
    {"vocal",       "Vocals",       TagDimension::FeaturedInstrument},
    {"singing",     "Vocals",       TagDimension::FeaturedInstrument},
    {"singer",      "Vocals",       TagDimension::FeaturedInstrument},
    {"ukulele",     "Guitar_Acoustic", TagDimension::FeaturedInstrument},
    // Usage aliases
    {"film",        "Drama",        TagDimension::Usage},
    {"movie",       "Drama",        TagDimension::Usage},
    {"game",        "Action",       TagDimension::Mood},
    {"commercial",  "Corporate",    TagDimension::Usage},
    {"advertisement","Corporate",   TagDimension::Usage},
    {"news",        "Documentary",  TagDimension::Usage},
    {"sport",       "Sports",       TagDimension::Usage},
};

std::vector<TagSuggestion> TagSystemManager::suggestTagsFromText(
    const std::string& filename,
    const std::string& description,
    const std::string& keywords,
    int maxResults)
{
    // Build combined searchable text (lowercased)
    std::string combined;
    combined.reserve(filename.size() + description.size() + keywords.size() + 8);
    combined += toLower(filename) + " ";
    combined += toLower(description) + " ";
    combined += toLower(keywords);

    // Replace common separators with spaces for word boundary matching
    for (char& c : combined) {
        if (c == '_' || c == '-' || c == '.' || c == ',' || c == ';' || c == '/' || c == '\\')
            c = ' ';
    }

    if (combined.size() < 3) return {};

    // Precompute padded text once (word-boundary matching via " text ")
    std::string padded = " " + combined + " ";

    // Score accumulator: tagValue -> {best confidence, dimension, zh}
    struct ScoreEntry {
        float score = 0.f;
        TagDimension dim;
        std::string value_zh;
    };
    std::unordered_map<std::string, ScoreEntry> scores;

    // 1) Direct tag name matching against vocabulary
    for (const auto& tag : s_allTags) {
        // Skip auto-generated dimensions (Tempo/Duration/BPMRange handled by autoGenerateTags)
        if (tag.dimension == TagDimension::Tempo ||
            tag.dimension == TagDimension::Duration ||
            tag.dimension == TagDimension::BPMRange)
            continue;

        std::string tagLower = toLower(tag.value);

        // Whole-word match gets highest confidence
        std::string needle = " " + tagLower + " ";
        float conf = 0.f;
        if (padded.find(needle) != std::string::npos) {
            conf = 0.85f; // Strong: whole word match in text
        } else if (combined.find(tagLower) != std::string::npos && tagLower.size() >= 4) {
            conf = 0.55f; // Moderate: substring match (only for longer tags to reduce false positives)
        }

        // Chinese value match
        if (conf < 0.8f && !tag.value_zh.empty() && combined.find(tag.value_zh) != std::string::npos) {
            conf = std::max(conf, 0.75f);
        }

        if (conf > 0.f) {
            auto& entry = scores[tag.value];
            if (conf > entry.score) {
                entry.score = conf;
                entry.dim = tag.dimension;
                entry.value_zh = tag.value_zh;
            }
        }
    }

    // 2) Keyword alias matching (synonym expansion)
    for (const auto& alias : s_keywordAliases) {
        std::string kw = alias.keyword;
        std::string needle = " " + kw + " ";
        float conf = 0.f;
        if (padded.find(needle) != std::string::npos) {
            conf = 0.70f; // Alias whole-word match
        } else if (combined.find(kw) != std::string::npos && kw.size() >= 4) {
            conf = 0.45f; // Alias substring
        }

        if (conf > 0.f) {
            // Look up the target tag to get zh translation
            auto& entry = scores[alias.tagValue];
            if (conf > entry.score) {
                entry.score = conf;
                entry.dim = alias.dim;
                // Find zh from tag list
                for (const auto& tag : s_allTags) {
                    if (tag.value == alias.tagValue) {
                        entry.value_zh = tag.value_zh;
                        break;
                    }
                }
            }
        }
    }

    // 3) Boost: filename matches get a small bonus (filename is most intentional)
    std::string filenameLower = toLower(filename);
    for (auto& [tagVal, entry] : scores) {
        std::string tagLower = toLower(tagVal);
        if (filenameLower.find(tagLower) != std::string::npos) {
            entry.score = std::min(entry.score + 0.10f, 1.0f);
        }
    }

    // Convert to result vector and sort by confidence
    std::vector<TagSuggestion> results;
    results.reserve(scores.size());
    for (auto& [tagVal, entry] : scores) {
        results.push_back({tagVal, entry.value_zh, entry.dim, entry.score});
    }
    std::sort(results.begin(), results.end(),
              [](const TagSuggestion& a, const TagSuggestion& b) { return a.confidence > b.confidence; });

    if ((int)results.size() > maxResults)
        results.resize(maxResults);

    return results;
}

bool TagSystemManager::isInitialized() {
    return s_tagInitialized;
}
