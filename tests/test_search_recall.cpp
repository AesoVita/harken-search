// test_search_recall.cpp
// F-034 Search Recall Enhancement — unit tests for prefix expansion
// Validates that short tokens, CJK input, and field syntax emit correct FTS5 MATCH.
// Returns 0 if all pass, 1 if any fail.

#include <cstdio>
#include <cstring>
#include <string>
#include "../src/SearchQuery.h"

static int passCount = 0;
static int failCount = 0;

#define CHECK(cond, name) \
    do { \
        if (cond) { std::printf("PASS: %s\n", name); ++passCount; } \
        else       { std::printf("FAIL: %s\n", name); ++failCount; } \
    } while(0)

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int main() {
    std::printf("=== F-034 Search Recall Tests ===\n\n");

    // --- 1. Short ASCII prefix expansion ---
    {
        auto r = ParseSearchQueryToFts5("r");
        CHECK(r.ok, "1-char query parses OK");
        CHECK(contains(r.ftsMatch, "r*"), "1-char 'r' gets prefix star");
    }
    {
        auto r = ParseSearchQueryToFts5("fi");
        CHECK(r.ok, "2-char query parses OK");
        CHECK(contains(r.ftsMatch, "fi*"), "2-char 'fi' gets prefix star");
    }
    {
        auto r = ParseSearchQueryToFts5("Fi");
        CHECK(r.ok, "2-char mixed case parses OK");
        CHECK(contains(r.ftsMatch, "Fi*"),
              "2-char 'Fi' gets prefix star (token preserved as-is, FTS handles case)");
    }

    // --- 2. 3+ char prefix (was already working) ---
    {
        auto r = ParseSearchQueryToFts5("loo");
        CHECK(r.ok, "3-char query parses OK");
        CHECK(contains(r.ftsMatch, "loo*"), "3-char 'loo' gets prefix star");
    }
    {
        auto r = ParseSearchQueryToFts5("fire");
        CHECK(r.ok, "4-char query parses OK");
        CHECK(contains(r.ftsMatch, "fire*"), "4-char 'fire' gets prefix star");
    }

    // --- 3. Multi-word query ---
    {
        auto r = ParseSearchQueryToFts5("lo fi");
        CHECK(r.ok, "Multi-word short query parses OK");
        CHECK(contains(r.ftsMatch, "lo*"), "'lo' in multi-word gets prefix");
        CHECK(contains(r.ftsMatch, "fi*"), "'fi' in multi-word gets prefix");
    }

    // --- 4. Phrase query (no prefix expansion) ---
    {
        auto r = ParseSearchQueryToFts5("\"fire hit\"");
        CHECK(r.ok, "Phrase query parses OK");
        CHECK(!contains(r.ftsMatch, "*"), "Phrase 'fire hit' does NOT get star");
        CHECK(contains(r.ftsMatch, "\"fire hit\""), "Phrase preserved");
    }

    // --- 5. Field query preserves field prefix ---
    {
        auto r = ParseSearchQueryToFts5("artist:fi");
        CHECK(r.ok, "Field query parses OK");
        CHECK(contains(r.ftsMatch, "artist:"), "Field name preserved");
        CHECK(contains(r.ftsMatch, "fi*"), "Field value gets prefix");
    }

    // --- 6. OR query ---
    {
        auto r = ParseSearchQueryToFts5("fire OR loop");
        CHECK(r.ok, "OR query parses OK");
        CHECK(contains(r.ftsMatch, "fire*"), "'fire' in OR gets prefix");
        CHECK(contains(r.ftsMatch, " OR "), "OR operator preserved");
        CHECK(contains(r.ftsMatch, "loop*"), "'loop' in OR gets prefix");
    }

    // --- 7. Exclusion query ---
    // Purely-negative queries ("-bad") route excluded terms to r.excludeTerms
    // and leave r.ftsMatch empty — FTS5 cannot match "NOT term*" with no
    // positive term (returns 0 rows). Database.cpp uses SQL NOT LIKE instead.
    {
        auto r = ParseSearchQueryToFts5("-bad");
        CHECK(r.ok, "Exclusion query parses OK");
        CHECK(!r.excludeTerms.empty(), "Excluded term collected in excludeTerms");
        CHECK(!r.excludeTerms.empty() && r.excludeTerms[0] == "bad", "Excluded term value is 'bad'");
        CHECK(r.ftsMatch.empty(), "ftsMatch empty for purely-negative query");
    }

    // --- 8. Path/tag/color (non-FTS) extraction ---
    {
        auto r = ParseSearchQueryToFts5("path:sfx fire");
        CHECK(r.ok, "path: + keyword parses OK");
        CHECK(r.pathContains.size() == 1 && r.pathContains[0] == "sfx", "path: extracted");
        CHECK(contains(r.ftsMatch, "fire*"), "Keyword alongside path: gets prefix");
    }
    {
        auto r = ParseSearchQueryToFts5("tag:rock");
        CHECK(r.ok, "tag: query parses OK");
        CHECK(r.tagFilters.size() == 1 && r.tagFilters[0] == "rock", "tag: extracted");
    }
    {
        auto r = ParseSearchQueryToFts5("color:red");
        CHECK(r.ok, "color: query parses OK");
        CHECK(r.colorFilters.size() == 1 && r.colorFilters[0] == "red", "color: extracted");
    }

    // --- 9. CJK input ---
    {
        auto r = ParseSearchQueryToFts5("fire");
        CHECK(r.ok, "Pure ASCII query parses OK");
    }

    // --- 10. User-provided trailing star (should not double-star) ---
    {
        auto r = ParseSearchQueryToFts5("loo*");
        CHECK(r.ok, "Explicit star parses OK");
        CHECK(r.ftsMatch == "loo*", "No double star");
    }

    // --- 11. Exact mode (no prefix expansion) ---
    {
        auto r = ParseSearchQueryToFts5("r", /*exactMode*/true);
        CHECK(r.ok, "exact: 1-char query parses OK");
        CHECK(r.ftsMatch == "r", "exact: 'r' has NO star");
    }
    {
        auto r = ParseSearchQueryToFts5("fire", /*exactMode*/true);
        CHECK(r.ok, "exact: 4-char query parses OK");
        CHECK(r.ftsMatch == "fire", "exact: 'fire' has NO star");
    }
    {
        auto r = ParseSearchQueryToFts5("lo fi", /*exactMode*/true);
        CHECK(r.ok, "exact: multi-word parses OK");
        CHECK(!contains(r.ftsMatch, "*"), "exact: multi-word has NO star");
    }
    {
        auto r = ParseSearchQueryToFts5("loo*", /*exactMode*/true);
        CHECK(r.ok, "exact: explicit star preserved");
        CHECK(r.ftsMatch == "loo*", "exact: user star kept as-is");
    }

    // --- 12. Synonym expansion (cross-language recall) ---
    {
        std::vector<SynonymGroup> syns = {
            { {"fire", "火"}, {"Flame", "Burn", "Blaze", "火", "燃烧"} },
            { {"hit", "打"}, {"Impact", "Punch", "Smash", "打击"} }
        };

        // 12a: English trigger expands to OR group
        {
            auto r = ParseSearchQueryToFts5("fire", false, &syns);
            CHECK(r.ok, "synonym: 'fire' parses OK");
            CHECK(contains(r.ftsMatch, "fire*"), "synonym: original token kept");
            CHECK(contains(r.ftsMatch, " OR "), "synonym: OR expansion present");
            CHECK(contains(r.ftsMatch, "Flame*"), "synonym: 'Flame' in expansion");
            CHECK(contains(r.ftsMatch, "Burn*"), "synonym: 'Burn' in expansion");
        }

        // 12b: Chinese trigger expands
        {
            auto r = ParseSearchQueryToFts5("火", false, &syns);
            CHECK(r.ok, "synonym: '火' parses OK");
            CHECK(contains(r.ftsMatch, " OR "), "synonym: CJK trigger expands");
            CHECK(contains(r.ftsMatch, "Flame*"), "synonym: '火' expands to 'Flame'");
        }

        // 12c: No expansion in exact mode
        {
            auto r = ParseSearchQueryToFts5("fire", true, &syns);
            CHECK(r.ok, "synonym+exact: parses OK");
            CHECK(!contains(r.ftsMatch, " OR "), "synonym+exact: no OR expansion");
            CHECK(r.ftsMatch == "fire", "synonym+exact: plain token only");
        }

        // 12d: Non-matching word has no expansion
        {
            auto r = ParseSearchQueryToFts5("kick", false, &syns);
            CHECK(r.ok, "synonym: 'kick' parses OK");
            CHECK(!contains(r.ftsMatch, " OR "), "synonym: 'kick' has no expansion");
            CHECK(contains(r.ftsMatch, "kick*"), "synonym: 'kick' gets prefix only");
        }

        // 12e: Multi-word query, only matching terms expand
        {
            auto r = ParseSearchQueryToFts5("fire kick", false, &syns);
            CHECK(r.ok, "synonym: multi-word parses OK");
            CHECK(contains(r.ftsMatch, "fire*"), "synonym: 'fire' present in multi-word");
            CHECK(contains(r.ftsMatch, "Flame*"), "synonym: 'fire' expands in multi-word");
            CHECK(contains(r.ftsMatch, "kick*"), "synonym: 'kick' present in multi-word");
        }

        // 12f: Phrase queries do NOT expand synonyms
        {
            auto r = ParseSearchQueryToFts5("\"fire hit\"", false, &syns);
            CHECK(r.ok, "synonym: phrase parses OK");
            CHECK(!contains(r.ftsMatch, " OR "), "synonym: phrase has no OR expansion");
        }

        // 12g: No synonyms pointer → no expansion
        {
            auto r = ParseSearchQueryToFts5("fire", false, nullptr);
            CHECK(r.ok, "synonym: null ptr parses OK");
            CHECK(!contains(r.ftsMatch, " OR "), "synonym: null ptr has no expansion");
            CHECK(contains(r.ftsMatch, "fire*"), "synonym: null ptr prefix-only");
        }
    }

    // --- Summary ---
    std::printf("\n=== Results: %d passed, %d failed ===\n", passCount, failCount);
    return failCount > 0 ? 1 : 0;
}
