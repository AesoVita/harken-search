
// L2 integration test — F-002-ext trigram substring search end-to-end.
//
// Reproduces the regression where searchFiles() returned 0 results after
// the trigram sidecar landed: the FTS+Trigram UNION branch embedded
// snippet(audio_files_fts, …) and rank into the outer SELECT, where those
// FTS5 helpers are inaccessible, so sqlite3_prepare_v2 failed and the
// call silently returned an empty vector.
//
// This test opens a fresh Database (running the full migration), inserts
// audio records with substring-only matches (file name middle, folder
// path), then exercises searchFiles with each of the three eligible
// parser branches: FTS-only, trigram-only (UNION supplements FTS), and
// exercises the count/random APIs too to guard the sibling code paths.

#include "Database.h"
#include "SearchQuery.h"
#include "Utils.h"

#include <filesystem>
#include <iostream>
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

static AudioRecord makeRec(const std::string& folder, const std::string& name) {
    AudioRecord r;
    r.fileName = name;
    r.filePath = folder + "/" + name;
    r.folderPath = folder;
    r.extension = ".wav";
    r.format = "WAV";
    r.durationSeconds = 1.0f;
    r.sampleRate = 48000;
    r.channels = 2;
    r.fileSize = 1024;
    r.dateAdded = 1;
    r.fileMtime = 1;
    return r;
}

static bool containsFile(const std::vector<AudioRecord>& rows, const std::string& needle) {
    for (const auto& r : rows) {
        if (r.fileName == needle) return true;
    }
    return false;
}

int main() {
    // Fresh temp DB per run; Aris creates the file on first open.
    fs::path tmpDir = fs::temp_directory_path() / "aura_trigram_e2e";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);
    std::string dbPath = (tmpDir / "test.db").string();

    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "FAIL: db.open failed: " << db.getLastError() << "\n";
        return 1;
    }

    // Insert fixtures covering every branch of interest.
    //   A) file-name substring only ("30017" inside "B30017_Gun_Shot.wav").
    //      Token-based FTS ("30017*" prefix) will NOT match because the FTS
    //      tokenizer splits "B30017_Gun_Shot" into tokens like {B30017, Gun, Shot}
    //      — none start with "30017".
    //   B) folder-path substring only ("bbc" inside "Sound Ideas BBC/…").
    //      file_name itself does not contain "bbc".
    //   C) FTS-hittable token (prefix match via audio_files_fts) used to
    //      prove the FTS-only branch is still healthy.
    db.addFile(makeRec("D:/Lib/B-Series", "B30017_Gun_Shot.wav"));
    db.addFile(makeRec("D:/Lib/Sound Ideas BBC/Vol-07", "track_01.wav"));
    db.addFile(makeRec("D:/Lib/Ambience", "Forest_Birds.wav"));
    db.addFile(makeRec("D:/Lib/Exact", "fire.wav"));
    db.addFile(makeRec("D:/Lib/Exact", "fireball.wav"));
    db.addFile(makeRec("D:/Lib/Exact", "Flame.wav"));
    const std::string noisySequenceName =
        u8"[嘈杂序列] DSGNMisc_MOVEMENT-Noisey Sequence Phasey_B00M_SHCK.wav";
    db.addFile(makeRec("D:/Lib/Boom/Movement", noisySequenceName));

    // --- branch A: trigram (UNION) — substring inside file_name ---
    {
        SearchFilter f;
        f.keyword = "30017";
        auto rows = db.searchFiles(f, 100, 0);
        int64_t cnt = db.searchFilesCount(f);
        expect(!rows.empty(), "search '30017' returned rows (not empty vector)");
        expect(containsFile(rows, "B30017_Gun_Shot.wav"),
               "search '30017' hit B30017_Gun_Shot.wav via trigram file_name");
        expect(cnt == (int64_t)rows.size(),
               "searchFilesCount == searchFiles.size for '30017'");
    }

    // --- branch B: trigram (UNION) — substring inside folder_path ---
    {
        SearchFilter f;
        f.keyword = "bbc";
        auto rows = db.searchFiles(f, 100, 0);
        expect(!rows.empty(), "search 'bbc' returned rows");
        expect(containsFile(rows, "track_01.wav"),
               "search 'bbc' hit track_01.wav via trigram folder_path");
    }

    // --- branch C: FTS-only (ensure no regression on prefix-tokenizable words) ---
    {
        SearchFilter f;
        f.keyword = "forest";
        auto rows = db.searchFiles(f, 100, 0);
        expect(containsFile(rows, "Forest_Birds.wav"),
               "search 'forest' still hits via FTS prefix (no regression)");
    }

    // Precise mode matches a complete word without prefix, synonym or substring widening.
    {
        std::vector<SynonymGroup> synonyms = {
            {{"fire"}, {"Flame"}}
        };
        SearchFilter f;
        f.keyword = "fire";
        f.exactMode = true;
        f.synonyms = &synonyms;
        auto rows = db.searchFiles(f, 100, 0);
        expect(containsFile(rows, "fire.wav"),
               "precise fire matches complete word in fire.wav");
        expect(!containsFile(rows, "fireball.wav"),
               "precise fire does not match fireball prefix");
        expect(!containsFile(rows, "Flame.wav"),
               "precise fire does not expand Flame synonym");
        expect(db.searchFilesCount(f) == static_cast<int64_t>(rows.size()),
               "precise fire count matches returned rows");
    }

    // --- full pasted filename: brackets, CJK label, spaces, hyphen, underscores, dot ---
    {
        SearchFilter f;
        f.keyword = noisySequenceName;
        auto rows = db.searchFiles(f, 100, 0);
        int64_t cnt = db.searchFilesCount(f);
        expect(containsFile(rows, noisySequenceName),
               "full pasted noisy-sequence filename returns the exact file");
        expect(cnt >= 1,
               "full pasted noisy-sequence filename count is non-zero");
    }

    // --- branch C2: random sampler also works on UNION branch ---
    {
        SearchFilter f;
        f.keyword = "30017";
        auto rows = db.searchFilesRandom(f, 10);
        expect(!rows.empty(),
               "searchFilesRandom('30017') returned rows under UNION branch");
    }

    db.close();
    fs::remove_all(tmpDir, ec);

    std::cout << "\n=== Summary: " << g_pass << " pass, " << g_fail << " fail ===\n";
    return g_fail == 0 ? 0 : 1;
}
