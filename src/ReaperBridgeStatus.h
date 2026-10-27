#pragma once

#include "ReaperBridgeProtocol.h"

#include <string>

// @stage7-contract
// API contract: ReaperBridge status evaluation helpers are pure synchronous
// mappings from caller-owned probe facts to enum/string values. They perform
// no installation, transport, or global mutation and preserve the explicit
// compatibility/repair states used by bridge UI and lifecycle code.
namespace HarkenSearch::DawBridge {

enum class InstallState {
    Unconfigured,
    ReaperPathMissing,
    ReaperExecutableMissing,
    NotInstalled,
    ResourcePathUnconfirmed,
    WrongLocation,
    WaitingForHostExit,
    FileInstalled,
    InstalledNotLoaded,
    RestartRequired,
    VersionIncompatible,
    ProtocolIncompatible,
    ArchitectureIncompatible,
    BinaryInvalid,
    UnknownSource,
    HandshakeReady,
    HandshakeFailed,
    BundledComponentMissing,
    Unknown,
};

enum class ReaperBridgeAvailability {
    Unconfigured,
    NeedsRepair,
    WaitingForHostExit,
    HostStopped,
    BridgeOffline,
    Ready,
};

inline ReaperBridgeAvailability EvaluateReaperBridgeAvailability(InstallState state) {
    switch (state) {
    case InstallState::HandshakeReady: return ReaperBridgeAvailability::Ready;
    case InstallState::FileInstalled: return ReaperBridgeAvailability::HostStopped;
    case InstallState::WaitingForHostExit: return ReaperBridgeAvailability::WaitingForHostExit;
    case InstallState::InstalledNotLoaded:
    case InstallState::RestartRequired:
    case InstallState::HandshakeFailed:
        return ReaperBridgeAvailability::BridgeOffline;
    case InstallState::NotInstalled:
    case InstallState::WrongLocation:
    case InstallState::VersionIncompatible:
    case InstallState::ProtocolIncompatible:
    case InstallState::ArchitectureIncompatible:
    case InstallState::BinaryInvalid:
    case InstallState::UnknownSource:
    case InstallState::BundledComponentMissing:
        return ReaperBridgeAvailability::NeedsRepair;
    case InstallState::Unconfigured:
    case InstallState::ReaperPathMissing:
    case InstallState::ReaperExecutableMissing:
    case InstallState::ResourcePathUnconfirmed:
    case InstallState::Unknown:
    default:
        return ReaperBridgeAvailability::Unconfigured;
    }
}

struct ReaperBridgeProbeFacts {
    bool configured = false;
    bool configuredPathExists = false;
    bool hostExecutableExists = false;
    bool targetResolved = false;
    bool bundledFilePresent = false;
    bool filePresentAtTarget = false;
    bool filePresentAtOtherLocation = false;
    bool hostRunning = false;
    bool restartRequired = false;
    bool binaryIntegrityOk = true;
    bool trustedSource = true;
    std::string installedVersion;
    std::string expectedVersion;
    int installedProtocolVersion = 0;
    int expectedProtocolVersion = ReaperBridge::kProtocolVersion;
    std::string installedArchitecture;
    std::string hostArchitecture;
    ReaperBridge::HandshakeCode handshakeCode =
        ReaperBridge::HandshakeCode::TransportUnavailable;
};

inline InstallState EvaluateReaperBridgeState(const ReaperBridgeProbeFacts& facts) {
    if (!facts.configured) return InstallState::Unconfigured;
    if (!facts.configuredPathExists) return InstallState::ReaperPathMissing;
    if (!facts.hostExecutableExists) return InstallState::ReaperExecutableMissing;
    if (facts.handshakeCode == ReaperBridge::HandshakeCode::Ready) {
        return InstallState::HandshakeReady;
    }
    if (!facts.targetResolved) return InstallState::ResourcePathUnconfirmed;
    if (!facts.filePresentAtTarget) {
        if (facts.hostRunning && facts.bundledFilePresent) return InstallState::WaitingForHostExit;
        if (facts.filePresentAtOtherLocation) return InstallState::WrongLocation;
        if (!facts.bundledFilePresent) return InstallState::BundledComponentMissing;
        return InstallState::NotInstalled;
    }
    if (!facts.trustedSource) return InstallState::UnknownSource;
    if (!facts.binaryIntegrityOk) {
        return facts.hostRunning ? InstallState::WaitingForHostExit : InstallState::BinaryInvalid;
    }
    if (!facts.installedArchitecture.empty() && !facts.hostArchitecture.empty() &&
        facts.installedArchitecture != facts.hostArchitecture) {
        return facts.hostRunning ? InstallState::WaitingForHostExit
                                 : InstallState::ArchitectureIncompatible;
    }
    if (facts.installedProtocolVersion != facts.expectedProtocolVersion) {
        return facts.hostRunning ? InstallState::WaitingForHostExit
                                 : InstallState::ProtocolIncompatible;
    }
    if (facts.installedVersion != facts.expectedVersion) {
        return facts.hostRunning ? InstallState::WaitingForHostExit
                                 : InstallState::VersionIncompatible;
    }
    if (facts.restartRequired) return InstallState::RestartRequired;
    if (facts.hostRunning) {
        return facts.handshakeCode == ReaperBridge::HandshakeCode::TransportUnavailable
            ? InstallState::InstalledNotLoaded
            : InstallState::HandshakeFailed;
    }
    return InstallState::FileInstalled;
}

inline const char* ReaperBridgeStateKey(InstallState state) {
    switch (state) {
    case InstallState::Unconfigured: return "unconfigured";
    case InstallState::ReaperPathMissing: return "reaper_path_missing";
    case InstallState::ReaperExecutableMissing: return "reaper_executable_missing";
    case InstallState::NotInstalled: return "not_installed";
    case InstallState::ResourcePathUnconfirmed: return "resource_path_unconfirmed";
    case InstallState::WrongLocation: return "wrong_location";
    case InstallState::WaitingForHostExit: return "waiting_for_host_exit";
    case InstallState::FileInstalled: return "file_installed";
    case InstallState::InstalledNotLoaded: return "installed_not_loaded";
    case InstallState::RestartRequired: return "restart_required";
    case InstallState::VersionIncompatible: return "version_incompatible";
    case InstallState::ProtocolIncompatible: return "protocol_incompatible";
    case InstallState::ArchitectureIncompatible: return "architecture_incompatible";
    case InstallState::BinaryInvalid: return "binary_invalid";
    case InstallState::UnknownSource: return "unknown_source";
    case InstallState::HandshakeReady: return "handshake_ready";
    case InstallState::HandshakeFailed: return "handshake_failed";
    case InstallState::BundledComponentMissing: return "bundled_component_missing";
    case InstallState::Unknown:
    default: return "unknown";
    }
}

} // namespace HarkenSearch::DawBridge
