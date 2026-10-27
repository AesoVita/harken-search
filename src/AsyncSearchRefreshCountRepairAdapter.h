#pragma once

#include "AsyncSearchRefreshCountRepairWorkflow.h"

#include <cstdint>
#include <functional>

struct AppState;
struct AsyncSearchResult;
struct SearchFilter;

namespace aura::search {

class AsyncSearchRefreshCountRepairAdapter final
    : public AsyncSearchRefreshCountRepairEffects {
public:
    using SnapshotProvider =
        std::function<AsyncSearchRefreshCountRepairInput()>;
    using Action = std::function<void()>;
    using SubmitRepair = std::function<void(bool)>;
    using SetTotalCount = std::function<void(std::int64_t)>;
    using ActiveFilterPredicate = std::function<bool(const AppState&)>;
    using StateRepairSubmitter =
        std::function<std::uint64_t(AppState&, const SearchFilter&, bool)>;

    AsyncSearchRefreshCountRepairAdapter(
        AppState& state,
        AsyncSearchResult& result,
        ActiveFilterPredicate hasActiveFilter,
        StateRepairSubmitter submitRepair);
    AsyncSearchRefreshCountRepairAdapter(
        SnapshotProvider snapshot,
        Action markPending,
        SubmitRepair submitRepair,
        Action reportPending,
        SetTotalCount setTotalCount);

    void apply();
    void markCountFollowUpPending() override;
    void submitRepair(bool needRepairFacets) override;
    void reportRepairPending() override;

private:
    SnapshotProvider snapshot_;
    Action markPending_;
    SubmitRepair submitRepair_;
    Action reportPending_;
    SetTotalCount setTotalCount_;
};

}  // namespace aura::search
