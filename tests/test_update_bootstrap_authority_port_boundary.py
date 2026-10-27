from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/platform/update_bootstrap_authority_port.h"
FALLBACK = ROOT / "src/platform/update_bootstrap_authority_unsupported.cpp"
WIN32 = ROOT / "src/platform/update_bootstrap_authority_win32.cpp"
IDENTITY = ROOT / "src/platform/update_deployment_identity.h"


def test_bootstrap_authority_factory_has_owned_neutral_fallback_contract() -> None:
    assert HEADER.is_file(), "neutral Bootstrap Authority factory header is required"
    assert FALLBACK.is_file(), "non-Windows Bootstrap Authority fallback is required"

    header = HEADER.read_text(encoding="utf-8")
    fallback = FALLBACK.read_text(encoding="utf-8")

    assert '#include "update/UpdateApplicationService.h"' in header
    assert "namespace Platform::UpdateBootstrapAuthority" in header
    for status in ("Success", "Unsupported", "RootUnavailable", "PolicyUnavailable"):
        assert status in header
    assert "std::unique_ptr<aris::update::UpdateBootstrapAuthorityPort> port" in header
    assert "std::uint32_t systemError" in header
    assert "bool ok() const" in header
    assert "CreateUpdateBootstrapAuthorityPort()" in header

    assert "CreateUpdateBootstrapAuthorityPort()" in fallback
    assert "CreateStatus::Unsupported" in fallback
    assert "nullptr" in fallback

    for forbidden in ("windows.h", "Windows.h", "HANDLE", "std::filesystem", "nlohmann"):
        assert forbidden not in header


def test_bootstrap_authority_factory_is_registered_in_product_sources_and_boundary_ctest() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    assert "src/platform/update_bootstrap_authority_unsupported.cpp" in cmake
    assert "src/platform/update_bootstrap_authority_win32.cpp" in cmake
    assert behavior.count("NAME TestUpdateBootstrapAuthorityPortBoundary") == 1
    assert "tests/test_update_bootstrap_authority_port_boundary.py" in behavior


def test_win32_bootstrap_authority_owns_the_platform_implementation() -> None:
    source = WIN32.read_text(encoding="utf-8")
    assert "class Authority final" in source
    assert "CreateStatus::Success" in source
    assert "Stage6-C2 placeholder" not in source
    for required in (
        "CreateNamedPipeW",
        "CreateProcessW",
        "SetSecurityInfo",
        "BCryptGenRandom",
        "mz_zip_reader_init",
    ):
        assert required in source


def test_updater_directories_are_owned_by_the_current_user() -> None:
    source = WIN32.read_text(encoding="utf-8")
    start = source.index("static bool SecureUpdaterDirectoryImpl")
    end = source.index("PrepareUpdaterDirectories(", start)
    secure_directory = source[start:end]

    assert "OWNER_SECURITY_INFORMATION" in secure_directory
    assert "user->User.Sid" in secure_directory
    assert "SetSecurityInfo" in secure_directory


def test_updater_root_traversal_does_not_require_ancestor_mutation_rights() -> None:
    source = WIN32.read_text(encoding="utf-8")
    start = source.index("static std::optional<ScopedNativeHandle> OpenPinnedAbsoluteDirectory")
    end = source.index("struct PinnedUpdaterDirectories", start)
    traversal = source[start:end]

    # A normal Aris process may traverse C:\\Users but cannot change its ACL.
    # Only the final updater-owned root may request a writable security handle.
    assert "const bool finalComponent = separator == std::wstring::npos;" in traversal
    assert "finalComponent && createMissing" in traversal


def test_authority_pins_updater_roots_and_extracts_without_following_reparse_points() -> None:
    source = WIN32.read_text(encoding="utf-8")
    secure_start = source.index("static bool SecureUpdaterDirectoryImpl")
    secure_end = source.index("PrepareUpdaterDirectories(", secure_start)
    secure_directory = source[secure_start:secure_end]
    extract_start = source.index("static bool ExtractPayload")
    extract_end = source.index("static AuthorityState PrepareLaunch", extract_start)
    extraction = source[extract_start:extract_end]

    assert "NtCreateFile" in source
    assert "OBJ_DONT_REPARSE" in source
    assert "SetSecurityInfo" in secure_directory
    assert "PinnedUpdaterDirectories" in source
    assert "mz_zip_reader_extract_file_to_callback" in extraction
    assert "CreateFileW" not in extraction
    assert "std::filesystem::create_directories" not in extraction


def test_deployment_identity_admission_replaces_legacy_installation_fallback() -> None:
    source = WIN32.read_text(encoding="utf-8")
    header = (IDENTITY.read_text(encoding="utf-8") +
              (ROOT / "src/bootstrap/HarkenSearchBootstrapPlan.h").read_text(encoding="utf-8"))
    assert '"aris-install"' not in source
    assert "ObserveDeploymentIdentity" in source
    assert "ResolveDeploymentIdentity" in source
    assert "consumed->exchange" in source
    import re
    installer = (ROOT / "installer/HarkenSearch_Setup.iss").read_text(encoding="utf-8")
    installed_id = re.search(r'kArisInstalledAppId\s*=\s*"\{([^}]+)\}"', header)
    setup_id = re.search(r'#define MyAppId "\{\{([^}]+)\}\}"', installer)
    assert installed_id and setup_id
    assert installed_id.group(1) == setup_id.group(1)
    assert installed_id.group(1) == "31B6E215-3D2B-43EA-88C8-72D73ABC9A0C"
    assert "Installed" in header and "Portable" in header


def test_product_policy_and_two_protected_roots_precede_payload_extraction() -> None:
    source = WIN32.read_text(encoding="utf-8")
    policy = source.index("EvaluateHarkenSearchPackagePolicy")
    protected_roots = source.index("ResolveProtectedDataRoots")
    extraction = source.index("ExtractPayload(pinnedArchive")
    capability = source.index("IssueCapability(state)")

    assert policy < extraction < capability
    assert protected_roots < extraction
    assert "FOLDERID_LocalAppData" in source
    assert "ProtectedDataRootsUtf8" in source
    assert "harken_user_data_layout.h" in source
    assert "invocation.protectedDataRoots = *protectedRoots" in source
    assert "ProtectedRootsOverlap" in source
    assert "UserDataDirectory()" not in source


def test_submit_reobserves_protected_roots_before_consuming_capability() -> None:
    source = WIN32.read_text(encoding="utf-8")
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    root_recheck = source.index("ResolveProtectedDataRoots(", submit)
    consume = source.index("consumed->exchange", submit)

    assert submit < root_recheck < consume
    assert "*protectedRoots != state->invocation.protectedDataRoots" in source


def test_runtime_pair_admission_precedes_extract_and_submit_consumption() -> None:
    source = WIN32.read_text(encoding="utf-8")
    authorize = source.index("UpdateBootstrapAuthorizationResult Authorize")
    sibling = source.index("ObserveRuntimePairFile(", authorize)
    launch_copy = source.index("CopyVerifiedBootstrap(", sibling)
    extraction = source.index("ExtractPayload(pinnedArchive", launch_copy)
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    sibling_recheck = source.index("ObserveRuntimePairFile(", submit)
    copy_recheck = source.index("ObserveRuntimePairFile(", sibling_recheck + 1)
    consume = source.index("consumed->exchange", copy_recheck)

    assert authorize < sibling < launch_copy < extraction
    assert submit < sibling_recheck < copy_recheck < consume
    assert "HarkenSearchRuntimePairIdentity.h" in source
    assert "HashSigningStableBootstrapImage" in source
    assert "kBootstrapImageSha256" in source
    assert "kBootstrapImageSize" in source
    assert "Runtime Pair sibling admission failed" in source
    assert "Runtime Pair changed after authorization" in source
    assert "Runtime Pair launch copy changed after authorization" in source


def test_runtime_pair_failures_use_transaction_scoped_cleanup() -> None:
    source = WIN32.read_text(encoding="utf-8")
    assert "TransactionRuntimePairCleanup" in source
    assert "cleanup.Release()" in source
    for marker in (
        "ExtractPayload(pinnedArchive",
        "ExistingInventory(ExecutableDirectory())",
        "PrepareLaunch(*state)",
        "ResolveDeploymentIdentity(observed)",
        "ResolveProtectedDataRoots(",
        "LaunchBootstrap(*state)",
    ):
        assert marker in source
    assert "RemoveRuntimePairCopy(*state)" in source


def test_submit_claims_exclusive_owner_before_constructing_cleanup() -> None:
    source = WIN32.read_text(encoding="utf-8")
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    submit_body = source[submit:source.index("private:", submit)]

    assert "AcquireSubmitLease" in submit_body
    claim = submit_body.index("AcquireSubmitLease")
    cleanup = submit_body.index("TransactionRuntimePairCleanup cleanup")
    launch = submit_body.index("LaunchBootstrap(*state)")

    assert claim < cleanup < launch
    assert "AlreadyAccepted" in submit_body[:cleanup]
    assert submit_body.find("RemoveRuntimePairCopy(*state)") in (-1, 0) or \
        submit_body.index("AcquireSubmitLease") < submit_body.index(
            "RemoveRuntimePairCopy(*state)")


def test_submit_launch_failure_releases_consumption_for_exact_retry() -> None:
    source = WIN32.read_text(encoding="utf-8")
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    failure = source.index("if (!LaunchBootstrap(*state))", submit)
    failure_body = source[failure:source.index("return result;", failure) + len("return result;")]

    assert "consumed->store(false)" in failure_body
    assert "SubmitState::Ready" in failure_body


def test_runtime_pair_cleanup_is_handle_relative_and_noncreating() -> None:
    source = WIN32.read_text(encoding="utf-8")
    class_start = source.index("class TransactionRuntimePairCleanup")
    class_end = source.index("static std::string ExecutableDirectory", class_start)
    cleanup = source[class_start:class_end]

    assert "OpenPinnedAbsoluteDirectory" in cleanup
    assert "OpenExistingDirectoryChild" in cleanup
    assert "DeleteFileByHandle" in cleanup
    assert "CleanupNow" in cleanup
    assert "std::filesystem::remove" not in cleanup
    assert "OpenOrCreateDirectoryChild" not in cleanup

    remove_start = source.index(
        "static TransactionRuntimePairCleanup::Status RemoveRuntimePairCopy"
    )
    remove_end = source.index("static bool SameProcessImage", remove_start)
    remove = source[remove_start:remove_end]
    assert "CleanupNow" in remove
    assert "std::filesystem::remove" not in remove


def test_runtime_pair_cleanup_failure_stays_retryable_and_is_propagated() -> None:
    source = WIN32.read_text(encoding="utf-8")
    class_start = source.index("class TransactionRuntimePairCleanup")
    class_end = source.index("class InvocationMutationCleanup", class_start)
    cleanup = source[class_start:class_end]
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    submit_end = source.index("private:", submit)
    submit_body = source[submit:submit_end]

    assert "enum class Status" in cleanup
    assert "RetryableFailure" in cleanup
    assert cleanup.rfind("armed_ = false;") > cleanup.index(
        "DeleteFileByHandle(runtimePair.get())"
    )
    assert "runtime-pair-cleanup-pending" in submit_body
    assert "SubmitState::Ready" in submit_body


def test_runtime_pair_cleanup_progress_survives_submit_retry() -> None:
    source = WIN32.read_text(encoding="utf-8")
    state_start = source.index("struct AuthorityState")
    state_end = source.index("static SubmitLeaseResult", state_start)
    state = source[state_start:state_end]
    cleanup_start = source.index("class TransactionRuntimePairCleanup")
    cleanup_end = source.index("class InvocationMutationCleanup", cleanup_start)
    cleanup = source[cleanup_start:cleanup_end]
    remove_start = source.index(
        "static TransactionRuntimePairCleanup::Status RemoveRuntimePairCopy"
    )
    remove_end = source.index("static bool SameProcessImage", remove_start)
    remove = source[remove_start:remove_end]

    assert "struct RuntimePairCleanupControl" in state
    assert "std::shared_ptr<RuntimePairCleanupControl> runtimePairCleanup" in state
    assert "RuntimePairDirectoryIdentity directoryAuthority" in state
    assert "bool fileRemoved" in state
    assert "TerminalMismatch" in cleanup
    assert "control_->fileRemoved" in cleanup
    assert "bool fileRemoved_" not in cleanup
    assert "state.runtimePairCleanup" in remove


def test_verified_package_binding_exposes_signed_inventory_metadata() -> None:
    admission = (ROOT / "src/bootstrap/HarkenSearchBootstrapAdmission.h").read_text(
        encoding="utf-8"
    )
    implementation = (ROOT / "src/bootstrap/HarkenSearchBootstrapAdmission.cpp").read_text(
        encoding="utf-8"
    )

    assert "approvedArchiveItems()" in admission
    assert "archiveItems" in implementation


def test_zip_extraction_uses_a_fixed_handle_and_signed_entry_bounds() -> None:
    source = WIN32.read_text(encoding="utf-8")
    extract_start = source.index("static bool ExtractPayload")
    extract_end = source.index("static AuthorityState PrepareLaunch", extract_start)
    extraction = source[extract_start:extract_end]

    assert "ReadZipChunk" in extraction
    assert "mz_zip_reader_init(" in extraction
    assert "mz_zip_reader_init_file" not in extraction
    assert "approvedArchiveItems()" in extraction
    write_start = source.index("struct ZipOutputContext")
    write_body = source[write_start:extract_start]
    assert "context->expectedSize - offset" in write_body
    assert "context->nextOffset" in write_body
    assert "offset > context->archiveSize" in source


def test_bootstrap_accepts_only_matching_invocation_root_namespaces() -> None:
    source = (ROOT / "src/bootstrap/HarkenSearchBootstrapMain.cpp").read_text(
        encoding="utf-8"
    )
    roots_start = source.index("bool rootsAreDerivedAndNonOverlapping")
    roots_end = source.index("bool invocationProcessesUseTrustedHosts", roots_start)
    roots = source[roots_start:roots_end]

    assert "staging-invocation-" in roots
    assert "transaction-invocation-" in roots
    assert "sameInvocationNamespace" in roots
    assert "expectedUpdaterOwnedBase" in roots
    assert "expectedStaging" not in roots
    assert 'joinRoot(plan.invocation.updaterOwnedBase, "staging")' not in roots
    assert 'joinRoot(plan.invocation.updaterOwnedBase, "transaction")' not in roots


def test_invocation_cleanup_owns_staging_payload_without_recursive_delete() -> None:
    source = WIN32.read_text(encoding="utf-8")
    assert "class InvocationMutationCleanup" in source
    cleanup_start = source.index("class InvocationMutationCleanup")
    cleanup_end = source.index("static std::string ExecutableDirectory", cleanup_start)
    cleanup = source[cleanup_start:cleanup_end]
    for marker in (
        "TrackPayloadFiles",
        "OpenExistingDirectoryPath",
        "SamePayloadFile",
        "DeleteFileByHandle",
        "Release",
    ):
        assert marker in cleanup
    assert "std::filesystem::remove_all" not in cleanup
    assert "std::filesystem::remove" not in cleanup

    authorize = source.index("UpdateBootstrapAuthorizationResult Authorize")
    extract = source.index("ExtractPayload(pinnedArchive", authorize)
    authorize_body = source[authorize:extract]
    assert "InvocationMutationCleanup cleanup" in authorize_body
    assert "TrackPayloadFiles" in source[extract:source.index("bootstrap::InvocationEnvelope", extract)]

    submit = source.index("UpdateBootstrapSubmitResult Submit")
    submit_end = source.index("private:", submit)
    submit_body = source[submit:submit_end]
    assert "InvocationMutationCleanup cleanup" in submit_body
    assert "cleanup.Release()" in submit_body


def test_invocation_cleanup_failure_stays_retryable_and_is_propagated() -> None:
    source = WIN32.read_text(encoding="utf-8")
    cleanup_start = source.index("class InvocationMutationCleanup")
    cleanup_end = source.index("static std::string ExecutableDirectory", cleanup_start)
    cleanup = source[cleanup_start:cleanup_end]

    assert "enum class Status" in cleanup
    assert "RetryableFailure" in cleanup
    assert cleanup.rfind("armed_ = false;") > cleanup.index(
        "DeleteFileByHandle(directory->get())"
    )
    assert "payload-cleanup-pending" in source
    assert "UpdateBootstrapAuthorizationStatus::RetryableFailure" in source


def test_invocation_cleanup_progress_survives_submit_retry_and_binds_directories() -> None:
    source = WIN32.read_text(encoding="utf-8")
    state_start = source.index("struct AuthorityState")
    state_end = source.index("static SubmitLeaseResult", state_start)
    state = source[state_start:state_end]
    cleanup_start = source.index("class InvocationMutationCleanup")
    cleanup_end = source.index("static std::string ExecutableDirectory", cleanup_start)
    cleanup = source[cleanup_start:cleanup_end]
    submit = source.index("UpdateBootstrapSubmitResult Submit")
    submit_end = source.index("private:", submit)
    submit_body = source[submit:submit_end]

    assert "struct InvocationMutationCleanupControl" in state
    assert (
        "std::shared_ptr<InvocationMutationCleanupControl>"
        " invocationMutationCleanup" in state
    )
    assert "control_->fileRemoved" in cleanup
    assert "control_->directoryRemoved" in cleanup
    assert "control_->terminalMismatch" in cleanup
    assert "payloadFiles_.clear()" not in cleanup
    assert "InvocationMutationCleanup cleanup" in submit_body
    assert "state->invocationMutationCleanup" in submit_body


def test_invocation_cleanup_does_not_report_parent_directory_failure_as_cleaned() -> None:
    source = WIN32.read_text(encoding="utf-8")
    cleanup_start = source.index("class InvocationMutationCleanup")
    cleanup_end = source.index("static std::string ExecutableDirectory", cleanup_start)
    cleanup = source[cleanup_start:cleanup_end]
    directory_failure = cleanup.index("DeleteFileByHandle(directory->get())")
    directory_failure_body = cleanup[directory_failure:]

    assert "return Status::RetryableFailure" in directory_failure_body
    assert directory_failure_body.index("return Status::RetryableFailure") < directory_failure_body.index(
        "return Status::Cleaned"
    )
    assert "ObservePayloadDirectoryHandle" in cleanup
    assert "SamePayloadDirectory" in cleanup


def test_zip_extraction_requires_exact_total_output_and_safe_native_bounds() -> None:
    source = WIN32.read_text(encoding="utf-8")
    extract_start = source.index("static bool ExtractPayload")
    extract_end = source.index("static AuthorityState PrepareLaunch", extract_start)
    extraction = source[extract_start:extract_end]
    read_start = source.index("static size_t ReadZipChunk")
    write_start = source.index("static size_t WriteZipChunk")
    read_write = source[read_start:extract_end]

    assert "totalWrittenBytes == totalExpectedBytes" in extraction
    assert "std::numeric_limits<LONGLONG>::max()" in source
    assert "data == nullptr && size != 0U" in read_write


def test_authorize_allocates_isolated_invocation_mutation_namespace() -> None:
    source = WIN32.read_text(encoding="utf-8")
    authorize = source.index("UpdateBootstrapAuthorizationResult Authorize")
    extraction = source.index("ExtractPayload(pinnedArchive", authorize)
    authorize_body = source[authorize:extraction]

    assert "invocationNamespace" in authorize_body
    assert "RandomHex" in authorize_body
    assert 'base + "/staging-"' in authorize_body
    assert 'base + "/transaction-"' in authorize_body
    assert 'const std::string staging = base + "/staging"' not in authorize_body
    assert 'const std::string transaction = base + "/transaction"' not in authorize_body
    assert "PrepareUpdaterDirectories(\n            base, download, staging, transaction)" in authorize_body


def test_authorize_resolves_versioned_staged_relative_path_before_legacy_root() -> None:
    source = WIN32.read_text(encoding="utf-8")
    assert "archive.stagedRelativePath" in source
    assert "IsSafeStagedRelativePath" in source
    assert "OpenVerifiedArchive" in source
    assert "LockFileEx" in source
    assert "UnlockFileEx" in source
    assert "staging / (\"Aris-\" + archive.version" not in source


def test_bootstrap_package_plan_and_sig_are_mapped_to_company_cloud() -> None:
    source = WIN32.read_text(encoding="utf-8")
    authorize = source.index("UpdateBootstrapAuthorizationResult Authorize")
    binding = source.index("VerifyPackagePlanBinding", authorize)
    body = source[authorize:binding]

    # The retired GitHub release prefix must not be the fetched authority URL:
    # the plan is mapped to the company-cloud mirror, and .sig stays bound to it.
    assert "ResolveUpdateDownloadUrl" in body
    assert "planUrl = ResolveUpdateDownloadUrl" in body
    assert 'signatureUrl = planUrl + ".sig"' in body
