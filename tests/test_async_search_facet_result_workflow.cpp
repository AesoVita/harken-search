#include "src/AsyncSearchFacetResultWorkflow.h"

#include <cassert>
#include <string>
#include <vector>

using namespace aura::search;

struct Effects final : AsyncSearchFacetResultEffects {
    std::vector<std::string> calls;
    void updateTotalCount(std::int64_t) override { calls.push_back("total"); }
    void reconcileHasMore() override { calls.push_back("more"); }
    void replaceFacets(bool preserve) override { calls.push_back(preserve ? "replace-preserve" : "replace"); }
    void persistFacets(bool path) override { calls.push_back(path ? "persist-both" : "persist-cover"); }
    void facetsChanged() override { calls.push_back("changed"); }
    void synchronizeView() override { calls.push_back("sync"); }
    void setCountFollowUp(bool pending) override { calls.push_back(pending ? "follow" : "clear-follow"); }
    void releasePrimarySearch() override { calls.push_back("release-primary"); }
    void releaseFacetRequest() override { calls.push_back("release-facet"); }
};

int main() {
    Effects facets;
    assert(HandleAsyncSearchFacetResult(
        {AsyncSearchFacetResultKind::FacetsOnly, true, true, true, true, true}, facets));
    assert((facets.calls == std::vector<std::string>{
        "replace-preserve", "persist-cover", "changed", "sync", "release-facet"}));

    Effects count;
    AsyncSearchFacetResultInput input;
    input.kind = AsyncSearchFacetResultKind::CountAndFacetsOnly;
    input.success = true;
    input.databaseView = true;
    input.facetsComputed = false;
    input.inconsistentWarmStartTotal = true;
    input.standaloneCountFollowUp = true;
    assert(HandleAsyncSearchFacetResult(input, count));
    assert((count.calls == std::vector<std::string>{
        "sync", "follow", "release-primary", "release-facet"}));

    Effects playlist;
    AsyncSearchFacetResultInput playlistInput;
    playlistInput.kind = AsyncSearchFacetResultKind::FacetsOnly;
    playlistInput.success = true;
    playlistInput.databaseView = false;
    playlistInput.playlistView = true;
    playlistInput.facetsComputed = true;
    playlistInput.persistenceEligible = true;
    assert(HandleAsyncSearchFacetResult(playlistInput, playlist));
    assert((playlist.calls == std::vector<std::string>{
        "replace", "changed", "sync", "release-facet"}));
    return 0;
}
