#pragma once

#include "AsyncSearchRefreshFacetsWorkflow.h"

#include <functional>

struct AppState;
struct AsyncSearchResult;
struct SearchFilter;

namespace aura::search {

class AsyncSearchRefreshFacetsAdapter final
    : public AsyncSearchRefreshFacetsEffects {
public:
    using SnapshotProvider = std::function<AsyncSearchRefreshFacetsInput()>;
    using ReplaceFacets = std::function<void(bool)>;
    using Action = std::function<void()>;
    using PersistFacets = std::function<void(bool)>;
    using ActiveFilterPredicate = std::function<bool(const AppState&)>;
    using PersistencePredicate = std::function<bool(const SearchFilter&)>;
    using RefreshRequest = std::function<void(AppState&)>;

    AsyncSearchRefreshFacetsAdapter(
        AppState& state,
        AsyncSearchResult& result,
        ActiveFilterPredicate hasActiveFilter,
        PersistencePredicate persistenceEligible,
        RefreshRequest requestRefresh);
    AsyncSearchRefreshFacetsAdapter(
        SnapshotProvider snapshot,
        ReplaceFacets replaceFacets,
        Action clearFacets,
        PersistFacets persistFacets,
        Action facetsChanged);

    void apply();
    void replaceFacets(bool preservePathFacets) override;
    void clearFacets() override;
    void persistFacets(bool persistPathFacets) override;
    void facetsChanged() override;

private:
    SnapshotProvider snapshot_;
    ReplaceFacets replaceFacets_;
    Action clearFacets_;
    PersistFacets persistFacets_;
    Action facetsChanged_;
};

}  // namespace aura::search
