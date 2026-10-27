#pragma once

#include "update/UpdateManifest.h"
#include "update/UpdateState.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aris::update {

// @stage7-contract
// API contract: the header exposes owned records and explicit persistence
// intents only. JSON and atomic filesystem execution remain implementation and
// caller concerns respectively.

struct UpdateRollbackMarker {
    bool pending = false;
    std::string version;
    std::string reason;
};

struct UpdateStoreRecord {
    UpdateChannel channel = UpdateChannel::Stable;
    UpdateSnapshot snapshot;
    std::uint32_t failureCount = 0;
    std::uint32_t applyAttempts = 0;
    UpdateRollbackMarker rollback;
    std::string stagedRelativePath;
    std::string diagnosticRelativePath;
};

enum class StoreError {
    None,
    JsonInvalid,
    SchemaUnsupported,
    ProductMismatch,
    ChannelInvalid,
    FieldInvalid,
};

struct StoreSerializeResult {
    StoreError error = StoreError::None;
    std::string bytes;

    bool ok() const { return error == StoreError::None; }
};

struct StoreDeserializeResult {
    StoreError error = StoreError::None;
    UpdateStoreRecord record;

    bool ok() const { return error == StoreError::None; }
};

enum class StoreIntentKind {
    WriteTempFile,
    AtomicReplace,
    DeleteStateFile,
    DeleteStagedPatch,
    QuarantineStagedPatch,
};

struct StoreIntent {
    StoreIntentKind kind = StoreIntentKind::WriteTempFile;
    // Paths are relative to the trusted update root owned by the future
    // persistence port. This policy never accepts or emits absolute roots.
    std::string path;
    std::string targetPath;
    std::string bytes;
};

struct StoreWritePlan {
    StoreError error = StoreError::None;
    std::vector<StoreIntent> intents;

    bool ok() const { return error == StoreError::None; }
};

struct StagedPatchIdentity {
    bool exists = false;
    std::string relativePath;
    std::uint64_t size = 0;
    std::string sha256;
};

enum class PatchRecordDecisionKind {
    NoReadyPatch,
    Ready,
    AlreadyApplied,
    InvalidStagedPatch,
    Quarantined,
    InvalidRecord,
};

struct PatchRecordDecision {
    PatchRecordDecisionKind kind = PatchRecordDecisionKind::InvalidRecord;
    bool usable = false;
    std::vector<StoreIntent> intents;
};

struct ApplyAttemptDecision {
    bool allowed = false;
    UpdateStoreRecord record;
};

class UpdateStateStore {
public:
    static StoreSerializeResult Serialize(const UpdateStoreRecord& record);
    static StoreDeserializeResult Deserialize(std::string_view bytes);
    static StoreWritePlan PlanAtomicWrite(const UpdateStoreRecord& record);
    static PatchRecordDecision EvaluatePatch(
        const UpdateStoreRecord& record,
        const std::string& currentVersion,
        const StagedPatchIdentity& stagedIdentity,
        std::uint32_t maxApplyAttempts);
    static ApplyAttemptDecision BeginApply(const UpdateStoreRecord& record,
                                           std::uint32_t maxApplyAttempts);
};

}  // namespace aris::update
