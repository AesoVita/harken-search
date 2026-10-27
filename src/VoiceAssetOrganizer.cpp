#include "VoiceAssetOrganizer.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (text.find(needle) != std::string::npos) return true;
    }
    return false;
}

void AddUnique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) return;
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string Join(const std::vector<std::string>& values, const char* separator) {
    std::ostringstream out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << separator;
        out << values[i];
    }
    return out.str();
}

std::string FormatSrtTime(int64_t ms) {
    if (ms < 0) ms = 0;
    const int64_t totalSeconds = ms / 1000;
    const int64_t hours = totalSeconds / 3600;
    const int64_t minutes = (totalSeconds / 60) % 60;
    const int64_t seconds = totalSeconds % 60;
    const int64_t millis = ms % 1000;
    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << ","
        << std::setw(3) << millis;
    return out.str();
}

std::string EscapeCsv(std::string value) {
    bool quote = false;
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '"') {
            escaped += "\"\"";
            quote = true;
        } else {
            if (c == ',' || c == '\n' || c == '\r') quote = true;
            escaped += c;
        }
    }
    if (!quote) return escaped;
    return "\"" + escaped + "\"";
}

std::string EscapeJson(const std::string& value) {
    std::ostringstream out;
    for (char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

} // namespace

VoiceAssetOrganization VoiceAssetOrganizer::Analyze(const std::string& fileName,
                                                     const TranscriptionResult& transcript,
                                                     float durationSeconds) {
    VoiceAssetOrganization org;
    const std::string textLower = ToLower(transcript.fullText);
    const std::string fileLower = ToLower(fileName);

    AddUnique(org.keywords, "voice");
    AddUnique(org.keywords, "transcript");

    if (transcript.language == "zh" || ContainsAny(transcript.fullText, {"我", "你", "他", "她", "是", "的"})) {
        AddUnique(org.keywords, "language:zh");
        AddUnique(org.keywords, "简体中文");
    } else if (transcript.language == "en" || ContainsAny(textLower, {" the ", " you ", " go ", " help ", "reload"})) {
        AddUnique(org.keywords, "language:en");
        AddUnique(org.keywords, "English");
    }

    if (ContainsAny(textLower, {"attack", "fire", "reload", "enemy", "kill"}) ||
        ContainsAny(transcript.fullText, {"攻击", "开火", "敌人", "撤退", "装弹"})) {
        AddUnique(org.keywords, "combat-dialogue");
        AddUnique(org.keywords, "战斗台词");
    }
    if (ContainsAny(textLower, {"help", "run", "danger", "hurry"}) ||
        ContainsAny(transcript.fullText, {"救命", "快跑", "危险", "快走"})) {
        AddUnique(org.keywords, "urgent");
        AddUnique(org.keywords, "紧张");
    }
    if (ContainsAny(textLower, {"ah", "ugh", "pain", "hurt"}) ||
        ContainsAny(transcript.fullText, {"啊", "呃", "疼", "受伤"})) {
        AddUnique(org.keywords, "effort");
        AddUnique(org.keywords, "受击");
    }
    if (ContainsAny(fileLower, {"npc", "dialog", "dialogue", "vo_", "voice"})) {
        AddUnique(org.keywords, "dialogue");
        AddUnique(org.keywords, "对白");
    }

    if (durationSeconds > 0.0f) {
        if (durationSeconds < 2.0f) AddUnique(org.keywords, "short-line");
        else if (durationSeconds < 8.0f) AddUnique(org.keywords, "medium-line");
        else AddUnique(org.keywords, "long-line");
    }

    if (!transcript.success || transcript.fullText.empty()) {
        org.needsHumanReview = true;
        org.qualityIssues.push_back({"empty_transcript", "No usable transcript was produced.", 2});
        AddUnique(org.keywords, "review-needed");
    }
    if (durationSeconds > 8.0f && transcript.fullText.size() < 8) {
        org.needsHumanReview = true;
        org.qualityIssues.push_back({"sparse_transcript", "Transcript is very short compared with audio duration.", 2});
        AddUnique(org.keywords, "review-needed");
    }
    for (size_t i = 1, repeated = 0; i < transcript.fullText.size(); ++i) {
        repeated = (transcript.fullText[i] == transcript.fullText[i - 1]) ? repeated + 1 : 0;
        if (repeated >= 8) {
            org.needsHumanReview = true;
            org.qualityIssues.push_back({"repeated_characters", "Transcript contains repeated characters that may indicate recognition noise.", 1});
            AddUnique(org.keywords, "review-needed");
            break;
        }
    }
    for (size_t i = 1; i < transcript.segments.size(); ++i) {
        const int64_t gapMs = transcript.segments[i].startMs - transcript.segments[i - 1].endMs;
        if (gapMs > 2500) {
            org.qualityIssues.push_back({"long_gap", "Long silence gap detected between transcript segments.", 1});
            AddUnique(org.keywords, "long-silence");
            break;
        }
    }

    std::vector<std::string> descParts;
    descParts.push_back("Voice asset");
    if (std::find(org.keywords.begin(), org.keywords.end(), "combat-dialogue") != org.keywords.end()) {
        descParts.push_back("combat dialogue");
    } else if (std::find(org.keywords.begin(), org.keywords.end(), "effort") != org.keywords.end()) {
        descParts.push_back("effort / pain vocal");
    } else {
        descParts.push_back("dialogue / spoken line");
    }
    if (org.needsHumanReview) descParts.push_back("needs human review");
    if (!transcript.fullText.empty()) {
        std::string excerpt = transcript.fullText.substr(0, 180);
        descParts.push_back("Transcript: " + excerpt);
    }

    org.keywordCsv = Join(org.keywords, ", ");
    org.searchableDescription = Join(descParts, ". ");
    return org;
}

std::string VoiceAssetOrganizer::BuildSrt(const TranscriptionResult& transcript) {
    std::ostringstream out;
    int index = 1;
    for (const auto& seg : transcript.segments) {
        if (seg.text.empty()) continue;
        out << index++ << "\n"
            << FormatSrtTime(seg.startMs) << " --> " << FormatSrtTime(seg.endMs) << "\n"
            << seg.text << "\n\n";
    }
    return out.str();
}

std::string VoiceAssetOrganizer::BuildCsv(const TranscriptionResult& transcript) {
    std::ostringstream out;
    out << "start_ms,end_ms,text\n";
    for (const auto& seg : transcript.segments) {
        out << seg.startMs << "," << seg.endMs << "," << EscapeCsv(seg.text) << "\n";
    }
    return out.str();
}

std::string VoiceAssetOrganizer::BuildJson(const TranscriptionResult& transcript) {
    std::ostringstream out;
    out << "{\"language\":\"" << EscapeJson(transcript.language) << "\",\"segments\":[";
    for (size_t i = 0; i < transcript.segments.size(); ++i) {
        const auto& seg = transcript.segments[i];
        if (i > 0) out << ",";
        out << "{\"startMs\":" << seg.startMs
            << ",\"endMs\":" << seg.endMs
            << ",\"text\":\"" << EscapeJson(seg.text) << "\"}";
    }
    out << "],\"fullText\":\"" << EscapeJson(transcript.fullText) << "\"}";
    return out.str();
}
