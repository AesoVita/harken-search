#include "update/UpdateCoordinator.h"

#include <cstdio>
#include <string>

namespace {

using aris::update::UpdateCandidate;
using aris::update::UpdateCoordinator;
using aris::update::UpdateEffectKind;
using aris::update::UpdateEvent;
using aris::update::UpdateEventKind;
using aris::update::UpdatePhase;
using aris::update::UpdateState;

static_assert(static_cast<int>(UpdateEventKind::CheckRequested) == 0);
static_assert(static_cast<int>(UpdateEventKind::CheckSucceeded) == 1);
static_assert(static_cast<int>(UpdateEventKind::CheckFailed) == 2);
static_assert(static_cast<int>(UpdateEventKind::DownloadRequested) == 3);
static_assert(static_cast<int>(UpdateEventKind::DownloadProgressed) == 4);
static_assert(static_cast<int>(UpdateEventKind::DownloadCompleted) == 5);
static_assert(static_cast<int>(UpdateEventKind::DownloadFailed) == 6);
static_assert(static_cast<int>(UpdateEventKind::DownloadCancelled) == 7);
static_assert(static_cast<int>(UpdateEventKind::RestartConfirmed) == 8);
static_assert(static_cast<int>(UpdateEventKind::CheckCompletedNoUpdate) == 9);
static_assert(static_cast<int>(UpdateEventKind::DownloadCandidateAccepted) == 10);

int failures = 0;

void check(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

UpdateCandidate candidate(const char* version, const char* buildId) {
    UpdateCandidate value;
    value.version = version;
    value.buildId = buildId;
    value.assetUrl = std::string("https://example.invalid/") + buildId + ".zip";
    value.assetSize = 1000;
    value.sha256 = std::string(64, 'a');
    return value;
}

bool sameCandidate(const UpdateCandidate& left, const UpdateCandidate& right) {
    return left.version == right.version && left.buildId == right.buildId &&
           left.assetUrl == right.assetUrl && left.assetSize == right.assetSize &&
           left.sha256 == right.sha256;
}

bool hasEffect(const aris::update::UpdateTransition& transition,
               UpdateEffectKind kind) {
    for (const auto& effect : transition.effects) {
        if (effect.kind == kind) return true;
    }
    return false;
}

int effectCount(const aris::update::UpdateTransition& transition,
                UpdateEffectKind kind) {
    int count = 0;
    for (const auto& effect : transition.effects) {
        if (effect.kind == kind) ++count;
    }
    return count;
}

UpdateState makeReady(const UpdateCandidate& patch) {
    UpdateState state;
    state.phase = UpdatePhase::Ready;
    state.readyPatch = patch;
    return state;
}

void test_basic_update_lifecycle_and_restart_confirmation() {
    UpdateState state;
    auto transition = UpdateCoordinator::Reduce(state, UpdateEvent::CheckRequested());
    check(transition.state.phase == UpdatePhase::Checking,
          "idle check enters checking");
    check(hasEffect(transition, UpdateEffectKind::CheckForUpdate),
          "check emits a check intent");

    const auto patch = candidate("1.2.0", "build-120");
    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::CheckSucceeded(patch));
    check(transition.state.phase == UpdatePhase::Available,
          "successful check enters available");
    check(transition.state.candidate.has_value() &&
              sameCandidate(*transition.state.candidate, patch),
          "available state owns candidate metadata");

    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::DownloadRequested());
    check(transition.state.phase == UpdatePhase::Downloading,
          "download request enters downloading");
    check(hasEffect(transition, UpdateEffectKind::StartDownload),
          "download request emits a start intent");

    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::DownloadProgressed(400));
    check(transition.state.downloadedBytes == 400,
          "download progress is deterministic state");

    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::DownloadCompleted());
    check(transition.state.phase == UpdatePhase::Ready,
          "completed download enters ready");
    check(transition.state.readyPatch.has_value() &&
              sameCandidate(*transition.state.readyPatch, patch),
          "ready state owns staged patch metadata");

    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::RestartConfirmed());
    check(transition.state.phase == UpdatePhase::Ready,
          "restart confirmation retains ready state until composition acts");
    check(transition.state.restartConfirmed,
          "restart confirmation is recorded deterministically");
    check(hasEffect(transition, UpdateEffectKind::RestartAndInstall),
          "restart confirmation emits install intent");
}

void test_failed_superseding_check_preserves_ready_patch() {
    const auto readyPatch = candidate("1.2.0", "build-120");
    auto transition = UpdateCoordinator::Reduce(
        makeReady(readyPatch), UpdateEvent::CheckRequested());
    check(transition.state.phase == UpdatePhase::Superseding,
          "checking with a ready patch enters superseding");
    check(transition.state.readyPatch.has_value() &&
              sameCandidate(*transition.state.readyPatch, readyPatch),
          "superseding check retains the ready patch");

    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::CheckFailed("manifest unavailable"));
    check(transition.state.phase == UpdatePhase::Ready,
          "failed superseding check returns to ready");
    check(transition.state.readyPatch.has_value() &&
              sameCandidate(*transition.state.readyPatch, readyPatch),
          "failed superseding check preserves ready metadata");
    check(transition.state.lastError == "manifest unavailable",
          "failed superseding check retains diagnostic detail");
}

void test_explicit_check_completion_and_no_update_outcomes() {
    const auto readyPatch = candidate("1.2.0", "build-120");
    const auto newerPatch = candidate("1.3.0", "build-130");
    auto ready = makeReady(readyPatch);
    ready.downloadedBytes = readyPatch.assetSize;

    auto superseding = UpdateCoordinator::Reduce(
        ready, UpdateEvent::CheckRequested());
    auto checked = UpdateCoordinator::Reduce(
        superseding.state, UpdateEvent::CheckSucceeded(newerPatch));
    check(checked.state.phase == UpdatePhase::Ready &&
              checked.state.readyPatch.has_value() &&
              sameCandidate(*checked.state.readyPatch, readyPatch) &&
              !checked.state.candidate &&
              checked.state.downloadedBytes == ready.downloadedBytes,
          "superseding check completion keeps the old ready patch until download admission");
    check(!hasEffect(checked, UpdateEffectKind::CheckForUpdate),
          "check completion does not emit a duplicate check intent");

    const auto accepted = UpdateCoordinator::Reduce(
        checked.state, UpdateEvent::DownloadCandidateAccepted(newerPatch));
    check(accepted.state.phase == UpdatePhase::Downloading &&
              accepted.state.candidate.has_value() &&
              sameCandidate(*accepted.state.candidate, newerPatch) &&
              accepted.state.readyPatch.has_value() &&
              sameCandidate(*accepted.state.readyPatch, readyPatch),
          "download admission atomically accepts the checked candidate and keeps fallback ready");
    check(effectCount(accepted, UpdateEffectKind::StartDownload) == 1,
          "checked candidate admission emits one download effect");

    auto checking = UpdateCoordinator::Reduce(
        UpdateState{}, UpdateEvent::CheckRequested());
    const auto idle = UpdateCoordinator::Reduce(
        checking.state, UpdateEvent::CheckCompletedNoUpdate());
    check(idle.state.phase == UpdatePhase::Idle && !idle.state.candidate &&
              !idle.state.readyPatch && idle.state.lastError.empty(),
          "no-update completion returns an ordinary check to idle");

    superseding = UpdateCoordinator::Reduce(
        ready, UpdateEvent::CheckRequested());
    const auto unchangedReady = UpdateCoordinator::Reduce(
        superseding.state, UpdateEvent::CheckCompletedNoUpdate());
    check(unchangedReady.state.phase == UpdatePhase::Ready &&
              unchangedReady.state.readyPatch.has_value() &&
              sameCandidate(*unchangedReady.state.readyPatch, readyPatch) &&
              unchangedReady.state.downloadedBytes == ready.downloadedBytes,
          "no-update completion restores identical ready metadata");
}

void test_restarted_active_check_emits_one_new_check_effect() {
    auto first = UpdateCoordinator::Reduce(
        UpdateState{}, UpdateEvent::CheckRequested());
    const auto second = UpdateCoordinator::Reduce(
        first.state, UpdateEvent::CheckRequested());
    check(second.state.phase == UpdatePhase::Checking,
          "restarted active check remains checking");
    check(effectCount(second, UpdateEffectKind::CheckForUpdate) == 1,
          "restarted active check emits exactly one new check effect");
}

void test_check_failure_without_ready_patch_enters_error() {
    auto transition = UpdateCoordinator::Reduce(
        UpdateState{}, UpdateEvent::CheckRequested());
    transition = UpdateCoordinator::Reduce(
        transition.state, UpdateEvent::CheckFailed("offline"));
    check(transition.state.phase == UpdatePhase::Error,
          "ordinary check failure enters error");
    check(transition.state.lastError == "offline",
          "error state owns diagnostic detail");
}

void test_snapshot_round_trip_and_interrupted_download_recovery() {
    UpdateState downloading;
    downloading.phase = UpdatePhase::Downloading;
    downloading.candidate = candidate("1.3.0", "build-130");
    downloading.readyPatch = candidate("1.2.0", "build-120");
    downloading.downloadedBytes = 375;
    downloading.lastError = "";

    const auto snapshot = UpdateCoordinator::Snapshot(downloading);
    const auto restored = UpdateCoordinator::Restore(snapshot);
    check(restored.state.phase == UpdatePhase::Downloading,
          "snapshot restores interrupted downloading phase");
    check(restored.state.candidate.has_value() &&
              sameCandidate(*restored.state.candidate, *downloading.candidate),
          "snapshot round-trip preserves active candidate");
    check(restored.state.readyPatch.has_value() &&
              sameCandidate(*restored.state.readyPatch, *downloading.readyPatch),
          "snapshot round-trip preserves fallback ready patch");
    check(restored.state.downloadedBytes == 375,
          "snapshot round-trip preserves download offset");
    check(hasEffect(restored, UpdateEffectKind::ResumeDownload),
          "restored interrupted download emits resume intent");

    const auto readySnapshot = UpdateCoordinator::Snapshot(
        makeReady(*downloading.readyPatch));
    const auto readyRestored = UpdateCoordinator::Restore(readySnapshot);
    check(readyRestored.state.phase == UpdatePhase::Ready,
          "ready snapshot round-trips as ready");
    check(!hasEffect(readyRestored, UpdateEffectKind::ResumeDownload),
          "ready snapshot does not emit resume intent");
}

void test_interrupted_check_reissues_check_intent() {
    UpdateState checking;
    checking.phase = UpdatePhase::Checking;
    auto restored = UpdateCoordinator::Restore(UpdateCoordinator::Snapshot(checking));
    check(restored.state.phase == UpdatePhase::Checking,
          "checking snapshot retains checking phase");
    check(hasEffect(restored, UpdateEffectKind::CheckForUpdate),
          "restored checking state reissues check intent");

    const auto readyPatch = candidate("1.2.0", "build-120");
    UpdateState superseding = makeReady(readyPatch);
    superseding.phase = UpdatePhase::Superseding;
    restored = UpdateCoordinator::Restore(UpdateCoordinator::Snapshot(superseding));
    check(restored.state.phase == UpdatePhase::Superseding,
          "superseding snapshot retains superseding phase");
    check(restored.state.readyPatch.has_value() &&
              sameCandidate(*restored.state.readyPatch, readyPatch),
          "superseding recovery preserves ready patch");
    check(hasEffect(restored, UpdateEffectKind::CheckForUpdate),
          "restored superseding state reissues check intent");
}

void test_restart_confirmation_is_idempotent() {
    UpdateState ready = makeReady(candidate("1.2.0", "build-120"));
    auto confirmed = UpdateCoordinator::Reduce(ready, UpdateEvent::RestartConfirmed());
    auto repeated = UpdateCoordinator::Reduce(
        confirmed.state, UpdateEvent::RestartConfirmed());
    check(repeated.state.restartConfirmed,
          "duplicate restart confirmation retains confirmation");
    check(!hasEffect(repeated, UpdateEffectKind::RestartAndInstall),
          "duplicate restart confirmation does not repeat install intent");
}

void test_invalid_download_snapshot_fails_deterministically() {
    aris::update::UpdateSnapshot invalid;
    invalid.phase = UpdatePhase::Downloading;
    invalid.downloadedBytes = 900;
    const auto restored = UpdateCoordinator::Restore(invalid);
    check(restored.state.phase == UpdatePhase::Error,
          "downloading snapshot without candidate becomes error");
    check(!hasEffect(restored, UpdateEffectKind::ResumeDownload),
          "invalid downloading snapshot does not emit resume intent");
}

void test_download_failure_and_cancellation_preserve_ready_fallback() {
    const auto oldPatch = candidate("1.2.0", "build-120");
    UpdateState downloading;
    downloading.phase = UpdatePhase::Downloading;
    downloading.candidate = candidate("1.3.0", "build-130");
    downloading.readyPatch = oldPatch;

    const auto failed = UpdateCoordinator::Reduce(
        downloading, UpdateEvent::DownloadFailed("http 404"));
    check(failed.state.phase == UpdatePhase::Ready &&
              failed.state.readyPatch.has_value() &&
              sameCandidate(*failed.state.readyPatch, oldPatch) &&
              !failed.state.candidate,
          "failed superseding download returns to the prior ready patch");

    const auto cancelled = UpdateCoordinator::Reduce(
        downloading, UpdateEvent::DownloadCancelled());
    check(cancelled.state.phase == UpdatePhase::Ready &&
              cancelled.state.readyPatch.has_value() &&
              sameCandidate(*cancelled.state.readyPatch, oldPatch) &&
              !cancelled.state.candidate,
          "cancelled superseding download returns to the prior ready patch");

    downloading.readyPatch.reset();
    const auto ordinaryCancel = UpdateCoordinator::Reduce(
        downloading, UpdateEvent::DownloadCancelled());
    check(ordinaryCancel.state.phase == UpdatePhase::Idle &&
              !ordinaryCancel.state.candidate,
          "cancelled ordinary download returns to idle");
}

}  // namespace

int main() {
    test_basic_update_lifecycle_and_restart_confirmation();
    test_failed_superseding_check_preserves_ready_patch();
    test_explicit_check_completion_and_no_update_outcomes();
    test_restarted_active_check_emits_one_new_check_effect();
    test_check_failure_without_ready_patch_enters_error();
    test_snapshot_round_trip_and_interrupted_download_recovery();
    test_interrupted_check_reissues_check_intent();
    test_restart_confirmation_is_idempotent();
    test_invalid_download_snapshot_fails_deterministically();
    test_download_failure_and_cancellation_preserve_ready_fallback();
    return failures == 0 ? 0 : 1;
}
