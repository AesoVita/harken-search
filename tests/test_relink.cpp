// test_relink.cpp
// Tests for Relink missing files feature (F-034)
// Run: ctest --build-config Debug -R TestRelink --output-on-failure

#include "Database.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>

namespace fs = std::filesystem;

static std::string tmpDir(const std::string& suffix) {
    auto p = fs::temp_directory_path() / ("aura_relink_test_" + suffix);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

static void writeFile(const std::string& path, size_t bytes = 1024) {
    std::ofstream f(path, std::ios::binary);
    std::string buf(bytes, 'X');
    f.write(buf.data(), buf.size());
}

// Parameter types match findOrphanByFingerprint signature (int64_t/int64_t/int).
// Assigned to AudioRecord fields (uint64_t/uint64_t/uint32_t) — values are
// always positive so the implicit narrowing is safe in these tests.
static AudioRecord makeRecord(const std::string& path,
                               int64_t size, int64_t frames, int rate) {
    AudioRecord r{};
    r.filePath    = path;
    r.fileName    = fs::path(path).filename().string();
    r.fileSize    = static_cast<uint64_t>(size);
    r.totalFrames = static_cast<uint64_t>(frames);
    r.sampleRate  = static_cast<uint32_t>(rate);
    r.extension   = ".wav";
    return r;
}

// Test 1: findOrphanByFingerprint returns nullopt when file exists on disk
static bool test_no_orphan_when_file_exists() {
    std::string dir = tmpDir("t1");
    std::string p = dir + "/sound.wav";
    writeFile(p, 2048);

    Database db;
    db.open(dir + "/test.db");
    db.addFile(makeRecord(p, 2048, 1000, 44100));

    auto result = db.findOrphanByFingerprint("sound.wav", 2048, 1000, 44100);
    bool pass = !result.has_value();
    std::cout << (pass ? "PASS" : "FAIL") << ": no orphan when file exists on disk\n";
    db.close();
    fs::remove_all(dir);
    return pass;
}

// Test 2: findOrphanByFingerprint returns candidate after file is deleted
static bool test_orphan_detected_after_delete() {
    std::string dir = tmpDir("t2");
    std::string p = dir + "/boom.wav";
    writeFile(p, 4096);

    Database db;
    db.open(dir + "/test.db");
    db.addFile(makeRecord(p, 4096, 2000, 48000));
    fs::remove(p);  // simulate file gone missing

    auto result = db.findOrphanByFingerprint("boom.wav", 4096, 2000, 48000);
    bool pass = result.has_value() && result->fileName == "boom.wav";
    std::cout << (pass ? "PASS" : "FAIL") << ": orphan detected after file delete\n";
    db.close();
    fs::remove_all(dir);
    return pass;
}

// Test 3: addFile with matching fingerprint auto-relinks orphan instead of INSERT
static bool test_addfile_relinks_orphan() {
    std::string dir    = tmpDir("t3_old");
    std::string newDir = tmpDir("t3_new");
    std::string oldPath = dir + "/whoosh.wav";
    std::string newPath = newDir + "/whoosh.wav";
    writeFile(oldPath, 8192);

    Database db;
    db.open(dir + "/test.db");
    db.addFile(makeRecord(oldPath, 8192, 4000, 96000));

    // Move file to new location (simulate user reorganising their library)
    fs::rename(oldPath, newPath);

    // addFile with new path but same fingerprint should relink, not INSERT
    db.addFile(makeRecord(newPath, 8192, 4000, 96000));

    // Verify exactly 1 row exists (relink updated the existing row, no duplicate INSERT)
    int64_t rowCount = db.getFileCount();
    bool rowCountOk = (rowCount == 1);
    if (!rowCountOk) {
        std::cout << "  [debug] expected 1 row after relink, got " << rowCount << "\n";
    }

    // After relink, oldPath entry should no longer be an orphan
    // (it was updated to newPath), so findOrphanByFingerprint should return nothing
    auto orphan = db.findOrphanByFingerprint("whoosh.wav", 8192, 4000, 96000);
    bool pass = !orphan.has_value() && rowCountOk;
    std::cout << (pass ? "PASS" : "FAIL") << ": addFile auto-relinks orphan on fingerprint match\n";
    db.close();
    fs::remove_all(dir);
    fs::remove_all(newDir);
    return pass;
}

// Test 4: findRelinkCandidates finds moved file in new directory
static bool test_find_relink_candidates() {
    std::string dir    = tmpDir("t4_old");
    std::string newDir = tmpDir("t4_new");
    std::string oldPath = dir + "/forest.wav";
    std::string newPath = newDir + "/forest.wav";
    writeFile(oldPath, 16384);

    Database db;
    db.open(dir + "/test.db");
    db.addFile(makeRecord(oldPath, 16384, 8000, 44100));

    // Move file to new location
    fs::rename(oldPath, newPath);

    auto results = db.findRelinkCandidates({newDir});
    bool found = false;
    for (auto& [candidate, paths] : results) {
        if (candidate.fileName == "forest.wav" && !paths.empty()) {
            found = true;
            break;
        }
    }
    bool pass = found;
    std::cout << (pass ? "PASS" : "FAIL") << ": findRelinkCandidates finds moved file\n";
    db.close();
    fs::remove_all(dir);
    fs::remove_all(newDir);
    return pass;
}

int main() {
    int failed = 0;
    if (!test_no_orphan_when_file_exists())   ++failed;
    if (!test_orphan_detected_after_delete()) ++failed;
    if (!test_addfile_relinks_orphan())        ++failed;
    if (!test_find_relink_candidates())        ++failed;

    std::cout << "\n" << (4 - failed) << "/4 passed\n";
    return failed == 0 ? 0 : 1;
}
