// test_tag_vocabulary_mapper.cpp — Unit tests for TagVocabularyMapper
#include <cstdio>
#include <string>
#include <vector>
#include "../src/TagVocabularyMapper.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== TagVocabularyMapper Tests ===\n");

// mapGenre
    std::printf("\n--- mapGenre ---\n");
    {
        auto r = TagVocabularyMapper::mapGenre("Rock");
        CHECK(!r.empty(), "mapGenre('Rock') returns non-empty");

        auto empty = TagVocabularyMapper::mapGenre("");
        // empty input should return "" (no mapping)
        CHECK(empty.empty(), "mapGenre('') returns empty");

        auto unknown = TagVocabularyMapper::mapGenre("ZZZZZ_NONEXISTENT_GENRE_12345");
        CHECK(unknown.empty(), "mapGenre unknown returns empty");
    }

// mapInstruments
    std::printf("\n--- mapInstruments ---\n");
    {
        auto r = TagVocabularyMapper::mapInstruments("Piano");
        CHECK(!r.empty(), "mapInstruments('Piano') non-empty");

        auto multi = TagVocabularyMapper::mapInstruments("Piano, Guitar");
        CHECK(multi.size() >= 2, "mapInstruments comma-separated: >= 2 results");

        auto semicolon = TagVocabularyMapper::mapInstruments("Piano; Guitar");
        CHECK(semicolon.size() >= 2, "mapInstruments semicolon-separated: >= 2 results");

        auto empty = TagVocabularyMapper::mapInstruments("");
        CHECK(empty.empty(), "mapInstruments('') returns empty");
    }

// mapMood
    std::printf("\n--- mapMood ---\n");
    {
        auto r = TagVocabularyMapper::mapMood("Tense");
        CHECK(!r.empty(), "mapMood('Tense') non-empty");

        auto empty = TagVocabularyMapper::mapMood("");
        CHECK(empty.empty(), "mapMood('') returns empty");

        auto unknown = TagVocabularyMapper::mapMood("ZZZZZ_NONEXISTENT_MOOD_12345");
        CHECK(unknown.empty(), "mapMood unknown returns empty");
    }

// inferVocalType
    std::printf("\n--- inferVocalType ---\n");
    {
        auto instrumental = TagVocabularyMapper::inferVocalType("Ambient", "atmospheric pad");
        // should return "instrumental" or "" depending on implementation
        CHECK(instrumental == "instrumental" || instrumental.empty(),
              "inferVocalType ambient returns instrumental or empty");

        auto vocals = TagVocabularyMapper::inferVocalType("Pop", "female vocals, pop song");
        CHECK(vocals == "has_vocals" || vocals == "has_lyrics" || !vocals.empty(),
              "inferVocalType with vocal hints returns non-empty");

        auto none = TagVocabularyMapper::inferVocalType("", "");
        // empty inputs should not crash
        CHECK(true, "inferVocalType with empty inputs doesn't crash");
    }

// normalize
    std::printf("\n--- normalize ---\n");
    {
        std::string normGenre, normInstruments, normMood, vocalType;
        TagVocabularyMapper::normalize("Rock", "Guitar, Drums",
                                       normGenre, normInstruments, normMood, vocalType);

        CHECK(!normGenre.empty(), "normalize: genre mapped");
        CHECK(!normInstruments.empty(), "normalize: instruments mapped");
        // mood and vocalType may be empty depending on heuristics
    }
    {
        std::string normGenre, normInstruments, normMood, vocalType;
        TagVocabularyMapper::normalize("", "", normGenre, normInstruments, normMood, vocalType);
        // empty inputs must not crash
        CHECK(true, "normalize with empty inputs doesn't crash");
    }

    std::printf("\n=== TagVocabularyMapper: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
