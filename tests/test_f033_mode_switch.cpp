// test_f033_mode_switch.cpp
// F-033 mode-routing regression checks

#include <cstdio>

#include "../src/BetaFeatureFlags.h"
#include "../src/ProductModeRouting.h"

static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) std::printf("PASS: %s\n", msg); \
        else { std::printf("FAIL: %s\n", msg); ++g_fail; } \
    } while (0)

static ColumnViewPreset MakePreset(const char* name, bool builtIn) {
    ColumnViewPreset preset;
    preset.name = name;
    preset.builtIn = builtIn;
    return preset;
}

int main() {
    CHECK(!HarkenSearch::BetaFeatureFlags::kExposeMusicModeSwitch,
          "first internal beta hides Music mode switching");

    ProductMode outMode = ProductMode::SFX;

    {
        ColumnViewPreset preset = MakePreset("Music", true);
        bool ok = aura::TryGetBuiltInPresetProductMode(preset, outMode);
        CHECK(ok, "built-in Music preset maps");
        CHECK(outMode == ProductMode::Music, "built-in Music preset -> Music mode");
    }

    {
        ColumnViewPreset preset = MakePreset("SFX", true);
        bool ok = aura::TryGetBuiltInPresetProductMode(preset, outMode);
        CHECK(ok, "built-in SFX preset maps");
        CHECK(outMode == ProductMode::SFX, "built-in SFX preset -> SFX mode");
    }

    {
        ColumnViewPreset preset = MakePreset("Meta", true);
        bool ok = aura::TryGetBuiltInPresetProductMode(preset, outMode);
        CHECK(ok, "built-in Meta preset maps");
        CHECK(outMode == ProductMode::SFX, "built-in Meta preset stays in SFX mode");
    }

    {
        ColumnViewPreset preset = MakePreset("My Music Columns", false);
        outMode = ProductMode::Music;
        bool ok = aura::TryGetBuiltInPresetProductMode(preset, outMode);
        CHECK(!ok, "custom preset does not force product mode");
        CHECK(outMode == ProductMode::Music, "custom preset leaves output untouched");
    }

    std::printf(g_fail ? "\nFAILED: %d\n" : "\nOK\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
