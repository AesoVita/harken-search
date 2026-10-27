#pragma once

#include "ResultRowHighlightSpans.h"
#include "SearchTextIndex.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

inline float SettingsSearchHighlightAlpha(float elapsedSeconds) {
    constexpr float kPulseSeconds = 1.0f;
    constexpr float kPulseCycles = 2.5f;
    constexpr float kSteadyAlpha = 0.38f;
    constexpr float kPulseLo = 0.15f;
    constexpr float kPulseHi = 0.55f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    if (elapsedSeconds >= 0.0f && elapsedSeconds < kPulseSeconds) {
        const float wave = 0.5f + 0.5f * std::cos(elapsedSeconds * (kPulseCycles * kTwoPi) / kPulseSeconds);
        return kPulseLo + (kPulseHi - kPulseLo) * wave;
    }
    return kSteadyAlpha;
}

inline bool FindAsciiCaseInsensitiveSpan(const std::string& text, const std::string& query, size_t& outStart) {
    if (text.empty() || query.empty() || query.size() > text.size()) return false;
    auto isAscii = [](unsigned char c) -> bool { return c < 0x80; };
    auto lowerAscii = [](unsigned char c) -> unsigned char {
        return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c;
    };
    for (unsigned char c : query) {
        if (!isAscii(c)) return false;
    }
    const size_t lastStart = text.size() - query.size();
    for (size_t i = 0; i <= lastStart; ++i) {
        bool matched = true;
        for (size_t j = 0; j < query.size(); ++j) {
            const unsigned char textByte = static_cast<unsigned char>(text[i + j]);
            if (!isAscii(textByte) || lowerAscii(textByte) != lowerAscii(static_cast<unsigned char>(query[j]))) {
                matched = false;
                break;
            }
        }
        if (matched) {
            outStart = i;
            return true;
        }
    }
    return false;
}

inline size_t SettingsUtf8CharLen(unsigned char lead) {
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

inline bool SettingsIsAsciiAlnum(unsigned char c) {
    return c < 0x80 && std::isalnum(c) != 0;
}

inline std::string SettingsLowerAsciiCopy(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x80) ch = static_cast<char>(std::tolower(c));
    }
    return out;
}

inline std::vector<std::string> SplitSettingsSearchTokens(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };
    for (unsigned char ch : text) {
        if (ch < 0x80 && !SettingsIsAsciiAlnum(ch)) {
            flush();
        } else {
            current.push_back(static_cast<char>(ch));
        }
    }
    flush();
    return tokens;
}

inline bool SettingsTokenMatchesQuery(const std::string& token, const std::string& query) {
    const std::string nTok = SettingsLowerAsciiCopy(token);
    const std::string nQuery = SettingsLowerAsciiCopy(query);
    if (nTok.empty() || nQuery.empty()) return false;
    if (nTok.find(nQuery) != std::string::npos) return true;
    return nTok.size() >= 3 && nQuery.find(nTok) != std::string::npos;
}

inline bool FindSettingsTokenInLabel(const std::string& label, const std::string& token, size_t& outStart) {
    if (token.empty() || token.size() > label.size()) return false;
    const size_t literal = label.find(token);
    if (literal != std::string::npos) {
        outStart = literal;
        return true;
    }
    return FindAsciiCaseInsensitiveSpan(label, token, outStart);
}

inline std::pair<size_t, size_t> ExpandSettingsLabelWordSpan(const std::string& label, size_t start, size_t end) {
    if (end > label.size()) end = label.size();
    if (start >= end) return {start, end};
    const unsigned char lead = static_cast<unsigned char>(label[start]);
    if (lead < 0x80) {
        while (start > 0 && SettingsIsAsciiAlnum(static_cast<unsigned char>(label[start - 1]))) --start;
        while (end < label.size() && SettingsIsAsciiAlnum(static_cast<unsigned char>(label[end]))) ++end;
        return {start, end};
    }
    size_t cp = 0;
    size_t cursor = start;
    while (cursor < label.size() && cp < 2) {
        const unsigned char c = static_cast<unsigned char>(label[cursor]);
        if (c < 0x80) break;
        cursor += SettingsUtf8CharLen(c);
        ++cp;
    }
    if (cursor > end) end = cursor;
    return {start, end};
}

inline std::pair<size_t, size_t> FirstSettingsLabelWordSpan(const std::string& label) {
    if (label.empty()) return {0, 0};
    size_t i = 0;
    while (i < label.size()) {
        const unsigned char c = static_cast<unsigned char>(label[i]);
        if (c >= 0x80 || SettingsIsAsciiAlnum(c)) break;
        ++i;
    }
    if (i >= label.size()) return {0, 0};
    const auto word = ExpandSettingsLabelWordSpan(label, i, i + SettingsUtf8CharLen(static_cast<unsigned char>(label[i])));
    if (word.second > word.first) return word;
    return {0, label.size()};
}

inline bool AppendDirectSettingsQuerySpans(std::vector<std::pair<size_t, size_t>>& spans,
                                           const std::string& label,
                                           const std::string& term) {
    const HarkenSearch::Search::PinyinSpan pinyinSpan = HarkenSearch::Search::FindPinyinSpanUtf8(label, term);
    if (pinyinSpan.found && pinyinSpan.byteStart < pinyinSpan.byteEnd &&
        pinyinSpan.byteEnd <= label.size()) {
        const auto word = ExpandSettingsLabelWordSpan(label, pinyinSpan.byteStart, pinyinSpan.byteEnd);
        AppendResultRowHighlightSpan(spans, word.first, word.second);
        return true;
    }
    size_t asciiStart = 0;
    if (FindAsciiCaseInsensitiveSpan(label, term, asciiStart)) {
        const auto word = ExpandSettingsLabelWordSpan(label, asciiStart, asciiStart + term.size());
        AppendResultRowHighlightSpan(spans, word.first, word.second);
        return true;
    }
    const size_t literal = label.find(term);
    if (literal != std::string::npos) {
        const auto word = ExpandSettingsLabelWordSpan(label, literal, literal + term.size());
        AppendResultRowHighlightSpan(spans, word.first, word.second);
        return true;
    }
    return false;
}

inline std::vector<std::pair<size_t, size_t>> BuildSettingsLabelHighlightSpans(const std::string& label,
                                                                              const std::string& query,
                                                                              const std::string& keywords = {}) {
    std::vector<std::pair<size_t, size_t>> spans;
    if (label.empty() || query.empty()) return spans;

    const std::vector<std::string> queryTokens = SplitSettingsSearchTokens(query);
    const std::vector<std::string> keywordTokens = SplitSettingsSearchTokens(keywords);

    size_t shortestRelated = static_cast<size_t>(-1);
    size_t relatedStart = 0;
    size_t relatedEnd = 0;
    size_t shortestVisible = static_cast<size_t>(-1);
    size_t visibleStart = 0;
    size_t visibleEnd = 0;
    for (const std::string& token : keywordTokens) {
        size_t pos = 0;
        if (!FindSettingsTokenInLabel(label, token, pos)) continue;
        size_t tokenEnd = pos + token.size();
        bool asciiToken = !token.empty();
        for (unsigned char c : token) {
            if (c >= 0x80) {
                asciiToken = false;
                break;
            }
        }
        if (asciiToken) {
            const auto word = ExpandSettingsLabelWordSpan(label, pos, tokenEnd);
            pos = word.first;
            tokenEnd = word.second;
        }
        const size_t spanLen = tokenEnd - pos;
        if (spanLen < shortestVisible) {
            shortestVisible = spanLen;
            visibleStart = pos;
            visibleEnd = tokenEnd;
        }
        bool related = queryTokens.empty();
        for (const std::string& queryToken : queryTokens) {
            if (SettingsTokenMatchesQuery(token, queryToken)) {
                related = true;
                break;
            }
        }
        if (related && spanLen < shortestRelated) {
            shortestRelated = spanLen;
            relatedStart = pos;
            relatedEnd = tokenEnd;
        }
    }
    if (shortestRelated != static_cast<size_t>(-1)) {
        AppendResultRowHighlightSpan(spans, relatedStart, relatedEnd);
        return spans;
    }

    for (const std::string& term : queryTokens) {
        AppendDirectSettingsQuerySpans(spans, label, term);
    }
    if (!spans.empty()) return spans;

    if (shortestVisible != static_cast<size_t>(-1)) {
        AppendResultRowHighlightSpan(spans, visibleStart, visibleEnd);
    }
    return spans;
}

inline bool SettingsLabelIsCurrentHit(const std::string& label, const std::string& activeLabel) {
    if (label.empty() || activeLabel.empty()) return false;
    if (label == activeLabel) return true;
    if (label.find(activeLabel) != std::string::npos) return true;
    return activeLabel.find(label) != std::string::npos;
}

inline void EnsureActiveSettingsLabelHighlight(std::vector<std::pair<size_t, size_t>>& spans,
                                               const std::string& label,
                                               const std::string& query,
                                               const std::string& activeLabel) {
    if (!spans.empty() || label.empty() || query.empty()) return;
    if (!SettingsLabelIsCurrentHit(label, activeLabel)) return;
    const size_t activeInLabel = label.find(activeLabel);
    if (activeInLabel != std::string::npos) {
        AppendResultRowHighlightSpan(spans, activeInLabel, activeInLabel + activeLabel.size());
        return;
    }
    const size_t labelInActive = activeLabel.find(label);
    if (labelInActive != std::string::npos) {
        AppendResultRowHighlightSpan(spans, 0, label.size());
        return;
    }
    const auto word = FirstSettingsLabelWordSpan(label);
    if (word.second > word.first) {
        AppendResultRowHighlightSpan(spans, word.first, word.second);
    }
}
