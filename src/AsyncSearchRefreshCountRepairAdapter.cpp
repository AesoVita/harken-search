#include "AsyncSearchRefreshCountRepairAdapter.h"

#include <utility>

namespace aura::search {

AsyncSearchRefreshCountRepairAdapter::AsyncSearchRefreshCountRepairAdapter(
    SnapshotProvider snapshot,
    Action markPending,
    SubmitRepair submitRepair,
    Action reportPending,
    SetTotalCount setTotalCount)
    : snapshot_(std::move(snapshot)),
      markPending_(std::move(markPending)),
      submitRepair_(std::move(submitRepair)),
      reportPending_(std::move(reportPending)),
      setTotalCount_(std::move(setTotalCount)) {}

void AsyncSearchRefreshCountRepairAdapter::apply() {
    const auto snapshot = snapshot_();
    if (!HandleAsyncSearchRefreshCountRepair(snapshot, *this)) {
        setTotalCount_(snapshot.totalCount);
    }
}

void AsyncSearchRefreshCountRepairAdapter::markCountFollowUpPending() {
    markPending_();
}

void AsyncSearchRefreshCountRepairAdapter::submitRepair(
    bool needRepairFacets) {
    submitRepair_(needRepairFacets);
}

void AsyncSearchRefreshCountRepairAdapter::reportRepairPending() {
    reportPending_();
}

}  // namespace aura::search
