// test_not_chip.cpp - unit tests for shared search query chip parsing.

#include "../src/SearchQuery.h"

#include <cstdio>
#include <string>
#include <vector>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { std::printf("PASS: %s\n", msg); ++g_pass; } \
    else { std::printf("FAIL: %s\n", msg); ++g_fail; } \
} while (0)

static bool chipIs(const SearchQueryChip& chip,
                   SearchQueryChipKind kind,
                   const std::string& field,
                   const std::string& label) {
    return chip.kind == kind && chip.field == field && chip.label == label;
}

int main() {
    {
        auto chips = BuildSearchQueryChips("NOT music NOT speech");
        CHECK(chips.size() == 2, "NOT music NOT speech -> 2 chips");
        CHECK(chipIs(chips[0], SearchQueryChipKind::ExcludeKeyword, "", "music"),
              "first NOT chip is ExcludeKeyword music");
        CHECK(chipIs(chips[1], SearchQueryChipKind::ExcludeKeyword, "", "speech"),
              "second NOT chip is ExcludeKeyword speech");
    }

    {
        auto chips = BuildSearchQueryChips("rain");
        CHECK(chips.size() == 1, "rain -> 1 chip");
        CHECK(chipIs(chips[0], SearchQueryChipKind::Keyword, "", "rain"),
              "plain word is Keyword");
    }

    {
        auto chips = BuildSearchQueryChips("NOT");
        CHECK(chips.size() == 1, "standalone NOT -> 1 chip");
        CHECK(chipIs(chips[0], SearchQueryChipKind::Operator, "", "NOT"),
              "standalone NOT is Operator");
    }

    {
        auto chips = BuildSearchQueryChips("hit OR whoosh");
        CHECK(chips.size() == 3, "hit OR whoosh -> 3 chips");
        CHECK(chipIs(chips[0], SearchQueryChipKind::Keyword, "", "hit"),
              "first chip is Keyword hit");
        CHECK(chipIs(chips[1], SearchQueryChipKind::Operator, "", "OR"),
              "middle chip is Operator OR");
        CHECK(chipIs(chips[2], SearchQueryChipKind::Keyword, "", "whoosh"),
              "third chip is Keyword whoosh");
        std::string withoutOr = RemoveSearchQueryChipFromText("hit OR whoosh", chips[1]);
        CHECK(withoutOr == "hit whoosh",
              "removing OR operator chip keeps surrounding keywords");
    }

    {
        auto chips = BuildSearchQueryChips("not music");
        CHECK(chips.size() == 1, "lowercase not music -> 1 chip");
        CHECK(chipIs(chips[0], SearchQueryChipKind::ExcludeKeyword, "", "music"),
              "lowercase not is ExcludeKeyword");
    }

    {
        auto chips = BuildSearchQueryChips("rain NOT wind");
        CHECK(chips.size() == 2, "rain NOT wind -> 2 chips");
        CHECK(chipIs(chips[0], SearchQueryChipKind::Keyword, "", "rain"),
              "mixed first chip is Keyword rain");
        CHECK(chipIs(chips[1], SearchQueryChipKind::ExcludeKeyword, "", "wind"),
              "mixed second chip is ExcludeKeyword wind");
    }

    {
        auto chips = BuildSearchQueryChips("artist:Ben path:SFX rain");
        CHECK(chips.size() == 3, "fielded query -> 3 chips");
        CHECK(chipIs(chips[0], SearchQueryChipKind::Field, "artist", "Ben"),
              "artist:Ben is Field chip");
        CHECK(chipIs(chips[1], SearchQueryChipKind::Field, "path", "SFX"),
              "path:SFX is Field chip");
        CHECK(chipIs(chips[2], SearchQueryChipKind::Keyword, "", "rain"),
              "bare word after fields remains Keyword");
    }

    {
        auto chips = BuildSearchQueryChips("NOT category:noise -tag:bad");
        CHECK(chips.size() == 2, "NOT field and minus field -> 2 chips");
        CHECK(chipIs(chips[0], SearchQueryChipKind::ExcludeField, "category", "noise"),
              "NOT category:noise is ExcludeField");
        CHECK(chipIs(chips[1], SearchQueryChipKind::ExcludeField, "tag", "bad"),
              "-tag:bad is ExcludeField");
    }

    {
        auto chips = BuildSearchQueryChips("artist:\"Ben Burtt\" \"laser blast\"");
        CHECK(chips.size() == 2, "quoted field and phrase -> 2 chips");
        CHECK(chipIs(chips[0], SearchQueryChipKind::Field, "artist", "Ben Burtt"),
              "quoted artist field label strips quotes");
        CHECK(chipIs(chips[1], SearchQueryChipKind::Keyword, "", "laser blast"),
              "quoted phrase label strips quotes");
    }

    {
        auto chips = BuildSearchQueryChips("artist:Ben path:SFX NOT noisy");
        CHECK(chips.size() == 3, "setup for removal has 3 chips");
        std::string withoutPath = RemoveSearchQueryChipFromText("artist:Ben path:SFX NOT noisy", chips[1]);
        CHECK(withoutPath == "artist:Ben NOT noisy",
              "removing field chip removes only the field token");
        std::string withoutNot = RemoveSearchQueryChipFromText("artist:Ben path:SFX NOT noisy", chips[2]);
        CHECK(withoutNot == "artist:Ben path:SFX",
              "removing NOT chip removes both NOT and value token");
    }

    std::printf("\n=== Search Query Chips: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
