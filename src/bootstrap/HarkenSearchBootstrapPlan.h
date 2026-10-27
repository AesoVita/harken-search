#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aris::platform {

enum class DeploymentMode { Installed, Portable };

struct DeploymentFileIdentity {
    std::string canonicalPath;
    std::uint64_t volumeSerial = 0;
    std::array<std::uint8_t, 16> fileId{};
    bool valid = false;

    friend bool operator==(const DeploymentFileIdentity& left,
                           const DeploymentFileIdentity& right) noexcept {
        return left.canonicalPath == right.canonicalPath &&
               left.volumeSerial == right.volumeSerial &&
               left.fileId == right.fileId && left.valid == right.valid;
    }
};

inline constexpr const char* kArisInstalledAppId =
    "{31B6E215-3D2B-43EA-88C8-72D73ABC9A0C}";

// Stable installation identity used by the signed bootstrap plan. Keep the
// canonical GUID in the shared contract so platform adapters cannot drift from
// the installer and admission rules.
inline constexpr const char* kArisInstalledInstallationGuid =
    "A8B3C9D1-E4F5-4A6B-8C9D-0E1F2A3B4C5D";

}  // namespace aris::platform

namespace aris::bootstrap {

// Raw values supplied by the signed plan parser. They intentionally contain no
// filesystem handles, process handles, or callbacks: platform adapters remain
// responsible for authentication, process queries, and durable file operations.
struct InstallIdentity {
    aris::platform::DeploymentMode deploymentMode =
        aris::platform::DeploymentMode::Portable;
    std::string targetRoot;
    std::string installationId;
    aris::platform::DeploymentFileIdentity targetRootIdentity;
    aris::platform::DeploymentFileIdentity oldExecutableIdentity;
    std::string registeredAppId;
    std::vector<std::string> protectedDataRoots;
};

struct InstallInventoryItem {
    std::string relativePath;
    std::string sha256;
    std::uint64_t byteSize = 0;
};

// A process identity is an owned value captured from a verified process
// handle.  A PID alone is never sufficient authority: creationTime and the
// executable identity remain bound through the process lifetime and journal
// recovery.  The layout deliberately has no native HANDLE dependency.
struct BootstrapProcessIdentity {
    std::uint64_t processId = 0;
    std::uint64_t creationTime = 0;
    std::string imagePath;
    std::uint64_t imageVolumeSerial = 0;
    std::array<std::uint8_t, 16> imageFileId{};
    bool valid = false;

    friend bool operator==(const BootstrapProcessIdentity& left,
                           const BootstrapProcessIdentity& right) noexcept {
        return left.valid == right.valid &&
               left.processId == right.processId &&
               left.creationTime == right.creationTime &&
               left.imagePath == right.imagePath &&
               left.imageVolumeSerial == right.imageVolumeSerial &&
               left.imageFileId == right.imageFileId;
    }
};

// These paths are part of the authenticated invocation envelope, not CLI
// write authority.  The filesystem port accepts only these normalized values
// from a ValidatedInstallPlan; a caller cannot redirect an elevated
// Bootstrap transaction by supplying a different same-volume directory.
struct BootstrapInvocationIdentity {
    aris::platform::DeploymentMode deploymentMode =
        aris::platform::DeploymentMode::Portable;
    std::string installationId;
    std::string targetRootIdentityBinding;
    std::string oldExecutableIdentityBinding;
    std::string updaterOwnedBase;
    std::string stagingRoot;
    std::string transactionRoot;
    std::string invocationIdentity;
    std::string transactionId;
    std::string packagePlanDigest;
    std::string healthNonceDigest;
    BootstrapProcessIdentity parentProcess;
    BootstrapProcessIdentity oldProcess;
};

struct SignedInstallPlan {
    std::uint32_t schemaVersion = 0;
    std::string product;
    std::string channel;
    std::string version;
    std::string buildId;
    std::string keyId;
    std::string detachedSignature;
    std::string payloadSha256;
    // Digest of the immutable, release-signed PackagePlan bytes.  Legacy
    // in-memory callers may leave this empty; the executable admission path
    // requires it to be a lowercase SHA-256 value.
    std::string packagePlanDigest;
    std::uint64_t oldProcessId = 0;
    std::string healthToken;
    InstallIdentity identity;
    BootstrapInvocationIdentity invocation;
    std::string entrypoint;
    std::vector<InstallInventoryItem> inventory;
    std::vector<std::string> removals;
};

// The crypto adapter may produce Verified only after it authenticates the raw
// payload bytes with a trusted release key. This core does not accept a plan
// whose signature result is anything else.
enum class SignatureStatus {
    Verified,
    Rejected,
};

enum class PlanError {
    None,
    SignatureRejected,
    InvalidSchema,
    InvalidProduct,
    InvalidChannel,
    InvalidVersion,
    InvalidBuildId,
    InvalidPackagePlanDigest,
    InvalidKeyId,
    InvalidSignatureEnvelope,
    InvalidPayloadDigest,
    InvalidProcessId,
    InvalidHealthToken,
    InvalidTargetRoot,
    DiskRootRejected,
    SystemDirectoryRejected,
    DataDirectoryRejected,
    InvalidInstallationId,
    InvalidInvocationIdentity,
    InvalidUpdaterOwnedBase,
    InvalidStagingRoot,
    InvalidTransactionRoot,
    InvocationRootOverlap,
    MissingProtectedDataRoots,
    InvalidProtectedDataRoot,
    EmptyInventory,
    InvalidRelativePath,
    DuplicateInventoryPath,
    InvalidInventoryDigest,
    InvalidRemovalPath,
    DuplicateRemovalPath,
    InventoryRemovalOverlap,
    InvalidEntrypoint,
    EntrypointNotInInventory,
};

struct InstallPlanValidation;

// This is only created by ValidateSignedInstallPlan. All output paths are
// canonicalized lexical values; no adapter may join an arbitrary raw path.
struct ValidatedInstallPlan {
    InstallIdentity identity;
    std::string version;
    std::string buildId;
    std::string packageKeyId;
    std::string packageChannel;
    std::string packagePlanDigest;
    // Immutable signed PackagePlan reference retained for restart recovery.
    // Empty values are permitted for legacy pure-core fixtures; the production
    // admission path fills both before any durable mutation.
    std::string packagePlanBytes;
    std::string packageSignatureHex;
    std::uint64_t oldProcessId = 0;
    std::uint32_t previousFailureCount = 0;
    std::string healthToken;
    BootstrapInvocationIdentity invocation;
    std::string entrypoint;
    std::vector<InstallInventoryItem> inventory;
    std::vector<std::string> removals;

    bool isValidated() const;

private:
    bool validated_ = false;
    std::string validationBinding_;

    friend InstallPlanValidation ValidateSignedInstallPlan(
        const SignedInstallPlan& plan,
        SignatureStatus signatureStatus);
};

struct InstallPlanValidation {
    PlanError error = PlanError::None;
    std::optional<ValidatedInstallPlan> plan;

    bool ok() const noexcept {
        return error == PlanError::None && plan.has_value() && plan->isValidated();
    }
};

InstallPlanValidation ValidateSignedInstallPlan(
    const SignedInstallPlan& plan,
    SignatureStatus signatureStatus);

struct ExistingInstallItem {
    std::string relativePath;
};

enum class JournalItemClass {
    ReplaceExisting,
    CreateNew,
    PreserveExisting,
    RemoveExisting,
};

struct ReplacementJournalItem {
    std::string relativePath;
    JournalItemClass classification = JournalItemClass::PreserveExisting;
    bool requiresBackup = false;
    bool backupRecorded = false;
    bool replacementRecorded = false;
};

enum class JournalError {
    None,
    InvalidPlan,
    InvalidTransactionId,
    InvalidExistingPath,
    DuplicateExistingPath,
    InvalidDurableJournal,
};

struct JournalBuildResult;

// The journal describes only inventory-relative names. An adapter turns these
// intents into disk operations only after it has validated the install root.
struct ReplacementJournal {
    std::string transactionId;
    std::vector<ReplacementJournalItem> items;

    bool isValidated() const;
    bool isValidatedFor(const ValidatedInstallPlan& plan) const;

private:
    bool validated_ = false;
    std::string planBinding_;
    std::string structureBinding_;

    friend JournalBuildResult BuildReplacementJournal(
        const ValidatedInstallPlan& plan,
        const std::vector<ExistingInstallItem>& existingInventory,
        const std::string& transactionId);
    friend JournalBuildResult RehydrateReplacementJournal(
        const ValidatedInstallPlan& plan,
        const std::vector<ReplacementJournalItem>& durableItems,
        const std::string& transactionId);
};

struct JournalBuildResult {
    JournalError error = JournalError::None;
    std::optional<ReplacementJournal> journal;

    bool ok() const noexcept {
        return error == JournalError::None && journal.has_value() && journal->isValidated();
    }
};

JournalBuildResult BuildReplacementJournal(
    const ValidatedInstallPlan& plan,
    const std::vector<ExistingInstallItem>& existingInventory,
    const std::string& transactionId);

// Reconstructs the pure journal value from a strictly decoded, plan-bound
// durable filesystem record.  This is the restart boundary: callers do not
// need to retain or recreate the original in-memory existing-inventory list.
JournalBuildResult RehydrateReplacementJournal(
    const ValidatedInstallPlan& plan,
    const std::vector<ReplacementJournalItem>& durableItems,
    const std::string& transactionId);

enum class BootstrapIntentKind {
    WaitForOldProcessExit,
    PersistJournal,
    SnapshotBackup,
    ReplaceInventoryItem,
    RemoveInstalledItem,
    LaunchUpdatedChild,
    AwaitHealthAcknowledgement,
    CommitJournal,
    RemoveBackup,
    TerminateUpdatedChild,
    RestoreBackup,
    RemoveNewItem,
    RelaunchPreviousApplication,
    QuarantinePackage,
    RecordFailure,
};

// Intent is an immutable-description-style value. It never carries an absolute
// target path, filesystem handle, PID handle, or a callable operation.
struct BootstrapIntent {
    BootstrapIntentKind kind = BootstrapIntentKind::RecordFailure;
    std::string relativePath;
    std::uint64_t processId = 0;
    std::string reason;
};

enum class TransactionPhase {
    WaitingForOldProcess,
    AwaitingUpdatedChildProcess,
    AwaitingHealth,
    Committed,
    RolledBack,
    RecoveryBlocked,
    FailedBeforeMutation,
};

struct FailureDisposition {
    std::uint32_t recordedFailureCount = 0;
    bool quarantine = false;
};

FailureDisposition NextFailureDisposition(std::uint32_t previousFailureCount);

struct TransactionState {
    ValidatedInstallPlan plan;
    ReplacementJournal journal;
    TransactionPhase phase = TransactionPhase::FailedBeforeMutation;
    std::uint32_t previousFailureCount = 0;
    std::uint32_t recordedFailureCount = 0;
    std::uint64_t updatedChildProcessId = 0;
    bool healthTokenConsumed = false;
};

struct TransactionDecision {
    TransactionState state;
    std::vector<BootstrapIntent> intents;
};

enum class UpdatedChildRecordResult {
    Recorded,
    Idempotent,
    InvalidState,
    InvalidProcessId,
};

struct UpdatedChildRecordDecision {
    UpdatedChildRecordResult result = UpdatedChildRecordResult::InvalidState;
    TransactionDecision transaction;
};

// The platform adapter records only the PID returned by its own launch call.
// No caller may nominate an unrelated process for termination during rollback.
UpdatedChildRecordDecision RecordUpdatedChildProcess(
    const TransactionState& state,
    std::uint64_t processId);

// These records are made only after the platform adapter durably performs the
// corresponding item operation. They make recovery from a crash between two
// inventory items conservative: rollback touches only items known to have been
// replaced, never merely planned items.
enum class JournalRecordResult {
    Recorded,
    Idempotent,
    InvalidState,
    InvalidPath,
    InvalidItem,
    BackupRequired,
};

struct JournalRecordDecision {
    JournalRecordResult result = JournalRecordResult::InvalidState;
    TransactionDecision transaction;
};

JournalRecordDecision RecordBackupSnapshot(
    const TransactionState& state,
    const std::string& relativePath);

JournalRecordDecision RecordReplacementWritten(
    const TransactionState& state,
    const std::string& relativePath);

enum class ProcessWaitOutcome {
    Exited,
    TimedOut,
};

TransactionDecision StartTransaction(
    const ValidatedInstallPlan& plan,
    const ReplacementJournal& journal,
    std::uint32_t previousFailureCount);

TransactionDecision OnOldProcessWait(
    const TransactionState& state,
    ProcessWaitOutcome outcome);

struct HealthAcknowledgement {
    std::string token;
    std::string version;
    std::string buildId;
    // Native health transports bind this to the exact child identity.  Pure
    // test callers may leave it zero; a production channel must provide it.
    std::uint64_t processId = 0;
    bool uiReady = false;
};

enum class HealthAcknowledgementResult {
    Accepted,
    ReplayRejected,
    TokenMismatch,
    VersionMismatch,
    BuildIdMismatch,
    ProcessMismatch,
    UiNotReady,
    IncompleteReplacement,
    NotAwaitingHealth,
};

struct HealthDecision {
    HealthAcknowledgementResult health = HealthAcknowledgementResult::NotAwaitingHealth;
    TransactionDecision transaction;
};

HealthDecision AcknowledgeHealth(
    const TransactionState& state,
    const HealthAcknowledgement& acknowledgement);

// Both terminal reducers are idempotent: a committed or restored state emits
// no second destructive intent when replayed after a crash/restart.
TransactionDecision CommitTransaction(const TransactionState& state);
TransactionDecision RollbackTransaction(const TransactionState& state);
TransactionDecision RollbackAfterHealthTimeout(const TransactionState& state);
TransactionDecision RollbackForUpdatedChildCrash(const TransactionState& state);

}  // namespace aris::bootstrap
