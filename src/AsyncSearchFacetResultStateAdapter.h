#pragma once

#include "AsyncSearchFacetResultWorkflow.h"

#include <functional>

struct AppState;
struct AsyncSearchResult;
struct SearchFilter;

namespace aura::search {

class AsyncSearchFacetResultStateAdapter final
    : private AsyncSearchFacetResultEffects {
public:
    using PersistencePredicate = std::function<bool(const SearchFilter&)>;
    using WarmStartPredicate = std::function<bool()>;
    using RefreshRequest = std::function<void(AppState&)>;

    AsyncSearchFacetResultStateAdapter(
        AppState& state,
        AsyncSearchResult& result,
        AsyncSearchFacetResultKind kind,
        PersistencePredicate persistenceEligible,
        WarmStartPredicate inconsistentWarmStartTotal,
        RefreshRequest requestRefresh);

    void apply();

private:
    void updateTotalCount(std::int64_t total) override;
    void reconcileHasMore() override;
    void replaceFacets(bool preservePathFacets) override;
    void persistFacets(bool persistPathFacets) override;
    void facetsChanged() override;
    void synchronizeView() override;
    void setCountFollowUp(bool pending) override;
    void releasePrimarySearch() override;
    void releaseFacetRequest() override;

    AppState& state_;
    AsyncSearchResult& result_;
    AsyncSearchFacetResultKind kind_;
    PersistencePredicate persistenceEligible_;
    WarmStartPredicate inconsistentWarmStartTotal_;
    RefreshRequest requestRefresh_;
};

}  // namespace aura::search
