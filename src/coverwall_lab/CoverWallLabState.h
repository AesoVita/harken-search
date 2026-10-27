#pragma once

#include "../CoverBrowseSurfaceState.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aura::coverwall_lab {

using TileSizePreset = aura::coverwall::TileSizePreset;

struct LabCoverItem {
    std::string coverKey;
    std::string displayLabel;
    std::string folderPath;
    std::string representativeFilePath;
    std::string coverSourcePath;
    int hitCount = 1;
};

struct LabFilterState {
    std::string keyword;
    std::string selectedPath;
    std::string selectedCoverKey;
    int tileSizePx = 132;
    int scrollAnchorItemId = 0;
};

struct LabResultPage {
    std::vector<LabCoverItem> items;
    std::int64_t totalCount = 0;
    bool isFromFixture = true;
    bool isStaleWhileRefreshing = false;
};

struct LabVisibleRange {
    int startIndex = 0;
    int endIndex = 0;
};

// @stage7-contract
// API contract: CoverWallLabState owns fixture/import results, filters, and
// settings snapshots. Mutators are synchronous owner-thread operations;
// filesystem paths are borrowed per load/import call and returned references
// remain valid only until the next mutation. Save/load report filesystem
// failure through bool and never transfer path ownership.
class CoverWallLabState {
public:
    static constexpr int kMinTileSizePx = 72;
    static constexpr int kMaxTileSizePx = 260;
    static constexpr int kCompactTileSizePx =
        aura::coverwall::PresetTileSizePx(TileSizePreset::Compact);
    static constexpr int kBrowseTileSizePx =
        aura::coverwall::PresetTileSizePx(TileSizePreset::Browse);
    static constexpr int kInspectTileSizePx =
        aura::coverwall::PresetTileSizePx(TileSizePreset::Inspect);
    static constexpr int kDefaultTileSizePx = kBrowseTileSizePx;
    static constexpr int kMaxImportAudioFiles = 5000;
    static constexpr int kMaxImportVisitedFiles = 50000;

    CoverWallLabState();

    void loadFixture();
    bool importFolder(const std::filesystem::path& root, std::string* errorMessage);

    void setKeyword(std::string keyword);
    void selectPath(std::string path);
    void selectCover(std::string coverKey);
    void clearKeyword();
    void clearPath();
    void clearCover();
    void clearAllFilters();

    int setTileSizePx(int requestedPx);
    int applyPreset(TileSizePreset preset);
    int calculateColumnCount(float availableWidth, float gapPx = 12.0f) const;
    static LabVisibleRange calculateVisibleRange(int rowStart, int rowEnd, int columns, int totalItems);

    bool saveSettings(const std::filesystem::path& path) const;
    bool loadSettings(const std::filesystem::path& path);

    const LabFilterState& filters() const { return filters_; }
    const LabResultPage& results() const { return results_; }
    const std::vector<std::string>& pathFacets() const { return pathFacets_; }
    const std::string& sourceLabel() const { return sourceLabel_; }

private:
    static std::string normalizeForSearch(const std::string& text);
    static bool containsInsensitive(const std::string& haystack, const std::string& needle);
    static bool pathMatches(const std::string& itemPath, const std::string& selectedPath);
    static bool isAudioFile(const std::filesystem::path& path);
    static bool isCoverImageFile(const std::filesystem::path& path);
    static std::string toUtf8Path(const std::filesystem::path& path);

    void rebuildFacets();
    void rebuildResults();
    void rebuildHitCounts();

    LabFilterState filters_;
    LabResultPage results_;
    std::vector<LabCoverItem> allItems_;
    std::vector<std::string> pathFacets_;
    std::string sourceLabel_;
};

}
