#include "src/coverwall_lab/CoverWallLabState.h"

#include <cstdio>
#include <fstream>
#include <filesystem>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    using aura::coverwall_lab::CoverWallLabState;
    using aura::coverwall_lab::TileSizePreset;

    CoverWallLabState state;
    if (!Check(state.filters().tileSizePx == CoverWallLabState::kDefaultTileSizePx,
               "default tile size should be Browse size")) {
        return 1;
    }
    if (!Check(state.results().totalCount > 0 && state.results().totalCount == static_cast<long long>(state.results().items.size()),
               "fixture results should have count truth")) {
        return 1;
    }

    state.setKeyword("metal");
    const auto metalCount = state.results().totalCount;
    if (!Check(metalCount == 2, "keyword filter should narrow fixture wall")) {
        return 1;
    }
    state.selectPath("Fixture/Impacts/Metal");
    if (!Check(state.results().totalCount == metalCount,
               "path filter should compose with active keyword")) {
        return 1;
    }
    const std::string selectedCover = state.results().items.front().coverKey;
    state.selectCover(selectedCover);
    if (!Check(state.results().totalCount == 2,
               "cover filter should keep matching items inside active path and keyword scope")) {
        return 1;
    }

    state.setTileSizePx(999);
    if (!Check(state.filters().tileSizePx == CoverWallLabState::kMaxTileSizePx,
               "tile size should clamp to max")) {
        return 1;
    }
    if (!Check(state.filters().keyword == "metal" &&
                   state.filters().selectedPath == "Fixture/Impacts/Metal" &&
                   state.filters().selectedCoverKey == selectedCover,
               "tile size changes must preserve filters")) {
        return 1;
    }
    state.setTileSizePx(-20);
    if (!Check(state.filters().tileSizePx == CoverWallLabState::kMinTileSizePx,
               "tile size should clamp to min")) {
        return 1;
    }
    state.applyPreset(TileSizePreset::Compact);
    if (!Check(state.filters().tileSizePx == CoverWallLabState::kCompactTileSizePx,
               "Compact preset should apply the product size")) {
        return 1;
    }
    state.applyPreset(TileSizePreset::Browse);
    if (!Check(state.filters().tileSizePx == CoverWallLabState::kBrowseTileSizePx,
               "Browse preset should apply the product size")) {
        return 1;
    }
    state.applyPreset(TileSizePreset::Inspect);
    if (!Check(state.filters().tileSizePx == CoverWallLabState::kInspectTileSizePx,
               "Inspect preset should apply the product size")) {
        return 1;
    }
    if (!Check(state.calculateColumnCount(10.0f) == 1,
               "narrow wall should still keep at least one column")) {
        return 1;
    }
    if (!Check(state.calculateColumnCount(10000.0f) == 12,
               "wide wall should cap columns to avoid over-dense rows")) {
        return 1;
    }
    const auto visibleRange = CoverWallLabState::calculateVisibleRange(2, 4, 5, 37);
    if (!Check(visibleRange.startIndex == 10 && visibleRange.endIndex == 20,
               "visible range should bound large wall rendering by visible rows")) {
        return 1;
    }
    const auto clampedRange = CoverWallLabState::calculateVisibleRange(-4, 999, 0, 7);
    if (!Check(clampedRange.startIndex == 0 && clampedRange.endIndex == 7,
               "visible range should clamp invalid rows and columns")) {
        return 1;
    }

    const std::filesystem::path settingsPath =
        std::filesystem::temp_directory_path() / "coverwall_lab_contract_settings.ini";
    if (!Check(state.saveSettings(settingsPath), "settings save should succeed")) {
        return 1;
    }
    CoverWallLabState restored;
    restored.setTileSizePx(CoverWallLabState::kCompactTileSizePx);
    if (!Check(restored.loadSettings(settingsPath), "settings load should succeed")) {
        return 1;
    }
    std::filesystem::remove(settingsPath);
    if (!Check(restored.filters().tileSizePx == CoverWallLabState::kInspectTileSizePx,
               "settings should persist the last tile size")) {
        return 1;
    }

    const std::filesystem::path importRoot =
        std::filesystem::temp_directory_path() / "coverwall_lab_contract_import";
    std::filesystem::remove_all(importRoot);
    std::filesystem::create_directories(importRoot / "Foley");
    {
        std::ofstream(importRoot / "Foley" / "cover.jpg").put('\0');
        std::ofstream(importRoot / "Foley" / "gravel_step.wav").put('\0');
        std::ofstream(importRoot / "Foley" / "wood_step.wav").put('\0');
    }
    CoverWallLabState imported;
    std::string importError;
    if (!Check(imported.importFolder(importRoot, &importError),
               "importFolder should accept supported audio files without mutating source files")) {
        std::filesystem::remove_all(importRoot);
        return 1;
    }
    if (!Check(imported.results().totalCount == 2 &&
                   imported.results().items.size() == 2 &&
                   imported.results().items.front().coverKey.rfind("image:", 0) == 0,
               "importFolder should build count-truth rows and associate folder cover art")) {
        std::filesystem::remove_all(importRoot);
        return 1;
    }
    const std::filesystem::path emptyRoot =
        std::filesystem::temp_directory_path() / "coverwall_lab_contract_empty";
    std::filesystem::remove_all(emptyRoot);
    std::filesystem::create_directories(emptyRoot);
    CoverWallLabState stableOnFailure;
    const auto beforeFailureCount = stableOnFailure.results().totalCount;
    if (!Check(!stableOnFailure.importFolder(emptyRoot, &importError) &&
                   stableOnFailure.results().totalCount == beforeFailureCount,
               "empty import should keep previous stable dataset")) {
        std::filesystem::remove_all(importRoot);
        std::filesystem::remove_all(emptyRoot);
        return 1;
    }
    std::filesystem::remove_all(importRoot);
    std::filesystem::remove_all(emptyRoot);

    state.clearCover();
    if (!Check(state.filters().selectedCoverKey.empty() && state.filters().keyword == "metal",
               "clearing cover should preserve keyword")) {
        return 1;
    }
    state.clearAllFilters();
    if (!Check(state.results().totalCount == static_cast<long long>(state.results().items.size()) &&
                   state.results().totalCount > metalCount,
               "clearing all filters should restore full fixture wall and count truth")) {
        return 1;
    }

    std::printf("PASS: CoverWallLab filter, count, size, and persistence contracts hold\n");
    return 0;
}
