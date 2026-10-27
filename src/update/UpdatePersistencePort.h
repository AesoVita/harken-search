#pragma once

#include "update/UpdateDownloadManager.h"
#include "update/UpdateHttpTransport.h"
#include "update/UpdateStateStore.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aris::update {

// @stage7-contract
// API contract: this blocking port consumes only owned, update-root-relative
// intents. It never exposes the trusted absolute root or platform handles.
// Plans are validated in full before mutation, then executed in caller order
// and stop at the first explicit error.

enum class UpdatePersistenceError {
    None,
    InvalidIntent,
    DirectoryCreateFailed,
    OpenFailed,
    WriteFailed,
    FlushFailed,
    ReplaceFailed,
    DeleteFailed,
    QuarantineFailed,
    StaleOffset,
    StaleGeneration,
};

struct UpdatePersistenceResult {
    UpdatePersistenceError error = UpdatePersistenceError::None;
    std::uint32_t systemError = 0;
    std::size_t completedIntents = 0;

    bool ok() const { return error == UpdatePersistenceError::None; }
};

enum class StagedIdentityStatus {
    Present,
    Missing,
    InvalidPath,
    ProbeFailed,
    NotRegularFile,
    HashFailed,
};

struct StagedIdentityObservation {
    StagedIdentityStatus status = StagedIdentityStatus::ProbeFailed;
    StagedPatchIdentity identity;
    std::uint32_t systemError = 0;
};

enum class ResumeMetadataStatus {
    Present,
    Missing,
    InvalidPath,
    Malformed,
    ReadFailed,
};

struct ResumeMetadataObservation {
    ResumeMetadataStatus status = ResumeMetadataStatus::ReadFailed;
    ResumeMetadata metadata;
    std::uint32_t systemError = 0;
};

class UpdatePersistencePort {
public:
    virtual ~UpdatePersistencePort() = default;

    // Optional, read-only presentation data from a hash-matched ready archive.
    // Failure is deliberately independent of update readiness/installation.
    virtual bool ReadReadyReleaseNotes(const std::string& relativePath,
        std::uint64_t expectedSize, const std::string& expectedSha256,
        std::string& json) {
        (void)relativePath; (void)expectedSize; (void)expectedSha256;
        json.clear();
        return false;
    }

    virtual UpdatePersistenceResult ExecuteStoreIntents(
        const std::vector<StoreIntent>& intents) = 0;
    virtual UpdatePersistenceResult ExecuteDownloadIntents(
        const std::vector<DownloadCleanupIntent>& intents) = 0;
    virtual UpdatePersistenceResult ActivateTransfer(
        const DownloadFlightState& activeFlight,
        const DownloadFlightToken& token,
        const HttpTransferIdentity& identity) = 0;
    virtual UpdatePersistenceResult DeactivateTransfer(
        const DownloadFlightToken& token,
        const HttpTransferIdentity& identity) = 0;
    virtual UpdatePersistenceResult ExecuteTransferChunk(
        const TransferAppendIntent& append,
        const ResumeMetadataIntent& metadata) = 0;
    virtual ResumeMetadataObservation ObserveResumeMetadata(
        const std::string& relativePath) = 0;
    virtual StagedIdentityObservation ObserveStagedPatch(
        const std::string& relativePath) = 0;
};

}  // namespace aris::update
