#include "AsyncSearchRefreshCountRepairAdapter.h"

#include <cassert>
#include <cstdint>

int main() {
    bool marked = false;
    bool repairFacets = false;
    bool submitted = false;
    bool reported = false;
    std::int64_t storedTotal = -1;

    aura::search::AsyncSearchRefreshCountRepairAdapter repair(
        [] {
            return aura::search::AsyncSearchRefreshCountRepairInput{
                false, 3, 0, true, false, true, false};
        },
        [&] { marked = true; },
        [&](bool needFacets) {
            submitted = true;
            repairFacets = needFacets;
        },
        [&] { reported = true; },
        [&](std::int64_t total) { storedTotal = total; });
    repair.apply();
    assert(marked);
    assert(submitted && repairFacets);
    assert(!reported);
    assert(storedTotal == -1);

    aura::search::AsyncSearchRefreshCountRepairAdapter normal(
        [] {
            return aura::search::AsyncSearchRefreshCountRepairInput{
                false, 3, 9, true, false, true, false};
        },
        [] {},
        [](bool) {},
        [] {},
        [&](std::int64_t total) { storedTotal = total; });
    normal.apply();
    assert(storedTotal == 9);
    return 0;
}
