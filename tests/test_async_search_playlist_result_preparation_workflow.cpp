#include "AsyncSearchPlaylistResultPreparationWorkflow.h"

#include <cassert>
#include <string>
#include <vector>

struct Effects final
    : aura::search::AsyncSearchPlaylistResultPreparationEffects {
    std::vector<std::string> calls;
    int filteredCount = 7;

    void clearPlaylistFolderSearch() override { calls.push_back("clear"); }
    int applyPlaylistFilters(bool rebuildFolderTree) override {
        calls.push_back(rebuildFolderTree ? "playlist:rebuild" : "playlist:keep");
        return filteredCount;
    }
    int applyLibraryFilters() override {
        calls.push_back("library");
        return filteredCount;
    }
    void reportPlaylistRefresh(int, bool rebuildFolderTree) override {
        calls.push_back(rebuildFolderTree ? "report:rebuild" : "report:keep");
    }
    void reportLibraryRefresh(std::size_t, int) override {
        calls.push_back("report:library");
    }
};

int main() {
    using aura::search::AsyncSearchPlaylistResultPreparationInput;
    using aura::search::HandleAsyncSearchPlaylistResultPreparation;

    Effects largeRefresh;
    AsyncSearchPlaylistResultPreparationInput input;
    input.playlistView = true;
    input.refresh = true;
    input.totalCount = 1001;
    input.loadAllThreshold = 1000;
    auto output = HandleAsyncSearchPlaylistResultPreparation(input, largeRefresh);
    assert(output.prepared && output.loadedCount == 7);
    assert((largeRefresh.calls ==
            std::vector<std::string>{"clear", "playlist:keep", "report:keep"}));

    Effects smallRefresh;
    input.totalCount = 1000;
    output = HandleAsyncSearchPlaylistResultPreparation(input, smallRefresh);
    assert((smallRefresh.calls ==
            std::vector<std::string>{"playlist:rebuild", "report:rebuild"}));

    Effects loadAll;
    input.refresh = false;
    input.loadAll = true;
    output = HandleAsyncSearchPlaylistResultPreparation(input, loadAll);
    assert((loadAll.calls == std::vector<std::string>{"playlist:rebuild"}));

    Effects library;
    input = {};
    input.libraryView = true;
    input.refresh = true;
    input.rawCount = 12;
    output = HandleAsyncSearchPlaylistResultPreparation(input, library);
    assert(output.prepared && output.loadedCount == 7);
    assert((library.calls ==
            std::vector<std::string>{"library", "report:library"}));

    Effects other;
    input = {};
    output = HandleAsyncSearchPlaylistResultPreparation(input, other);
    assert(!output.prepared && other.calls.empty());
    return 0;
}
