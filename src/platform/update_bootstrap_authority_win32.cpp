#include "platform/update_bootstrap_authority_port.h"
#include "platform/harken_user_data_layout.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Aclapi.h>

#include "nlohmann_json.hpp"

#include "bootstrap/HarkenSearchBootstrapAdmission.h"
#include "bootstrap/HarkenSearchBootstrapRuntime.h"
#include "platform/update_deployment_identity.h"
#include "update/UpdateCheckHttpsTransport.h"
#include "update/UpdateReleaseConfig.h"
#if defined(ARIS_RUNTIME_PAIR_PROVISIONED)
#include "HarkenSearchRuntimePairIdentity.h"
#endif

#include "platform/miniz_archive_reader.h"

#include <bcrypt.h>
#include <fileapi.h>
#include <processthreadsapi.h>
#include <shlobj.h>
#include <winternl.h>
#include <wintrust.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Platform::UpdateBootstrapAuthority {
namespace {

using namespace aris::update;
namespace bootstrap = aris::bootstrap;

class NoCheckCancel final : public UpdateCheckCancellation {
public:
    bool IsCancellationRequested() const override { return false; }
    class Registration final : public InterruptRegistration {};
    std::unique_ptr<InterruptRegistration> RegisterInterrupt(
        std::function<void()> handler) const override {
        static_cast<void>(handler);
        return std::make_unique<Registration>();
    }
};

bool IsSafeStagedRelativePath(const std::string& value) {
    if (value.empty() || value.front() == '/' || value.front() == '\\' ||
        value.find(':') != std::string::npos) return false;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find_first_of("/\\", begin);
        const std::string component = value.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        if (component.empty() || component == "." || component == "..") return false;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return true;
}

class Authority final : public UpdateBootstrapAuthorityPort {
public:
    UpdateBootstrapAuthorizationResult Authorize(
        const VerifiedUpdateArchive& archive) override {
        UpdateBootstrapAuthorizationResult result;
        const auto observed = aris::platform::ObserveDeploymentIdentity();
        const auto deployment = aris::platform::ResolveDeploymentIdentity(observed);
        if (!deployment.ok() || !deployment.identity.has_value()) {
            result.status = UpdateBootstrapAuthorizationStatus::TerminalFailure;
            result.detail = "deployment identity admission failed";
            return result;
        }
        // Resolve the updater base *after* the install root is known: it has to
        // land on the install volume.  While this was pinned to %LOCALAPPDATA%,
        // any install off the system drive failed at filesystem-begin with
        // RootVolumeMismatch (exit code 30) on every attempt, and reinstalling
        // to the same drive could not fix it.
        const std::string base = UpdaterOwnedBase(deployment.identity->targetRoot);
        const std::string invocationNamespace =
            "invocation-" + std::to_string(GetCurrentProcessId()) + "-" +
            RandomHex(32U);
        if (invocationNamespace.size() <= 12U) {
            result.status = UpdateBootstrapAuthorizationStatus::RetryableFailure;
            result.detail = "invocation namespace generation failed";
            return result;
        }
        const std::string download = base + "/staging";
        const std::string staging = base + "/staging-" + invocationNamespace;
        const std::string transaction =
            base + "/transaction-" + invocationNamespace;
        if (!IsSafeStagedRelativePath(archive.stagedRelativePath)) {
            result.status = UpdateBootstrapAuthorizationStatus::TerminalFailure;
            result.detail = "verified staged path is unsafe";
            return result;
        }
        auto pinnedDirectories = PrepareUpdaterDirectories(
            base, download, staging, transaction);
        if (base.empty() || !pinnedDirectories.has_value()) {
            result.status = UpdateBootstrapAuthorizationStatus::RetryableFailure;
            result.detail = "verified update archive is missing";
            return result;
        }
        const std::string planGitHubUrl =
            std::string(ARIS_UPDATE_GITHUB_RELEASE_DOWNLOAD_PREFIX) + "v" +
            archive.version + "/" +
            (archive.channel == UpdateChannel::Stable
                 ? "ArisBootstrap.package.stable.json"
                 : "ArisBootstrap.package.json");
        const std::string planUrl = ResolveUpdateDownloadUrl(planGitHubUrl);
        const std::string signatureUrl = planUrl + ".sig";
        auto transport = CreateUpdateCheckHttpsTransport();
        if (!transport.ok()) {
            result.status = UpdateBootstrapAuthorizationStatus::RetryableFailure;
            result.detail = "Bootstrap package transport unavailable";
            return result;
        }

        const NoCheckCancel cancel;
        auto package = transport.port->Fetch({planUrl, 15'000U, 512U * 1024U}, cancel);
        auto signature = transport.port->Fetch({signatureUrl, 15'000U, 16U * 1024U}, cancel);
        if (!package.ok() || !signature.ok()) {
            result.status = UpdateBootstrapAuthorizationStatus::RetryableFailure;
            result.detail = "Bootstrap package authority fetch failed";
            return result;
        }
        const std::string packageBytes(package.body.begin(), package.body.end());
        const std::string signatureBytes(signature.body.begin(), signature.body.end());
        const auto binding = bootstrap::VerifyPackagePlanBinding(
            packageBytes, signatureBytes,
            bootstrap::ProductionPackagePlanTrust());
        if (!binding.ok() || binding.binding->assetSha256() != archive.assetSha256 ||
            binding.binding->assetSize() != archive.assetSize ||
            binding.binding->version() != archive.version ||
            binding.binding->buildId() != archive.buildId ||
            binding.binding->channel() != ArchiveChannel(archive.channel)) {
            result.status = UpdateBootstrapAuthorizationStatus::BindingMismatch;
            result.detail = "Bootstrap package binding mismatch";
            return result;
        }
        const auto productPolicy =
            bootstrap::EvaluateHarkenSearchPackagePolicy(*binding.binding);
        if (!productPolicy.ok()) {
            result.status = UpdateBootstrapAuthorizationStatus::TerminalFailure;
            result.detail = productPolicy.diagnostic;
            return result;
        }
        auto pinnedArchive = OpenVerifiedArchive(
            pinnedDirectories->base.get(), archive.stagedRelativePath,
            binding.binding->assetSize(), binding.binding->assetSha256());
        if (!pinnedArchive.has_value()) {
            result.status = UpdateBootstrapAuthorizationStatus::RetryableFailure;
            result.detail = "verified update archive changed during admission";
            return result;
        }
        const auto protectedRoots = ResolveProtectedDataRoots(
            deployment.identity->targetRoot, base, staging, transaction);
        if (!protectedRoots.has_value()) {
            result.status = UpdateBootstrapAuthorizationStatus::TerminalFailure;
            result.detail = "protected data root admission failed";
            return result;
        }
        const std::string siblingPath =
            ExecutableDirectory() + "/HarkenSearchBootstrap.exe";
        const auto sibling = ObserveRuntimePairFile(siblingPath);
        if (!sibling.has_value()) {
            result.status = UpdateBootstrapAuthorizationStatus::TerminalFailure;
            result.detail = "Runtime Pair sibling admission failed";
            return result;
        }
        const auto copiedBootstrap = CopyVerifiedBootstrap(
            *sibling, pinnedDirectories->transaction.get());
        if (!copiedBootstrap.has_value()) {
            result.status = UpdateBootstrapAuthorizationStatus::TerminalFailure;
            result.detail = "Runtime Pair launch copy admission failed";
            return result;
        }
        auto runtimeCleanupControl =
            std::make_shared<RuntimePairCleanupControl>();
        TransactionRuntimePairCleanup runtimeCleanup(
            transaction, runtimeCleanupControl);
        const auto runtimeTrackStatus = runtimeCleanup.Track(*copiedBootstrap);
        if (runtimeTrackStatus !=
            TransactionRuntimePairCleanup::Status::Armed) {
            result.status = runtimeTrackStatus ==
                    TransactionRuntimePairCleanup::Status::TerminalMismatch
                ? UpdateBootstrapAuthorizationStatus::TerminalFailure
                : UpdateBootstrapAuthorizationStatus::RetryableFailure;
            result.detail = "Runtime Pair cleanup authority admission failed";
            return result;
        }
        auto invocationCleanupControl =
            std::make_shared<InvocationMutationCleanupControl>();
        InvocationMutationCleanup cleanup(staging, invocationCleanupControl);
        const auto finishAuthorizationCleanupFailure =
            [&](const UpdateBootstrapAuthorizationStatus fallbackStatus,
                const std::string& fallbackDetail) {
                const auto cleanupStatus = cleanup.CleanupNow();
                if (cleanupStatus ==
                    InvocationMutationCleanup::Status::RetryableFailure) {
                    result.status =
                        UpdateBootstrapAuthorizationStatus::RetryableFailure;
                    result.detail = "payload-cleanup-pending";
                } else {
                    result.status = fallbackStatus;
                    result.detail = fallbackDetail;
                }
            };
        std::vector<PayloadFile> payloadFiles;
        const bool extracted = ExtractPayload(pinnedArchive->handle.get(),
            pinnedArchive->size,
            pinnedDirectories->staging.get(), *binding.binding, &payloadFiles);
        const auto tracked = cleanup.TrackPayloadFiles(
            pinnedDirectories->staging.get(), payloadFiles);
        if (!extracted || tracked != InvocationMutationCleanup::Status::Armed) {
            finishAuthorizationCleanupFailure(
                UpdateBootstrapAuthorizationStatus::RetryableFailure,
                extracted ? "update payload mutation ownership failed"
                          : "update payload extraction failed");
            return result;
        }
        bootstrap::InvocationEnvelope invocation;
        invocation.transactionId = "aris-" + archive.version + "-" +
            invocationNamespace;
        invocation.installationId = deployment.identity->installationId;
        invocation.registeredAppId = deployment.identity->appId;
        invocation.targetRoot = deployment.identity->targetRoot;
        invocation.targetRootIdentity = deployment.identity->openedRootIdentity;
        invocation.oldExecutableIdentity = deployment.identity->oldExecutable;
        invocation.deploymentMode = deployment.identity->mode;
        invocation.targetRootIdentityBinding = deployment.identity->openedRootIdentity.canonicalPath;
        invocation.oldExecutableIdentityBinding = deployment.identity->oldExecutable.canonicalPath;
        invocation.updaterOwnedBase = base;
        invocation.stagingRoot = staging;
        invocation.transactionRoot = transaction;
        invocation.invocationIdentity = invocation.transactionId;
        invocation.packagePlanDigest = binding.binding->packagePlanDigest();
        invocation.healthNonce = RandomHex(64U);
        invocation.entrypoint = "HarkenSearch.exe";
        invocation.oldProcess = CurrentIdentity();
        invocation.parentProcess = invocation.oldProcess;
        invocation.oldProcessId = invocation.oldProcess.processId;
        const auto existingInventory = ExistingInventory(ExecutableDirectory());
        if (!existingInventory.has_value()) {
            finishAuthorizationCleanupFailure(
                UpdateBootstrapAuthorizationStatus::TerminalFailure,
                "installed payload inventory is ambiguous");
            return result;
        }
        invocation.existingInventory = *existingInventory;
        invocation.protectedDataRoots = *protectedRoots;
        invocation.previousFailureCount = 0U;
        auto state = std::make_shared<AuthorityState>();
        state->archive = archive;
        state->package = packageBytes;
        state->signature = signatureBytes;
        state->invocation = invocation;
        state->deployment = *deployment.identity;
        state->observedDeployment = observed;
        state->consumed = std::make_shared<std::atomic_bool>(false);
        state->submitControl = std::make_shared<SubmitControl>();
        state->runtimePairCleanup = runtimeCleanupControl;
        state->parent = invocation.parentProcess;
        state->siblingPath = siblingPath;
        state->siblingIdentity = *sibling;
        const std::string launchCopy = copiedBootstrap->canonicalPath;
        state->bootstrapPath = launchCopy;
        state->launchCopyIdentity = *copiedBootstrap;
        state->payloadFiles = payloadFiles;
        state->invocationMutationCleanup = invocationCleanupControl;
        *state = PrepareLaunch(*state);
        if (!state->ready) {
            finishAuthorizationCleanupFailure(
                UpdateBootstrapAuthorizationStatus::RetryableFailure,
                state->detail);
            return result;
        }
        cleanup.Release();
        runtimeCleanup.Release();
        result.status = UpdateBootstrapAuthorizationStatus::Authorized;
        result.capability = IssueCapability(state);
        return result;
    }

    UpdateBootstrapSubmitResult Submit(
        const VerifiedUpdateArchive& archive,
        const UpdateBootstrapLaunchCapability& capability) override {
        UpdateBootstrapSubmitResult result;
        const auto state = std::static_pointer_cast<const AuthorityState>(
            UpdateBootstrapAuthorityPort::Authority(capability));
        if (!state || state->archive.assetSha256 != archive.assetSha256 ||
            state->archive.assetSize != archive.assetSize ||
            state->archive.version != archive.version ||
            state->archive.buildId != archive.buildId ||
            state->archive.channel != archive.channel || !state->ready) {
            result.status = UpdateBootstrapSubmitStatus::TerminalFailure;
            result.detail = "Bootstrap capability mismatch";
            return result;
        }

        const SubmitLeaseResult lease = AcquireSubmitLease(state);
        if (lease == SubmitLeaseResult::AlreadyAccepted) {
            result.status = UpdateBootstrapSubmitStatus::AlreadyAccepted;
            result.detail = "Bootstrap capability already accepted";
            return result;
        }
        if (lease == SubmitLeaseResult::TerminalFailure) {
            result.status = UpdateBootstrapSubmitStatus::TerminalFailure;
            result.detail = SubmitDetail(state, "Bootstrap capability is terminal");
            return result;
        }

        TransactionRuntimePairCleanup cleanup(
            state->invocation.transactionRoot, state->runtimePairCleanup);
        auto& runtimeCleanup = cleanup;
        runtimeCleanup.Track(state->launchCopyIdentity);
        {
            InvocationMutationCleanup cleanup(
                state->invocation.stagingRoot,
                state->invocationMutationCleanup);
            const auto finishSubmitCleanupFailure =
                [&](const UpdateBootstrapSubmitStatus fallbackStatus,
                    const std::string& fallbackDetail) {
                const auto cleanupStatus = cleanup.CleanupNow();
                    const auto runtimeCleanupStatus =
                        RemoveRuntimePairCopy(*state);
                    runtimeCleanup.Release();
                    const bool invocationCleanupPending = cleanupStatus ==
                        InvocationMutationCleanup::Status::RetryableFailure;
                    const bool runtimeCleanupPending = runtimeCleanupStatus ==
                        TransactionRuntimePairCleanup::Status::RetryableFailure;
                    const bool runtimeCleanupMismatch = runtimeCleanupStatus ==
                        TransactionRuntimePairCleanup::Status::TerminalMismatch;
                    const bool pending = !runtimeCleanupMismatch &&
                        (invocationCleanupPending || runtimeCleanupPending ||
                        fallbackStatus ==
                            UpdateBootstrapSubmitStatus::RetryableFailure);
                    result.status = runtimeCleanupMismatch
                        ? UpdateBootstrapSubmitStatus::TerminalFailure
                        : (pending
                               ? UpdateBootstrapSubmitStatus::RetryableFailure
                               : fallbackStatus);
                    result.detail = runtimeCleanupMismatch
                        ? "runtime-pair-cleanup-mismatch"
                        : (invocationCleanupPending
                               ? "payload-cleanup-pending"
                               : (runtimeCleanupPending
                                      ? "runtime-pair-cleanup-pending"
                                      : fallbackDetail));
                    FinishSubmit(state,
                                 pending ? SubmitState::Ready
                                         : SubmitState::Terminal,
                                 result.detail);
                };
            auto staging = OpenPinnedAbsoluteDirectory(
                state->invocation.stagingRoot, false);
            const auto trackStatus = staging.has_value()
                ? cleanup.TrackPayloadFiles(
                      staging->get(), state->payloadFiles)
                : InvocationMutationCleanup::Status::RetryableFailure;
            if (trackStatus != InvocationMutationCleanup::Status::Armed) {
                finishSubmitCleanupFailure(
                    trackStatus == InvocationMutationCleanup::Status::RetryableFailure
                        ? UpdateBootstrapSubmitStatus::RetryableFailure
                        : UpdateBootstrapSubmitStatus::TerminalFailure,
                    trackStatus == InvocationMutationCleanup::Status::RetryableFailure
                        ? "payload-cleanup-pending"
                        : "staged payload mutation identity changed");
                return result;
            }
        const auto observed = aris::platform::ObserveDeploymentIdentity();
        const auto deployment = aris::platform::ResolveDeploymentIdentity(observed);
        if (!deployment.ok() || !deployment.identity.has_value() ||
            deployment.identity->mode != state->deployment.mode ||
            deployment.identity->installationId != state->deployment.installationId ||
            deployment.identity->targetRoot != state->deployment.targetRoot ||
            !(deployment.identity->openedRootIdentity == state->deployment.openedRootIdentity) ||
            !(deployment.identity->oldExecutable == state->deployment.oldExecutable)) {
            finishSubmitCleanupFailure(
                UpdateBootstrapSubmitStatus::TerminalFailure,
                "deployment identity changed after authorization");
            return result;
        }
        const auto protectedRoots = ResolveProtectedDataRoots(
            state->invocation.targetRoot,
            state->invocation.updaterOwnedBase,
            state->invocation.stagingRoot,
            state->invocation.transactionRoot);
        if (!protectedRoots.has_value() ||
            *protectedRoots != state->invocation.protectedDataRoots) {
            finishSubmitCleanupFailure(
                UpdateBootstrapSubmitStatus::TerminalFailure,
                "protected data roots changed after authorization");
            return result;
        }
        const auto sibling = ObserveRuntimePairFile(state->siblingPath);
        if (!sibling.has_value() ||
            !SameRuntimePairFile(*sibling, state->siblingIdentity)) {
            finishSubmitCleanupFailure(
                UpdateBootstrapSubmitStatus::TerminalFailure,
                "Runtime Pair changed after authorization");
            return result;
        }
        const auto launchCopy = ObserveRuntimePairFile(state->bootstrapPath);
        if (!launchCopy.has_value() ||
            !SameRuntimePairFile(*launchCopy, state->launchCopyIdentity)) {
            finishSubmitCleanupFailure(
                UpdateBootstrapSubmitStatus::TerminalFailure,
                "Runtime Pair launch copy changed after authorization");
            return result;
        }
        if (state->consumed != nullptr && state->consumed->exchange(true)) {
            cleanup.Release();
            runtimeCleanup.Release();
            FinishSubmit(state, SubmitState::Accepted,
                         "Bootstrap capability already accepted");
            result.status = UpdateBootstrapSubmitStatus::AlreadyAccepted;
            result.detail = "Bootstrap capability already consumed";
            return result;
        }
        BeginSubmitLaunch(state);
        if (!LaunchBootstrap(*state)) {
            if (state->consumed != nullptr) state->consumed->store(false);
            cleanup.Release();
            runtimeCleanup.Release();
            FinishSubmit(state, SubmitState::Ready,
                         "Bootstrap process launch failed");
            result.status = UpdateBootstrapSubmitStatus::RetryableFailure;
            result.detail = "Bootstrap process launch failed";
            return result;
        }
        cleanup.Release();
        runtimeCleanup.Release();
        FinishSubmit(state, SubmitState::Accepted, {});
        result.status = UpdateBootstrapSubmitStatus::Accepted;
        return result;
        }
    }

private:
    static std::string ArchiveChannel(const UpdateChannel channel) {
        return channel == UpdateChannel::Stable ? "stable" : "testing";
    }

    enum class SubmitState : std::uint8_t {
        Ready,
        Validating,
        Launching,
        Accepted,
        Terminal,
    };

    struct SubmitControl {
        mutable std::mutex mutex;
        mutable std::condition_variable condition;
        SubmitState state = SubmitState::Ready;
        std::string detail;
    };

    enum class SubmitLeaseResult : std::uint8_t {
        Owner,
        AlreadyAccepted,
        TerminalFailure,
    };

    struct RuntimePairDirectoryIdentity {
        std::string canonicalPath;
        std::uint64_t volumeSerial = 0U;
        std::array<std::uint8_t, 16U> fileId{};
    };

    struct RuntimePairCleanupControl;
    struct InvocationMutationCleanupControl;

    struct PayloadFile {
        std::string relativePath;
        std::string canonicalPath;
        std::uint64_t volumeSerial = 0U;
        std::array<std::uint8_t, 16U> fileId{};
        std::uint64_t size = 0U;
        std::string sha256;
    };

    struct PayloadDirectoryIdentity {
        std::string relativePath;
        std::string canonicalPath;
        std::uint64_t volumeSerial = 0U;
        std::array<std::uint8_t, 16U> fileId{};
    };

    struct AuthorityState {
        VerifiedUpdateArchive archive;
        std::string package;
        std::string signature;
        bootstrap::InvocationEnvelope invocation;
        bootstrap::BootstrapProcessIdentity parent;
        struct RuntimePairFile {
            std::string canonicalPath;
            std::uint64_t volumeSerial = 0U;
            std::array<std::uint8_t, 16U> fileId{};
            std::uint64_t size = 0U;
            std::string sha256;
        };
        std::string siblingPath;
        RuntimePairFile siblingIdentity;
        std::string bootstrapPath;
        RuntimePairFile launchCopyIdentity;
        std::vector<PayloadFile> payloadFiles;
        std::string handoff;
        bool ready = false;
        std::string detail;
        aris::platform::DeploymentIdentity deployment;
        aris::platform::DeploymentIdentitySnapshot observedDeployment;
        std::shared_ptr<std::atomic_bool> consumed;
        std::shared_ptr<SubmitControl> submitControl;
        std::shared_ptr<RuntimePairCleanupControl> runtimePairCleanup;
        std::shared_ptr<InvocationMutationCleanupControl> invocationMutationCleanup;
    };

    struct RuntimePairCleanupControl {
        mutable std::mutex mutex;
        RuntimePairDirectoryIdentity directoryAuthority;
        AuthorityState::RuntimePairFile fileAuthority;
        bool initialized = false;
        bool fileRemoved = false;
        bool cleaned = false;
        bool terminalMismatch = false;
    };

    struct InvocationMutationCleanupControl {
        mutable std::mutex mutex;
        std::string stagingRoot;
        PayloadDirectoryIdentity stagingAuthority;
        std::vector<PayloadFile> fileAuthority;
        std::vector<bool> fileRemoved;
        std::vector<PayloadDirectoryIdentity> directoryAuthorities;
        std::vector<bool> directoryRemoved;
        bool initialized = false;
        bool cleaned = false;
        bool terminalMismatch = false;
    };

    static SubmitLeaseResult AcquireSubmitLease(
        const std::shared_ptr<const AuthorityState>& state) {
        if (!state || !state->submitControl) {
            return SubmitLeaseResult::TerminalFailure;
        }
        std::unique_lock<std::mutex> lock(state->submitControl->mutex);
        for (;;) {
            if (state->submitControl->state == SubmitState::Accepted) {
                return SubmitLeaseResult::AlreadyAccepted;
            }
            if (state->submitControl->state == SubmitState::Terminal) {
                return SubmitLeaseResult::TerminalFailure;
            }
            if (state->submitControl->state == SubmitState::Validating ||
                state->submitControl->state == SubmitState::Launching) {
                state->submitControl->condition.wait(lock);
                continue;
            }
            state->submitControl->state = SubmitState::Validating;
            state->submitControl->detail.clear();
            return SubmitLeaseResult::Owner;
        }
    }

    static void BeginSubmitLaunch(
        const std::shared_ptr<const AuthorityState>& state) {
        if (!state || !state->submitControl) return;
        {
            std::lock_guard<std::mutex> lock(state->submitControl->mutex);
            state->submitControl->state = SubmitState::Launching;
        }
    }

    static void FinishSubmit(
        const std::shared_ptr<const AuthorityState>& state,
        const SubmitState next,
        const std::string_view detail) {
        if (!state || !state->submitControl) return;
        {
            std::lock_guard<std::mutex> lock(state->submitControl->mutex);
            state->submitControl->state = next;
            state->submitControl->detail.assign(detail.data(), detail.size());
        }
        state->submitControl->condition.notify_all();
    }

    static std::string SubmitDetail(
        const std::shared_ptr<const AuthorityState>& state,
        const std::string_view fallback) {
        if (!state || !state->submitControl) return std::string(fallback);
        std::lock_guard<std::mutex> lock(state->submitControl->mutex);
        return state->submitControl->detail.empty()
            ? std::string(fallback)
            : state->submitControl->detail;
    }

    using RuntimePairFile = AuthorityState::RuntimePairFile;

    class TransactionRuntimePairCleanup final {
    public:
        enum class Status : std::uint8_t {
            NotArmed,
            Armed,
            Cleaned,
            RetryableFailure,
            TerminalMismatch,
        };

        TransactionRuntimePairCleanup(
            const std::string& transactionRoot,
            std::shared_ptr<RuntimePairCleanupControl> control)
            : transactionRoot_(transactionRoot),
              directory_(std::filesystem::path(transactionRoot) / "runtime-pair"),
              control_(std::move(control)) {}

        Status Track(const RuntimePairFile& identity) {
            const auto candidate = std::filesystem::path(identity.canonicalPath);
            const auto expected = directory_ / "HarkenSearchBootstrap.exe";
            if (Normalize(candidate.parent_path()) != Normalize(directory_) ||
                candidate.filename() != expected.filename()) {
                if (control_ != nullptr) {
                    std::lock_guard<std::mutex> lock(control_->mutex);
                    control_->terminalMismatch = true;
                }
                return Status::TerminalMismatch;
            }
            if (control_ == nullptr) return Status::RetryableFailure;

            std::lock_guard<std::mutex> lock(control_->mutex);
            if (control_->terminalMismatch) return Status::TerminalMismatch;
            if (control_->cleaned) return Status::Cleaned;
            if (control_->initialized) {
                if (!SameRuntimePairFile(control_->fileAuthority, identity)) {
                    control_->terminalMismatch = true;
                    return Status::TerminalMismatch;
                }
                armed_ = true;
                return Status::Armed;
            }

            // Bind the cleanup lease to the exact directory and file that were
            // observed by the authority.  A same-name replacement must leave
            // the cleanup path inert instead of becoming a delete target.
            auto transaction = OpenPinnedAbsoluteDirectory(transactionRoot_, false);
            if (!transaction.has_value()) return Status::RetryableFailure;
            auto runtimePair = OpenExistingDirectoryChild(
                transaction->get(), L"runtime-pair");
            if (!runtimePair.has_value()) return Status::RetryableFailure;
            const auto directoryIdentity =
                ObserveRuntimePairDirectoryHandle(runtimePair->get());
            if (!directoryIdentity.has_value()) return Status::RetryableFailure;
            auto file = OpenExistingCleanupFile(
                runtimePair->get(), L"HarkenSearchBootstrap.exe");
            if (!file.has_value()) return Status::RetryableFailure;
            const auto observed = ObserveRuntimePairHandle(file->get());
            if (!observed.has_value() || !SameRuntimePairFile(*observed, identity)) {
                control_->terminalMismatch = true;
                return Status::TerminalMismatch;
            }
            control_->fileAuthority = identity;
            control_->directoryAuthority = *directoryIdentity;
            control_->initialized = true;
            control_->fileRemoved = false;
            control_->cleaned = false;
            armed_ = true;
            return Status::Armed;
        }

        void Release() noexcept {
            armed_ = false;
        }

        Status CleanupNow() noexcept {
            if (!armed_ || control_ == nullptr) return Status::NotArmed;

            std::lock_guard<std::mutex> lock(control_->mutex);
            const auto terminalMismatch = [&]() noexcept {
                control_->terminalMismatch = true;
                armed_ = false;
                return Status::TerminalMismatch;
            };
            if (control_->terminalMismatch) return Status::TerminalMismatch;
            if (control_->cleaned) {
                armed_ = false;
                return Status::Cleaned;
            }
            if (!control_->initialized) return Status::RetryableFailure;

            auto transaction = OpenPinnedAbsoluteDirectory(transactionRoot_, false);
            if (!transaction.has_value()) return Status::RetryableFailure;
            auto openedDirectory = OpenRelativeNative(
                transaction->get(), L"runtime-pair",
                DELETE | FILE_LIST_DIRECTORY | FILE_TRAVERSE |
                    FILE_READ_ATTRIBUTES | READ_CONTROL,
                kFileOpen, kFileDirectoryFile, FILE_ATTRIBUTE_DIRECTORY);
            if (!openedDirectory.ok()) {
                if (IsMissingNativeStatus(openedDirectory.status)) {
                    if (!control_->fileRemoved) return terminalMismatch();
                    control_->cleaned = true;
                    armed_ = false;
                    return Status::Cleaned;
                }
                return Status::RetryableFailure;
            }
            if (!IsNonReparseDirectoryHandle(openedDirectory.handle.get())) {
                return terminalMismatch();
            }
            auto runtimePair = std::move(openedDirectory.handle);
            const auto directoryIdentity =
                ObserveRuntimePairDirectoryHandle(runtimePair.get());
            if (!directoryIdentity.has_value()) {
                return Status::RetryableFailure;
            }
            if (!SameRuntimePairDirectory(*directoryIdentity,
                                          control_->directoryAuthority)) {
                return terminalMismatch();
            }

            auto openedFile = OpenRelativeNative(
                runtimePair.get(), L"HarkenSearchBootstrap.exe",
                GENERIC_READ | DELETE | FILE_READ_ATTRIBUTES,
                kFileOpen, kFileNonDirectoryFile, FILE_ATTRIBUTE_NORMAL);
            if (!openedFile.ok()) {
                if (!IsMissingNativeStatus(openedFile.status)) {
                    return Status::RetryableFailure;
                }
                if (!control_->fileRemoved) return terminalMismatch();
            } else {
                if (!IsNonReparseRegularFileHandle(openedFile.handle.get()) ||
                    control_->fileRemoved) {
                    return terminalMismatch();
                }
                const auto observed =
                    ObserveRuntimePairHandle(openedFile.handle.get());
                if (!observed.has_value() ||
                    !SameRuntimePairFile(*observed, control_->fileAuthority)) {
                    return terminalMismatch();
                }
                if (!DeleteFileByHandle(openedFile.handle.get())) {
                    return Status::RetryableFailure;
                }
                openedFile.handle.reset();
                control_->fileRemoved = true;
            }
            // Deleting the directory by its still-open handle avoids a second
            // path resolution and leaves any unknown child untouched.
            if (!DeleteFileByHandle(runtimePair.get())) {
                return Status::RetryableFailure;
            }
            control_->cleaned = true;
            armed_ = false;
            return Status::Cleaned;
        }

        ~TransactionRuntimePairCleanup() {
            static_cast<void>(CleanupNow());
        }

    private:
        static std::string Normalize(const std::filesystem::path& path) {
            std::string value = path.lexically_normal().generic_u8string();
            std::transform(value.begin(), value.end(), value.begin(),
                           [](const unsigned char character) {
                               return static_cast<char>(std::tolower(character));
                           });
            return value;
        }

        std::string transactionRoot_;
        std::filesystem::path directory_;
        std::shared_ptr<RuntimePairCleanupControl> control_;
        bool armed_ = false;
    };

    class InvocationMutationCleanup final {
    public:
        enum class Status : std::uint8_t {
            NotArmed,
            Armed,
            Cleaned,
            RetryableFailure,
            TerminalMismatch,
        };

        InvocationMutationCleanup(
            const std::string& stagingRoot,
            std::shared_ptr<InvocationMutationCleanupControl> control)
            : stagingRoot_(stagingRoot), control_(std::move(control)) {}

        Status TrackPayloadFiles(
            HANDLE stagingRoot, const std::vector<PayloadFile>& candidates) {
            armed_ = false;
            if (stagingRoot == nullptr || stagingRoot == INVALID_HANDLE_VALUE ||
                control_ == nullptr) {
                return Status::RetryableFailure;
            }

            std::lock_guard<std::mutex> lock(control_->mutex);
            if (control_->terminalMismatch) return Status::TerminalMismatch;
            if (control_->cleaned) return Status::Cleaned;

            const auto stagingIdentity = ObservePayloadDirectoryHandle(
                stagingRoot, {});
            if (!stagingIdentity.has_value()) return Status::RetryableFailure;

            const auto terminalMismatch = [&]() noexcept {
                control_->terminalMismatch = true;
                armed_ = false;
                return Status::TerminalMismatch;
            };

            if (!control_->initialized) {
                if (candidates.empty()) return Status::RetryableFailure;
                std::vector<PayloadDirectoryIdentity> directories;
                std::vector<std::string> seenFiles;
                for (const auto& candidate : candidates) {
                    if (!SafeRelativeComponents(candidate.relativePath).has_value()) {
                        return terminalMismatch();
                    }
                    const std::string normalizedFile = NormalizeProtectedRoot(
                        candidate.relativePath);
                    if (std::find(seenFiles.begin(), seenFiles.end(),
                                  normalizedFile) != seenFiles.end()) {
                        return terminalMismatch();
                    }
                    seenFiles.push_back(normalizedFile);
                    for (const auto& path : ParentRelativePaths(candidate.relativePath)) {
                        if (std::none_of(
                                directories.begin(), directories.end(),
                                [&](const PayloadDirectoryIdentity& previous) {
                                    return NormalizeProtectedRoot(previous.relativePath) ==
                                           NormalizeProtectedRoot(path);
                                })) {
                            const auto components = SafeRelativeComponents(path);
                            if (!components.has_value()) return terminalMismatch();
                            auto directory = OpenExistingDirectoryPath(
                                stagingRoot, *components, components->size());
                            if (!directory.has_value()) {
                                return Status::RetryableFailure;
                            }
                            const auto identity = ObservePayloadDirectoryHandle(
                                directory->get(), path);
                            if (!identity.has_value()) return Status::RetryableFailure;
                            directories.push_back(*identity);
                        }
                    }
                }
                std::sort(
                    directories.begin(), directories.end(),
                    [](const PayloadDirectoryIdentity& left,
                       const PayloadDirectoryIdentity& right) {
                        const auto depth = [](const std::string& value) {
                            return static_cast<std::size_t>(
                                std::count(value.begin(), value.end(), '/'));
                        };
                        const auto leftDepth = depth(left.relativePath);
                        const auto rightDepth = depth(right.relativePath);
                        return leftDepth != rightDepth
                            ? leftDepth > rightDepth
                            : left.relativePath < right.relativePath;
                    });
                control_->stagingRoot = stagingRoot_;
                control_->stagingAuthority = *stagingIdentity;
                control_->fileAuthority = candidates;
                control_->fileRemoved.assign(candidates.size(), false);
                control_->directoryAuthorities = std::move(directories);
                control_->directoryRemoved.assign(
                    control_->directoryAuthorities.size(), false);
                control_->initialized = true;
            } else {
                if (!SamePayloadDirectory(
                        *stagingIdentity, control_->stagingAuthority) ||
                    control_->fileAuthority.size() != candidates.size()) {
                    return terminalMismatch();
                }
                for (std::size_t index = 0U; index < candidates.size(); ++index) {
                    if (!SamePayloadFile(
                            candidates[index], control_->fileAuthority[index])) {
                        return terminalMismatch();
                    }
                }
                for (std::size_t index = 0U;
                     index < control_->fileAuthority.size(); ++index) {
                    if (control_->fileRemoved[index]) continue;
                    const auto components = SafeRelativeComponents(
                        control_->fileAuthority[index].relativePath);
                    if (!components.has_value()) return terminalMismatch();
                    const std::size_t parentCount = components->size() - 1U;
                    ScopedNativeHandle parent;
                    if (parentCount != 0U) {
                        auto openedParent = OpenExistingDirectoryPath(
                            stagingRoot, *components, parentCount);
                        if (!openedParent.has_value()) {
                            return Status::RetryableFailure;
                        }
                        parent = std::move(*openedParent);
                    }
                    HANDLE parentHandle = parentCount == 0U
                        ? stagingRoot
                        : parent.get();
                    auto file = OpenExistingPayloadFile(
                        parentHandle, components->back());
                    if (!file.has_value()) return Status::RetryableFailure;
                    const auto observed = ObservePayloadFileHandle(
                        file->get(), control_->fileAuthority[index].relativePath);
                    if (!observed.has_value() ||
                        !SamePayloadFile(
                            *observed, control_->fileAuthority[index])) {
                        return terminalMismatch();
                    }
                }
                for (std::size_t index = 0U;
                     index < control_->directoryAuthorities.size(); ++index) {
                    if (control_->directoryRemoved[index]) continue;
                    const auto components = SafeRelativeComponents(
                        control_->directoryAuthorities[index].relativePath);
                    if (!components.has_value()) return terminalMismatch();
                    auto directory = OpenExistingDirectoryPath(
                        stagingRoot, *components, components->size());
                    if (!directory.has_value()) return Status::RetryableFailure;
                    const auto observed = ObservePayloadDirectoryHandle(
                        directory->get(),
                        control_->directoryAuthorities[index].relativePath);
                    if (!observed.has_value() ||
                        !SamePayloadDirectory(
                            *observed, control_->directoryAuthorities[index])) {
                        return terminalMismatch();
                    }
                }
            }

            armed_ = HasPendingLocked();
            if (!armed_) {
                control_->cleaned = true;
                return Status::Cleaned;
            }
            return Status::Armed;
        }

        void Release() noexcept { armed_ = false; }

        Status CleanupNow() noexcept {
            if (!armed_ || control_ == nullptr) return Status::NotArmed;

            std::lock_guard<std::mutex> lock(control_->mutex);
            if (control_->terminalMismatch) return Status::TerminalMismatch;
            if (control_->cleaned) {
                armed_ = false;
                return Status::Cleaned;
            }
            if (!control_->initialized) return Status::RetryableFailure;

            const auto terminalMismatch = [&]() noexcept {
                control_->terminalMismatch = true;
                armed_ = false;
                return Status::TerminalMismatch;
            };
            auto staging = OpenPinnedAbsoluteDirectory(stagingRoot_, false);
            if (!staging.has_value()) return Status::RetryableFailure;
            const auto stagingIdentity = ObservePayloadDirectoryHandle(
                staging->get(), {});
            if (!stagingIdentity.has_value()) return Status::RetryableFailure;
            if (!SamePayloadDirectory(
                    *stagingIdentity, control_->stagingAuthority)) {
                return terminalMismatch();
            }

            for (std::size_t index = control_->fileAuthority.size(); index > 0U;
                 --index) {
                const std::size_t fileIndex = index - 1U;
                if (control_->fileRemoved[fileIndex]) continue;
                const auto& fileIdentity = control_->fileAuthority[fileIndex];
                const auto components = SafeRelativeComponents(
                    fileIdentity.relativePath);
                if (!components.has_value()) return terminalMismatch();
                const std::size_t parentCount = components->size() - 1U;
                ScopedNativeHandle parent;
                if (parentCount != 0U) {
                    auto openedParent = OpenExistingDirectoryPath(
                        staging->get(), *components, parentCount);
                    if (!openedParent.has_value()) {
                        return Status::RetryableFailure;
                    }
                    parent = std::move(*openedParent);
                }
                HANDLE parentHandle = parentCount == 0U
                    ? staging->get()
                    : parent.get();
                auto file = OpenExistingPayloadFile(
                    parentHandle, components->back());
                if (!file.has_value()) return Status::RetryableFailure;
                const auto observed = ObservePayloadFileHandle(
                    file->get(), fileIdentity.relativePath);
                if (!observed.has_value() ||
                    !SamePayloadFile(*observed, fileIdentity)) {
                    return terminalMismatch();
                }
                if (!DeleteFileByHandle(file->get())) {
                    return Status::RetryableFailure;
                }
                file.reset();
                parent.reset();
                control_->fileRemoved[fileIndex] = true;
            }

            for (std::size_t index = 0U;
                 index < control_->directoryAuthorities.size(); ++index) {
                if (control_->directoryRemoved[index]) continue;
                const auto& directoryIdentity =
                    control_->directoryAuthorities[index];
                const auto components = SafeRelativeComponents(
                    directoryIdentity.relativePath);
                if (!components.has_value()) return terminalMismatch();
                auto directory = OpenExistingDirectoryPath(
                    staging->get(), *components, components->size());
                if (!directory.has_value()) return Status::RetryableFailure;
                const auto observed = ObservePayloadDirectoryHandle(
                    directory->get(), directoryIdentity.relativePath);
                if (!observed.has_value() ||
                    !SamePayloadDirectory(*observed, directoryIdentity)) {
                    return terminalMismatch();
                }
                if (!DeleteFileByHandle(directory->get())) {
                    return Status::RetryableFailure;
                }
                directory.reset();
                control_->directoryRemoved[index] = true;
            }

            control_->cleaned = true;
            armed_ = false;
            return Status::Cleaned;
        }

        ~InvocationMutationCleanup() {
            static_cast<void>(CleanupNow());
        }

    private:
        static std::vector<std::string> ParentRelativePaths(
            const std::string& relativePath) {
            std::string normalized = relativePath;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            std::vector<std::string> result;
            std::size_t slash = normalized.find('/');
            while (slash != std::string::npos) {
                result.push_back(normalized.substr(0U, slash));
                slash = normalized.find('/', slash + 1U);
            }
            return result;
        }

        bool HasPendingLocked() const noexcept {
            if (control_ == nullptr) return false;
            return std::any_of(
                       control_->fileRemoved.begin(),
                       control_->fileRemoved.end(),
                       [](const bool removed) { return !removed; }) ||
                std::any_of(
                    control_->directoryRemoved.begin(),
                    control_->directoryRemoved.end(),
                    [](const bool removed) { return !removed; });
        }

        std::string stagingRoot_;
        std::shared_ptr<InvocationMutationCleanupControl> control_;
        bool armed_ = false;
    };

    static std::string ExecutableDirectory() {
        std::wstring buffer(32768U, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
        if (length == 0U || length >= buffer.size()) return {};
        buffer.resize(length);
        const auto slash = buffer.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return {};
        buffer.resize(slash);
        return std::filesystem::path(buffer).u8string();
    }

    static std::optional<std::string> KnownFolderChild(
        const KNOWNFOLDERID& folder,
        const std::filesystem::path& child) {
        PWSTR path = nullptr;
        if (SHGetKnownFolderPath(folder, 0, nullptr, &path) != S_OK ||
            path == nullptr) return {};
        std::filesystem::path value(path);
        CoTaskMemFree(path);
        value /= child;
        if (!value.is_absolute()) return {};
        return value.lexically_normal().u8string();
    }

    static std::string NormalizeProtectedRoot(const std::string_view value) {
        std::string normalized;
        normalized.reserve(value.size());
        for (char character : value) {
            if (character == '\\') character = '/';
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
            normalized.push_back(character);
        }
        while (normalized.size() > 3U && normalized.back() == '/') {
            normalized.pop_back();
        }
        return normalized;
    }

    static bool ProtectedRootsOverlap(const std::string& left,
                                      const std::string& right) {
        const std::string normalizedLeft = NormalizeProtectedRoot(left);
        const std::string normalizedRight = NormalizeProtectedRoot(right);
        const auto containsRoot = [](const std::string& root,
                                     const std::string& candidate) {
            return candidate == root ||
                (candidate.size() > root.size() &&
                 candidate.compare(0U, root.size(), root) == 0 &&
                 candidate[root.size()] == '/');
        };
        return containsRoot(normalizedLeft, normalizedRight) ||
               containsRoot(normalizedRight, normalizedLeft);
    }

    static std::optional<std::vector<std::string>> ResolveProtectedDataRoots(
        const std::string& targetRoot, const std::string& updaterOwnedBase,
        const std::string& stagingRoot, const std::string& transactionRoot) {
        const std::vector<std::string> resolved =
            Platform::HarkenUserData::ProtectedDataRootsUtf8();
        if (resolved.size() < 2U) {
            return std::nullopt;
        }
        for (size_t i = 0; i < resolved.size(); ++i) {
            if (resolved[i].empty() ||
                !std::filesystem::path(resolved[i]).is_absolute()) {
                return std::nullopt;
            }
            for (size_t j = i + 1; j < resolved.size(); ++j) {
                if (ProtectedRootsOverlap(resolved[i], resolved[j])) {
                    return std::nullopt;
                }
            }
        }
        const std::array<std::string, 4U> mutationRoots{
            targetRoot, updaterOwnedBase, stagingRoot, transactionRoot};
        for (const std::string& protectedRoot : resolved) {
            for (const std::string& mutationRoot : mutationRoots) {
                if (mutationRoot.empty() ||
                    ProtectedRootsOverlap(protectedRoot, mutationRoot)) {
                    return std::nullopt;
                }
            }
        }
        return resolved;
    }

#ifndef OBJ_DONT_REPARSE
#define OBJ_DONT_REPARSE 0x00001000L
#endif

    using NtCreateFileFunction = NTSTATUS(NTAPI*)(
        PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
        PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
    using NtSetInformationFileFunction = NTSTATUS(NTAPI*)(
        HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

    struct NativeFunctions {
        NtCreateFileFunction createFile = nullptr;
        NtSetInformationFileFunction setInformationFile = nullptr;
    };

    static const NativeFunctions& NativeApi() {
        static const NativeFunctions functions = [] {
            NativeFunctions result;
            const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            if (ntdll == nullptr) return result;
            result.createFile = reinterpret_cast<NtCreateFileFunction>(
                GetProcAddress(ntdll, "NtCreateFile"));
            result.setInformationFile =
                reinterpret_cast<NtSetInformationFileFunction>(
                    GetProcAddress(ntdll, "NtSetInformationFile"));
            return result;
        }();
        return functions;
    }

    static constexpr ULONG kFileDirectoryFile = 0x00000001UL;
    static constexpr ULONG kFileSynchronousIoNonAlert = 0x00000020UL;
    static constexpr ULONG kFileNonDirectoryFile = 0x00000040UL;
    static constexpr ULONG kFileOpenReparsePoint = 0x00200000UL;
    static constexpr ULONG kFileOpen = 1UL;
    static constexpr ULONG kFileCreate = 2UL;
    static constexpr ULONG kFileOpenIf = 3UL;
    static constexpr ULONG kFileOverwriteIf = 5UL;
    static constexpr FILE_INFORMATION_CLASS kFileRenameInformation =
        static_cast<FILE_INFORMATION_CLASS>(10);

    class ScopedNativeHandle final {
    public:
        ScopedNativeHandle() = default;
        explicit ScopedNativeHandle(HANDLE handle) noexcept : handle_(handle) {}
        ~ScopedNativeHandle() { reset(); }

        ScopedNativeHandle(const ScopedNativeHandle&) = delete;
        ScopedNativeHandle& operator=(const ScopedNativeHandle&) = delete;

        ScopedNativeHandle(ScopedNativeHandle&& other) noexcept
            : handle_(other.release()) {}
        ScopedNativeHandle& operator=(ScopedNativeHandle&& other) noexcept {
            if (this != &other) reset(other.release());
            return *this;
        }

        bool valid() const noexcept {
            return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
        }
        HANDLE get() const noexcept { return handle_; }
        HANDLE release() noexcept {
            HANDLE result = handle_;
            handle_ = INVALID_HANDLE_VALUE;
            return result;
        }
        void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept {
            if (valid()) CloseHandle(handle_);
            handle_ = handle;
        }

    private:
        HANDLE handle_ = INVALID_HANDLE_VALUE;
    };

    struct NativeOpenResult {
        ScopedNativeHandle handle;
        ULONG_PTR information = 0U;
        NTSTATUS status = static_cast<NTSTATUS>(0xC0000001L);

        bool ok() const noexcept {
            return status >= 0 && handle.valid();
        }
    };

    static bool IsMissingNativeStatus(const NTSTATUS status) noexcept {
        return status == static_cast<NTSTATUS>(0xC000000FUL) ||
               status == static_cast<NTSTATUS>(0xC0000034UL) ||
               status == static_cast<NTSTATUS>(0xC000003AUL);
    }

    static bool IsNonReparseDirectoryHandle(HANDLE handle) {
        FILE_ATTRIBUTE_TAG_INFO info{};
        return handle != nullptr && handle != INVALID_HANDLE_VALUE &&
               GetFileInformationByHandleEx(
                   handle, FileAttributeTagInfo, &info, sizeof(info)) != FALSE &&
               (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U &&
               (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
    }

    static bool IsNonReparseRegularFileHandle(HANDLE handle) {
        FILE_ATTRIBUTE_TAG_INFO info{};
        return handle != nullptr && handle != INVALID_HANDLE_VALUE &&
               GetFileInformationByHandleEx(
                   handle, FileAttributeTagInfo, &info, sizeof(info)) != FALSE &&
               (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U &&
               (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
    }

    static NativeOpenResult OpenRelativeNative(
        HANDLE parent, const std::wstring& name, ACCESS_MASK access,
        ULONG disposition, ULONG options, ULONG attributes) {
        NativeOpenResult result;
        const auto& native = NativeApi();
        if (native.createFile == nullptr || parent == nullptr ||
            parent == INVALID_HANDLE_VALUE || name.empty() ||
            name.find_first_of(L"\\/") != std::wstring::npos ||
            name == L"." || name == L".." ||
            name.size() >
                static_cast<std::size_t>((std::numeric_limits<USHORT>::max)()) /
                    sizeof(wchar_t)) {
            return result;
        }
        UNICODE_STRING unicodeName{};
        unicodeName.Buffer = const_cast<PWSTR>(name.data());
        unicodeName.Length = static_cast<USHORT>(name.size() * sizeof(wchar_t));
        unicodeName.MaximumLength = unicodeName.Length;
        OBJECT_ATTRIBUTES object{};
        object.Length = sizeof(object);
        object.RootDirectory = parent;
        object.ObjectName = &unicodeName;
        object.Attributes = OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE;
        IO_STATUS_BLOCK io{};
        HANDLE raw = INVALID_HANDLE_VALUE;
        result.status = native.createFile(
            &raw, access | SYNCHRONIZE, &object, &io, nullptr, attributes,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            disposition,
            options | kFileSynchronousIoNonAlert | kFileOpenReparsePoint,
            nullptr, 0U);
        if (result.status >= 0) {
            result.handle.reset(raw);
            result.information = io.Information;
        }
        return result;
    }

    static bool SecureUpdaterDirectoryImpl(HANDLE directory) {
        if (!IsNonReparseDirectoryHandle(directory)) return false;
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            return false;
        }
        DWORD required = 0U;
        static_cast<void>(GetTokenInformation(
            token, TokenUser, nullptr, 0U, &required));
        if (required == 0U || required > 64U * 1024U) {
            CloseHandle(token);
            return false;
        }
        std::vector<std::uint8_t> userStorage(required);
        const bool queried = GetTokenInformation(
            token, TokenUser, userStorage.data(), required,
            &required) != FALSE;
        CloseHandle(token);
        if (!queried) return false;
        const auto* user = reinterpret_cast<const TOKEN_USER*>(
            userStorage.data());
        std::array<std::uint8_t, SECURITY_MAX_SID_SIZE> systemStorage{};
        std::array<std::uint8_t, SECURITY_MAX_SID_SIZE> adminStorage{};
        DWORD systemSize = static_cast<DWORD>(systemStorage.size());
        DWORD adminSize = static_cast<DWORD>(adminStorage.size());
        if (!CreateWellKnownSid(WinLocalSystemSid, nullptr,
                                systemStorage.data(), &systemSize) ||
            !CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr,
                                adminStorage.data(), &adminSize)) {
            return false;
        }
        std::array<EXPLICIT_ACCESSW, 3U> entries{};
        const std::array<PSID, 3U> sids{
            user->User.Sid, systemStorage.data(), adminStorage.data()};
        for (std::size_t index = 0U; index < entries.size(); ++index) {
            entries[index].grfAccessPermissions = GENERIC_ALL;
            entries[index].grfAccessMode = GRANT_ACCESS;
            entries[index].grfInheritance =
                SUB_CONTAINERS_AND_OBJECTS_INHERIT;
            entries[index].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            entries[index].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
            entries[index].Trustee.ptstrName =
                reinterpret_cast<LPWSTR>(sids[index]);
        }
        PACL dacl = nullptr;
        const DWORD built = SetEntriesInAclW(
            static_cast<ULONG>(entries.size()), entries.data(),
            nullptr, &dacl);
        if (built != ERROR_SUCCESS || dacl == nullptr) return false;
        const DWORD applied = SetSecurityInfo(
            directory, SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
                PROTECTED_DACL_SECURITY_INFORMATION,
            user->User.Sid, nullptr, dacl, nullptr);
        LocalFree(dacl);
        return applied == ERROR_SUCCESS;
    }

    static std::optional<ScopedNativeHandle> OpenExistingDirectoryChild(
        HANDLE parent, const std::wstring& name) {
        const ACCESS_MASK access =
            FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES |
            READ_CONTROL;
        NativeOpenResult opened = OpenRelativeNative(
            parent, name, access, kFileOpen, kFileDirectoryFile,
            FILE_ATTRIBUTE_DIRECTORY);
        if (!opened.ok() || !IsNonReparseDirectoryHandle(opened.handle.get())) {
            return std::nullopt;
        }
        return std::move(opened.handle);
    }

    static std::optional<ScopedNativeHandle> OpenOrCreateDirectoryChild(
        HANDLE parent, const std::wstring& name) {
        const ACCESS_MASK access =
            FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES |
            READ_CONTROL | WRITE_DAC | WRITE_OWNER;
        NativeOpenResult opened = OpenRelativeNative(
            parent, name, access, kFileOpenIf, kFileDirectoryFile,
            FILE_ATTRIBUTE_DIRECTORY);
        if (!opened.ok() || !IsNonReparseDirectoryHandle(opened.handle.get())) {
            return std::nullopt;
        }
        return std::move(opened.handle);
    }

    static std::optional<ScopedNativeHandle> OpenPinnedAbsoluteDirectory(
        const std::string& pathText, const bool createMissing = true) {
        std::wstring normalized = std::filesystem::u8path(pathText).wstring();
        if (normalized.rfind(L"\\\\?\\", 0U) == 0U) {
            normalized.erase(0U, 4U);
        }
        std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
        while (normalized.size() > 3U && normalized.back() == L'\\') {
            normalized.pop_back();
        }
        if (normalized.size() < 4U || normalized[1] != L':' ||
            normalized[2] != L'\\') {
            return std::nullopt;
        }
        const std::wstring volumePath = normalized.substr(0U, 3U);
        const std::wstring extendedVolumePath = L"\\\\?\\" + volumePath;
        ScopedNativeHandle current(CreateFileW(
            extendedVolumePath.c_str(),
            FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!current.valid() || !IsNonReparseDirectoryHandle(current.get())) {
            return std::nullopt;
        }
        std::size_t begin = 3U;
        bool openedComponent = false;
        while (begin < normalized.size()) {
            const std::size_t separator = normalized.find(L'\\', begin);
            const std::wstring component = normalized.substr(
                begin, separator == std::wstring::npos
                           ? std::wstring::npos
                           : separator - begin);
            if (component.empty()) return std::nullopt;
            const bool finalComponent = separator == std::wstring::npos;
            // A standard user may traverse ancestors such as C:\\Users but
            // must not need permission to change their owner or ACL.  Only
            // the final updater-owned root is opened with the capability
            // required to create and secure it.
            auto child = finalComponent && createMissing
                ? OpenOrCreateDirectoryChild(current.get(), component)
                : OpenExistingDirectoryChild(current.get(), component);
            if (!child.has_value()) return std::nullopt;
            if (createMissing && separator == std::wstring::npos &&
                !SecureUpdaterDirectoryImpl(child->get())) {
                return std::nullopt;
            }
            current = std::move(*child);
            openedComponent = true;
            if (separator == std::wstring::npos) break;
            begin = separator + 1U;
        }
        return openedComponent ? std::optional<ScopedNativeHandle>(
                                      std::move(current))
                               : std::nullopt;
    }

    struct PinnedUpdaterDirectories {
        ScopedNativeHandle base;
        ScopedNativeHandle download;
        ScopedNativeHandle staging;
        ScopedNativeHandle transaction;
    };

    struct PinnedArchive {
        ScopedNativeHandle handle;
        std::uint64_t size = 0U;
        std::string sha256;
        bool locked = false;

        PinnedArchive() = default;
        PinnedArchive(const PinnedArchive&) = delete;
        PinnedArchive& operator=(const PinnedArchive&) = delete;
        PinnedArchive(PinnedArchive&& other) noexcept
            : handle(std::move(other.handle)), size(other.size),
              sha256(std::move(other.sha256)), locked(other.locked) {
            other.locked = false;
        }
        PinnedArchive& operator=(PinnedArchive&& other) noexcept {
            if (this == &other) return *this;
            Unlock();
            handle = std::move(other.handle);
            size = other.size;
            sha256 = std::move(other.sha256);
            locked = other.locked;
            other.locked = false;
            return *this;
        }
        ~PinnedArchive() { Unlock(); }

        void Unlock() noexcept {
            if (!locked || !handle.valid()) return;
            OVERLAPPED overlap{};
            static_cast<void>(UnlockFileEx(
                handle.get(), 0U, MAXDWORD, MAXDWORD, &overlap));
            locked = false;
        }
    };

    static std::string UpdaterOwnedBase(const std::string& installDirectory) {
#if defined(ARIS_UPDATE_LOCAL_DEMO) && ARIS_UPDATE_LOCAL_DEMO
        static_cast<void>(installDirectory);
        return std::string(ARIS_UPDATE_LOCAL_DEMO_ROOT);
#else
        PWSTR path = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                  &path) != S_OK || path == nullptr) {
            return {};
        }
        std::filesystem::path value(path);
        CoTaskMemFree(path);
        // Follows the install volume; see ResolveUpdaterOwnedBaseForInstall.
        return ResolveUpdaterOwnedBaseForInstall(installDirectory,
                                                 value.u8string());
#endif
    }

    static std::optional<PinnedUpdaterDirectories> PrepareUpdaterDirectories(
        const std::string& base, const std::string& download,
        const std::string& staging,
        const std::string& transaction) {
        auto baseHandle = OpenPinnedAbsoluteDirectory(base);
        if (!baseHandle.has_value()) return std::nullopt;
        const std::wstring downloadName =
            std::filesystem::u8path(download).filename().wstring();
        const std::wstring stagingName =
            std::filesystem::u8path(staging).filename().wstring();
        const std::wstring transactionName =
            std::filesystem::u8path(transaction).filename().wstring();
        auto downloadHandle = OpenOrCreateDirectoryChild(
            baseHandle->get(), downloadName);
        auto stagingHandle = OpenOrCreateDirectoryChild(
            baseHandle->get(), stagingName);
        auto transactionHandle = OpenOrCreateDirectoryChild(
            baseHandle->get(), transactionName);
        if (!downloadHandle.has_value() || !stagingHandle.has_value() ||
            !transactionHandle.has_value() ||
            !SecureUpdaterDirectoryImpl(downloadHandle->get()) ||
            !SecureUpdaterDirectoryImpl(stagingHandle->get()) ||
            !SecureUpdaterDirectoryImpl(transactionHandle->get())) {
            return std::nullopt;
        }
        return PinnedUpdaterDirectories{
            std::move(*baseHandle), std::move(*downloadHandle),
            std::move(*stagingHandle),
            std::move(*transactionHandle)};
    }

    static std::string RandomHex(std::size_t length) {
        std::vector<std::uint8_t> bytes(length / 2U);
        if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return {};
        static constexpr char digits[] = "0123456789abcdef";
        std::string output;
        for (const auto byte : bytes) {
            output.push_back(digits[byte >> 4U]);
            output.push_back(digits[byte & 0x0fU]);
        }
        return output;
    }

    static bootstrap::BootstrapProcessIdentity CurrentIdentity() {
        bootstrap::BootstrapProcessIdentity identity;
        identity.processId = GetCurrentProcessId();
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return {};
        ULARGE_INTEGER time{};
        time.LowPart = created.dwLowDateTime;
        time.HighPart = created.dwHighDateTime;
        identity.creationTime = time.QuadPart;
        std::vector<wchar_t> pathBuffer(512U, L'\0');
        DWORD pathLength = static_cast<DWORD>(pathBuffer.size());
        while (!QueryFullProcessImageNameW(
            GetCurrentProcess(), 0U, pathBuffer.data(), &pathLength)) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
                pathBuffer.size() >= 32768U) return {};
            pathBuffer.resize(pathBuffer.size() * 2U);
            pathLength = static_cast<DWORD>(pathBuffer.size());

        }
        const std::wstring requested(pathBuffer.data(), pathLength);
        HANDLE image = CreateFileW(
            requested.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);
        if (image == INVALID_HANDLE_VALUE) return {};
        FILE_ATTRIBUTE_TAG_INFO tags{};
        FILE_ID_INFO fileId{};
        const bool metadata =
            GetFileInformationByHandleEx(
                image, FileAttributeTagInfo, &tags, sizeof(tags)) != FALSE &&
            GetFileInformationByHandleEx(
                image, FileIdInfo, &fileId, sizeof(fileId)) != FALSE;
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            image, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()),
            FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                image, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()),
                FILE_NAME_NORMALIZED);
        }
        const std::wstring canonical(
            canonicalBuffer.data(), canonicalLength);
        CloseHandle(image);
        if (!metadata || canonical.empty() ||
            (tags.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
            return {};
        }
        std::wstring portable = canonical;
        if (portable.rfind(L"\\\\?\\", 0U) == 0U) {
            portable.erase(0U, 4U);
        }
        identity.imagePath = std::filesystem::path(portable).u8string();
        identity.imageVolumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier),
                  identity.imageFileId.begin());
        identity.valid = !identity.imagePath.empty() &&
                         identity.creationTime != 0U &&
                         identity.processId != 0U;
        return identity;
    }

    static std::optional<std::string> HashRuntimePairFile(HANDLE file) {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0U;
        DWORD resultSize = 0U;
        if (BCryptOpenAlgorithmProvider(
                &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U) < 0 ||
            BCryptGetProperty(
                algorithm, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                &resultSize, 0U) < 0 || objectSize == 0U) {
            if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0U);
            return std::nullopt;
        }
        std::vector<std::uint8_t> object(objectSize);
        std::array<std::uint8_t, 32U> digest{};
        if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize,
                             nullptr, 0U, 0U) < 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0U);
            return std::nullopt;
        }
        LARGE_INTEGER beginning{};
        bool ok = SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN) != FALSE;
        std::array<std::uint8_t, 64U * 1024U> buffer{};
        while (ok) {
            DWORD read = 0U;
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                          &read, nullptr)) {
                ok = false;
                break;
            }
            if (read == 0U) break;
            ok = BCryptHashData(hash, buffer.data(), read, 0U) >= 0;
        }
        ok = ok && BCryptFinishHash(
            hash, digest.data(), static_cast<ULONG>(digest.size()), 0U) >= 0;
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0U);
        if (!ok) return std::nullopt;
        static constexpr char digits[] = "0123456789abcdef";
        std::string encoded;
        encoded.reserve(digest.size() * 2U);
        for (const std::uint8_t byte : digest) {
            encoded.push_back(digits[byte >> 4U]);
            encoded.push_back(digits[byte & 0x0fU]);
        }
        return encoded;
    }

    struct BootstrapImageIdentity {
        std::uint64_t size = 0U;
        std::string sha256;
    };

    static std::optional<BootstrapImageIdentity>
    HashSigningStableBootstrapImage(HANDLE file) {
#if !defined(ARIS_RUNTIME_PAIR_PROVISIONED)
        static_cast<void>(file);
        return std::nullopt;
#else
        LARGE_INTEGER nativeSize{};
        if (GetFileSizeEx(file, &nativeSize) == FALSE || nativeSize.QuadPart < 0) {
            return std::nullopt;
        }
        constexpr std::uint64_t kMaximumCertificateOverlay =
            16U * 1024U * 1024U;
        const auto fileSize = static_cast<std::uint64_t>(nativeSize.QuadPart);
        if (fileSize > aris::runtime_pair::kBootstrapImageSize +
                           kMaximumCertificateOverlay ||
            fileSize > static_cast<std::uint64_t>(
                           std::numeric_limits<std::size_t>::max())) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
        LARGE_INTEGER beginning{};
        if (SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN) == FALSE) {
            return std::nullopt;
        }
        std::size_t consumed = 0U;
        while (consumed < bytes.size()) {
            const auto remaining = bytes.size() - consumed;
            const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
                remaining, static_cast<std::size_t>(MAXDWORD)));
            DWORD read = 0U;
            if (ReadFile(file, bytes.data() + consumed, requested, &read,
                         nullptr) == FALSE || read == 0U) {
                return std::nullopt;
            }
            consumed += read;
        }
        const auto read16 = [&bytes](const std::size_t offset)
            -> std::optional<std::uint16_t> {
            if (offset > bytes.size() || bytes.size() - offset < 2U) {
                return std::nullopt;
            }
            std::uint16_t value = 0U;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        };
        const auto read32 = [&bytes](const std::size_t offset)
            -> std::optional<std::uint32_t> {
            if (offset > bytes.size() || bytes.size() - offset < 4U) {
                return std::nullopt;
            }
            std::uint32_t value = 0U;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        };
        if (bytes.size() < 0x40U || bytes[0] != 'M' || bytes[1] != 'Z') {
            return std::nullopt;
        }
        const auto peOffsetValue = read32(0x3cU);
        if (!peOffsetValue.has_value()) return std::nullopt;
        const std::size_t peOffset = *peOffsetValue;
        if (peOffset > bytes.size() || bytes.size() - peOffset < 24U ||
            bytes[peOffset] != 'P' || bytes[peOffset + 1U] != 'E' ||
            bytes[peOffset + 2U] != 0U || bytes[peOffset + 3U] != 0U) {
            return std::nullopt;
        }
        const auto optionalSizeValue = read16(peOffset + 20U);
        if (!optionalSizeValue.has_value()) return std::nullopt;
        const std::size_t optionalOffset = peOffset + 24U;
        const std::size_t optionalSize = *optionalSizeValue;
        if (optionalOffset > bytes.size() ||
            optionalSize > bytes.size() - optionalOffset) {
            return std::nullopt;
        }
        const auto magic = read16(optionalOffset);
        if (!magic.has_value()) return std::nullopt;
        std::size_t securityOffset = 0U;
        if (*magic == 0x10bU) {
            securityOffset = optionalOffset + 128U;
        } else if (*magic == 0x20bU) {
            securityOffset = optionalOffset + 144U;
        } else {
            return std::nullopt;
        }
        const std::size_t checksumOffset = optionalOffset + 64U;
        const std::size_t optionalEnd = optionalOffset + optionalSize;
        if (checksumOffset + 4U > optionalEnd ||
            securityOffset + 8U > optionalEnd) {
            return std::nullopt;
        }
        const auto certificateOffsetValue = read32(securityOffset);
        const auto certificateSizeValue = read32(securityOffset + 4U);
        if (!certificateOffsetValue.has_value() ||
            !certificateSizeValue.has_value()) {
            return std::nullopt;
        }
        const std::size_t certificateOffset = *certificateOffsetValue;
        const std::size_t certificateSize = *certificateSizeValue;
        if ((certificateOffset == 0U) != (certificateSize == 0U)) {
            return std::nullopt;
        }
        std::size_t imageSize = bytes.size();
        if (certificateOffset != 0U) {
            if (certificateOffset % 8U != 0U || certificateSize < 8U ||
                certificateOffset < optionalEnd ||
                certificateOffset > bytes.size() ||
                certificateSize > bytes.size() - certificateOffset ||
                certificateOffset + certificateSize != bytes.size()) {
                return std::nullopt;
            }
            imageSize = certificateOffset;
        }
        if (securityOffset + 8U > imageSize) return std::nullopt;

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0U;
        DWORD resultSize = 0U;
        if (BCryptOpenAlgorithmProvider(
                &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U) < 0 ||
            BCryptGetProperty(
                algorithm, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                &resultSize, 0U) < 0 || objectSize == 0U) {
            if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0U);
            return std::nullopt;
        }
        std::vector<std::uint8_t> object(objectSize);
        std::array<std::uint8_t, 32U> digest{};
        if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize,
                             nullptr, 0U, 0U) < 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0U);
            return std::nullopt;
        }
        const auto hashRange = [&](const std::size_t first,
                                   const std::size_t last) {
            if (last < first || last - first >
                    static_cast<std::size_t>(
                        std::numeric_limits<ULONG>::max())) {
                return false;
            }
            if (last == first) return true;
            return BCryptHashData(
                       hash, bytes.data() + first,
                       static_cast<ULONG>(last - first), 0U) >= 0;
        };
        const bool hashed =
            hashRange(0U, checksumOffset) &&
            hashRange(checksumOffset + 4U, securityOffset) &&
            hashRange(securityOffset + 8U, imageSize) &&
            BCryptFinishHash(hash, digest.data(),
                             static_cast<ULONG>(digest.size()), 0U) >= 0;
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0U);
        if (!hashed) return std::nullopt;

        static constexpr char digits[] = "0123456789abcdef";
        BootstrapImageIdentity identity;
        identity.size = imageSize;
        identity.sha256.reserve(digest.size() * 2U);
        for (const std::uint8_t byte : digest) {
            identity.sha256.push_back(digits[byte >> 4U]);
            identity.sha256.push_back(digits[byte & 0x0fU]);
        }
        return identity;
#endif
    }

    static std::optional<RuntimePairFile> ObserveRuntimePairFile(
        const std::string& requestedPath) {
#if !defined(ARIS_RUNTIME_PAIR_PROVISIONED)
        static_cast<void>(requestedPath);
        return std::nullopt;
#else
        const std::filesystem::path path(requestedPath);
        HANDLE file = CreateFileW(
            path.wstring().c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE) return std::nullopt;
        FILE_ATTRIBUTE_TAG_INFO tags{};
        FILE_ID_INFO fileId{};
        LARGE_INTEGER size{};
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            file, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                file, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        }
        const auto digest = HashRuntimePairFile(file);
        const auto bootstrapImage = HashSigningStableBootstrapImage(file);
        const bool valid =
            GetFileInformationByHandleEx(file, FileAttributeTagInfo, &tags,
                                         sizeof(tags)) != FALSE &&
            GetFileInformationByHandleEx(file, FileIdInfo, &fileId,
                                         sizeof(fileId)) != FALSE &&
            GetFileSizeEx(file, &size) != FALSE && size.QuadPart >= 0 &&
            canonicalLength != 0U &&
            (tags.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U &&
            (tags.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U &&
            digest.has_value() && bootstrapImage.has_value();
        CloseHandle(file);
        if (!valid) return std::nullopt;
        std::wstring canonical(canonicalBuffer.data(), canonicalLength);
        if (canonical.rfind(L"\\\\?\\", 0U) == 0U) canonical.erase(0U, 4U);
        RuntimePairFile identity;
        identity.canonicalPath = std::filesystem::path(canonical).u8string();
        identity.volumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier), identity.fileId.begin());
        identity.size = static_cast<std::uint64_t>(size.QuadPart);
        identity.sha256 = *digest;
        if (bootstrapImage->size !=
                aris::runtime_pair::kBootstrapImageSize ||
            bootstrapImage->sha256 !=
                aris::runtime_pair::kBootstrapImageSha256) {
            return std::nullopt;
        }
        return identity;
#endif
    }

    static bool SameRuntimePairFile(const RuntimePairFile& left,
                                    const RuntimePairFile& right) {
        return NormalizeProtectedRoot(left.canonicalPath) ==
                   NormalizeProtectedRoot(right.canonicalPath) &&
               left.volumeSerial == right.volumeSerial &&
               left.fileId == right.fileId && left.size == right.size &&
               left.sha256 == right.sha256;
    }

    static std::optional<RuntimePairFile> ObserveRuntimePairHandle(
        HANDLE file) {
#if !defined(ARIS_RUNTIME_PAIR_PROVISIONED)
        static_cast<void>(file);
        return std::nullopt;
#else
        if (!IsNonReparseRegularFileHandle(file)) return std::nullopt;
        FILE_ID_INFO fileId{};
        LARGE_INTEGER size{};
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            file, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                file, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        }
        const auto digest = HashRuntimePairFile(file);
        const auto bootstrapImage = HashSigningStableBootstrapImage(file);
        if (GetFileInformationByHandleEx(
                file, FileIdInfo, &fileId, sizeof(fileId)) == FALSE ||
            GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 ||
            canonicalLength == 0U || !digest.has_value() ||
            !bootstrapImage.has_value()) {
            return std::nullopt;
        }
        std::wstring canonical(canonicalBuffer.data(), canonicalLength);
        if (canonical.rfind(L"\\\\?\\", 0U) == 0U) canonical.erase(0U, 4U);
        RuntimePairFile identity;
        identity.canonicalPath = std::filesystem::path(canonical).u8string();
        identity.volumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier), identity.fileId.begin());
        identity.size = static_cast<std::uint64_t>(size.QuadPart);
        identity.sha256 = *digest;
        if (bootstrapImage->size !=
                aris::runtime_pair::kBootstrapImageSize ||
            bootstrapImage->sha256 !=
                aris::runtime_pair::kBootstrapImageSha256) {
            return std::nullopt;
        }
        return identity;
#endif
    }

    static std::optional<RuntimePairDirectoryIdentity>
    ObserveRuntimePairDirectoryHandle(HANDLE directory) {
#if !defined(ARIS_RUNTIME_PAIR_PROVISIONED)
        static_cast<void>(directory);
        return std::nullopt;
#else
        if (!IsNonReparseDirectoryHandle(directory)) return std::nullopt;
        FILE_ID_INFO fileId{};
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            directory, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                directory, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        }
        if (canonicalLength == 0U || canonicalLength >= canonicalBuffer.size() ||
            GetFileInformationByHandleEx(
                directory, FileIdInfo, &fileId, sizeof(fileId)) == FALSE) {
            return std::nullopt;
        }
        std::wstring canonical(canonicalBuffer.data(), canonicalLength);
        if (canonical.rfind(L"\\\\?\\", 0U) == 0U) {
            canonical.erase(0U, 4U);
        }
        RuntimePairDirectoryIdentity identity;
        identity.canonicalPath = std::filesystem::path(canonical).u8string();
        identity.volumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier), identity.fileId.begin());
        return identity;
#endif
    }

    static bool SameRuntimePairDirectory(
        const RuntimePairDirectoryIdentity& left,
        const RuntimePairDirectoryIdentity& right) {
        return NormalizeProtectedRoot(left.canonicalPath) ==
                   NormalizeProtectedRoot(right.canonicalPath) &&
               left.volumeSerial == right.volumeSerial &&
               left.fileId == right.fileId;
    }

    static std::optional<PayloadDirectoryIdentity>
    ObservePayloadDirectoryHandle(
        HANDLE directory, const std::string_view relativePath) {
        if (!IsNonReparseDirectoryHandle(directory) ||
            (!relativePath.empty() &&
             !SafeRelativeComponents(std::string(relativePath)).has_value())) {
            return std::nullopt;
        }
        FILE_ID_INFO fileId{};
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            directory, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                directory, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()),
                FILE_NAME_NORMALIZED);
        }
        if (canonicalLength == 0U || canonicalLength >= canonicalBuffer.size() ||
            GetFileInformationByHandleEx(
                directory, FileIdInfo, &fileId, sizeof(fileId)) == FALSE) {
            return std::nullopt;
        }
        std::wstring canonical(canonicalBuffer.data(), canonicalLength);
        if (canonical.rfind(L"\\\\?\\", 0U) == 0U) {
            canonical.erase(0U, 4U);
        }
        PayloadDirectoryIdentity identity;
        identity.relativePath = std::string(relativePath);
        std::replace(identity.relativePath.begin(), identity.relativePath.end(),
                      '\\', '/');
        identity.canonicalPath = std::filesystem::path(canonical).u8string();
        identity.volumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier), identity.fileId.begin());
        return identity;
    }

    static bool SamePayloadDirectory(
        const PayloadDirectoryIdentity& left,
        const PayloadDirectoryIdentity& right) {
        return NormalizeProtectedRoot(left.relativePath) ==
                   NormalizeProtectedRoot(right.relativePath) &&
               NormalizeProtectedRoot(left.canonicalPath) ==
                   NormalizeProtectedRoot(right.canonicalPath) &&
               left.volumeSerial == right.volumeSerial &&
               left.fileId == right.fileId;
    }

    static std::optional<PayloadFile> ObservePayloadFileHandle(
        HANDLE file, const std::string& relativePath) {
        if (!IsNonReparseRegularFileHandle(file) ||
            !SafeRelativeComponents(relativePath).has_value()) {
            return std::nullopt;
        }
        FILE_ID_INFO fileId{};
        LARGE_INTEGER size{};
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            file, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                file, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        }
        const auto digest = HashRuntimePairFile(file);
        if (GetFileInformationByHandleEx(
                file, FileIdInfo, &fileId, sizeof(fileId)) == FALSE ||
            GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 ||
            canonicalLength == 0U || canonicalLength >= canonicalBuffer.size() ||
            !digest.has_value()) {
            return std::nullopt;
        }
        std::wstring canonical(canonicalBuffer.data(), canonicalLength);
        if (canonical.rfind(L"\\\\?\\", 0U) == 0U) {
            canonical.erase(0U, 4U);
        }
        std::string normalized = relativePath;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        PayloadFile identity;
        identity.relativePath = std::move(normalized);
        identity.canonicalPath = std::filesystem::path(canonical).u8string();
        identity.volumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier), identity.fileId.begin());
        identity.size = static_cast<std::uint64_t>(size.QuadPart);
        identity.sha256 = *digest;
        return identity;
    }

    static bool SamePayloadFile(
        const PayloadFile& left, const PayloadFile& right) {
        return NormalizeProtectedRoot(left.relativePath) ==
                   NormalizeProtectedRoot(right.relativePath) &&
               NormalizeProtectedRoot(left.canonicalPath) ==
                   NormalizeProtectedRoot(right.canonicalPath) &&
               left.volumeSerial == right.volumeSerial &&
               left.fileId == right.fileId && left.size == right.size &&
               left.sha256 == right.sha256;
    }

    static std::optional<ScopedNativeHandle> OpenRelativeRegularFile(
        HANDLE parent, const std::wstring& name, ULONG disposition) {
        NativeOpenResult opened = OpenRelativeNative(
            parent, name, GENERIC_READ | GENERIC_WRITE | DELETE |
                FILE_READ_ATTRIBUTES, disposition, kFileNonDirectoryFile,
            FILE_ATTRIBUTE_NORMAL);
        if (!opened.ok() || !IsNonReparseRegularFileHandle(opened.handle.get())) {
            return std::nullopt;
        }
        return std::move(opened.handle);
    }

    static std::optional<ScopedNativeHandle> OpenExistingCleanupFile(
        HANDLE parent, const std::wstring& name) {
        NativeOpenResult opened = OpenRelativeNative(
            parent, name, GENERIC_READ | DELETE | FILE_READ_ATTRIBUTES, kFileOpen,
            kFileNonDirectoryFile, FILE_ATTRIBUTE_NORMAL);
        if (!opened.ok() || !IsNonReparseRegularFileHandle(opened.handle.get())) {
            return std::nullopt;
        }
        return std::move(opened.handle);
    }

    static std::optional<ScopedNativeHandle> OpenExistingPayloadFile(
        HANDLE parent, const std::wstring& name) {
        NativeOpenResult opened = OpenRelativeNative(
            parent, name, GENERIC_READ | DELETE | FILE_READ_ATTRIBUTES,
            kFileOpen, kFileNonDirectoryFile, FILE_ATTRIBUTE_NORMAL);
        if (!opened.ok() || !IsNonReparseRegularFileHandle(opened.handle.get())) {
            return std::nullopt;
        }
        return std::move(opened.handle);
    }

    static std::optional<ScopedNativeHandle> OpenExistingCleanupDirectoryChild(
        HANDLE parent, const std::wstring& name) {
        const ACCESS_MASK access =
            DELETE | FILE_LIST_DIRECTORY | FILE_TRAVERSE |
            FILE_READ_ATTRIBUTES | READ_CONTROL;
        NativeOpenResult opened = OpenRelativeNative(
            parent, name, access, kFileOpen, kFileDirectoryFile,
            FILE_ATTRIBUTE_DIRECTORY);
        if (!opened.ok() || !IsNonReparseDirectoryHandle(opened.handle.get())) {
            return std::nullopt;
        }
        return std::move(opened.handle);
    }

    struct NativeFileRenameInformation {
        BOOLEAN replaceIfExists = FALSE;
        HANDLE rootDirectory = nullptr;
        ULONG fileNameLength = 0U;
        WCHAR fileName[1]{};
    };

    static bool RenameRelativeFile(
        HANDLE file, HANDLE parent, const std::wstring& destination) {
        const auto& native = NativeApi();
        if (native.setInformationFile == nullptr || destination.empty() ||
            destination.find_first_of(L"\\/") != std::wstring::npos) {
            return false;
        }
        const std::size_t bytes =
            offsetof(NativeFileRenameInformation, fileName) +
            destination.size() * sizeof(wchar_t);
        std::vector<std::uint8_t> storage(bytes, 0U);
        auto* rename = reinterpret_cast<NativeFileRenameInformation*>(
            storage.data());
        rename->replaceIfExists = TRUE;
        rename->rootDirectory = parent;
        rename->fileNameLength = static_cast<ULONG>(
            destination.size() * sizeof(wchar_t));
        std::memcpy(rename->fileName, destination.data(),
                    destination.size() * sizeof(wchar_t));
        IO_STATUS_BLOCK io{};
        return native.setInformationFile(
                   file, &io, rename, static_cast<ULONG>(storage.size()),
                   kFileRenameInformation) >= 0;
    }

    static bool DeleteFileByHandle(HANDLE file) {
        const auto& native = NativeApi();
        if (native.setInformationFile == nullptr) return false;
        struct NativeFileDispositionInformation {
            BOOLEAN deleteFile = FALSE;
        } disposition;
        disposition.deleteFile = TRUE;
        IO_STATUS_BLOCK io{};
        constexpr FILE_INFORMATION_CLASS kFileDispositionInformation =
            static_cast<FILE_INFORMATION_CLASS>(13);
        return native.setInformationFile(
                   file, &io, &disposition, sizeof(disposition),
                   kFileDispositionInformation) >= 0;
    }

    static bool CopyBytesToHandle(HANDLE source, HANDLE destination) {
        std::array<std::uint8_t, 64U * 1024U> buffer{};
        while (true) {
            DWORD read = 0U;
            if (!ReadFile(source, buffer.data(), static_cast<DWORD>(buffer.size()),
                          &read, nullptr)) {
                return false;
            }
            if (read == 0U) return FlushFileBuffers(destination) != FALSE;
            DWORD written = 0U;
            if (!WriteFile(destination, buffer.data(), read, &written, nullptr) ||
                written != read) {
                return false;
            }
        }
    }

    static std::optional<RuntimePairFile> CopyVerifiedBootstrap(
        const RuntimePairFile& source,
        HANDLE transactionRoot) {
        auto directory = OpenOrCreateDirectoryChild(
            transactionRoot, L"runtime-pair");
        if (!directory.has_value() ||
            !SecureUpdaterDirectoryImpl(directory->get())) return std::nullopt;
        HANDLE sourceFile = CreateFileW(
            std::filesystem::u8path(source.canonicalPath).wstring().c_str(),
            GENERIC_READ | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
        ScopedNativeHandle sourceHandle(sourceFile);
        if (!sourceHandle.valid() || !IsNonReparseRegularFileHandle(sourceHandle.get())) {
            return std::nullopt;
        }
        const auto observedSource = ObserveRuntimePairHandle(sourceHandle.get());
        if (!observedSource.has_value() ||
            !SameRuntimePairFile(*observedSource, source)) {
            return std::nullopt;
        }
        LARGE_INTEGER beginning{};
        if (!SetFilePointerEx(sourceHandle.get(), beginning, nullptr, FILE_BEGIN)) {
            return std::nullopt;
        }
        auto temporary = OpenRelativeRegularFile(
            directory->get(), L"HarkenSearchBootstrap.exe.copying", kFileCreate);
        if (!temporary.has_value() ||
            !CopyBytesToHandle(sourceHandle.get(), temporary->get())) {
            if (temporary.has_value()) DeleteFileByHandle(temporary->get());
            return std::nullopt;
        }
        const auto copied = ObserveRuntimePairHandle(temporary->get());
        if (!copied.has_value() || copied->size != source.size ||
            copied->sha256 != source.sha256 ||
            !RenameRelativeFile(temporary->get(), directory->get(),
                                L"HarkenSearchBootstrap.exe")) {
            DeleteFileByHandle(temporary->get());
            return std::nullopt;
        }
        temporary.reset();
        auto publishedHandle = OpenRelativeRegularFile(
            directory->get(), L"HarkenSearchBootstrap.exe", kFileOpen);
        if (!publishedHandle.has_value()) return std::nullopt;
        const auto published = ObserveRuntimePairHandle(publishedHandle->get());
        if (!published.has_value() || published->size != source.size ||
            published->sha256 != source.sha256) {
            DeleteFileByHandle(publishedHandle->get());
            return std::nullopt;
        }
        return published;
    }

    static TransactionRuntimePairCleanup::Status RemoveRuntimePairCopy(
        const AuthorityState& state) {
        TransactionRuntimePairCleanup cleanup(
            state.invocation.transactionRoot, state.runtimePairCleanup);
        const auto tracked = cleanup.Track(state.launchCopyIdentity);
        const auto status = tracked ==
                TransactionRuntimePairCleanup::Status::Armed
            ? cleanup.CleanupNow()
            : tracked;
        cleanup.Release();
        return status;
    }

    static bool SameProcessImage(
        const bootstrap::BootstrapProcessIdentity& process,
        const RuntimePairFile& file) {
        return process.valid &&
            NormalizeProtectedRoot(process.imagePath) ==
                NormalizeProtectedRoot(file.canonicalPath) &&
            process.imageVolumeSerial == file.volumeSerial &&
            process.imageFileId == file.fileId;
    }

    static std::optional<std::vector<bootstrap::ExistingInstallItem>>
    ExistingInventory(const std::string& rootText) {
        std::vector<bootstrap::ExistingInstallItem> inventory;
        const std::filesystem::path root(rootText);
        std::error_code error;
        if (rootText.empty() || !std::filesystem::is_directory(root, error) || error) {
            return std::nullopt;
        }
        std::filesystem::recursive_directory_iterator iterator(
            root, std::filesystem::directory_options::none, error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && iterator != end) {
            const auto& entry = *iterator;
            if (entry.is_symlink(error) || error) return std::nullopt;
            if (entry.is_regular_file(error)) {
                std::string relative = entry.path().lexically_relative(root).u8string();
                std::replace(relative.begin(), relative.end(), '\\', '/');
                if (relative.empty() || relative == "." ||
                    relative.find("..") != std::string::npos) {
                    return std::nullopt;
                }
                inventory.push_back({std::move(relative)});
            } else if (!entry.is_directory(error) || error) {
                return std::nullopt;
            }
            iterator.increment(error);
        }
        if (error) return std::nullopt;
        std::sort(inventory.begin(), inventory.end(),
                  [](const auto& left, const auto& right) {
                      return left.relativePath < right.relativePath;
                  });
        return inventory;
    }

    static nlohmann::json IdentityJson(
        const bootstrap::BootstrapProcessIdentity& identity) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string fileId;
        fileId.reserve(identity.imageFileId.size() * 2U);
        for (const std::uint8_t byte : identity.imageFileId) {
            fileId.push_back(digits[byte >> 4U]);
            fileId.push_back(digits[byte & 0x0fU]);
        }
        return {
            {"creation_time", identity.creationTime},
            {"file_id", fileId},
            {"image_path", identity.imagePath},
            {"image_volume_serial", identity.imageVolumeSerial},
            {"process_id", identity.processId},
        };
    }

    static nlohmann::json DeploymentFileIdentityJson(
        const aris::platform::DeploymentFileIdentity& identity) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string fileId;
        fileId.reserve(identity.fileId.size() * 2U);
        for (const std::uint8_t byte : identity.fileId) {
            fileId.push_back(digits[byte >> 4U]);
            fileId.push_back(digits[byte & 0x0fU]);
        }
        return {
            {"canonical_path", identity.canonicalPath},
            {"file_id", fileId},
            {"volume_serial", identity.volumeSerial},
        };
    }

    static std::optional<std::vector<std::wstring>> SafeRelativeComponents(
        const std::string& value) {
        if (!IsSafeStagedRelativePath(value)) return std::nullopt;
        std::string normalized = value;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::vector<std::wstring> components;
        std::size_t begin = 0U;
        while (begin < normalized.size()) {
            const std::size_t end = normalized.find('/', begin);
            const std::string component = normalized.substr(
                begin, end == std::string::npos ? std::string::npos
                                                : end - begin);
            if (component.empty() || component == "." || component == ".." ||
                component.find(':') != std::string::npos) {
                return std::nullopt;
            }
            const std::wstring wide = std::filesystem::u8path(component).wstring();
            if (wide.empty()) return std::nullopt;
            components.push_back(wide);
            if (end == std::string::npos) break;
            begin = end + 1U;
        }
        return components.empty() ? std::nullopt
                                   : std::optional<std::vector<std::wstring>>(
                                         std::move(components));
    }

    static std::optional<ScopedNativeHandle> OpenRelativeDirectoryPath(
        HANDLE root, const std::vector<std::wstring>& components,
        std::size_t count) {
        if (root == nullptr || root == INVALID_HANDLE_VALUE ||
            count > components.size()) {
            return std::nullopt;
        }
        HANDLE parent = root;
        ScopedNativeHandle current;
        for (std::size_t index = 0U; index < count; ++index) {
            auto child = OpenOrCreateDirectoryChild(parent, components[index]);
            if (!child.has_value()) return std::nullopt;
            current = std::move(*child);
            parent = current.get();
        }
        return count == 0U ? std::nullopt
                           : std::optional<ScopedNativeHandle>(
                                 std::move(current));
    }

    static std::optional<ScopedNativeHandle> OpenExistingDirectoryPath(
        HANDLE root, const std::vector<std::wstring>& components,
        std::size_t count) {
        if (root == nullptr || root == INVALID_HANDLE_VALUE ||
            count == 0U || count > components.size()) {
            return std::nullopt;
        }
        HANDLE parent = root;
        ScopedNativeHandle current;
        for (std::size_t index = 0U; index < count; ++index) {
            auto child = OpenExistingDirectoryChild(parent, components[index]);
            if (!child.has_value()) return std::nullopt;
            current = std::move(*child);
            parent = current.get();
        }
        return std::optional<ScopedNativeHandle>(std::move(current));
    }

    static std::optional<PinnedArchive> OpenVerifiedArchive(
        HANDLE baseRoot, const std::string& relativePath,
        const std::uint64_t expectedSize,
        const std::string& expectedSha256) {
        const auto components = SafeRelativeComponents(relativePath);
        if (!components.has_value() || baseRoot == nullptr ||
            baseRoot == INVALID_HANDLE_VALUE || expectedSize == 0U ||
            expectedSize > static_cast<std::uint64_t>(
                (std::numeric_limits<LONGLONG>::max)()) ||
            expectedSha256.size() != 64U) {
            return std::nullopt;
        }
        const std::size_t parentCount = components->size() - 1U;
        ScopedNativeHandle parent;
        if (parentCount != 0U) {
            auto openedParent = OpenExistingDirectoryPath(
                baseRoot, *components, parentCount);
            if (!openedParent.has_value()) return std::nullopt;
            parent = std::move(*openedParent);
        }
        HANDLE parentHandle = parentCount == 0U ? baseRoot : parent.get();
        NativeOpenResult opened = OpenRelativeNative(
            parentHandle, components->back(),
            GENERIC_READ | FILE_READ_ATTRIBUTES, kFileOpen,
            kFileNonDirectoryFile, FILE_ATTRIBUTE_NORMAL);
        if (!opened.ok() || !IsNonReparseRegularFileHandle(opened.handle.get())) {
            return std::nullopt;
        }
        OVERLAPPED overlap{};
        if (!LockFileEx(opened.handle.get(),
                        LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                        0U, MAXDWORD, MAXDWORD, &overlap)) {
            return std::nullopt;
        }
        LARGE_INTEGER size{};
        if (GetFileSizeEx(opened.handle.get(), &size) == FALSE ||
            size.QuadPart < 0 ||
            static_cast<std::uint64_t>(size.QuadPart) != expectedSize) {
            return std::nullopt;
        }
        const auto digest = HashRuntimePairFile(opened.handle.get());
        if (!digest.has_value() || *digest != expectedSha256) {
            return std::nullopt;
        }
        PinnedArchive result;
        result.handle = std::move(opened.handle);
        result.size = expectedSize;
        result.sha256 = *digest;
        result.locked = true;
        return result;
    }

    struct ZipInputContext {
        HANDLE handle = INVALID_HANDLE_VALUE;
        mz_uint64 archiveSize = 0U;
        bool ok = true;
    };

    static size_t ReadZipChunk(
        void* opaque, mz_uint64 offset, void* data, size_t size) {
        auto* context = static_cast<ZipInputContext*>(opaque);
        if (context == nullptr || !context->ok ||
            context->handle == nullptr ||
            context->handle == INVALID_HANDLE_VALUE ||
            (data == nullptr && size != 0U) ||
            offset > context->archiveSize ||
            static_cast<mz_uint64>(size) > context->archiveSize - offset ||
            offset > static_cast<mz_uint64>(std::numeric_limits<LONGLONG>::max())) {
            if (context != nullptr) context->ok = false;
            return 0U;
        }
        if (size == 0U) return 0U;
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(context->handle, position, nullptr, FILE_BEGIN)) {
            context->ok = false;
            return 0U;
        }
        auto* output = static_cast<std::uint8_t*>(data);
        size_t remaining = size;
        while (remaining != 0U) {
            const DWORD request = static_cast<DWORD>(
                std::min<std::size_t>(remaining, MAXDWORD));
            DWORD read = 0U;
            if (!ReadFile(context->handle, output, request, &read, nullptr) ||
                read != request) {
                context->ok = false;
                return 0U;
            }
            output += read;
            remaining -= read;
        }
        return size;
    }

    struct ZipOutputContext {
        HANDLE handle = INVALID_HANDLE_VALUE;
        mz_uint64 expectedSize = 0U;
        mz_uint64 nextOffset = 0U;
        bool ok = true;
    };

    static size_t WriteZipChunk(
        void* opaque, mz_uint64 offset, const void* data, size_t size) {
        auto* context = static_cast<ZipOutputContext*>(opaque);
        if (context == nullptr || !context->ok || context->handle == nullptr ||
            context->handle == INVALID_HANDLE_VALUE ||
            (data == nullptr && size != 0U) ||
            offset != context->nextOffset ||
            offset > context->expectedSize ||
            static_cast<mz_uint64>(size) > context->expectedSize - offset ||
            size > MAXDWORD) {
            if (context != nullptr) context->ok = false;
            return 0U;
        }
        DWORD written = 0U;
        if (!WriteFile(context->handle, data, static_cast<DWORD>(size),
                       &written, nullptr) || written != size) {
            context->ok = false;
            return 0U;
        }
        context->nextOffset += size;
        return size;
    }

    static bool ExtractPayload(
        HANDLE zipHandle, const std::uint64_t zipSize, HANDLE stagingRoot,
        const bootstrap::VerifiedPackagePlanBinding& binding,
        std::vector<PayloadFile>* payloadFiles) {
        constexpr std::uint64_t kMaximumExtractedPayloadBytes =
            16ULL * 1024ULL * 1024ULL * 1024ULL;
        if (payloadFiles == nullptr || zipHandle == nullptr ||
            zipHandle == INVALID_HANDLE_VALUE || zipSize == 0U ||
            zipSize > static_cast<std::uint64_t>(
                          (std::numeric_limits<LONGLONG>::max)())) {
            return false;
        }
        payloadFiles->clear();
        std::uint64_t totalExpectedBytes = 0U;
        for (const auto& item : binding.approvedArchiveItems()) {
            if (item.byteSize > kMaximumExtractedPayloadBytes ||
                totalExpectedBytes > kMaximumExtractedPayloadBytes -
                    item.byteSize) {
                return false;
            }
            totalExpectedBytes += item.byteSize;
        }
        ZipInputContext input{zipHandle, zipSize, true};
        mz_zip_archive archive{};
        archive.m_pRead = ReadZipChunk;
        archive.m_pIO_opaque = &input;
        if (!mz_zip_reader_init(&archive, zipSize, 0)) return false;
        bool ok = true;
        std::uint64_t totalWrittenBytes = 0U;
        for (const auto& item : binding.approvedArchiveItems()) {
            const std::string& name = item.relativePath;
            const auto components = SafeRelativeComponents(name);
            if (!components.has_value()) {
                ok = false;
                break;
            }
            const std::string relativePath = [&] {
                std::string normalized = name;
                std::replace(normalized.begin(), normalized.end(), '\\', '/');
                return normalized;
            }();
            if (std::any_of(
                    payloadFiles->begin(), payloadFiles->end(),
                    [&](const PayloadFile& file) {
                        return NormalizeProtectedRoot(file.relativePath) ==
                               NormalizeProtectedRoot(relativePath);
                    })) {
                ok = false;
                break;
            }
            const std::size_t parentCount = components->size() - 1U;
            ScopedNativeHandle parent;
            if (parentCount == 0U) {
                parent.reset();
            } else {
                auto openedParent = OpenRelativeDirectoryPath(
                    stagingRoot, *components, parentCount);
                if (!openedParent.has_value()) {
                    ok = false;
                    break;
                }
                parent = std::move(*openedParent);
            }
            HANDLE parentHandle = parentCount == 0U ? stagingRoot : parent.get();
            auto output = OpenRelativeRegularFile(
                parentHandle, components->back(), kFileCreate);
            if (!output.has_value()) {
                ok = false;
                break;
            }
            ZipOutputContext context{output->get(), item.byteSize, 0U, true};
            if (!mz_zip_reader_extract_file_to_callback(
                    &archive, name.c_str(), WriteZipChunk, &context, 0U) ||
                !context.ok || context.nextOffset != item.byteSize ||
                totalWrittenBytes > kMaximumExtractedPayloadBytes -
                    context.nextOffset ||
                !FlushFileBuffers(output->get())) {
                DeleteFileByHandle(output->get());
                ok = false;
                break;
            }
            totalWrittenBytes += context.nextOffset;
            const auto identity = ObservePayloadFileHandle(
                output->get(), relativePath);
            if (!identity.has_value() || identity->size != item.byteSize ||
                identity->sha256 != item.sha256) {
                DeleteFileByHandle(output->get());
                ok = false;
                break;
            }
            payloadFiles->push_back(*identity);
        }
        const bool complete = ok && input.ok &&
            totalWrittenBytes == totalExpectedBytes;
        mz_zip_reader_end(&archive);
        return complete;
    }

    static AuthorityState PrepareLaunch(AuthorityState state) {
        const nlohmann::json invocation = {
            {"entrypoint", state.invocation.entrypoint},
            {"existing_inventory", [&] {
                nlohmann::json values = nlohmann::json::array();
                for (const auto& item : state.invocation.existingInventory) {
                    values.push_back({{"relative_path", item.relativePath}});
                }
                return values;
            }()},
            {"health_nonce", state.invocation.healthNonce},
            {"deployment_mode", state.invocation.deploymentMode ==
                aris::platform::DeploymentMode::Installed ? "installed" : "portable"},
            {"installation_id", state.invocation.installationId},
            {"invocation_identity", state.invocation.invocationIdentity},
            {"old_process", IdentityJson(state.invocation.oldProcess)},
            {"old_executable_identity", DeploymentFileIdentityJson(
                state.deployment.oldExecutable)},
            {"old_process_id", state.invocation.oldProcessId},
            {"package_plan_digest", state.invocation.packagePlanDigest},
            {"parent_process", IdentityJson(state.invocation.parentProcess)},
            {"previous_failure_count", 0U},
            {"protected_data_roots", state.invocation.protectedDataRoots},
            {"registered_app_id", state.deployment.appId},
            {"staging_root", state.invocation.stagingRoot},
            {"target_root", state.invocation.targetRoot},
            {"target_root_identity", DeploymentFileIdentityJson(
                state.deployment.openedRootIdentity)},
            {"transaction_id", state.invocation.transactionId},
            {"transaction_root", state.invocation.transactionRoot},
            {"updater_owned_base", state.invocation.updaterOwnedBase},
        };
        state.handoff = nlohmann::json{
            {"invocation", invocation.dump()},
            {"package_plan", state.package},
            {"package_signature", state.signature},
        }.dump();
        state.ready = true;
        return state;
    }

    static bool WriteAll(HANDLE handle, const std::string& bytes) {
        std::size_t offset = 0U;
        while (offset < bytes.size()) {
            DWORD written = 0U;
            const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
                bytes.size() - offset, 64U * 1024U));
            if (!WriteFile(handle, bytes.data() + offset, request,
                           &written, nullptr) || written == 0U) {
                return false;
            }
            offset += written;
        }
        return true;
    }

    static bool CaptureProcessIdentity(
        HANDLE process, bootstrap::BootstrapProcessIdentity& identity) {
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) {
            return false;
        }
        ULARGE_INTEGER creation{};
        creation.LowPart = created.dwLowDateTime;
        creation.HighPart = created.dwHighDateTime;
        std::vector<wchar_t> path(512U, L'\0');
        DWORD length = static_cast<DWORD>(path.size());
        while (!QueryFullProcessImageNameW(process, 0U, path.data(),
                                            &length)) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
                path.size() >= 32768U) return false;
            path.resize(path.size() * 2U);
            length = static_cast<DWORD>(path.size());
        }
        const std::wstring requested(path.data(), length);
        HANDLE image = CreateFileW(
            requested.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (image == INVALID_HANDLE_VALUE) return false;
        FILE_ATTRIBUTE_TAG_INFO tags{};
        FILE_ID_INFO fileId{};
        std::vector<wchar_t> canonicalBuffer(512U, L'\0');
        DWORD canonicalLength = GetFinalPathNameByHandleW(
            image, canonicalBuffer.data(),
            static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        if (canonicalLength >= canonicalBuffer.size()) {
            canonicalBuffer.resize(canonicalLength + 1U);
            canonicalLength = GetFinalPathNameByHandleW(
                image, canonicalBuffer.data(),
                static_cast<DWORD>(canonicalBuffer.size()), FILE_NAME_NORMALIZED);
        }
        const bool ok = GetFileInformationByHandleEx(
                            image, FileAttributeTagInfo, &tags,
                            sizeof(tags)) != FALSE &&
                        GetFileInformationByHandleEx(
                            image, FileIdInfo, &fileId,
                            sizeof(fileId)) != FALSE &&
                        canonicalLength != 0U &&
                        (tags.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
        CloseHandle(image);
        if (!ok) return false;
        std::wstring portable(canonicalBuffer.data(), canonicalLength);
        if (portable.rfind(L"\\\\?\\", 0U) == 0U) portable.erase(0U, 4U);
        identity.processId = GetProcessId(process);
        identity.creationTime = creation.QuadPart;
        identity.imagePath = std::filesystem::path(portable).u8string();
        identity.imageVolumeSerial = fileId.VolumeSerialNumber;
        std::copy(std::begin(fileId.FileId.Identifier),
                  std::end(fileId.FileId.Identifier), identity.imageFileId.begin());
        identity.valid = identity.processId != 0U &&
                         identity.creationTime != 0U &&
                         !identity.imagePath.empty();
        return identity.valid;
    }

    static bool StartLaunchBroker(
        const std::string& executable,
        HANDLE& channel, HANDLE& process, HANDLE& job,
        bootstrap::BootstrapProcessIdentity& identity,
        const RuntimePairFile& expectedImage) {
        const std::wstring pipeName =
            L"\\\\.\\pipe\\aris-bootstrap-production-broker-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64());
        channel = CreateNamedPipeW(
            pipeName.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
                PIPE_REJECT_REMOTE_CLIENTS,
            1U, 64U * 1024U, 64U * 1024U, 0U, nullptr);
        if (channel == INVALID_HANDLE_VALUE) return false;
        SECURITY_ATTRIBUTES inherited{};
        inherited.nLength = sizeof(inherited);

        inherited.bInheritHandle = TRUE;
        HANDLE brokerPipe = CreateFileW(
            pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0U,
            &inherited, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (brokerPipe == INVALID_HANDLE_VALUE ||
            (ConnectNamedPipe(channel, nullptr) == FALSE &&
             GetLastError() != ERROR_PIPE_CONNECTED) ||
            !SetHandleInformation(channel, HANDLE_FLAG_INHERIT,
                                   HANDLE_FLAG_INHERIT)) {
            if (brokerPipe != INVALID_HANDLE_VALUE) CloseHandle(brokerPipe);
            CloseHandle(channel); channel = INVALID_HANDLE_VALUE;
            return false;
        }
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        if (job == nullptr ||
            !SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                     &limits, sizeof(limits)) ||
            !SetHandleInformation(job, HANDLE_FLAG_INHERIT,
                                  HANDLE_FLAG_INHERIT)) {
            if (job != nullptr) CloseHandle(job);
            CloseHandle(brokerPipe); CloseHandle(channel);
            channel = INVALID_HANDLE_VALUE; job = nullptr;
            return false;
        }
        SIZE_T attributeBytes = 0U;
        static_cast<void>(InitializeProcThreadAttributeList(
            nullptr, 2U, 0U, &attributeBytes));
        std::vector<std::uint8_t> attributes(attributeBytes);
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = brokerPipe;
        startup.StartupInfo.hStdOutput = brokerPipe;
        startup.StartupInfo.hStdError = brokerPipe;
        startup.lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        HANDLE brokerJob = job;
        if (attributeBytes == 0U ||
            !InitializeProcThreadAttributeList(startup.lpAttributeList, 2U,
                                               0U, &attributeBytes) ||
            !UpdateProcThreadAttribute(
                startup.lpAttributeList, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                &brokerPipe, sizeof(brokerPipe), nullptr, nullptr) ||
            !UpdateProcThreadAttribute(
                startup.lpAttributeList, 0U, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                &brokerJob, sizeof(brokerJob), nullptr, nullptr)) {
            if (startup.lpAttributeList != nullptr) {
                DeleteProcThreadAttributeList(startup.lpAttributeList);
            }
            CloseHandle(job); CloseHandle(brokerPipe); CloseHandle(channel);
            channel = INVALID_HANDLE_VALUE; job = nullptr;
            return false;
        }
        std::wstring command = L"\"" + std::filesystem::path(
            std::filesystem::path(executable)).wstring() +
            L"\" --aris-bootstrap-launch-broker";
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        PROCESS_INFORMATION info{};
        const BOOL launched = CreateProcessW(
            std::filesystem::path(executable).wstring().c_str(),
            mutableCommand.data(), nullptr, nullptr, TRUE,
            CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT |
                CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
            nullptr, nullptr, &startup.StartupInfo, &info);
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        CloseHandle(brokerPipe);
        if (!launched) {
            CloseHandle(job); CloseHandle(channel);
            channel = INVALID_HANDLE_VALUE; job = nullptr;
            return false;
        }
        const bool captured = CaptureProcessIdentity(info.hProcess, identity) &&
            SameProcessImage(identity, expectedImage) &&
            ResumeThread(info.hThread) != static_cast<DWORD>(-1);
        CloseHandle(info.hThread);
        if (!captured) {
            TerminateProcess(info.hProcess, 1U);
            CloseHandle(info.hProcess); CloseHandle(job); CloseHandle(channel);
            channel = INVALID_HANDLE_VALUE; job = nullptr;
            return false;
        }
        process = info.hProcess;
        return true;
    }

    static bool LaunchBootstrap(const AuthorityState& state) {
        HANDLE brokerChannel = INVALID_HANDLE_VALUE;
        HANDLE brokerProcess = nullptr;
        HANDLE brokerJob = nullptr;
        bootstrap::BootstrapProcessIdentity brokerIdentity;
        if (!StartLaunchBroker(state.bootstrapPath, brokerChannel,
                               brokerProcess, brokerJob, brokerIdentity,
                               state.launchCopyIdentity)) {
            return false;
        }
        nlohmann::json handoff = nlohmann::json::parse(
            state.handoff, nullptr, false);
        if (handoff.is_discarded() || !handoff.is_object()) {
            CloseHandle(brokerChannel);
            CloseHandle(brokerProcess);
            CloseHandle(brokerJob);
            return false;
        }
        handoff["launch_broker"] = {
            {"channel_handle", reinterpret_cast<std::uintptr_t>(brokerChannel)},
            {"containment_job_handle",
             reinterpret_cast<std::uintptr_t>(brokerJob)},
            {"launch_timeout_ms", 5000U},
            {"process_handle", reinterpret_cast<std::uintptr_t>(brokerProcess)},
            {"process_identity", IdentityJson(brokerIdentity)},
        };
        const std::string handoffBytes = handoff.dump();
        const std::wstring pipeName =
            L"\\\\.\\pipe\\aris-bootstrap-production-handoff-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(GetTickCount64());
        HANDLE server = CreateNamedPipeW(
            pipeName.c_str(), PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1U,
            2U * 1024U * 1024U, 2U * 1024U * 1024U, 5000U, nullptr);
        SECURITY_ATTRIBUTES inherited{};
        inherited.nLength = sizeof(inherited);
        inherited.bInheritHandle = TRUE;
        HANDLE client = server == INVALID_HANDLE_VALUE
            ? INVALID_HANDLE_VALUE
            : CreateFileW(pipeName.c_str(), GENERIC_READ, 0U,
                          &inherited, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
        HANDLE parent = INVALID_HANDLE_VALUE;
        if (client != INVALID_HANDLE_VALUE) {
            SetHandleInformation(client, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(),
                            GetCurrentProcess(), &parent,
                            PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                            TRUE, 0U);
        }
        HANDLE diagnosticRead = INVALID_HANDLE_VALUE;
        HANDLE diagnosticWrite = INVALID_HANDLE_VALUE;
        if (client == INVALID_HANDLE_VALUE || parent == INVALID_HANDLE_VALUE ||
            !CreatePipe(&diagnosticRead, &diagnosticWrite, &inherited, 0U) ||
            !SetHandleInformation(diagnosticRead, HANDLE_FLAG_INHERIT, 0U)) {
            if (diagnosticRead != INVALID_HANDLE_VALUE) CloseHandle(diagnosticRead);
            if (diagnosticWrite != INVALID_HANDLE_VALUE) CloseHandle(diagnosticWrite);
            if (parent != INVALID_HANDLE_VALUE) CloseHandle(parent);
            if (client != INVALID_HANDLE_VALUE) CloseHandle(client);
            if (server != INVALID_HANDLE_VALUE) CloseHandle(server);
            CloseHandle(brokerChannel); CloseHandle(brokerProcess); CloseHandle(brokerJob);
            return false;
        }
        SetHandleInformation(brokerChannel, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(brokerProcess, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(brokerJob, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SIZE_T attributeBytes = 0U;
        static_cast<void>(InitializeProcThreadAttributeList(
            nullptr, 1U, 0U, &attributeBytes));
        std::vector<std::uint8_t> attributes(attributeBytes);
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = client;
        startup.StartupInfo.hStdOutput = parent;
        startup.StartupInfo.hStdError = diagnosticWrite;
        startup.lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        const std::array<HANDLE, 6U> inheritedHandles{
            client, parent, diagnosticWrite, brokerChannel,
            brokerProcess, brokerJob};
        if (attributeBytes == 0U ||
            !InitializeProcThreadAttributeList(startup.lpAttributeList, 1U,
                                               0U, &attributeBytes) ||
            !UpdateProcThreadAttribute(
                startup.lpAttributeList, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                const_cast<HANDLE*>(inheritedHandles.data()),
                sizeof(inheritedHandles), nullptr, nullptr)) {
            if (startup.lpAttributeList != nullptr) {
                DeleteProcThreadAttributeList(startup.lpAttributeList);
            }
            CloseHandle(diagnosticRead); CloseHandle(diagnosticWrite);
            CloseHandle(parent); CloseHandle(client); CloseHandle(server);
            CloseHandle(brokerChannel); CloseHandle(brokerProcess); CloseHandle(brokerJob);
            return false;
        }
        std::wstring command = std::filesystem::path(state.bootstrapPath).wstring();
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        PROCESS_INFORMATION info{};
        const BOOL launched = CreateProcessW(
            command.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT |
                CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, std::filesystem::path(
                    state.invocation.targetRoot).wstring().c_str(),
            &startup.StartupInfo, &info);
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        CloseHandle(client); CloseHandle(parent); CloseHandle(diagnosticWrite);
        CloseHandle(brokerChannel); CloseHandle(brokerProcess); CloseHandle(brokerJob);
        if (!launched) {
            CloseHandle(diagnosticRead); CloseHandle(server);
            return false;
        }
        bootstrap::BootstrapProcessIdentity childIdentity;
        const bool imageAccepted =
            CaptureProcessIdentity(info.hProcess, childIdentity) &&
            SameProcessImage(childIdentity, state.launchCopyIdentity);
        const bool wrote = imageAccepted && WriteAll(server, handoffBytes);
        const bool resumed = wrote &&
            ResumeThread(info.hThread) != static_cast<DWORD>(-1);
        if (!resumed) TerminateProcess(info.hProcess, 1U);
        CloseHandle(server);
        CloseHandle(diagnosticRead);
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
        return resumed;
    }
};

}  // namespace

CreateResult CreateUpdateBootstrapAuthorityPort() {
    auto port = std::make_unique<Authority>();
    return {CreateStatus::Success, std::move(port), 0U};
}

}  // namespace Platform::UpdateBootstrapAuthority

#endif
