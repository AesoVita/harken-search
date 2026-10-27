#include "update/UpdateCoordinator.h"

#include <algorithm>
#include <utility>

namespace aris::update {
namespace {

UpdateEffect Effect(UpdateEffectKind kind,
                    const std::optional<UpdateCandidate>& candidate = std::nullopt,
                    std::uint64_t resumeOffset = 0) {
    UpdateEffect effect;
    effect.kind = kind;
    effect.candidate = candidate;
    effect.resumeOffset = resumeOffset;
    return effect;
}

void Persist(UpdateTransition& transition) {
    transition.effects.push_back(Effect(UpdateEffectKind::PersistSnapshot));
}

void AdmitDownload(UpdateTransition& transition,
                   const UpdateCandidate& candidate) {
    transition.state.phase = UpdatePhase::Downloading;
    transition.state.candidate = candidate;
    transition.state.downloadedBytes = 0;
    transition.state.lastError.clear();
    transition.state.restartConfirmed = false;
    transition.effects.push_back(
        Effect(UpdateEffectKind::StartDownload, candidate));
}

}  // namespace

UpdateEvent UpdateEvent::CheckRequested() {
    return UpdateEvent(UpdateEventKind::CheckRequested);
}

UpdateEvent UpdateEvent::CheckSucceeded(UpdateCandidate candidate) {
    UpdateEvent event(UpdateEventKind::CheckSucceeded);
    event.candidate_ = std::move(candidate);
    return event;
}

UpdateEvent UpdateEvent::CheckCompletedNoUpdate() {
    return UpdateEvent(UpdateEventKind::CheckCompletedNoUpdate);
}

UpdateEvent UpdateEvent::CheckFailed(std::string detail) {
    UpdateEvent event(UpdateEventKind::CheckFailed);
    event.detail_ = std::move(detail);
    return event;
}

UpdateEvent UpdateEvent::DownloadCandidateAccepted(UpdateCandidate candidate) {
    UpdateEvent event(UpdateEventKind::DownloadCandidateAccepted);
    event.candidate_ = std::move(candidate);
    return event;
}

UpdateEvent UpdateEvent::DownloadRequested() {
    return UpdateEvent(UpdateEventKind::DownloadRequested);
}

UpdateEvent UpdateEvent::DownloadProgressed(std::uint64_t downloadedBytes) {
    UpdateEvent event(UpdateEventKind::DownloadProgressed);
    event.downloadedBytes_ = downloadedBytes;
    return event;
}

UpdateEvent UpdateEvent::DownloadCompleted() {
    return UpdateEvent(UpdateEventKind::DownloadCompleted);
}

UpdateEvent UpdateEvent::DownloadFailed(std::string detail) {
    UpdateEvent event(UpdateEventKind::DownloadFailed);
    event.detail_ = std::move(detail);
    return event;
}

UpdateEvent UpdateEvent::DownloadCancelled() {
    return UpdateEvent(UpdateEventKind::DownloadCancelled);
}

UpdateEvent UpdateEvent::RestartConfirmed() {
    return UpdateEvent(UpdateEventKind::RestartConfirmed);
}

bool UpdateCoordinator::CanBeginCheck(UpdatePhase phase) {
    return phase == UpdatePhase::Idle || phase == UpdatePhase::Available ||
           phase == UpdatePhase::Ready || phase == UpdatePhase::Error ||
           phase == UpdatePhase::Checking ||
           phase == UpdatePhase::Superseding;
}

UpdateTransition UpdateCoordinator::Reduce(const UpdateState& state,
                                           const UpdateEvent& event) {
    UpdateTransition transition;
    transition.state = state;

    switch (event.kind()) {
    case UpdateEventKind::CheckRequested:
        if (!CanBeginCheck(state.phase)) return transition;
        transition.state.phase = state.readyPatch ? UpdatePhase::Superseding
                                                  : UpdatePhase::Checking;
        transition.state.candidate.reset();
        transition.state.lastError.clear();
        transition.state.restartConfirmed = false;
        transition.effects.push_back(Effect(UpdateEffectKind::CheckForUpdate));
        break;
    case UpdateEventKind::CheckSucceeded:
        if (state.phase != UpdatePhase::Checking &&
            state.phase != UpdatePhase::Superseding) {
            return transition;
        }
        if (state.phase == UpdatePhase::Superseding) {
            transition.state.phase = UpdatePhase::Ready;
            transition.state.candidate.reset();
        } else {
            transition.state.phase = UpdatePhase::Available;
            transition.state.candidate = event.candidate();
            transition.state.downloadedBytes = 0;
        }
        transition.state.lastError.clear();
        break;
    case UpdateEventKind::CheckCompletedNoUpdate:
        if (state.phase != UpdatePhase::Checking &&
            state.phase != UpdatePhase::Superseding) {
            return transition;
        }
        transition.state.phase = state.readyPatch ? UpdatePhase::Ready
                                                  : UpdatePhase::Idle;
        transition.state.candidate.reset();
        if (!state.readyPatch) transition.state.downloadedBytes = 0;
        transition.state.lastError.clear();
        break;
    case UpdateEventKind::CheckFailed:
        if (state.phase != UpdatePhase::Checking &&
            state.phase != UpdatePhase::Superseding) {
            return transition;
        }
        transition.state.phase = state.readyPatch ? UpdatePhase::Ready
                                                  : UpdatePhase::Error;
        transition.state.candidate.reset();
        transition.state.lastError = event.detail();
        break;
    case UpdateEventKind::DownloadCandidateAccepted:
        if (state.phase == UpdatePhase::Checking ||
            state.phase == UpdatePhase::Superseding) {
            return transition;
        }
        AdmitDownload(transition, event.candidate());
        break;
    case UpdateEventKind::DownloadRequested:
        if (state.phase != UpdatePhase::Available || !state.candidate) {
            return transition;
        }
        AdmitDownload(transition, *state.candidate);
        break;
    case UpdateEventKind::DownloadProgressed:
        if (state.phase != UpdatePhase::Downloading || !state.candidate) {
            return transition;
        }
        transition.state.downloadedBytes =
            std::min(event.downloadedBytes(), state.candidate->assetSize);
        break;
    case UpdateEventKind::DownloadCompleted:
        if (state.phase != UpdatePhase::Downloading || !state.candidate) {
            return transition;
        }
        transition.state.phase = UpdatePhase::Ready;
        transition.state.readyPatch = state.candidate;
        transition.state.downloadedBytes = state.candidate->assetSize;
        transition.state.candidate.reset();
        transition.state.lastError.clear();
        transition.state.restartConfirmed = false;
        break;
    case UpdateEventKind::DownloadFailed:
        if (state.phase != UpdatePhase::Downloading || !state.candidate) {
            return transition;
        }
        transition.state.phase = state.readyPatch ? UpdatePhase::Ready
                                                  : UpdatePhase::Error;
        transition.state.candidate.reset();
        transition.state.downloadedBytes = 0;
        transition.state.lastError = event.detail();
        break;
    case UpdateEventKind::DownloadCancelled:
        if (state.phase != UpdatePhase::Downloading || !state.candidate) {
            return transition;
        }
        transition.state.phase = state.readyPatch ? UpdatePhase::Ready
                                                  : UpdatePhase::Idle;
        transition.state.candidate.reset();
        transition.state.downloadedBytes = 0;
        transition.state.lastError = "download cancelled";
        break;
    case UpdateEventKind::RestartConfirmed:
        if (state.phase != UpdatePhase::Ready || !state.readyPatch ||
            state.restartConfirmed) {
            return transition;
        }
        transition.state.restartConfirmed = true;
        transition.effects.push_back(
            Effect(UpdateEffectKind::RestartAndInstall, state.readyPatch));
        break;
    }

    Persist(transition);
    return transition;
}

UpdateSnapshot UpdateCoordinator::Snapshot(const UpdateState& state) {
    UpdateSnapshot snapshot;
    snapshot.phase = state.phase;
    snapshot.candidate = state.candidate;
    snapshot.readyPatch = state.readyPatch;
    snapshot.downloadedBytes = state.downloadedBytes;
    snapshot.lastError = state.lastError;
    snapshot.restartConfirmed = state.restartConfirmed;
    return snapshot;
}

UpdateTransition UpdateCoordinator::Restore(const UpdateSnapshot& snapshot) {
    UpdateTransition transition;
    if (snapshot.schemaVersion != 1) {
        transition.state.phase = UpdatePhase::Error;
        transition.state.lastError = "unsupported update snapshot schema";
        Persist(transition);
        return transition;
    }

    // Error is the outcome of one process-local update attempt, not a
    // resumable lifecycle state.  A structurally valid Error loaded here came
    // from the previous process, so the successor starts Idle and lets its
    // freshly scheduled check establish current status.  Malformed snapshots
    // still take the schema-error path above, while new failures produced by
    // Reduce in this runtime remain Error.
    if (snapshot.phase == UpdatePhase::Error) {
        transition.state.phase = UpdatePhase::Idle;
        return transition;
    }

    transition.state.phase = snapshot.phase;
    transition.state.candidate = snapshot.candidate;
    transition.state.readyPatch = snapshot.readyPatch;
    transition.state.downloadedBytes = snapshot.downloadedBytes;
    transition.state.lastError = snapshot.lastError;
    transition.state.restartConfirmed = snapshot.restartConfirmed;

    if (snapshot.phase == UpdatePhase::Checking ||
        snapshot.phase == UpdatePhase::Superseding) {
        transition.effects.push_back(Effect(UpdateEffectKind::CheckForUpdate));
    } else if (snapshot.phase == UpdatePhase::Downloading && !snapshot.candidate) {
        transition.state.phase = snapshot.readyPatch ? UpdatePhase::Ready
                                                     : UpdatePhase::Error;
        transition.state.lastError = "invalid interrupted download snapshot";
        transition.state.downloadedBytes = 0;
        Persist(transition);
    } else if (snapshot.phase == UpdatePhase::Downloading) {
        transition.state.downloadedBytes =
            std::min(snapshot.downloadedBytes, snapshot.candidate->assetSize);
        transition.effects.push_back(Effect(UpdateEffectKind::ResumeDownload,
                                            snapshot.candidate,
                                            transition.state.downloadedBytes));
    }
    return transition;
}

}  // namespace aris::update
