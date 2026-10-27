#include "AsyncSearchRefreshFacetsWorkflow.h"

#include <cassert>
#include <vector>

struct Effects final : aura::search::AsyncSearchRefreshFacetsEffects {
    std::vector<int> events;
    void replaceFacets(bool) override { events.push_back(1); }
    void clearFacets() override { events.push_back(4); }
    void persistFacets(bool) override { events.push_back(2); }
    void facetsChanged() override { events.push_back(3); }
};

int main() {
    Effects effects;
    aura::search::AsyncSearchRefreshFacetsInput payload;
    payload.databaseView = true;
    payload.loadedCount = 12;
    payload.totalCount = 12;
    payload.hasCoverFacetPayload = true;
    payload.persistenceEligible = true;
    assert(aura::search::HandleAsyncSearchRefreshFacets(payload, effects));
    assert((effects.events == std::vector<int>{1, 2, 3}));

    Effects skipped;
    aura::search::AsyncSearchRefreshFacetsInput noDecision;
    noDecision.databaseView = true;
    noDecision.loadedCount = 12;
    noDecision.totalCount = 12;
    assert(!aura::search::HandleAsyncSearchRefreshFacets(noDecision, skipped));
    assert(skipped.events.empty());

    Effects staleFollowUp;
    auto followUp = noDecision;
    followUp.pendingFacetMatchesRequest = true;
    followUp.hasActiveDbFilter = true;
    assert(aura::search::HandleAsyncSearchRefreshFacets(followUp, staleFollowUp));
    assert((staleFollowUp.events == std::vector<int>{1, 3}));

    Effects nonDatabase;
    assert(aura::search::HandleAsyncSearchRefreshFacets({}, nonDatabase));
    assert((nonDatabase.events == std::vector<int>{4, 3}));

    Effects playlist;
    aura::search::AsyncSearchRefreshFacetsInput playlistPayload;
    playlistPayload.databaseView = false;
    playlistPayload.playlistView = true;
    playlistPayload.loadedCount = 2;
    playlistPayload.totalCount = 4;
    playlistPayload.hasCoverFacetPayload = true;
    playlistPayload.persistenceEligible = true;
    assert(aura::search::HandleAsyncSearchRefreshFacets(playlistPayload, playlist));
    assert((playlist.events == std::vector<int>{1, 3}));

    Effects playlistEmpty;
    aura::search::AsyncSearchRefreshFacetsInput playlistEmptyPayload;
    playlistEmptyPayload.databaseView = false;
    playlistEmptyPayload.playlistView = true;
    playlistEmptyPayload.loadedCount = 0;
    playlistEmptyPayload.totalCount = 0;
    playlistEmptyPayload.hasCoverFacetPayload = false;
    assert(aura::search::HandleAsyncSearchRefreshFacets(playlistEmptyPayload, playlistEmpty));
    assert((playlistEmpty.events == std::vector<int>{4, 3}));
    return 0;
}
