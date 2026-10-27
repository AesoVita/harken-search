// test_tag_system.cpp — Unit tests for TagSystem (F-007)
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include "../src/TagSystem.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== TagSystem Tests ===\n");

// Initialize
    TagSystemManager::initialize();
    CHECK(TagSystemManager::isInitialized(), "initialize() sets initialized flag");
    CHECK(!TagSystemManager::getAllTags().empty(), "getAllTags() returns non-empty after init");
    CHECK(TagSystemManager::getAllTags().size() > 50, "getAllTags() has 50+ entries");

// getAllDimensions
    auto dims = TagSystemManager::getAllDimensions();
    CHECK(dims.size() == 9, "getAllDimensions() returns 9 dimensions");

// getTagsByDimension
    auto catTags = TagSystemManager::getTagsByDimension(TagDimension::Category);
    CHECK(!catTags.empty(), "Category dimension has tags");
    auto moodTags = TagSystemManager::getTagsByDimension(TagDimension::Mood);
    CHECK(!moodTags.empty(), "Mood dimension has tags");
    auto usageTags = TagSystemManager::getTagsByDimension(TagDimension::Usage);
    CHECK(!usageTags.empty(), "Usage dimension has tags");

// getDimensionName
    CHECK(std::string(TagSystemManager::getDimensionName(TagDimension::Category)) == "Category",
          "getDimensionName(Category) == 'Category'");
    CHECK(std::string(TagSystemManager::getDimensionName(TagDimension::Mood)) == "Mood",
          "getDimensionName(Mood) == 'Mood'");
    CHECK(std::string(TagSystemManager::getDimensionName(TagDimension::FeaturedInstrument)) == "Instrument",
          "getDimensionName(FeaturedInstrument) == 'Instrument'");
    CHECK(std::string(TagSystemManager::getDimensionName(TagDimension::BPMRange)) == "BPM Range",
          "getDimensionName(BPMRange) == 'BPM Range'");

// getDimensionNameZh
    CHECK(std::string(TagSystemManager::getDimensionNameZh(TagDimension::Category)).size() > 0,
          "getDimensionNameZh(Category) returns non-empty UTF-8");
    CHECK(std::string(TagSystemManager::getDimensionNameZh(TagDimension::Tempo)).size() > 0,
          "getDimensionNameZh(Tempo) returns non-empty UTF-8");

// autoTempoFromBPM boundary values
    std::printf("\n--- autoTempoFromBPM ---\n");
    CHECK(TagSystemManager::autoTempoFromBPM("") == "",           "empty bpm -> empty");
    CHECK(TagSystemManager::autoTempoFromBPM("3") == "",          "bpm<=5 -> empty");
    CHECK(TagSystemManager::autoTempoFromBPM("5") == "",          "bpm==5 -> empty");
    CHECK(TagSystemManager::autoTempoFromBPM("30") == "Very Slow","bpm<40 -> Very Slow");
    CHECK(TagSystemManager::autoTempoFromBPM("39") == "Very Slow","bpm=39 -> Very Slow");
    CHECK(TagSystemManager::autoTempoFromBPM("40") == "Slow",     "bpm=40 -> Slow");
    CHECK(TagSystemManager::autoTempoFromBPM("72") == "Slow",     "bpm=72 -> Slow");
    CHECK(TagSystemManager::autoTempoFromBPM("73") == "Medium Slow","bpm=73 -> Medium Slow");
    CHECK(TagSystemManager::autoTempoFromBPM("90") == "Medium Slow","bpm=90 -> Medium Slow");
    CHECK(TagSystemManager::autoTempoFromBPM("91") == "Medium",   "bpm=91 -> Medium");
    CHECK(TagSystemManager::autoTempoFromBPM("110") == "Medium",  "bpm=110 -> Medium");
    CHECK(TagSystemManager::autoTempoFromBPM("111") == "Medium Fast","bpm=111 -> Medium Fast");
    CHECK(TagSystemManager::autoTempoFromBPM("130") == "Medium Fast","bpm=130 -> Medium Fast");
    CHECK(TagSystemManager::autoTempoFromBPM("131") == "Fast",    "bpm=131 -> Fast");
    CHECK(TagSystemManager::autoTempoFromBPM("160") == "Fast",    "bpm=160 -> Fast");
    CHECK(TagSystemManager::autoTempoFromBPM("161") == "Very Fast","bpm=161 -> Very Fast");
    CHECK(TagSystemManager::autoTempoFromBPM("abc") == "",        "non-numeric -> empty");

// autoDurationTag boundary values
    std::printf("\n--- autoDurationTag ---\n");
    CHECK(TagSystemManager::autoDurationTag(0)   == "",           "dur<=0 -> empty");
    CHECK(TagSystemManager::autoDurationTag(-1)  == "",           "dur<0 -> empty");
    CHECK(TagSystemManager::autoDurationTag(1)   == ":01 - :15", "dur=1 -> :01-:15");
    CHECK(TagSystemManager::autoDurationTag(15)  == ":01 - :15", "dur=15 -> :01-:15");
    CHECK(TagSystemManager::autoDurationTag(16)  == ":16 - :30", "dur=16 -> :16-:30");
    CHECK(TagSystemManager::autoDurationTag(30)  == ":16 - :30", "dur=30 -> :16-:30");
    CHECK(TagSystemManager::autoDurationTag(31)  == ":31 - 1:00","dur=31 -> :31-1:00");
    CHECK(TagSystemManager::autoDurationTag(60)  == ":31 - 1:00","dur=60 -> :31-1:00");
    CHECK(TagSystemManager::autoDurationTag(61)  == "1:01 - 2:59","dur=61 -> 1:01-2:59");
    CHECK(TagSystemManager::autoDurationTag(180) == "1:01 - 2:59","dur=180 -> 1:01-2:59");
    CHECK(TagSystemManager::autoDurationTag(181) == "3:00+",     "dur=181 -> 3:00+");

// autoBPMRangeTag
    std::printf("\n--- autoBPMRangeTag ---\n");
    CHECK(TagSystemManager::autoBPMRangeTag("").empty(),    "empty -> empty");
    CHECK(TagSystemManager::autoBPMRangeTag("5").empty(),   "bpm<=5 -> empty");
    CHECK(TagSystemManager::autoBPMRangeTag("30").find("Less than 40") != std::string::npos,
          "bpm=30 -> contains 'Less than 40'");
    CHECK(TagSystemManager::autoBPMRangeTag("100").find("Medium") != std::string::npos,
          "bpm=100 -> contains 'Medium'");
    CHECK(TagSystemManager::autoBPMRangeTag("170").find("More than 160") != std::string::npos,
          "bpm=170 -> contains 'More than 160'");

// splitTags / joinTags
    std::printf("\n--- splitTags / joinTags ---\n");
    auto split = TagSystemManager::splitTags("rock, pop,  jazz ");
    CHECK(split.size() == 3, "splitTags('rock, pop, jazz') -> 3 items");
    CHECK(split[0] == "rock", "splitTags first item trimmed");
    CHECK(split[2] == "jazz", "splitTags last item trimmed");

    auto joined = TagSystemManager::joinTags({"rock", "pop", "jazz"});
    CHECK(joined == "rock, pop, jazz", "joinTags round-trip");

    auto joinedDedup = TagSystemManager::joinTags({"Rock", "rock", "POP", "pop"});
    CHECK(joinedDedup == "Rock, POP", "joinTags deduplicates case-insensitively");

    CHECK(TagSystemManager::splitTags("").empty(), "splitTags empty -> empty");
    CHECK(TagSystemManager::splitTags(",,,").empty(), "splitTags only commas -> empty");

// FacetFilterState
    std::printf("\n--- FacetFilterState ---\n");
    TagSystemManager::clearAllFacets();
    CHECK(!TagSystemManager::hasAnyFacetFilter(), "no facet after clearAll");

    auto& catFacet = TagSystemManager::getFacetState(TagDimension::Category);
    catFacet.toggle("Rock");
    CHECK(catFacet.hasFilter(), "facet has filter after toggle");
    CHECK(catFacet.isSelected("Rock"), "Rock is selected");
    CHECK(!catFacet.isSelected("Pop"), "Pop is not selected");
    CHECK(TagSystemManager::hasAnyFacetFilter(), "hasAnyFacetFilter true");

    catFacet.toggle("Rock");
    CHECK(!catFacet.hasFilter(), "facet empty after toggle off");
    CHECK(!TagSystemManager::hasAnyFacetFilter(), "hasAnyFacetFilter false after clear");

// matchesFacetFilter
    std::printf("\n--- matchesFacetFilter ---\n");
    TagSystemManager::clearAllFacets();
    std::unordered_map<std::string, std::string> fields = {
        {"Category", "Rock, Pop"},
        {"Mood", "Happy"}
    };
    CHECK(TagSystemManager::matchesFacetFilter(fields), "no active facets -> true");

    TagSystemManager::getFacetState(TagDimension::Category).toggle("Rock");
    CHECK(TagSystemManager::matchesFacetFilter(fields), "Category=Rock in 'Rock, Pop' -> true");

    TagSystemManager::getFacetState(TagDimension::Category).toggle("Rock");
    TagSystemManager::getFacetState(TagDimension::Category).toggle("Jazz");
    CHECK(!TagSystemManager::matchesFacetFilter(fields), "Category=Jazz not in 'Rock, Pop' -> false");

    TagSystemManager::clearAllFacets();
    TagSystemManager::getFacetState(TagDimension::Mood).toggle("Happy");
    CHECK(TagSystemManager::matchesFacetFilter(fields), "Mood=Happy matches");

    TagSystemManager::getFacetState(TagDimension::Category).toggle("Pop");
    CHECK(TagSystemManager::matchesFacetFilter(fields), "Mood=Happy AND Category=Pop -> true (AND across dims)");

    TagSystemManager::clearAllFacets();

// searchTags
    std::printf("\n--- searchTags ---\n");
    auto results = TagSystemManager::searchTags("Rock", 5);
    CHECK(!results.empty(), "searchTags('Rock') returns results");

    auto noResults = TagSystemManager::searchTags("xyznonexistent123", 5);
    CHECK(noResults.empty(), "searchTags nonsense returns empty");

// Summary
    std::printf("\n=== TagSystem: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
