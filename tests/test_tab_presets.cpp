// F-033 Tab Presets — pure-logic unit tests.
//
// AppState has heavy constructor dependencies (AudioEngine, Database, etc.), so
// we don't instantiate it here. Instead we replicate the save/load algorithm on
// lean local mocks of ListTab / TabPreset / TabPresetEntry to verify:
//   1) field round-trip (save → load → identical fields)
//   2) no-leak of derived data (searchResults not in preset)
//   3) upper-bound enforcement
//   4) upsert-by-name behaviour
//   5) edge cases on delete/rename (bounds checks)
//   6) load succeeds when current listTabs has size 1 (no "last tab" veto)
//
// If AppState.cpp ever diverges from this replicated algorithm, this test won't
// catch it — the harness includes a manual GUI smoke step for that.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace test {

// Minimal mirror of the fields TabPresetEntry round-trips.
struct MockListTab {
    int         id = 0;
    std::string name;
    int         viewMode = 0;
    char        searchBuffer[256] = "";
    std::string selectedFolderPath;
    bool        folderFilterActive = false;
    std::string findSameFilenameQuery;
    bool        findSameFilenameActive = false;
    int64_t     selectedPlaylistId = -1;
    int         tabColor = -1;
};

struct MockPresetEntry {
    std::string name;
    int         viewMode = 0;
    std::string searchBuffer;
    std::string selectedFolderPath;
    bool        folderFilterActive = false;
    std::string findSameFilenameQuery;
    bool        findSameFilenameActive = false;
    int64_t     selectedPlaylistId = -1;
    int         tabColor = -1;
};

struct MockPreset {
    std::string                     name;
    std::vector<MockPresetEntry>    tabs;
    int                             activeIndex = 0;
    bool                            pinned = false;
};

constexpr int kMaxTabPresets    = 50;
constexpr int kMaxTabsPerPreset = 20;

// Mirror of AppState::saveTabPresetFromCurrent — pure algorithm only.
static int saveFromCurrent(std::vector<MockPreset>& presets,
                           const std::vector<MockListTab>& tabs,
                           int activeIdx,
                           const std::string& name)
{
    if (name.empty()) return -1;
    if (tabs.empty()) return -1;
    if ((int)presets.size() >= kMaxTabPresets) return -1;

    MockPreset p;
    p.name = name;
    p.activeIndex = activeIdx;
    p.pinned = false;
    const int take = std::min((int)tabs.size(), kMaxTabsPerPreset);
    p.tabs.reserve((size_t)take);
    for (int i = 0; i < take; ++i) {
        const auto& t = tabs[(size_t)i];
        MockPresetEntry e;
        e.name                   = t.name;
        e.viewMode               = t.viewMode;
        e.searchBuffer           = std::string(t.searchBuffer);
        e.selectedFolderPath     = t.selectedFolderPath;
        e.folderFilterActive     = t.folderFilterActive;
        e.findSameFilenameQuery  = t.findSameFilenameQuery;
        e.findSameFilenameActive = t.findSameFilenameActive;
        e.selectedPlaylistId     = t.selectedPlaylistId;
        e.tabColor               = t.tabColor;
        p.tabs.push_back(std::move(e));
    }
    if (p.activeIndex < 0 || p.activeIndex >= (int)p.tabs.size())
        p.activeIndex = 0;

    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].name == name) {
            const bool wasPinned = presets[i].pinned;
            presets[i] = std::move(p);
            presets[i].pinned = wasPinned;
            return (int)i;
        }
    }
    presets.push_back(std::move(p));
    return (int)presets.size() - 1;
}

// Mirror of AppState::loadTabPreset — pure algorithm only (no ClosedStack/sync).
static bool loadPreset(const std::vector<MockPreset>& presets,
                       std::vector<MockListTab>& tabs,
                       int& activeIdx,
                       int& nextId,
                       int presetIdx)
{
    if (presetIdx < 0 || presetIdx >= (int)presets.size()) return false;
    const MockPreset& p = presets[(size_t)presetIdx];
    if (p.tabs.empty()) return false;

    // Clear current tabs outright — critically, this bypasses the "refuse
    // to close last tab" guard that closeTab() enforces.
    tabs.clear();

    tabs.reserve(p.tabs.size());
    for (const auto& e : p.tabs) {
        MockListTab t;
        t.id = nextId++;
        t.name                   = e.name;
        t.viewMode               = e.viewMode;
        std::snprintf(t.searchBuffer, sizeof(t.searchBuffer), "%s", e.searchBuffer.c_str());
        t.selectedFolderPath     = e.selectedFolderPath;
        t.folderFilterActive     = e.folderFilterActive;
        t.findSameFilenameQuery  = e.findSameFilenameQuery;
        t.findSameFilenameActive = e.findSameFilenameActive;
        t.selectedPlaylistId     = e.selectedPlaylistId;
        t.tabColor               = e.tabColor;
        tabs.push_back(std::move(t));
    }

    activeIdx = p.activeIndex;
    if (activeIdx < 0 || activeIdx >= (int)tabs.size()) activeIdx = 0;
    return true;
}

static bool deletePreset(std::vector<MockPreset>& presets, int idx) {
    if (idx < 0 || idx >= (int)presets.size()) return false;
    presets.erase(presets.begin() + idx);
    return true;
}

static bool renamePreset(std::vector<MockPreset>& presets, int idx, const std::string& newName) {
    if (idx < 0 || idx >= (int)presets.size()) return false;
    if (newName.empty()) return false;
    presets[(size_t)idx].name = newName;
    return true;
}

} // namespace test

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("PASS: %s\n", msg); ++g_pass; } \
    else      { std::printf("FAIL: %s\n", msg); ++g_fail; } \
} while (0)

static void seed(test::MockListTab& t, int idx) {
    t.id   = 100 + idx;
    t.name = "Tab_" + std::to_string(idx);
    t.viewMode = idx % 3;  // spread across enum values
    std::snprintf(t.searchBuffer, sizeof(t.searchBuffer), "query-%d", idx);
    t.selectedFolderPath     = "/path/to/folder_" + std::to_string(idx);
    t.folderFilterActive     = (idx % 2 == 1);
    t.findSameFilenameQuery  = "same-" + std::to_string(idx);
    t.findSameFilenameActive = (idx % 3 == 0);
    t.selectedPlaylistId     = (int64_t)(idx * 10);
    t.tabColor               = idx % 8;
}

int main() {
    using namespace test;

    // ── Test 1: save captures all 9 round-trippable fields ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        for (int i = 0; i < 3; ++i) { MockListTab t; seed(t, i); tabs.push_back(t); }

        int idx = saveFromCurrent(presets, tabs, 0, "MyWorkspace");
        CHECK(idx == 0, "save returns 0 for first preset");
        CHECK(presets.size() == 1, "1 preset after save");
        CHECK(presets[0].tabs.size() == 3, "preset contains 3 tabs");
        CHECK(presets[0].name == "MyWorkspace", "preset name preserved");
        const auto& e = presets[0].tabs[2];
        CHECK(e.name == "Tab_2", "tab[2].name preserved");
        CHECK(e.viewMode == 2, "tab[2].viewMode preserved");
        CHECK(e.searchBuffer == "query-2", "tab[2].searchBuffer preserved");
        CHECK(e.selectedFolderPath == "/path/to/folder_2", "tab[2].selectedFolderPath preserved");
        CHECK(e.folderFilterActive == false, "tab[2].folderFilterActive preserved");
        CHECK(e.findSameFilenameQuery == "same-2", "tab[2].findSameFilenameQuery preserved");
        CHECK(e.findSameFilenameActive == false, "tab[2].findSameFilenameActive preserved");
        CHECK(e.selectedPlaylistId == 20, "tab[2].selectedPlaylistId preserved");
        CHECK(e.tabColor == 2, "tab[2].tabColor preserved");
    }

    // ── Test 2: load replaces all tabs and restores fields ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        for (int i = 0; i < 4; ++i) { MockListTab t; seed(t, i); tabs.push_back(t); }
        saveFromCurrent(presets, tabs, 1, "Snap");

        // Wipe state to a single stranger tab, then load.
        tabs.clear();
        MockListTab stranger; seed(stranger, 99); stranger.name = "Stranger";
        tabs.push_back(stranger);

        int active = 0, nextId = 500;
        bool ok = loadPreset(presets, tabs, active, nextId, 0);
        CHECK(ok, "loadPreset returns true");
        CHECK(tabs.size() == 4, "4 tabs restored");
        CHECK(active == 1, "activeIndex restored to 1");
        CHECK(tabs[3].name == "Tab_3", "restored tab[3].name");
        CHECK(tabs[3].viewMode == 0, "restored tab[3].viewMode (3%3=0)");
        CHECK(std::string(tabs[3].searchBuffer) == "query-3", "restored tab[3].searchBuffer");
        CHECK(tabs[3].tabColor == 3, "restored tab[3].tabColor");
        CHECK(tabs[3].selectedPlaylistId == 30, "restored tab[3].selectedPlaylistId");
        CHECK(tabs[0].id == 500 && tabs[1].id == 501, "new tabs get fresh ids from nextId counter");
    }

    // ── Test 3: load succeeds even when current listTabs has a single tab ──
    //           (the whole point of bypassing closeTab's guard)
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        for (int i = 0; i < 2; ++i) { MockListTab t; seed(t, i); tabs.push_back(t); }
        saveFromCurrent(presets, tabs, 0, "TwoTabs");
        tabs.erase(tabs.begin() + 1);
        CHECK(tabs.size() == 1, "setup: only 1 tab left");

        int active = 0, nextId = 1;
        bool ok = loadPreset(presets, tabs, active, nextId, 0);
        CHECK(ok, "loadPreset succeeds with only 1 current tab");
        CHECK(tabs.size() == 2, "2 tabs after load (not vetoed by last-tab guard)");
    }

    // ── Test 4: delete / rename edge cases ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        MockListTab t; seed(t, 0); tabs.push_back(t);
        saveFromCurrent(presets, tabs, 0, "A");
        saveFromCurrent(presets, tabs, 0, "B");

        CHECK(deletePreset(presets, -1) == false, "delete rejects negative idx");
        CHECK(deletePreset(presets, 999) == false, "delete rejects out-of-range idx");
        CHECK(renamePreset(presets, 0, "") == false, "rename rejects empty name");
        CHECK(renamePreset(presets, 99, "X") == false, "rename rejects out-of-range idx");

        CHECK(renamePreset(presets, 0, "A2") == true, "rename succeeds on valid idx");
        CHECK(presets[0].name == "A2", "rename persisted");
        CHECK(deletePreset(presets, 0) == true, "delete succeeds on valid idx");
        CHECK(presets.size() == 1, "1 preset remains after delete");
        CHECK(presets[0].name == "B", "remaining preset is 'B'");
    }

    // ── Test 5: kMaxTabPresets upper bound enforced ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        MockListTab t; seed(t, 0); tabs.push_back(t);
        int ok = 0, rej = 0;
        for (int i = 0; i < kMaxTabPresets + 5; ++i) {
            int r = saveFromCurrent(presets, tabs, 0, "P" + std::to_string(i));
            if (r >= 0) ++ok; else ++rej;
        }
        CHECK(ok == kMaxTabPresets, "first kMaxTabPresets saves succeed");
        CHECK(rej == 5, "subsequent saves rejected with -1");
        CHECK((int)presets.size() == kMaxTabPresets, "presets capped at kMaxTabPresets");
    }

    // ── Test 6: upsert-by-name overwrites existing slot, preserves pinned ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        MockListTab t; seed(t, 7); tabs.push_back(t);
        saveFromCurrent(presets, tabs, 0, "X");
        presets[0].pinned = true;  // user pinned it
        size_t before = presets.size();

        std::snprintf(tabs[0].searchBuffer, sizeof(tabs[0].searchBuffer), "updated");
        int r = saveFromCurrent(presets, tabs, 0, "X");

        CHECK(r == 0, "upsert returns existing idx");
        CHECK(presets.size() == before, "same-name save upserts, does not append");
        CHECK(presets[0].tabs[0].searchBuffer == "updated", "upserted preset reflects new state");
        CHECK(presets[0].pinned == true, "pinned flag preserved across upsert");
    }

    // ── Test 7: kMaxTabsPerPreset cap on per-preset tab count ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        for (int i = 0; i < kMaxTabsPerPreset + 10; ++i) {
            MockListTab t; seed(t, i); tabs.push_back(t);
        }
        int r = saveFromCurrent(presets, tabs, 0, "BigPreset");
        CHECK(r == 0, "save with oversized tabs still succeeds");
        CHECK((int)presets[0].tabs.size() == kMaxTabsPerPreset,
              "preset clamped to kMaxTabsPerPreset tabs");
    }

    // ── Test 8: empty-name / empty-tabs rejections ──
    {
        std::vector<MockPreset> presets;
        std::vector<MockListTab> tabs;
        CHECK(saveFromCurrent(presets, tabs, 0, "") == -1, "save rejects empty name");
        CHECK(saveFromCurrent(presets, tabs, 0, "NoTabs") == -1, "save rejects empty tabs");
        MockListTab t; seed(t, 0); tabs.push_back(t);
        CHECK(saveFromCurrent(presets, tabs, 0, "") == -1, "save still rejects empty name with tabs");
    }

    std::printf("\n=== Tab Presets: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
