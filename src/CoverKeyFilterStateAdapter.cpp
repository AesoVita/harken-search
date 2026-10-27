#include "CoverKeyFilterStateAdapter.h"

#include "AppState.h"
#include "CoverKeyFilterWorkflow.h"
#include "CoverModule.h"

#include <utility>
#include <vector>

namespace aura::search {
namespace {

class Effects final : public CoverKeyFilterEffects {
public:
    bool fileHasCover(
        const std::string& filePath,
        const std::string& key) override {
        return CoverModule::FileHasCoverKey(filePath, key);
    }
};

}  // namespace

void ApplyCoverKeyFilter(AppState& state) {
    Effects effects;

    CoverKeyFilterInput input;
    input.active =
        state.coverFilterActive && !state.coverFilterPath.empty();
    input.databaseView = state.viewMode == ViewMode::Database;
    input.browserView = state.viewMode == ViewMode::Browser;
    input.key = state.coverFilterPath;
    const bool hasCachedMembership =
        state.coverFilterMatchSig != 0 &&
        !state.coverFilterMatchPaths.empty();
    input.records.reserve(state.searchResults.size());
    for (const auto& record : state.searchResults) {
        CoverKeyFilterRecord filterRecord;
        filterRecord.filePath = record.filePath;
        filterRecord.embeddedCoverKey = record.embeddedCoverKey;
        filterRecord.folderCoverKey = record.folderCoverKey;
        filterRecord.cachedMatchAvailable = hasCachedMembership;
        filterRecord.cachedMatch =
            hasCachedMembership &&
            state.coverFilterMatchPaths.find(record.filePath) !=
                state.coverFilterMatchPaths.end();
        input.records.push_back(std::move(filterRecord));
    }

    const auto kept = HandleCoverKeyFilter(input, effects);
    if (kept.size() == state.searchResults.size()) return;

    std::vector<AudioRecord> filtered;
    filtered.reserve(kept.size());
    for (const auto index : kept) {
        filtered.push_back(std::move(state.searchResults[index]));
    }
    state.searchResults = std::move(filtered);
    state.loadedCount = static_cast<int>(state.searchResults.size());
    state.bumpSearchResultsGen();
}

}  // namespace aura::search
