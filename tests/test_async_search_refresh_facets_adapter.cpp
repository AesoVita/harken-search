#include "AsyncSearchRefreshFacetsAdapter.h"

#include <cassert>
#include <cstdint>

int main() {
    int replaced = 0;
    int persisted = 0;
    int changed = 0;
    int cleared = 0;
    aura::search::AsyncSearchRefreshFacetsAdapter adapter(
        [] {
            aura::search::AsyncSearchRefreshFacetsInput input;
            input.databaseView = true;
            input.loadedCount = 4;
            input.totalCount = 4;
            input.hasCoverFacetPayload = true;
            input.persistenceEligible = true;
            return input;
        },
        [&](bool) { ++replaced; },
        [&] { ++cleared; },
        [&](bool) { ++persisted; },
        [&] { ++changed; });
    adapter.apply();
    assert(replaced == 1 && persisted == 1 && changed == 1 && cleared == 0);

    aura::search::AsyncSearchRefreshFacetsAdapter nonDatabase(
        [] { return aura::search::AsyncSearchRefreshFacetsInput{}; },
        [&](bool) { ++replaced; },
        [&] { ++cleared; },
        [&](bool) { ++persisted; },
        [&] { ++changed; });
    nonDatabase.apply();
    assert(cleared == 1 && changed == 2);

    int playlistReplaced = 0;
    int playlistPersisted = 0;
    int playlistChanged = 0;
    int playlistCleared = 0;
    aura::search::AsyncSearchRefreshFacetsAdapter playlistKeep(
        [] {
            aura::search::AsyncSearchRefreshFacetsInput input;
            input.databaseView = false;
            input.playlistView = true;
            input.loadedCount = 2;
            input.totalCount = 4;
            input.hasCoverFacetPayload = true;
            input.persistenceEligible = true;
            return input;
        },
        [&](bool) { ++playlistReplaced; },
        [&] { ++playlistCleared; },
        [&](bool) { ++playlistPersisted; },
        [&] { ++playlistChanged; });
    playlistKeep.apply();
    assert(playlistReplaced == 1 && playlistPersisted == 0 &&
           playlistChanged == 1 && playlistCleared == 0);

    aura::search::AsyncSearchRefreshFacetsAdapter playlistEmpty(
        [] {
            aura::search::AsyncSearchRefreshFacetsInput input;
            input.databaseView = false;
            input.playlistView = true;
            return input;
        },
        [&](bool) { ++playlistReplaced; },
        [&] { ++playlistCleared; },
        [&](bool) { ++playlistPersisted; },
        [&] { ++playlistChanged; });
    playlistEmpty.apply();
    assert(playlistCleared == 1 && playlistChanged == 2 && playlistPersisted == 0);
    return 0;
}
