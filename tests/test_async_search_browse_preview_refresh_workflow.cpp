#include "AsyncSearchBrowsePreviewRefreshWorkflow.h"

#include <cassert>
#include <string>
#include <vector>

struct Effects final : aura::search::AsyncSearchBrowsePreviewRefreshEffects {
    aura::search::AsyncSearchBrowsePreviewRefreshState state;
    aura::search::AsyncSearchBrowsePreviewRecord selected;
    std::vector<aura::search::AsyncSearchBrowsePreviewRecord> records;
    mutable std::vector<std::string> calls;
    bool hasSelected = true;
    int selectedIndex = -1;

    aura::search::AsyncSearchBrowsePreviewRefreshState readState() const override {
        calls.push_back("state");
        return state;
    }
    bool readSelectedRecord(
        aura::search::AsyncSearchBrowsePreviewRecord& record) const override {
        if (!hasSelected) return false;
        record = selected;
        return true;
    }
    std::vector<aura::search::AsyncSearchBrowsePreviewRecord>
    readRefreshedRecords() const override {
        calls.push_back("records");
        return records;
    }
    void startDeferredCoverPriming() override { calls.push_back("prime"); }
    void applyCoverKeyFilter() override { calls.push_back("filter"); }
    void selectResult(int index) override {
        calls.push_back("select");
        selectedIndex = index;
    }
};

int main() {
    Effects effects;
    effects.state.databaseView = true;
    effects.state.integratedMode = true;
    effects.state.coverFilterActive = true;
    effects.state.coverFilterKey = "cover";
    effects.state.pinnedCoverKey = "cover";
    effects.state.pinnedFilePath = "preferred.wav";
    effects.state.folderFilterActive = true;
    effects.state.selectedFolderPath = "Folder";
    effects.selected = {11, "old.wav", "Folder"};
    effects.records = {
        {10, "first.wav", "Other"},
        {11, "old.wav", "Folder"},
        {12, "preferred.wav", "Folder"},
    };

    const auto selection =
        aura::search::CaptureAsyncSearchBrowsePreviewRefresh(effects);
    aura::search::HandleAsyncSearchBrowsePreviewRefreshRestore(
        selection, effects);
    assert(effects.selectedIndex == 2);
    assert((effects.calls == std::vector<std::string>{
        "state", "prime", "filter", "state", "records", "select"}));

    effects.selectedIndex = -1;
    effects.calls.clear();
    effects.state.databaseView = false;
    aura::search::HandleAsyncSearchBrowsePreviewRefreshRestore(
        selection, effects);
    assert(effects.selectedIndex == -1);
    assert((effects.calls ==
            std::vector<std::string>{"prime", "filter", "state", "records"}));

    effects.selectedIndex = -1;
    effects.calls.clear();
    effects.state.databaseView = true;
    effects.state.coverFilterActive = false;
    effects.state.coverFilterKey.clear();
    effects.state.pinnedCoverKey.clear();
    effects.state.pinnedFilePath.clear();
    effects.hasSelected = false;
    const auto clearedSelection =
        aura::search::CaptureAsyncSearchBrowsePreviewRefresh(effects);
    aura::search::HandleAsyncSearchBrowsePreviewRefreshRestore(
        clearedSelection, effects);
    if (effects.selectedIndex != -1) return 1;
    if (effects.calls != std::vector<std::string>{
            "state", "prime", "filter", "state", "records"}) return 2;
    return 0;
}
