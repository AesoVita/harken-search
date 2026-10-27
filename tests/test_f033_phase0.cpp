// test_f033_phase0.cpp
// F-033 Music Mode — Phase 0 QA tests
//   QA-1: DB schema migration checks (opens real auro.db)
//   QA-3: TagVocabularyMapper unit tests
//
// Returns 0 if all pass, 1 if any fail.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../thirdparty/sqlite/sqlite3.h"
#include "../src/TagVocabularyMapper.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int passCount = 0;
static int failCount = 0;

#define CHECK(cond, name) \
    do { \
        if (cond) { std::printf("PASS: %s\n", name); ++passCount; } \
        else       { std::printf("FAIL: %s\n", name); ++failCount; } \
    } while(0)

// Try executing a SQL statement against db.
// Returns true if sqlite3_prepare_v2 + sqlite3_step succeed without error
// (SQLITE_OK, SQLITE_ROW, or SQLITE_DONE all count as success).
static bool sqlOk(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE || rc == SQLITE_OK);
}

// Try executing a SQL statement and retrieve the first column of the first row
// as a string. Returns false if no row returned or prepare failed.
static bool sqlValue(sqlite3* db, const char* sql, std::string& out) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    rc = sqlite3_step(stmt);
    bool got = (rc == SQLITE_ROW);
    if (got) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out = text ? text : "";
    }
    sqlite3_finalize(stmt);
    return got;
}

// ---------------------------------------------------------------------------
// QA-1: Schema migration checks
// ---------------------------------------------------------------------------

static void runQA1(const char* dbPath) {
    std::printf("\n=== QA-1: DB Schema Migration (%s) ===\n", dbPath);

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(dbPath, &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        std::printf("SKIP (cannot open DB): %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // audio_files column checks
    CHECK(sqlOk(db, "SELECT album_artist     FROM audio_files LIMIT 0"), "column album_artist");
    CHECK(sqlOk(db, "SELECT track_number     FROM audio_files LIMIT 0"), "column track_number");
    CHECK(sqlOk(db, "SELECT disc_number      FROM audio_files LIMIT 0"), "column disc_number");
    CHECK(sqlOk(db, "SELECT replay_gain_track FROM audio_files LIMIT 0"), "column replay_gain_track");
    CHECK(sqlOk(db, "SELECT replay_gain_album FROM audio_files LIMIT 0"), "column replay_gain_album");
    CHECK(sqlOk(db, "SELECT raw_genre        FROM audio_files LIMIT 0"), "column raw_genre");
    CHECK(sqlOk(db, "SELECT norm_genre       FROM audio_files LIMIT 0"), "column norm_genre");
    CHECK(sqlOk(db, "SELECT norm_instruments FROM audio_files LIMIT 0"), "column norm_instruments");
    CHECK(sqlOk(db, "SELECT norm_mood_tags   FROM audio_files LIMIT 0"), "column norm_mood_tags");
    CHECK(sqlOk(db, "SELECT vocal_type       FROM audio_files LIMIT 0"), "column vocal_type");
    CHECK(sqlOk(db, "SELECT energy_tension   FROM audio_files LIMIT 0"), "column energy_tension");
    CHECK(sqlOk(db, "SELECT energy_drive     FROM audio_files LIMIT 0"), "column energy_drive");
    CHECK(sqlOk(db, "SELECT highlight_start_ms FROM audio_files LIMIT 0"), "column highlight_start_ms");

    // Table existence checks
    CHECK(sqlOk(db, "SELECT count(*) FROM play_queue"),      "table play_queue");
    CHECK(sqlOk(db, "SELECT count(*) FROM play_queue_meta"), "table play_queue_meta");

    // View existence checks
    CHECK(sqlOk(db, "SELECT count(*) FROM v_albums"),  "view v_albums");
    CHECK(sqlOk(db, "SELECT count(*) FROM v_artists"), "view v_artists");

    // app_meta checks
    {
        std::string val;
        bool found = sqlValue(db, "SELECT value FROM app_meta WHERE key='library_type'", val);
        CHECK(found, "app_meta key library_type exists");
    }
    {
        std::string val;
        bool found = sqlValue(db, "SELECT value FROM app_meta WHERE key='music_views_version'", val);
        bool correct = (found && val == "3");
        CHECK(correct, "app_meta music_views_version = '3'");
        if (found && !correct) {
            std::printf("      (actual value: '%s')\n", val.c_str());
        }
    }

    sqlite3_close(db);
}

// ---------------------------------------------------------------------------
// QA-3: TagVocabularyMapper unit tests
// ---------------------------------------------------------------------------

static void runQA3() {
    std::printf("\n=== QA-3: TagVocabularyMapper Unit Tests ===\n");

    // Genre mapping
    CHECK(TagVocabularyMapper::mapGenre("Chill")          == "chillout",   "Chill -> chillout");
    CHECK(TagVocabularyMapper::mapGenre("chill")          == "chillout",   "chill lowercase");
    CHECK(TagVocabularyMapper::mapGenre("Orchestral")     == "orchestral", "Orchestral");
    CHECK(TagVocabularyMapper::mapGenre("ELECTRONIC")     == "electronic", "ELECTRONIC uppercase");
    CHECK(TagVocabularyMapper::mapGenre("Dark Hip Hop")   == "hiphop",     "Dark Hip Hop -> hiphop");
    CHECK(TagVocabularyMapper::mapGenre("Feel Good")      == "pop",        "Feel Good -> pop");
    CHECK(TagVocabularyMapper::mapGenre("Cinematic")      == "cinematic",  "Cinematic");
    CHECK(TagVocabularyMapper::mapGenre("nonexistent_xyz") == "",          "unknown genre -> empty");

    // Instruments
    {
        auto inst1 = TagVocabularyMapper::mapInstruments("Piano");
        CHECK(!inst1.empty() && inst1[0] == "piano", "Piano -> piano");
    }
    {
        auto inst2 = TagVocabularyMapper::mapInstruments("Acoustic Guitar");
        bool hasAcoustic = false;
        for (auto& s : inst2) if (s == "guitar_acoustic") hasAcoustic = true;
        CHECK(hasAcoustic, "Acoustic Guitar -> guitar_acoustic (not guitar_electric)");
    }
    {
        auto inst3 = TagVocabularyMapper::mapInstruments("Electric Guitar");
        bool hasElectric = false;
        for (auto& s : inst3) if (s == "guitar_electric") hasElectric = true;
        CHECK(hasElectric, "Electric Guitar -> guitar_electric");
    }
    {
        auto inst4 = TagVocabularyMapper::mapInstruments("Piano, Strings, Choir");
        bool hasPiano = false, hasStrings = false, hasChoir = false;
        for (auto& s : inst4) {
            if (s == "piano")           hasPiano   = true;
            if (s == "strings_section") hasStrings = true;
            if (s == "choir")           hasChoir   = true;
        }
        CHECK(hasPiano && hasStrings && hasChoir, "Piano,Strings,Choir -> 3 instruments");
    }

    // Mood mapping
    CHECK(TagVocabularyMapper::mapMood("Tense")      == "tense",      "Tense -> tense");
    CHECK(TagVocabularyMapper::mapMood("Feel Good")  == "happy",      "Feel Good -> happy");
    CHECK(TagVocabularyMapper::mapMood("Melancholic") == "melancholic","Melancholic");
    CHECK(TagVocabularyMapper::mapMood("EPIC")       == "epic",       "EPIC uppercase -> epic");

    // normalize() end-to-end
    {
        std::string outGenre, outInst, outMood, outVocal;
        TagVocabularyMapper::normalize("Orchestral", "Piano, Strings",
                                       outGenre, outInst, outMood, outVocal);
        CHECK(outGenre == "orchestral", "normalize: genre=orchestral");
        CHECK(!outInst.empty(),         "normalize: instruments not empty");
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::printf("F-033 Phase 0 QA Tests\n");
    std::printf("======================\n");

    runQA1("auro.db");
    runQA3();

    std::printf("\n======================\n");
    std::printf("Results: %d passed, %d failed\n", passCount, failCount);

    if (failCount == 0) {
        std::printf("ALL PASS\n");
        return 0;
    } else {
        std::printf("SOME TESTS FAILED\n");
        return 1;
    }
}
