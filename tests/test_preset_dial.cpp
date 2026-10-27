// tests/test_preset_dial.cpp
// Tests for ColumnViewPreset data model (F-034)
// Compile standalone — no ImGui, no AppState linkage needed for pure data logic.
#include <vector>
#include <string>
#include <cstdio>
#include <unordered_map>

// ── Minimal copies of the structs under test ────────────────────────────────
// (These will live in AppState.h after Task 2; duplicated here so the test
//  can compile before AppState.h is modified.)

struct ColumnViewPreset {
    std::string name;
    bool        col[13];
    bool        showMeter;
    bool        builtIn;
};

static void InitDefaultColumnPresets(std::vector<ColumnViewPreset>& out) {
    out.clear();
    // Dur  Fmt   Size  SR    Desc  Ch    Cat   Art   Alb   BPM   Key   Mood  Date
    ColumnViewPreset sfx  = {"SFX",   {true,true, false,true, false,true, true, false,false,false,false,false,false}, false, true};
    ColumnViewPreset music= {"Music", {true,false,false,false,false,false,false,true, true, true, true, true, false}, false, true};
    ColumnViewPreset meta = {"Meta",  {true,false,false,false,true, false,true, true, false,false,false,false,true }, false, true};
    ColumnViewPreset loud = {"Loud",  {true,true, false,true, false,false,false,false,false,false,false,false,false}, true,  true};
    out = {sfx, music, meta, loud};
}

// ── Tests ────────────────────────────────────────────────────────────────────

static int pass = 0, fail = 0;
#define CHECK(cond, msg) do { if (cond) { printf("PASS: %s\n", msg); ++pass; } \
    else { printf("FAIL: %s\n", msg); ++fail; } } while(0)

static void test_init_default_presets() {
    std::vector<ColumnViewPreset> presets;
    InitDefaultColumnPresets(presets);
    CHECK(presets.size() == 4,       "InitDefaultColumnPresets produces 4 entries");
    CHECK(presets[0].name == "SFX",  "preset[0] name is SFX");
    CHECK(presets[3].name == "Loud", "preset[3] name is Loud");
    CHECK(presets[0].builtIn,        "SFX is builtIn");
    CHECK(presets[3].showMeter,      "Loud has showMeter=true");
    CHECK(!presets[0].showMeter,     "SFX has showMeter=false");
    CHECK(presets[0].col[0],  "SFX col[0] Dur=true");
    CHECK(presets[0].col[1],  "SFX col[1] Fmt=true");
    CHECK(!presets[0].col[2], "SFX col[2] Size=false");
    CHECK(presets[0].col[3],  "SFX col[3] SR=true");
    CHECK(presets[0].col[5],  "SFX col[5] Ch=true");
    CHECK(presets[0].col[6],  "SFX col[6] Cat=true");
}

static void test_user_preset_add_delete() {
    std::vector<ColumnViewPreset> presets;
    InitDefaultColumnPresets(presets);

    ColumnViewPreset custom = {"MyPreset", {}, false, false};
    custom.col[0] = true; custom.col[4] = true;
    presets.push_back(custom);
    CHECK(presets.size() == 5, "After add: 5 presets");
    CHECK(!presets[4].builtIn, "Custom preset is not builtIn");
    CHECK(presets[4].name == "MyPreset", "Custom preset name preserved");

    int activeIdx = 4;
    if (!presets[activeIdx].builtIn) {
        presets.erase(presets.begin() + activeIdx);
        activeIdx = (int)presets.size() - 1;
        if (activeIdx < 0) activeIdx = 0;
    }
    CHECK(presets.size() == 4, "After delete: back to 4 presets");
    CHECK(activeIdx == 3,      "activeIdx clamped to 3");

    int builtInIdx = 0;
    bool deleted = false;
    if (!presets[builtInIdx].builtIn) {
        presets.erase(presets.begin() + builtInIdx);
        deleted = true;
    }
    CHECK(!deleted,            "Delete builtIn is blocked");
    CHECK(presets.size() == 4, "Size unchanged after blocked delete");
}

static void test_settings_round_trip() {
    std::vector<ColumnViewPreset> original;
    InitDefaultColumnPresets(original);
    ColumnViewPreset user = {"FX_Quick", {true,true,false,false,false,false,true,false,false,false,false,false,false}, false, false};
    original.push_back(user);

    std::vector<std::pair<std::string, std::string>> items;
    items.push_back({"columnPresetCount", std::to_string(original.size())});
    items.push_back({"activeColumnPreset", "1"});
    for (int i = 0; i < (int)original.size(); ++i) {
        std::string p = "cp" + std::to_string(i) + "_";
        items.push_back({p + "name",    original[i].name});
        items.push_back({p + "meter",   original[i].showMeter ? "1" : "0"});
        items.push_back({p + "builtIn", original[i].builtIn   ? "1" : "0"});
        for (int c = 0; c < 13; ++c)
            items.push_back({p + "col" + std::to_string(c), original[i].col[c] ? "1" : "0"});
    }

    std::unordered_map<std::string,std::string> kv;
    for (auto& kp : items) kv[kp.first] = kp.second;

    int count = std::stoi(kv["columnPresetCount"]);
    int active = std::stoi(kv["activeColumnPreset"]);
    std::vector<ColumnViewPreset> loaded(count);
    for (int i = 0; i < count; ++i) {
        std::string p = "cp" + std::to_string(i) + "_";
        loaded[i].name      = kv[p + "name"];
        loaded[i].showMeter = kv[p + "meter"]   == "1";
        loaded[i].builtIn   = kv[p + "builtIn"] == "1";
        for (int c = 0; c < 13; ++c)
            loaded[i].col[c] = kv[p + "col" + std::to_string(c)] == "1";
    }

    CHECK(count == 5,                          "Round-trip: count=5");
    CHECK(active == 1,                         "Round-trip: activeColumnPreset=1");
    CHECK(loaded[4].name == "FX_Quick",        "Round-trip: user preset name");
    CHECK(!loaded[4].builtIn,                  "Round-trip: user preset not builtIn");
    CHECK(loaded[4].col[0],                    "Round-trip: user preset col[0]=true");
    CHECK(!loaded[4].col[2],                   "Round-trip: user preset col[2]=false");
    CHECK(loaded[0].builtIn,                   "Round-trip: built-in still builtIn");
    CHECK(loaded[1].name == "Music",           "Round-trip: Music name preserved");
    CHECK(loaded[2].name == "Meta",            "Round-trip: Meta name preserved");
    CHECK(loaded[3].showMeter,                 "Round-trip: Loud showMeter=true preserved");
    CHECK(loaded[3].builtIn,                   "Round-trip: Loud builtIn=true preserved");
}

int main() {
    test_init_default_presets();
    test_user_preset_add_delete();
    test_settings_round_trip();
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
