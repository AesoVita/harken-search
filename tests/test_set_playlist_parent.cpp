// Tests for Database::setPlaylistParent — moving playlists into/out of folders.
#include "Database.h"
#include "DiagnosticsLogger.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (cond) { g_pass++; std::printf("PASS: %s\n", msg); } \
    else      { g_fail++; std::printf("FAIL: %s\n", msg); } \
} while (0)

static void waitDiagDrain() {
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
}

int main() {
    const fs::path tmp = fs::temp_directory_path() / "aura_setparent_test.db";
    std::error_code ec;
    fs::remove(tmp, ec);
    // Best-effort cleanup of sidecar split DBs from any prior run.
    fs::remove(tmp.string() + "_blobs.db", ec);
    fs::remove(tmp.string() + "_waveforms.db", ec);

    Database db;
    if (!db.open(tmp.string())) {
        std::printf("FAIL: cannot open temp DB at %s\n", tmp.string().c_str());
        return 1;
    }

    auto& diag = DiagnosticsLogger::instance();
    diag.setEnabled(true);
    diag.setMaxEntries(512);
    diag.clear();

    // Setup: two folders + three regular playlists.
    int64_t folderA = db.createPlaylist("FolderA");
    db.markAsFolder(folderA);
    int64_t folderB = db.createPlaylist("FolderB");
    db.markAsFolder(folderB);
    int64_t pl1 = db.createPlaylist("Playlist1");
    int64_t pl2 = db.createPlaylist("Playlist2");
    int64_t pl3 = db.createPlaylist("Playlist3");

    EXPECT(folderA > 0 && folderB > 0 && pl1 > 0 && pl2 > 0 && pl3 > 0,
           "setup created 5 playlists");

    // T1: move regular playlist into folder.
    EXPECT(db.setPlaylistParent(pl1, folderA), "T1: setPlaylistParent(pl1, folderA) returns true");
    waitDiagDrain();
    std::string diagAfterMove = diag.getRecentJsonLines(12);
    EXPECT(diagAfterMove.find("SetParentApplied: child=") != std::string::npos,
           "T1: successful parent change emits playlist diagnostics");
    {
        auto all = db.getAllPlaylists();
        bool ok = false;
        for (const auto& p : all) {
            if (p.id == pl1) { ok = (p.parent_id == folderA); break; }
        }
        EXPECT(ok, "T1: pl1.parent_id == folderA after move");
    }

    // T2: unparent.
    EXPECT(db.setPlaylistParent(pl1, -1), "T2: setPlaylistParent(pl1, -1) returns true");
    {
        auto all = db.getAllPlaylists();
        bool ok = false;
        for (const auto& p : all) {
            if (p.id == pl1) { ok = (p.parent_id < 0); break; }
        }
        EXPECT(ok, "T2: pl1.parent_id is NULL after unparent");
    }

    // T3: folder cannot become a child.
    diag.clear();
    EXPECT(!db.setPlaylistParent(folderA, folderB),
           "T3: folderA -> folderB rejected (folder cannot be child)");
    waitDiagDrain();
    std::string diagAfterFolderReject = diag.getRecentJsonLines(12);
    EXPECT(diagAfterFolderReject.find("SetParentRejected: reason=child_is_folder") != std::string::npos,
           "T3: folder child rejection emits diagnostics");

    // T4: self-loop rejected.
    diag.clear();
    EXPECT(!db.setPlaylistParent(pl2, pl2), "T4: self-loop rejected");
    waitDiagDrain();
    std::string diagAfterSelfLoop = diag.getRecentJsonLines(12);
    EXPECT(diagAfterSelfLoop.find("SetParentRejected: reason=self_loop") != std::string::npos,
           "T4: self-loop rejection emits diagnostics");

    // T5: non-existent target rejected.
    diag.clear();
    EXPECT(!db.setPlaylistParent(pl2, 99999), "T5: non-existent parent rejected");
    waitDiagDrain();
    std::string diagAfterMissingParent = diag.getRecentJsonLines(12);
    EXPECT(diagAfterMissingParent.find("SetParentRejected: reason=target_not_folder") != std::string::npos,
           "T5: missing parent rejection emits diagnostics");

    // T6: non-folder cannot be a parent.
    EXPECT(!db.setPlaylistParent(pl2, pl3), "T6: target is not a folder, rejected");

    db.close();
    diag.setEnabled(false);
    fs::remove(tmp, ec);
    fs::remove(tmp.string() + "_blobs.db", ec);
    fs::remove(tmp.string() + "_waveforms.db", ec);

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
