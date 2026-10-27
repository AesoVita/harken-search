#include "AsyncSearchCoverPreloadAdapter.h"

#include <cassert>
#include <string>
#include <unordered_map>

int main() {
    std::unordered_map<std::size_t, aura::search::AsyncSearchCoverPreloadRecord> records{
        {0, {"Folder", "", ""}},
        {1, {"Folder", "embedded", "file.wav"}},
        {2, {"", "embedded2", "file2.wav"}},
    };
    int folderCalls = 0;
    int embeddedCalls = 0;
    aura::search::AsyncSearchCoverPreloadAdapter adapter(
        [&records](std::size_t index,
                   aura::search::AsyncSearchCoverPreloadRecord& record) {
            const auto it = records.find(index);
            if (it == records.end()) return false;
            record = it->second;
            return true;
        },
        [&folderCalls](const std::string&) { ++folderCalls; },
        [&embeddedCalls](const std::string&, const std::string&) { ++embeddedCalls; });

    adapter.preloadResult(0);
    adapter.preloadResult(0);
    adapter.preloadResult(1);
    adapter.preloadResult(2);
    adapter.preloadResult(99);
    assert(folderCalls == 1);
    assert(embeddedCalls == 2);
    return 0;
}
