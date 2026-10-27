#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace HarkenSearch::ProTools {

// @stage7-contract
// API contract: bridge models preserve the published protocol/version and own
// request/result values. Transport/process lifetime stays with the bridge
// implementation; callers handle DeliveryPath and result errors explicitly.

constexpr const char* kProtocolVersion = "auro-protools-host-send-v1";
constexpr const char* kProToolsExeName = "ProTools.exe";
constexpr const char* kPtslDllName = "PTSL.dll";
constexpr const char* kPtslClientDllName = "PTSLC_CPP.dll"; // user-supplied Avid runtime by default
constexpr const char* kPtAafDllName = "PtAAF.dll";
constexpr const char* kAafCoApiDllName = "AAFCOAPI.dll";

enum class DeliveryPath {
    LivePtslSpot,
    AafFallback,
};

enum class Readiness {
    NotInstalled,
    InstalledButHostNotReady,
    LivePtslPossible,
    AafFallbackPossible,
};

enum class FailureCode {
    None,
    ProToolsNotInstalled,
    ActivationGate,
    HostWindowUnavailable,
    PtslMissing,
    AafMissing,
    ProtocolUnavailable,
};

struct InstallProbePlan {
    std::string installRootUtf8;
    std::string proToolsExePathUtf8;
    std::string ptslDllPathUtf8;
    std::string userSdkDirectoryUtf8;
    std::string userPtslDllPathUtf8;
    std::string userPtslClientDllPathUtf8;
    std::string sdkGuideReadmePathUtf8;
    std::string sdkDownloadUrl;
    std::string ptAafDllPathUtf8;
    std::string aafCoApiDllPathUtf8;
    std::vector<std::string> liveWindowClasses;
    std::vector<std::string> activationWindowClasses;
};

struct CapabilitySnapshot {
    bool proToolsExePresent = false;
    bool ptslPresent = false;
    bool userPtslPresent = false;
    bool userPtslClientPresent = false;
    bool ptAafPresent = false;
    bool aafCoApiPresent = false;
    bool proToolsProcessRunning = false;
    bool liveHostWindowPresent = false;
    bool activationProxyVisible = false;
    std::string matchedLiveWindowClass;
    std::string matchedActivationWindowClass;
    Readiness readiness = Readiness::NotInstalled;
    FailureCode failure = FailureCode::ProToolsNotInstalled;
    std::string summary;
};

struct ProToolsSetupChecklistItem {
    std::string key;
    std::string label;
    bool ok = false;
    bool blocking = true;
    std::string detail;
};

struct ProToolsTimelineSendPresentation {
    std::string capabilityTag;
    std::string preflightSummary;
    std::string recommendedAction;
    std::string actionLabel = "Spot to Pro Tools Timeline";
    bool sendEnabled = false;
    std::string disabledReason;
};

struct ProToolsConnectionResetPresentation {
    std::string actionLabel = "Reset Pro Tools Connection";
    std::string safetySummary;
    std::string closeProToolsLabel = "Close Pro Tools after saving";
    bool canResetHarkenSearchBridgeHelpers = true;
    bool requiresProToolsRestart = false;
    bool canRequestProToolsClose = false;
    std::string disabledReason;
};

struct ProToolsConnectionResetResult {
    bool attempted = false;
    bool ok = false;
    bool helperCleanupAttempted = false;
    int helperProcessesTerminated = 0;
    bool proToolsWasRunning = false;
    bool proToolsCloseRequested = false;
    int proToolsCloseMessagesPosted = 0;
    std::string status;
    std::string detail;
};

struct ProtocolMessage {
    DeliveryPath path = DeliveryPath::LivePtslSpot;
    std::string correlationId;
    std::string stagedPathUtf8;
    std::string displayNameUtf8;
    bool spotToTimeline = true;
};

// Raw-source spot semantics. Persisted per DAW target in settings.
enum class SpotMode {
    AtCursor,       // Read GetTimelineSelection.in_time; sequential layout for multi-file.
    ToTimecode,     // Per-file BWF time_reference converted to session sample rate.
    ToSelectionIn,  // Same source as AtCursor but semantically bound to timeline selection start.
};

// Behavior when the resolver cannot pin exactly one target audio track.
enum class TrackFallback {
    AutoCreateNew,     // Create a new track (transparent to user; toast marks it as fallback).
    RequireExplicit,   // Reserved: return an error asking the user to click a track. Not yet wired.
};

struct LivePtslSpotFile {
    std::string stagedPathUtf8;
    std::string displayNameUtf8;
    // Populated by HostSendRuntime from Database.AudioRecord + MetadataReader:
    std::uint32_t sourceSampleRate = 0;        // Source file sample rate (needed to convert BWF TC to session samples).
    std::uint16_t channelCount = 0;            // Informational.
    std::uint64_t durationSamples = 0;         // Source-rate samples; scaled to session SR for layout.
    std::uint64_t bwfTimeReferenceSamples = 0; // 0 = absent; only meaningful when spotMode == ToTimecode.
};

struct LivePtslSpotRequest {
    std::string correlationId;
    // Legacy free-text hint (protocol v1). Kept for wire compatibility; new helper ignores it.
    std::string target = "selected_track_or_edit_cursor";
    SpotMode spotMode = SpotMode::AtCursor;
    TrackFallback trackFallback = TrackFallback::AutoCreateNew;
    std::vector<LivePtslSpotFile> files;
};

struct LivePtslSpotResponse {
    bool parseOk = false;
    bool ok = false;
    DeliveryPath path = DeliveryPath::LivePtslSpot;
    int insertedCount = 0;
    std::string sessionName;
    std::string trackName;
    std::string errorCode;
    std::string errorMessage;
    std::vector<std::string> ptslCommandTrace;

    // v2 fields (populated by helper on success paths; zeros/empties otherwise):
    std::int32_t ptVersionMajor = 0;
    std::int32_t ptVersionMinor = 0;
    std::int32_t ptVersionRevision = 0;
    std::int32_t sessionSampleRate = 0;
    std::string pathTaken;                       // "modern-2025.06+" or "legacy-newtrack"
    std::string resolvedTrackId;
    std::string resolvedTrackName;
    bool trackWasCreated = false;                // True when the helper created a new track as fallback.
    std::vector<std::int64_t> perClipSampleOffsets; // Absolute session-sample positions the helper actually spotted at.
    // M16 diagnostic: raw GetTrackList JSON entry for the resolved track (type, format,
    // attributes). Populated by the helper on successful resolve so we can post-mortem
    // silent-Spot-noop cases where PT accepts the payload but places nothing.
    std::string resolvedTrackRawJson;
    // M19 diagnostic: raw GetTimelineSelection response fields (as PT returned them).
    // Diagnoses why AtCursor spot landed at 0 vs cursor.
    std::string timelineInTime;
    std::string timelineOutTime;
    std::string timelinePlayStartMarker;
};

const char* SpotModeKey(SpotMode mode);
SpotMode SpotModeFromKey(const std::string& key);
const char* TrackFallbackKey(TrackFallback fallback);
TrackFallback TrackFallbackFromKey(const std::string& key);

struct LivePtslDispatchResult {
    bool helperAvailable = false;
    int exitCode = -1;
    std::string helperPathUtf8;
    std::string requestPathUtf8;
    std::string responsePathUtf8;
    std::string detail;
    LivePtslSpotResponse response;
};

const char* DeliveryPathKey(DeliveryPath path);
const char* ReadinessKey(Readiness readiness);
const char* FailureCodeKey(FailureCode code);

InstallProbePlan BuildInstallProbePlan(const std::string& avidProToolsRootUtf8 = {});
CapabilitySnapshot EvaluateCapability(
    const InstallProbePlan& plan,
    const std::function<bool(const std::string&)>& pathExists,
    bool liveHostWindowPresent,
    bool activationProxyVisible);
CapabilitySnapshot ProbeInstalledCapability(const InstallProbePlan& plan = BuildInstallProbePlan());
std::vector<ProToolsSetupChecklistItem> BuildRuntimeSetupChecklist(
    const InstallProbePlan& plan,
    const CapabilitySnapshot& snapshot,
    bool bridgeHelperPresent);
ProToolsTimelineSendPresentation BuildTimelineSendPresentation(
    const CapabilitySnapshot& snapshot,
    bool bridgeHelperPresent);
ProToolsConnectionResetPresentation BuildProToolsConnectionResetPresentation(
    const CapabilitySnapshot& snapshot,
    bool bridgeHelperPresent);
ProToolsConnectionResetResult ResetProToolsConnection(bool closeProToolsAfterUserSaved);
ProToolsConnectionResetResult ResetProToolsBridgeOnHarkenSearchStartup();

ProtocolMessage BuildLivePtslSpotMessage(const std::string& correlationId,
                                          const std::string& stagedPathUtf8,
                                          const std::string& displayNameUtf8);
ProtocolMessage BuildAafFallbackMessage(const std::string& correlationId,
                                         const std::string& stagedPathUtf8,
                                         const std::string& displayNameUtf8);
std::string BuildProtocolJson(const ProtocolMessage& message);
std::string BuildUserSdkGuideReadmeText(const InstallProbePlan& plan);

std::string BuildLivePtslSpotRequestJson(const LivePtslSpotRequest& request);
LivePtslSpotResponse ParseLivePtslSpotResponseJson(const std::string& json);

// S4: Compute per-clip session-sample positions for sequential or timecode-based spotting.
// basePosition: edit cursor in session samples (from GetTimelineSelection.in_time), used for
//               AtCursor and ToSelectionIn. Ignored for ToTimecode (each clip uses its BWF TC).
// sessionSampleRate: from GetSessionSampleRate; 0 = unknown (no SR conversion applied).
// Returns one position per file; size always == files.size().
std::vector<std::int64_t> ComputeSpotPositions(
    const std::vector<LivePtslSpotFile>& files,
    SpotMode spotMode,
    std::int64_t basePosition,
    std::int32_t sessionSampleRate);
std::string SelectTimelineBaseSamples(SpotMode spotMode,
                                      const std::string& timelineInTime,
                                      const std::string& timelinePlayStartMarker);
std::string BuildLivePtslSpotResponseJson(const LivePtslSpotResponse& response);
bool IsLivePtslSpotSuccess(const LivePtslDispatchResult& result);
std::string ResolveProToolsBridgeHelperPath();
LivePtslDispatchResult DispatchLivePtslSmokeTest();
LivePtslDispatchResult DispatchLivePtslSpotRequest(const LivePtslSpotRequest& request);

} // namespace HarkenSearch::ProTools
