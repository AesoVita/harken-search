// test_thesaurus_bidirectional.cpp
// BUG-Search: Synonym lookup must be bidirectional.
// Verifies that hits on either `triggers` or `related` expand the FTS5 query
// with the other side, with correct cap, dedupe, case-fold, and exactMode behavior.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
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
    std::printf("=== Thesaurus Bidirectional Lookup Tests ===\n\n");

    // --- 1. Forward hit: trigger -> related ---
    // After pre-expansion + InjectCJKSpaces, a CJK related word gets per-char
    // split inside the OR group, e.g. (step* OR "啦"* "啦"* "啦"*).
    // We assert the original ASCII trigger AND the CJK char are both present.
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"啦啦啦"} } };
        auto r = ParseSearchQueryToFts5("step", /*exactMode*/false, &dict);
        CHECK(r.ok, "1.parse ok");
        CHECK(contains(r.ftsMatch, "step*"), "1.contains step* (original token)");
        CHECK(contains(r.ftsMatch, "啦"), "1.CJK related word 啦啦啦 present (per-char-split form)");
        CHECK(contains(r.ftsMatch, " OR "), "1.uses OR to combine expansions");
    }

    // --- 2. Reverse hit: related word -> triggers ---
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"啦啦啦"} } };
        auto r = ParseSearchQueryToFts5("啦啦啦", /*exactMode*/false, &dict);
        CHECK(r.ok, "2.parse ok (CJK input)");
        CHECK(contains(r.ftsMatch, "step"), "2.reverse expansion includes original trigger 'step'");
        CHECK(contains(r.ftsMatch, "啦"), "2.input CJK token still in query (per-char-split form)");
        CHECK(contains(r.ftsMatch, " OR "), "2.uses OR for reverse expansion");
    }

    // --- 3. Case-insensitive ASCII match ---
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"hop"} } };
        auto r = ParseSearchQueryToFts5("STEP", /*exactMode*/false, &dict);
        CHECK(r.ok, "3.parse ok");
        CHECK(contains(r.ftsMatch, "hop"), "3.uppercase STEP still matches lowercase trigger and expands to hop");
    }

    // --- 4. Multi-trigger / multi-related: reverse hit on a related word
    //        expands to ALL triggers PLUS the other related entries. ---
    {
        std::vector<SynonymGroup> dict = {
            { {"fire", "火"}, {"Burn", "Flame", "燃烧"} }
        };
        auto r = ParseSearchQueryToFts5("燃烧", /*exactMode*/false, &dict);
        CHECK(r.ok, "4.parse ok");
        CHECK(contains(r.ftsMatch, "fire"),  "4.expansion includes trigger 'fire'");
        CHECK(contains(r.ftsMatch, "火"),    "4.expansion includes trigger '火' (single CJK char)");
        CHECK(contains(r.ftsMatch, "Burn"),  "4.expansion includes other related 'Burn'");
        CHECK(contains(r.ftsMatch, "Flame"), "4.expansion includes other related 'Flame'");
        CHECK(contains(r.ftsMatch, "燃"),    "4.input '燃烧' present (per-char-split form)");
    }

    // --- 5. kMaxSynonymExpansion=8 cap respected ---
    {
        std::vector<SynonymGroup> dict = {
            { {"x"}, {"a","b","c","d","e","f","g","h","i","j","k","l"} }
        };
        auto r = ParseSearchQueryToFts5("x", /*exactMode*/false, &dict);
        CHECK(r.ok, "5.parse ok");
        // Count OR occurrences: with cap=8, expect 8 OR'd expansions => 8 ORs.
        int orCount = 0;
        size_t pos = 0;
        while ((pos = r.ftsMatch.find(" OR ", pos)) != std::string::npos) { ++orCount; pos += 4; }
        CHECK(orCount == 8, "5.expansion capped at 8 (8 OR separators between 1 base + 8 expansions)");
    }

    // --- 6. No-match returns no expansion (no regression) ---
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"hop"} } };
        auto r = ParseSearchQueryToFts5("walk", /*exactMode*/false, &dict);
        CHECK(r.ok, "6.parse ok");
        CHECK(!contains(r.ftsMatch, "step"), "6.unrelated input does NOT pull in step");
        CHECK(!contains(r.ftsMatch, "hop"),  "6.unrelated input does NOT pull in hop");
        CHECK(contains(r.ftsMatch, "walk*"), "6.original token preserved as walk*");
    }

    // --- 7. exactMode disables synonym expansion (existing behavior) ---
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"啦啦啦"} } };
        auto r = ParseSearchQueryToFts5("step", /*exactMode*/true, &dict);
        CHECK(r.ok, "7.parse ok");
        CHECK(!contains(r.ftsMatch, "啦啦啦"), "7.exactMode suppresses synonym expansion");
        CHECK(!contains(r.ftsMatch, "*"),     "7.exactMode also suppresses prefix wildcards");
    }

    // --- 8a. Trigram substring path (nameContains) survives synonym expansion.
    // Regression: pre-expansion turns Term(step) into Group(Or(...)), which makes
    // isPositiveConjunctionOnly() return false and silently empties nameContains —
    // killing the file-name substring trigram lookup that backs "step → footstep.wav".
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"啦啦啦"} } };
        auto r = ParseSearchQueryToFts5("step", /*exactMode*/false, &dict);
        CHECK(r.ok, "8a.parse ok");
        CHECK(r.nameContains.size() == 1 && r.nameContains[0] == "step",
              "8a.original 'step' still routed to nameContains for trigram fallback");
    }
    {
        std::vector<SynonymGroup> dict = { { {"step"}, {"啦啦啦"} } };
        auto r = ParseSearchQueryToFts5("啦啦啦", /*exactMode*/false, &dict);
        CHECK(r.ok, "8b.parse ok (CJK reverse)");
        // CJK chars are <3 bytes per char after split, so single 啦 is filtered out
        // by collectTrigramTerms (size < 3). That's existing behavior — we just
        // assert pre-expansion doesn't crash or produce surprising entries.
        CHECK(r.nameContains.empty() ||
              std::all_of(r.nameContains.begin(), r.nameContains.end(),
                          [](const std::string& s){ return s.size() >= 3; }),
              "8b.no sub-3-byte tokens leak into nameContains");
    }

    // --- 9. Self-token is excluded from expansion (no x OR x) ---
    {
        std::vector<SynonymGroup> dict = {
            { {"step", "Step"}, {"hop"} }  // "step" appears in triggers list
        };
        auto r = ParseSearchQueryToFts5("step", /*exactMode*/false, &dict);
        CHECK(r.ok, "9.parse ok");
        // "Step" (case-equiv) should NOT appear as a duplicate expansion of "step"
        // We expect: (step* OR hop*) — only ONE occurrence of step ignoring case.
        size_t firstStep = r.ftsMatch.find("step");
        size_t secondStep = (firstStep == std::string::npos) ? std::string::npos
                            : r.ftsMatch.find("step", firstStep + 1);
        size_t firstStepCap = r.ftsMatch.find("Step");
        CHECK(secondStep == std::string::npos && firstStepCap == std::string::npos,
              "9.case-equivalent self-token deduplicated");
        CHECK(contains(r.ftsMatch, "hop"), "9.legitimate expansion still present");
    }

    std::printf("\n=== %d passed, %d failed ===\n", passCount, failCount);
    return failCount == 0 ? 0 : 1;
}
