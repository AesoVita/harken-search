#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aris::update {

// @stage7-contract
// API contract: these are owned domain values only. They carry no external
// handles, execute no effects, and may be copied across composition boundaries.

enum class UpdatePhase {
    Idle,
    Checking,
    Available,
    Downloading,
    Ready,
    Superseding,
    Error,
};

struct UpdateCandidate {
    std::string version;
    std::string buildId;
    std::string assetUrl;
    std::uint64_t assetSize = 0;
    std::string sha256;
};

struct UpdateState {
    UpdatePhase phase = UpdatePhase::Idle;
    std::optional<UpdateCandidate> candidate;
    std::optional<UpdateCandidate> readyPatch;
    std::uint64_t downloadedBytes = 0;
    std::string lastError;
    bool restartConfirmed = false;
};

// Owned primitives only: composition may serialize this value, but the update
// domain performs no filesystem or database persistence itself.
struct UpdateSnapshot {
    int schemaVersion = 1;
    UpdatePhase phase = UpdatePhase::Idle;
    std::optional<UpdateCandidate> candidate;
    std::optional<UpdateCandidate> readyPatch;
    std::uint64_t downloadedBytes = 0;
    std::string lastError;
    bool restartConfirmed = false;
};

enum class UpdateEffectKind {
    CheckForUpdate,
    StartDownload,
    ResumeDownload,
    RestartAndInstall,
    PersistSnapshot,
};

struct UpdateEffect {
    UpdateEffectKind kind = UpdateEffectKind::PersistSnapshot;
    std::optional<UpdateCandidate> candidate;
    std::uint64_t resumeOffset = 0;
};

struct UpdateTransition {
    UpdateState state;
    std::vector<UpdateEffect> effects;
};

}  // namespace aris::update
