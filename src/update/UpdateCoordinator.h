#pragma once

#include "update/UpdateState.h"

#include <cstdint>
#include <string>

namespace aris::update {

enum class UpdateEventKind {
    CheckRequested = 0,
    CheckSucceeded = 1,
    CheckFailed = 2,
    DownloadRequested = 3,
    DownloadProgressed = 4,
    DownloadCompleted = 5,
    DownloadFailed = 6,
    DownloadCancelled = 7,
    RestartConfirmed = 8,
    CheckCompletedNoUpdate = 9,
    DownloadCandidateAccepted = 10,
};

class UpdateEvent {
public:
    static UpdateEvent CheckRequested();
    static UpdateEvent CheckSucceeded(UpdateCandidate candidate);
    static UpdateEvent CheckCompletedNoUpdate();
    static UpdateEvent CheckFailed(std::string detail);
    static UpdateEvent DownloadCandidateAccepted(UpdateCandidate candidate);
    static UpdateEvent DownloadRequested();
    static UpdateEvent DownloadProgressed(std::uint64_t downloadedBytes);
    static UpdateEvent DownloadCompleted();
    static UpdateEvent DownloadFailed(std::string detail);
    static UpdateEvent DownloadCancelled();
    static UpdateEvent RestartConfirmed();

    UpdateEventKind kind() const { return kind_; }
    const UpdateCandidate& candidate() const { return candidate_; }
    std::uint64_t downloadedBytes() const { return downloadedBytes_; }
    const std::string& detail() const { return detail_; }

private:
    explicit UpdateEvent(UpdateEventKind kind) : kind_(kind) {}

    UpdateEventKind kind_;
    UpdateCandidate candidate_;
    std::uint64_t downloadedBytes_ = 0;
    std::string detail_;
};

// @stage7-contract
// API contract: reductions borrow immutable input values and return entirely
// owned state/effect values. No effect is executed by this domain boundary.
class UpdateCoordinator {
public:
    static bool CanBeginCheck(UpdatePhase phase);
    static UpdateTransition Reduce(const UpdateState& state,
                                   const UpdateEvent& event);
    static UpdateSnapshot Snapshot(const UpdateState& state);
    static UpdateTransition Restore(const UpdateSnapshot& snapshot);
};

}  // namespace aris::update
