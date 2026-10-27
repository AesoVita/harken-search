#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace HarkenSearch::UI {

// @stage7-contract
// API contract: send-to-target models are value/state descriptions for UI
// routing. They do not own DAW/Wwise/export sessions; callers preserve enum
// domains and validate capabilities before performing handoff actions.

enum class SendTargetKind {
    Daw,
    Wwise,
    Export,
};

enum class SendTargetModeId {
    HostHandoff,
    BringIntoHost,
    SpotToHost,
    OpenTransferPanel,
    ExportFallback,
    ExportFiles,
    ConfigureTargets,
};

enum class SendTargetCapabilityState {
    DirectReady,
    ExperimentalReady,
    LaunchReady,
    PasteReady,
    ClipboardReady,
    AdapterMissing,
    HostNotReady,
    BridgeOffline,
    Unavailable,
    NeedsConfig,
    PanelReady,
    FeatureLocked,
    ExportReady,
};

struct SendTargetHistoryItem {
    SendTargetKind targetKind = SendTargetKind::Daw;
    SendTargetModeId modeId = SendTargetModeId::HostHandoff;
    int dawIndex = -1;
    std::string targetLabel;
    std::string modeLabel;
    std::string resultSummary;
    std::string resultDetail;
    std::int64_t timestampUtc = 0;
    bool rerunnable = true;
};

struct SendTargetDawContext {
    int dawIndex = -1;
    std::string label;
    SendTargetCapabilityState capability = SendTargetCapabilityState::NeedsConfig;
    bool visible = true;
    std::string capabilityTagOverride;
    std::string preflightSummaryOverride;
    std::string recommendedActionOverride;
    bool hasHostModeEnabledOverride = false;
    bool hostModeEnabledOverride = false;
    std::string hostModeLabelOverride;
    std::string hostModeDisabledReasonOverride;
    bool exposeTransferActionSplit = false;
    std::string bringModeLabelOverride;
    std::string spotModeLabelOverride;
};

struct SendToTargetSurfaceContext {
    bool isMulti = false;
    int selectedCount = 0;
    std::vector<SendTargetDawContext> dawTargets;
    bool exposeWwiseTransfer = false;
    bool wwiseEnabled = false;
    bool exportFallbackVisible = true;
    std::vector<SendTargetHistoryItem> recentHistory;
    std::size_t maxRecentHistory = 5;
};

struct SendTargetModeDescriptor {
    SendTargetModeId id = SendTargetModeId::HostHandoff;
    std::string key;
    std::string label;
    bool visible = true;
    bool enabled = true;
    std::string disabledReason;
};

struct SendTargetDescriptor {
    SendTargetKind kind = SendTargetKind::Daw;
    int dawIndex = -1;
    std::string key;
    std::string label;
    bool visible = true;
    bool enabled = true;
    std::string capabilityTag;
    std::string preflightSummary;
    std::string recommendedActionLabel;
    std::vector<SendTargetModeDescriptor> modes;
};

struct SendToTargetSurfaceModel {
    std::vector<SendTargetDescriptor> targets;
    std::vector<SendTargetHistoryItem> recentHistory;
};

const char* CapabilityTagForState(SendTargetCapabilityState state);
std::string PreflightSummaryForState(SendTargetCapabilityState state);
std::string RecommendedActionLabelForState(SendTargetCapabilityState state);

SendToTargetSurfaceModel BuildSendToTargetSurface(
    const SendToTargetSurfaceContext& ctx);

const SendTargetDescriptor* FindSendTarget(
    const SendToTargetSurfaceModel& model,
    SendTargetKind kind,
    int dawIndex = -1);

const SendTargetModeDescriptor* FindSendTargetMode(
    const SendTargetDescriptor& target,
    SendTargetModeId id);

} // namespace HarkenSearch::UI
