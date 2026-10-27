// Source-text contract: clearing playback history must not touch unrelated caches.
//
// Background: historically the Tools-menu "Clear Play History" handler called
// state.performGlobalMemoryGC() right after wiping playHistory/playedFiles.
// performGlobalMemoryGC() runs CoverModule::ClearAllCoverCaches(), which wipes
// the cover wall's GPU texture cache and forces a multi-second visible reload.
// The status-bar "Clear History" button never did this, so the two entry
// points produced different user-facing behavior.
//
// This test pins the contract:
//   - Both clear-history sites only touch playHistory / playedFiles / playHistoryIndex
//   - Neither site invokes performGlobalMemoryGC, ClearAllCoverCaches, or
//     waveformCache.clear (which would also be a side-effect leak)
//   - Both sites reset playHistoryIndex so navigation state stays consistent
//
// Anyone reintroducing the side effect must consciously remove these checks.

#include <cstdio>
#include <fstream>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* desc) {
    if (cond) {
        std::printf("PASS: %s\n", desc);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", desc);
        ++g_fail;
    }
}

static std::string readTextFile(const char* filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return {};
    std::string out;
    in.seekg(0, std::ios::end);
    out.resize(static_cast<size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(&out[0], static_cast<std::streamsize>(out.size()));
    return out;
}

// Extract the body of an `if (...) { ... }` block whose condition contains
// `anchorInCondition`. Returns the substring between the matching braces,
// inclusive. Useful for ImGui::MenuItem / Button blocks that aren't functions.
static std::string extractIfBlockBody(const std::string& source,
                                      const std::string& anchorInCondition) {
    size_t pos = 0;
    while (pos < source.size()) {
        const size_t hit = source.find(anchorInCondition, pos);
        if (hit == std::string::npos) return {};
        // Walk backwards to find the enclosing `if (` on the same line/region.
        size_t scan = hit;
        while (scan > 0 && source[scan] != '\n') --scan;
        const size_t lineStart = (source[scan] == '\n') ? scan + 1 : scan;
        const std::string line = source.substr(lineStart, hit - lineStart);
        if (line.find("if (") == std::string::npos &&
            line.find("if(") == std::string::npos) {
            // Not an `if (...)` line — keep searching.
            pos = hit + anchorInCondition.size();
            continue;
        }
        const size_t open = source.find('{', hit);
        if (open == std::string::npos) return {};
        int depth = 0;
        for (size_t i = open; i < source.size(); ++i) {
            if (source[i] == '{') {
                ++depth;
            } else if (source[i] == '}') {
                --depth;
                if (depth == 0) {
                    return source.substr(open, i - open + 1);
                }
            }
        }
        return {};
    }
    return {};
}

int main() {
#ifdef AURA_SOURCE_DIR
    const char* mainCpp = AURA_SOURCE_DIR "/src/main.cpp";
#else
    const char* mainCpp = "src/main.cpp";
#endif

    const std::string mainSource = readTextFile(mainCpp);
    check(!mainSource.empty(), "main.cpp source is readable");

    // ── Tools menu entry: "ImGui::MenuItem(L(Tools_ClearHistory))" ────────
    const std::string menuBody = extractIfBlockBody(mainSource, "L(Tools_ClearHistory)");
    check(!menuBody.empty(), "Tools menu Clear-History block is locatable");

    if (!menuBody.empty()) {
        check(menuBody.find("state.playHistory.clear();") != std::string::npos,
              "SEMANTIC: tools menu Clear-History clears playHistory");
        check(menuBody.find("state.playedFiles.clear();") != std::string::npos,
              "SEMANTIC: tools menu Clear-History clears playedFiles");
        check(menuBody.find("state.playHistoryIndex = -1;") != std::string::npos,
              "SEMANTIC: tools menu Clear-History resets playHistoryIndex so navigation stays consistent");

        // The bug: previously this block called performGlobalMemoryGC(),
        // which wipes the cover wall texture cache.
        check(menuBody.find("performGlobalMemoryGC") == std::string::npos,
              "SEMANTIC: tools menu Clear-History must NOT call performGlobalMemoryGC "
              "(would wipe cover wall textures — the 2026-06-12 cover-wall-blank bug)");
        check(menuBody.find("ClearAllCoverCaches") == std::string::npos,
              "SEMANTIC: tools menu Clear-History must NOT clear cover caches");
        check(menuBody.find("waveformCache.clear") == std::string::npos,
              "SEMANTIC: tools menu Clear-History must NOT clear waveform cache");
    }

    // ── Status-bar entry: "ImGui::Button(L(Status_ClearHistory))" ─────────
    const std::string statusBody = extractIfBlockBody(mainSource, "L(Status_ClearHistory)");
    check(!statusBody.empty(), "Status-bar Clear-History block is locatable");

    if (!statusBody.empty()) {
        check(statusBody.find("state.playHistory.clear();") != std::string::npos,
              "SEMANTIC: status-bar Clear-History clears playHistory");
        check(statusBody.find("state.playedFiles.clear();") != std::string::npos,
              "SEMANTIC: status-bar Clear-History clears playedFiles");
        check(statusBody.find("state.playHistoryIndex = -1;") != std::string::npos,
              "SEMANTIC: status-bar Clear-History resets playHistoryIndex");
        check(statusBody.find("performGlobalMemoryGC") == std::string::npos,
              "SEMANTIC: status-bar Clear-History must NOT call performGlobalMemoryGC");
        check(statusBody.find("ClearAllCoverCaches") == std::string::npos,
              "SEMANTIC: status-bar Clear-History must NOT clear cover caches");
    }

    // ── Equivalence: both entries should clear the same state ─────────────
    if (!menuBody.empty() && !statusBody.empty()) {
        const bool menuClearsBoth =
            menuBody.find("state.playHistory.clear();") != std::string::npos &&
            menuBody.find("state.playedFiles.clear();") != std::string::npos;
        const bool statusClearsBoth =
            statusBody.find("state.playHistory.clear();") != std::string::npos &&
            statusBody.find("state.playedFiles.clear();") != std::string::npos;
        check(menuClearsBoth && statusClearsBoth,
              "SEMANTIC: both Clear-History entry points clear the same state set "
              "(menu and status-bar must stay equivalent)");
    }

    std::printf("\n=== Summary: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
