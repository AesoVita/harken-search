#include "Database.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const std::string& label) {
    if (cond) {
        std::cout << "PASS: " << label << "\n";
        ++g_pass;
    } else {
        std::cout << "FAIL: " << label << "\n";
        ++g_fail;
    }
}

static AudioRecord makeRec(const std::string& folder,
                           const std::string& name,
                           const std::string& genre,
                           const std::string& instruments,
                           const std::string& keywords = {}) {
    AudioRecord r;
    r.fileName = name;
    r.displayName = name;
    r.filePath = folder + "\\" + name;
    r.folderPath = folder;
    r.extension = ".wav";
    r.format = "WAV";
    r.durationSeconds = 1.0f;
    r.sampleRate = 48000;
    r.channels = 2;
    r.fileSize = 1024;
    r.totalFrames = 48000;
    r.dateAdded = 1;
    r.fileMtime = 1;
    r.normGenre = genre;
    r.normInstruments = instruments;
    r.keywords = keywords;
    return r;
}

static bool hasFile(const std::vector<AudioRecord>& rows, const std::string& name) {
    for (const auto& row : rows) {
        if (row.fileName == name) return true;
    }
    return false;
}

static bool hasFacet(const std::vector<Database::TagFacetEntry>& facets,
                     const std::string& tag,
                     int count) {
    for (const auto& facet : facets) {
        if (facet.tag == tag && facet.count == count) return true;
    }
    return false;
}

static bool hasTopTag(const std::vector<std::pair<std::string, int>>& tags,
                      const std::string& tag,
                      int count) {
    for (const auto& entry : tags) {
        if (entry.first == tag && entry.second == count) return true;
    }
    return false;
}

static void expectSourceContains(const std::string& source,
                                 const std::string& needle,
                                 const std::string& label) {
    expect(source.find(needle) != std::string::npos, label);
}

int main() {
    {
        const auto workerPath = fs::path(__FILE__).parent_path().parent_path() /
            "src" / "AsyncWorker.cpp";
        std::ifstream input(workerPath);
        expect(input.good(),
               "keyword rows-first source contract can open AsyncWorker.cpp");
        if (input.good()) {
            std::ostringstream buffer;
            buffer << input.rdbuf();
            const std::string source = buffer.str();
            expectSourceContains(
                source,
                "const bool hasKeywordFilter = !req.filter.keyword.empty();",
                "keyword refresh explicitly classifies keyword filters before warm-start");
            expectSourceContains(
                source,
                "!hasKeywordFilter &&\n                    hasStructuralFilter;",
                "keyword refresh is excluded from filtered facet warm-start so first rows are not gated by facets");
            expectSourceContains(
                source,
                "AppendDbQueryPerfLog(\"[DBQuery] searchFiles (deferred): %.2f ms, items=%d\\n\"",
                "keyword refresh can still use the deferred-count rows-first DB query lane");
            expectSourceContains(
                source,
                "if (req.deferCount) {\n                    followup.totalCount = m_db->searchFilesCount(req.filter);",
                "keyword refresh keeps a deferred count/facet follow-up after first rows");
            expectSourceContains(
                source,
                "auto isRequestStale = [&]() -> bool {\n            return req.requestId < m_latestRequestId.load(std::memory_order_acquire);\n        };",
                "search worker exposes a single acquire-load stale-request gate for latest-wins checks");
            expectSourceContains(
                source,
                "SkipStaleBeforeDeferredRows",
                "rows-first refresh drops stale requests before starting the deferred paged list query");
            expectSourceContains(
                source,
                "SkipStaleAfterCount",
                "count-plus-rows refresh rechecks staleness after count before paying for the paged list query");
        }
    }

    {
        const auto mainPath = fs::path(__FILE__).parent_path().parent_path() /
            "src" / "main.cpp";
        std::ifstream input(mainPath);
        expect(input.good(),
               "main-thread facet refresh contract can open main.cpp");
        if (input.good()) {
            std::ostringstream buffer;
            buffer << input.rdbuf();
            const std::string source = buffer.str();
            expect(source.find("static void UpdateFacetCachesForFilter(AppState& state, const SearchFilter& filter)") == std::string::npos,
                   "dead synchronous UpdateFacetCachesForFilter helper is retired so filtered facets no longer have a dormant UI-thread DB entrypoint");
        }
    }

    fs::path tmpDir = fs::temp_directory_path() / "auro_db_search_facets";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    const std::string dbPath = (tmpDir / "test.db").string();
    const std::string folder = "e:\\facetlib";

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: db.open failed: " << db.getLastError() << "\n";
        return 1;
    }

    expect(db.addFile(makeRec(folder, "cinematic_boom.wav", "cinematic;impact", "drums", "metallic")),
           "fixture add cinematic_boom");
    expect(db.addFile(makeRec(folder, "cinematic_whoosh.wav", "cinematic", "synth")),
           "fixture add cinematic_whoosh");
    expect(db.addFile(makeRec(folder, "dialog_soft.wav", "dialog", "voice", "dialogue")),
           "fixture add dialog_soft");
    expect(db.addFile(makeRec(folder, "keyword_noise.wav", "texture", "voice", "drums impact")),
           "fixture add keyword_noise");

    {
        auto topTags = db.getTopTags(20);
        expect(hasTopTag(topTags, "cinematic", 2),
               "top tags fall back to normalized genre metadata when relation tags are empty");
        expect(hasTopTag(topTags, "metallic", 1),
               "top tags fall back to keyword metadata when relation tags are empty");
    }

    {
        SearchFilter filter;
        auto genreFacets = db.getTagFacets("norm_genre", filter, 20);
        expect(hasFacet(genreFacets, "cinematic", 2),
               "norm_genre facet sees ingested normalized cinematic tags");
        expect(hasFacet(genreFacets, "impact", 1),
               "norm_genre facet splits semicolon-separated tags");
    }

    {
        SearchFilter filter;
        filter.folderPath = folder;
        filter.includeGenres.push_back("cinematic");
        auto rows = db.searchFiles(filter, 20, 0);
        int64_t count = db.searchFilesCount(filter);
        expect(rows.size() == 2, "pill genre list returns two cinematic rows");
        expect(count == static_cast<int64_t>(rows.size()),
               "pill genre count matches filtered list inside folder");
        expect(!hasFile(rows, "dialog_soft.wav"),
               "pill genre list excludes non-matching folder row");

        SearchFilter dialogFilter;
        dialogFilter.folderPath = folder;
        dialogFilter.includeGenres.push_back("dialog");
        auto dialogRows = db.searchFiles(dialogFilter, 20, 0);
        int64_t dialogCount = db.searchFilesCount(dialogFilter);
        expect(dialogRows.size() == 1 && hasFile(dialogRows, "dialog_soft.wav"),
               "second pill genre list returns only dialog row");
        expect(dialogCount == static_cast<int64_t>(dialogRows.size()),
               "count cache key separates different pill genre filters");
    }

    {
        SearchFilter positive;
        positive.keyword = "metallic";
        auto positiveRows = db.searchFiles(positive, 20, 0);
        expect(hasFile(positiveRows, "cinematic_boom.wav"),
               "positive keyword finds metadata keyword field");

        SearchFilter negative;
        negative.keyword = "NOT metallic";
        auto negativeRows = db.searchFiles(negative, 20, 0);
        expect(!hasFile(negativeRows, "cinematic_boom.wav"),
               "negative keyword excludes the same keyword field positive search can find");
        expect(hasFile(negativeRows, "cinematic_whoosh.wav"),
               "negative keyword keeps unrelated rows");
    }

    {
        SearchFilter genrePower;
        genrePower.keyword = "genre:impact";
        auto genreRows = db.searchFiles(genrePower, 20, 0);
        int64_t genreCount = db.searchFilesCount(genrePower);
        expect(genreRows.size() == 1 && hasFile(genreRows, "cinematic_boom.wav"),
               "genre: power query matches norm_genre semicolon tag");
        expect(!hasFile(genreRows, "keyword_noise.wav"),
               "genre: power query ignores keyword-only false positives");
        expect(genreCount == static_cast<int64_t>(genreRows.size()),
               "genre: power query count matches list");

        SearchFilter instrumentPower;
        instrumentPower.keyword = "instrument:drums";
        auto instrumentRows = db.searchFiles(instrumentPower, 20, 0);
        int64_t instrumentCount = db.searchFilesCount(instrumentPower);
        expect(instrumentRows.size() == 1 && hasFile(instrumentRows, "cinematic_boom.wav"),
               "instrument: power query matches norm_instruments semicolon tag");
        expect(!hasFile(instrumentRows, "keyword_noise.wav"),
               "instrument: power query ignores keyword-only false positives");
        expect(instrumentCount == static_cast<int64_t>(instrumentRows.size()),
               "instrument: power query count matches list");

        SearchFilter notGenrePower;
        notGenrePower.keyword = "NOT genre:impact";
        auto notGenreRows = db.searchFiles(notGenrePower, 20, 0);
        expect(!hasFile(notGenreRows, "cinematic_boom.wav"),
               "NOT genre: power query excludes norm_genre tag");
        expect(hasFile(notGenreRows, "keyword_noise.wav"),
               "NOT genre: power query keeps keyword-only rows");

        SearchFilter notInstrumentPower;
        notInstrumentPower.keyword = "-instrument:drums";
        auto notInstrumentRows = db.searchFiles(notInstrumentPower, 20, 0);
        expect(!hasFile(notInstrumentRows, "cinematic_boom.wav"),
               "-instrument: power query excludes norm_instruments tag");
        expect(hasFile(notInstrumentRows, "keyword_noise.wav"),
               "-instrument: power query keeps keyword-only rows");
    }

    {
        expect(db.addFile(makeRec(folder, "bass_hit.wav", "impact", "voice")),
               "fixture add genre-only impact");
        expect(db.addFile(makeRec(folder, "metallic_other.wav", "texture", "voice", "metallic")),
               "fixture add keyword-only metallic");
        SearchFilter genreOrKeyword;
        genreOrKeyword.keyword = "genre:impact OR metallic";
        auto orRows = db.searchFiles(genreOrKeyword, 20, 0);
        int64_t orCount = db.searchFilesCount(genreOrKeyword);
        expect(hasFile(orRows, "bass_hit.wav"),
               "OR keeps a row matched only by normalized genre");
        expect(hasFile(orRows, "metallic_other.wav"),
               "OR keeps a row matched only by keyword text");
        expect(!hasFile(orRows, "keyword_noise.wav"),
               "OR does not treat genre field value as ordinary keyword text");
        expect(orRows.size() == 3 && orCount == static_cast<int64_t>(orRows.size()),
               "OR list and count return the same three matching rows");

        SearchFilter groupedOr = genreOrKeyword;
        groupedOr.keyword = "(genre:impact OR metallic)";
        auto groupedRows = db.searchFiles(groupedOr, 20, 0);
        expect(groupedRows.size() == 3 && hasFile(groupedRows, "bass_hit.wav"),
               "parentheses around OR retain normalized field semantics");

        SearchFilter scopedOr = genreOrKeyword;
        scopedOr.includeGenres.push_back("texture");
        auto scopedRows = db.searchFiles(scopedOr, 20, 0);
        expect(scopedRows.size() == 1 && hasFile(scopedRows, "metallic_other.wav"),
               "other active filters apply to both OR branches");

        SearchFilter genreOrInstrument;
        genreOrInstrument.keyword = "genre:impact OR instrument:voice";
        auto fieldRows = db.searchFiles(genreOrInstrument, 20, 0);
        expect(hasFile(fieldRows, "bass_hit.wav") && hasFile(fieldRows, "dialog_soft.wav"),
               "OR preserves both normalized field branches");

        SearchFilter groupedFieldOr = genreOrInstrument;
        groupedFieldOr.keyword = "genre:impact OR (instrument:voice)";
        auto groupedFieldRows = db.searchFiles(groupedFieldOr, 20, 0);
        int64_t groupedFieldCount = db.searchFilesCount(groupedFieldOr);
        expect(groupedFieldRows.size() == 5 &&
                   hasFile(groupedFieldRows, "cinematic_boom.wav") &&
                   hasFile(groupedFieldRows, "bass_hit.wav") &&
                   hasFile(groupedFieldRows, "dialog_soft.wav") &&
                   hasFile(groupedFieldRows, "keyword_noise.wav") &&
                   hasFile(groupedFieldRows, "metallic_other.wav") &&
                   !hasFile(groupedFieldRows, "cinematic_whoosh.wav"),
               "parenthesized instrument OR branch preserves normalized field union");
        expect(groupedFieldCount == static_cast<int64_t>(groupedFieldRows.size()),
               "parenthesized field OR count matches list");
    }

    db.close();
    fs::remove_all(tmpDir, ec);

    std::cout << "\n=== Summary: " << g_pass << " pass, " << g_fail << " fail ===\n";
    return g_fail == 0 ? 0 : 1;
}
