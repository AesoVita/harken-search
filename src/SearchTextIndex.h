#pragma once

#include "platform/search_text_gb2312.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "generated/mandarin_pinyin_table.inc"

namespace HarkenSearch::Search {

// @stage7-contract
// API contract: search index helpers own normalized strings/spans and do not
// retain caller text buffers. UTF-8/GB2312 conversion preserves byte offsets
// and explicit not-found results for downstream query code.

struct PinyinSpan {
    bool found = false;
    size_t byteStart = 0;
    size_t byteEnd = 0;
    std::string text;
};

namespace Detail {

struct Utf8Char {
    uint32_t codepoint = 0;
    size_t begin = 0;
    size_t end = 0;
};

inline bool IsAsciiAlphaNum(unsigned char c) {
    return std::isalnum(c) != 0;
}

inline char ToLowerAscii(unsigned char c) {
    return static_cast<char>(std::tolower(c));
}

inline bool IsCjk(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x20000 && cp <= 0x2A6DF);
}

inline bool DecodeNextUtf8(const std::string& text, size_t& offset, Utf8Char& out) {
    if (offset >= text.size()) return false;

    const size_t begin = offset;
    const unsigned char c0 = static_cast<unsigned char>(text[offset]);
    uint32_t cp = c0;
    size_t len = 1;

    if ((c0 & 0x80) == 0) {
        cp = c0;
    } else if ((c0 & 0xE0) == 0xC0 && offset + 1 < text.size()) {
        const unsigned char c1 = static_cast<unsigned char>(text[offset + 1]);
        if ((c1 & 0xC0) == 0x80) {
            cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            len = 2;
        }
    } else if ((c0 & 0xF0) == 0xE0 && offset + 2 < text.size()) {
        const unsigned char c1 = static_cast<unsigned char>(text[offset + 1]);
        const unsigned char c2 = static_cast<unsigned char>(text[offset + 2]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
            cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            len = 3;
        }
    } else if ((c0 & 0xF8) == 0xF0 && offset + 3 < text.size()) {
        const unsigned char c1 = static_cast<unsigned char>(text[offset + 1]);
        const unsigned char c2 = static_cast<unsigned char>(text[offset + 2]);
        const unsigned char c3 = static_cast<unsigned char>(text[offset + 3]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
            cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            len = 4;
        }
    }

    offset += len;
    out = Utf8Char{cp, begin, offset};
    return true;
}

inline const char* PinyinFor(uint32_t cp) {
    if (cp < kMandarinPinyinBase) return nullptr;
    const uint32_t offset = cp - kMandarinPinyinBase;
    if (offset >= kMandarinPinyinCount) return nullptr;
    const size_t entry = static_cast<size_t>(offset) * 2;
    const uint16_t syllableIndex = static_cast<uint16_t>(
        kMandarinPinyinEntries[entry] |
        (static_cast<uint16_t>(kMandarinPinyinEntries[entry + 1]) << 8));
    if (syllableIndex == kMandarinPinyinMissing) return nullptr;
    size_t cursor = 0;
    for (uint16_t i = 0; i < syllableIndex; ++i) {
        while (kMandarinSyllablePool[cursor] != 0) ++cursor;
        ++cursor;
    }
    return reinterpret_cast<const char*>(kMandarinSyllablePool + cursor);
}

inline const char* const* KnownPinyinSyllables(size_t& count) {
    static constexpr const char* kSyllables[] = {
        "bao", "bo", "bu", "chang", "che", "da", "dan", "dao", "dian", "dong",
        "duan", "fa", "fei", "feng", "gao", "gong", "guai", "hai", "he", "hu",
        "huan", "huo", "ji", "jian", "jiao", "jing", "jin", "kong", "ku", "lei",
        "li", "man", "mao", "men", "mo", "mu", "nei", "niao", "nu", "peng",
        "pin", "po", "qi", "qiang", "qun", "ren", "sheng", "shi", "shui",
        "sou", "sui", "wu", "xia", "xiao", "xie", "xing", "yu", "ye", "zha",
        "zhong", "zhuang", "zi", "zong"
    };
    count = sizeof(kSyllables) / sizeof(kSyllables[0]);
    return kSyllables;
}

inline char Gb2312FallbackInitialFor(uint32_t cp) {
    return Platform::SearchText::Gb2312InitialForCodepoint(cp);
}

inline char InitialFor(uint32_t cp) {
    if (const char* py = PinyinFor(cp)) return py[0];
    return Gb2312FallbackInitialFor(cp);
}

inline std::string LowerAscii(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (unsigned char c : text) {
        lowered.push_back(ToLowerAscii(c));
    }
    return lowered;
}

inline void AppendToken(std::string& out, const std::string& token) {
    if (token.empty()) return;
    if (!out.empty() && out.back() != ' ') out.push_back(' ');
    out.append(token);
}

inline std::vector<Utf8Char> DecodeUtf8(const std::string& text) {
    std::vector<Utf8Char> chars;
    chars.reserve(text.size());
    size_t offset = 0;
    Utf8Char ch;
    while (DecodeNextUtf8(text, offset, ch)) {
        chars.push_back(ch);
    }
    return chars;
}

} // namespace Detail

inline bool DeriveInitialsFromKnownPinyinToken(const std::string& token, std::string& initials) {
    initials.clear();
    if (token.empty()) return false;

    const std::string s = Detail::LowerAscii(token);
    for (unsigned char c : s) {
        if (c < 'a' || c > 'z') return false;
    }

    size_t syllableCount = 0;
    const char* const* syllables = Detail::KnownPinyinSyllables(syllableCount);

    struct Segment {
        bool can = false;
        int prev = -1;
        const char* syllable = nullptr;
    };

    std::vector<char> can(s.size() + 1, 0);
    std::vector<Segment> path(s.size() + 1);
    can[0] = 1;
    path[0].can = true;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!can[i]) continue;
        for (size_t j = 0; j < syllableCount; ++j) {
            const char* syllable = syllables[j];
            const size_t len = std::char_traits<char>::length(syllable);
            if (i + len <= s.size() && s.compare(i, len, syllable) == 0) {
                can[i + len] = 1;
                if (!path[i + len].can) {
                    path[i + len].can = true;
                    path[i + len].prev = static_cast<int>(i);
                    path[i + len].syllable = syllable;
                }
            }
        }
    }
    if (!can[s.size()]) return false;

    std::vector<char> reversed;
    for (int pos = static_cast<int>(s.size()); pos > 0; pos = path[pos].prev) {
        if (path[pos].prev < 0 || path[pos].syllable == nullptr) {
            initials.clear();
            return false;
        }
        reversed.push_back(path[pos].syllable[0]);
    }
    if (reversed.size() <= 1) {
        initials.clear();
        return false;
    }
    initials.assign(reversed.rbegin(), reversed.rend());
    return !initials.empty() && initials != s;
}

inline std::string BuildSearchDataForFts(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);

    std::string asciiToken;
    std::string compactAscii;
    std::string fullPinyin;
    std::string initials;
    std::string segmentPinyin;
    std::string segmentInitials;
    bool fullPinyinComplete = true;
    bool segmentPinyinComplete = true;
    bool segmentCrossedSeparator = false;
    bool segmentHasCjkAfterSeparator = false;

    const auto flushAscii = [&]() {
        Detail::AppendToken(out, asciiToken);
        asciiToken.clear();
    };
    const auto flushPinyin = [&]() {
        if (fullPinyinComplete) {
            Detail::AppendToken(out, fullPinyin);
        }
        Detail::AppendToken(out, initials);
        fullPinyin.clear();
        initials.clear();
        fullPinyinComplete = true;
    };
    const auto flushSegmentPinyin = [&]() {
        const bool shouldEmitSegmentAlias = segmentCrossedSeparator && segmentHasCjkAfterSeparator;
        if (shouldEmitSegmentAlias && segmentPinyinComplete) {
            Detail::AppendToken(out, segmentPinyin);
        }
        if (shouldEmitSegmentAlias) {
            Detail::AppendToken(out, segmentInitials);
        }
        segmentPinyin.clear();
        segmentInitials.clear();
        segmentPinyinComplete = true;
        segmentCrossedSeparator = false;
        segmentHasCjkAfterSeparator = false;
    };

    for (const Detail::Utf8Char& ch : Detail::DecodeUtf8(input)) {
        if (ch.codepoint < 0x80 && Detail::IsAsciiAlphaNum(static_cast<unsigned char>(ch.codepoint))) {
            flushPinyin();
            const char lowered = Detail::ToLowerAscii(static_cast<unsigned char>(ch.codepoint));
            asciiToken.push_back(lowered);
            compactAscii.push_back(lowered);
            continue;
        }

        flushAscii();
        if (Detail::IsCjk(ch.codepoint)) {
            Detail::AppendToken(out, input.substr(ch.begin, ch.end - ch.begin));
            if (const char* py = Detail::PinyinFor(ch.codepoint)) {
                if (fullPinyinComplete) {
                    fullPinyin.append(py);
                }
                initials.push_back(py[0]);
                if (segmentPinyinComplete) {
                    segmentPinyin.append(py);
                }
                segmentInitials.push_back(py[0]);
                if (segmentCrossedSeparator) {
                    segmentHasCjkAfterSeparator = true;
                }
            } else if (const char initial = Detail::Gb2312FallbackInitialFor(ch.codepoint)) {
                fullPinyinComplete = false;
                initials.push_back(initial);
                segmentPinyinComplete = false;
                segmentInitials.push_back(initial);
                if (segmentCrossedSeparator) {
                    segmentHasCjkAfterSeparator = true;
                }
            } else {
                flushPinyin();
            }
        } else {
            flushPinyin();
            if (ch.codepoint < 0x80 && std::isspace(static_cast<unsigned char>(ch.codepoint))) {
                flushSegmentPinyin();
            } else if (!segmentInitials.empty()) {
                segmentCrossedSeparator = true;
            }
        }
    }

    flushAscii();
    flushPinyin();
    flushSegmentPinyin();
    if (compactAscii.size() >= 2) {
        Detail::AppendToken(out, compactAscii);
    }
    return out;
}

inline PinyinSpan FindPinyinSpanUtf8(const std::string& text, const std::string& asciiQuery) {
    const std::string query = Detail::LowerAscii(asciiQuery);
    if (query.empty()) return {};

    const std::vector<Detail::Utf8Char> chars = Detail::DecodeUtf8(text);
    size_t i = 0;
    while (i < chars.size()) {
        if (!Detail::IsCjk(chars[i].codepoint)) {
            ++i;
            continue;
        }

        const size_t runBegin = i;
        while (i < chars.size() && Detail::IsCjk(chars[i].codepoint)) {
            ++i;
        }
        const size_t runEnd = i;

        for (size_t start = runBegin; start < runEnd; ++start) {
            std::string fullPinyin;
            std::string initials;
            bool fullPinyinComplete = true;
            for (size_t end = start; end < runEnd; ++end) {
                const char* py = Detail::PinyinFor(chars[end].codepoint);
                if (py) {
                    if (fullPinyinComplete) {
                        fullPinyin.append(py);
                    }
                    initials.push_back(py[0]);
                } else if (const char initial = Detail::Gb2312FallbackInitialFor(chars[end].codepoint)) {
                    fullPinyinComplete = false;
                    initials.push_back(initial);
                } else {
                    break;
                }

                const char* currentSyllable = Detail::PinyinFor(chars[end].codepoint);
                const bool wholeOrLeading = fullPinyinComplete &&
                    (fullPinyin == query || (end == start && currentSyllable && query == currentSyllable));
                if (wholeOrLeading || initials == query) {
                    const size_t byteStart = chars[start].begin;
                    const size_t byteEnd = chars[end].end;
                    return PinyinSpan{true, byteStart, byteEnd, text.substr(byteStart, byteEnd - byteStart)};
                }
            }
        }
    }

    return {};
}

// A cursor that moved still has later rows. The migration marker stays unset
// until a later pass finds nothing past the cursor. A failed write is not
// "still pending": retrying it forever is the read-only rebuild spin.
enum class SearchDataRecomputeStep {
    Continue,
    Finished,
    Failed,
};

inline SearchDataRecomputeStep ClassifySearchDataRecompute(
    int64_t cursor, int64_t nextCursor, bool writeSucceeded) {
    if (!writeSucceeded) return SearchDataRecomputeStep::Failed;
    if (nextCursor == cursor) return SearchDataRecomputeStep::Finished;
    return SearchDataRecomputeStep::Continue;
}

inline bool SearchDataRecomputeStillPending(int64_t cursor, int64_t nextCursor) {
    return ClassifySearchDataRecompute(cursor, nextCursor, true) == SearchDataRecomputeStep::Continue;
}

} // namespace HarkenSearch::Search
