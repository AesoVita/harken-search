#include "AsyncSearchRefreshFacetsAdapter.h"

#include <utility>

namespace aura::search {

AsyncSearchRefreshFacetsAdapter::AsyncSearchRefreshFacetsAdapter(
    SnapshotProvider snapshot,
    ReplaceFacets replaceFacets,
    Action clearFacets,
    PersistFacets persistFacets,
    Action facetsChanged)
    : snapshot_(std::move(snapshot)),
      replaceFacets_(std::move(replaceFacets)),
      clearFacets_(std::move(clearFacets)),
      persistFacets_(std::move(persistFacets)),
      facetsChanged_(std::move(facetsChanged)) {}

void AsyncSearchRefreshFacetsAdapter::apply() {
    (void)HandleAsyncSearchRefreshFacets(snapshot_(), *this);
}

void AsyncSearchRefreshFacetsAdapter::replaceFacets(bool preservePathFacets) {
    replaceFacets_(preservePathFacets);
}

void AsyncSearchRefreshFacetsAdapter::clearFacets() {
    clearFacets_();
}

void AsyncSearchRefreshFacetsAdapter::persistFacets(bool persistPathFacets) {
    persistFacets_(persistPathFacets);
}

void AsyncSearchRefreshFacetsAdapter::facetsChanged() {
    facetsChanged_();
}

}  // namespace aura::search
