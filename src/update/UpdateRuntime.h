#pragma once

#include "update/UpdateCoordinator.h"
#include "update/UpdateDownloadManager.h"
#include "update/UpdateHttpTransport.h"
#include "update/UpdatePersistencePort.h"
#include "update/UpdateStateStore.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace aris::update {

// @stage7-contract
// API contract: this application service owns updater sequencing and state,
// but no thread, clock, sleep, platform handle, or absolute path. Each call
// performs at most one blocking transport attempt. Retry is an owned schedule
// intent that an interruptible external scheduler may invoke later.
// Calls are serialized by the owning service; this object is intentionally not
// a thread scheduler and concurrent method calls are outside the contract.

struct UpdateRuntimeConfig {
    UpdateChannel channel = UpdateChannel::Stable;
    RetryPolicy retryPolicy;
    std::uint64_t reserveBytes = 0;
};

struct UpdateRetryIntent {
    std::uint64_t runtimeGeneration = 0;
    std::uint64_t serial = 0;
    std::uint32_t completedAttempts = 0;
    std::uint64_t delayMs = 0;
    DownloadRequest request;
};

struct UpdateRuntimeCapability;
struct UpdateCandidateHandoffPayload;

// Opaque, lifetime-owned capability for one runtime's serialized check flight.
class UpdateCheckToken {
public:
    UpdateCheckToken(const UpdateCheckToken&) = default;
    UpdateCheckToken& operator=(const UpdateCheckToken&) = default;

private:
    friend class UpdateRuntime;
    UpdateCheckToken(
        std::shared_ptr<const UpdateRuntimeCapability> capability,
        std::uint64_t generation)
        : capability_(std::move(capability)), generation_(generation) {}

    std::shared_ptr<const UpdateRuntimeCapability> capability_;
    std::uint64_t generation_ = 0;
};

// Opaque, copyable, consume-once authorization for one checked candidate.
class UpdateCandidateHandoff {
public:
    UpdateCandidateHandoff(const UpdateCandidateHandoff&) = default;
    UpdateCandidateHandoff& operator=(const UpdateCandidateHandoff&) = default;

    // The checked version is immutable evidence used only to expand a
    // caller-owned relative staging template. It does not grant download or
    // Bootstrap authority.
    const std::string& version() const noexcept;

private:
    friend class UpdateRuntime;
    explicit UpdateCandidateHandoff(
        std::shared_ptr<const UpdateCandidateHandoffPayload> payload)
        : payload_(std::move(payload)) {}

    std::shared_ptr<const UpdateCandidateHandoffPayload> payload_;
};

enum class UpdateRuntimeDisposition {
    NoOp = 0,
    Ready = 1,
    RetryScheduled = 2,
    Cancelled = 3,
    TerminalError = 4,
    CheckStarted = 5,
    UpdateAvailable = 6,
    NoUpdate = 7,
    RestartAuthorized = 8,
};

struct UpdateRuntimeResult {
    UpdateRuntimeDisposition disposition = UpdateRuntimeDisposition::NoOp;
    UpdateSnapshot snapshot;
    std::optional<UpdateRetryIntent> retry;
    std::optional<UpdateCheckToken> checkToken;
    std::optional<UpdateCandidateHandoff> candidateHandoff;
    DownloadErrorKind error = DownloadErrorKind::InvalidArgument;
    std::string detail;
};

class UpdateRuntime {
public:
    UpdateRuntime(HttpTransportPort& transport,
                  UpdatePersistencePort& persistence,
                  UpdateRuntimeConfig config,
                  UpdateStoreRecord initialRecord = {});
    UpdateRuntime(const UpdateRuntime&) = delete;
    UpdateRuntime& operator=(const UpdateRuntime&) = delete;
    UpdateRuntime(UpdateRuntime&&) = delete;
    UpdateRuntime& operator=(UpdateRuntime&&) = delete;

    UpdateRuntimeResult BeginCheck();
    UpdateRuntimeResult CompleteCheckedCandidate(
        const UpdateCheckToken& token,
        const UpdateCandidate& candidate);
    UpdateRuntimeResult CompleteNoUpdate(const UpdateCheckToken& token);
    UpdateRuntimeResult CancelActiveCheck(const UpdateCheckToken& token,
                                          std::string detail);
    UpdateRuntimeResult FailActiveCheck(const UpdateCheckToken& token,
                                        DownloadErrorKind error,
                                        std::string detail);
    UpdateRuntimeResult DownloadCandidate(
        const UpdateCandidateHandoff& handoff,
        const std::string& relativeTarget,
        std::uint64_t availableBytes,
        const HttpTransportCancellation& cancellation);
    UpdateRuntimeResult DownloadCandidate(
        const UpdateCandidate& candidate,
        const std::string& relativeTarget,
        std::uint64_t availableBytes,
        const HttpTransportCancellation& cancellation);
    UpdateRuntimeResult ResumeInterrupted(
        const std::string& relativeTarget,
        std::uint64_t availableBytes,
        const HttpTransportCancellation& cancellation);
    UpdateRuntimeResult RunScheduledRetry(
        const UpdateRetryIntent& intent,
        std::uint64_t availableBytes,
        const HttpTransportCancellation& cancellation);
    UpdateRuntimeResult ConfirmRestart();

    UpdateSnapshot snapshot() const { return UpdateCoordinator::Snapshot(state_); }
    const UpdateStoreRecord& record() const { return record_; }
    const DownloadFlightState& flight() const { return flight_; }

private:
    class Sink;

    UpdateRuntimeResult RunAttempt(
        const DownloadRequest& request,
        std::uint32_t completedAttempts,
        std::uint64_t availableBytes,
        const HttpTransportCancellation& cancellation,
        bool cancellationAlreadyChecked = false);
    UpdateRuntimeResult AdmitDownloadCandidate(
        const UpdateCandidate& candidate,
        const std::string& relativeTarget,
        std::uint64_t availableBytes,
        const HttpTransportCancellation& cancellation);
    bool OwnsActiveCheck(const UpdateCheckToken& token) const;
    bool OwnsPendingHandoff(const UpdateCandidateHandoff& handoff) const;
    bool Apply(const UpdateEvent& event,
               const std::optional<std::string>& stagedPath = std::nullopt);
    bool Persist();
    UpdateRuntimeResult ScheduleRetry(const DownloadRequest& request,
                                      std::uint32_t completedAttempts,
                                      std::uint64_t delayMs);
    UpdateRuntimeResult Terminal(DownloadErrorKind error,
                                 std::string detail);
    UpdateRuntimeResult PersistenceFailure(std::string detail) const;
    UpdateRuntimeResult Result(UpdateRuntimeDisposition disposition,
                               DownloadErrorKind error =
                                   DownloadErrorKind::InvalidArgument,
                               std::string detail = {}) const;

    HttpTransportPort& transport_;
    UpdatePersistencePort& persistence_;
    std::shared_ptr<const UpdateRuntimeCapability> capability_;
    UpdateRuntimeConfig config_;
    UpdateStoreRecord record_;
    UpdateState state_;
    DownloadFlightState flight_;
    std::uint64_t nextCheckGeneration_ = 0;
    std::uint64_t activeCheckGeneration_ = 0;
    std::uint64_t nextHandoffSerial_ = 0;
    std::optional<UpdateCandidateHandoff> pendingCandidateHandoff_;
    std::uint64_t runtimeGeneration_ = 0;
    std::uint64_t nextRetrySerial_ = 0;
    std::uint64_t pendingRetrySerial_ = 0;
    std::uint32_t pendingRetryCompletedAttempts_ = 0;
    std::optional<DownloadRequest> pendingRetryRequest_;
};

}  // namespace aris::update
