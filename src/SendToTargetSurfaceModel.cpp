#include "SendToTargetSurfaceModel.h"

#include <algorithm>

namespace HarkenSearch::UI {

namespace {

SendTargetModeDescriptor MakeMode(
    SendTargetModeId id,
    const std::string& key,
    const std::string& label,
    bool enabled,
    const std::string& disabledReason = {}) {
    SendTargetModeDescriptor mode;
    mode.id = id;
    mode.key = key;
    mode.label = label;
    mode.enabled = enabled;
    mode.disabledReason = disabledReason;
    return mode;
}

std::string ExportLabelForSelection(const SendToTargetSurfaceContext& ctx) {
    return ctx.isMulti ? "Export Selected" : "Export File";
}

SendTargetDescriptor BuildDawTargetDescriptor(
    const SendToTargetSurfaceContext& ctx,
    const SendTargetDawContext& daw) {
    SendTargetDescriptor target;
    target.kind = SendTargetKind::Daw;
    target.dawIndex = daw.dawIndex;
    target.key = "daw_" + std::to_string(daw.dawIndex);
    target.label = daw.label;
    target.visible = daw.visible;
    target.capabilityTag = daw.capabilityTagOverride.empty()
        ? CapabilityTagForState(daw.capability)
        : daw.capabilityTagOverride;
    target.preflightSummary = daw.preflightSummaryOverride.empty()
        ? PreflightSummaryForState(daw.capability)
        : daw.preflightSummaryOverride;
    target.recommendedActionLabel = daw.recommendedActionOverride.empty()
        ? RecommendedActionLabelForState(daw.capability)
        : daw.recommendedActionOverride;

    const bool hostReadyDefault =
        daw.capability != SendTargetCapabilityState::NeedsConfig &&
        daw.capability != SendTargetCapabilityState::AdapterMissing &&
        daw.capability != SendTargetCapabilityState::HostNotReady &&
        daw.capability != SendTargetCapabilityState::BridgeOffline &&
        daw.capability != SendTargetCapabilityState::Unavailable;
    const bool hostReady = daw.hasHostModeEnabledOverride
        ? daw.hostModeEnabledOverride
        : hostReadyDefault;
    const std::string hostModeLabel = daw.hostModeLabelOverride.empty()
        ? "Host Handoff"
        : daw.hostModeLabelOverride;
    std::string disabledReason = daw.hostModeDisabledReasonOverride;
    if (disabledReason.empty()) {
        if (daw.capability == SendTargetCapabilityState::AdapterMissing) {
            disabledReason = "Direct insert adapter is not available yet";
        } else if (daw.capability == SendTargetCapabilityState::HostNotReady) {
            disabledReason = "Direct insert bridge is online, but the current Cubase project/track is not ready yet";
        } else if (daw.capability == SendTargetCapabilityState::BridgeOffline) {
            disabledReason = "Direct insert bridge is installed but not responding yet";
        } else if (daw.capability == SendTargetCapabilityState::Unavailable) {
            disabledReason = "This route is not available for true host send yet";
        } else if (!hostReady) {
            disabledReason = "Configure this target first";
        }
    }

    if (daw.exposeTransferActionSplit) {
        const std::string bringLabel = daw.bringModeLabelOverride.empty()
            ? "Bring Into Host"
            : daw.bringModeLabelOverride;
        const std::string spotLabel = daw.spotModeLabelOverride.empty()
            ? "Spot To Host"
            : daw.spotModeLabelOverride;
        target.modes.push_back(MakeMode(
            SendTargetModeId::BringIntoHost,
            target.key + "_bring",
            bringLabel,
            hostReady,
            disabledReason));
        target.modes.push_back(MakeMode(
            SendTargetModeId::SpotToHost,
            target.key + "_spot",
            spotLabel,
            hostReady,
            disabledReason));
    } else {
        target.modes.push_back(MakeMode(
            SendTargetModeId::HostHandoff,
            target.key + "_host",
            hostModeLabel,
            hostReady,
            disabledReason));
    }

    if (ctx.exportFallbackVisible) {
        target.modes.push_back(MakeMode(
            SendTargetModeId::ExportFallback,
            target.key + "_fallback",
            "Export Fallback...",
            true));
    }

    target.modes.push_back(MakeMode(
        SendTargetModeId::ConfigureTargets,
        target.key + "_configure",
        "Configure Targets...",
        true));

    return target;
}

SendTargetDescriptor BuildWwiseTargetDescriptor(
    const SendToTargetSurfaceContext& ctx) {
    SendTargetDescriptor target;
    target.kind = SendTargetKind::Wwise;
    target.key = "wwise";
    target.label = "Wwise";

    const auto capability = ctx.wwiseEnabled
        ? SendTargetCapabilityState::PanelReady
        : SendTargetCapabilityState::FeatureLocked;
    target.capabilityTag = CapabilityTagForState(capability);
    target.preflightSummary = PreflightSummaryForState(capability);
    target.recommendedActionLabel = RecommendedActionLabelForState(capability);

    target.modes.push_back(MakeMode(
        SendTargetModeId::OpenTransferPanel,
        "wwise_open_panel",
        "Open Transfer Panel",
        ctx.wwiseEnabled,
        ctx.wwiseEnabled ? "" : "Wwise transfer is not enabled"));

    if (ctx.exportFallbackVisible) {
        target.modes.push_back(MakeMode(
            SendTargetModeId::ExportFallback,
            "wwise_export_fallback",
            "Export Fallback...",
            true));
    }

    return target;
}

SendTargetDescriptor BuildExportTargetDescriptor(
    const SendToTargetSurfaceContext& ctx) {
    SendTargetDescriptor target;
    target.kind = SendTargetKind::Export;
    target.key = "export";
    target.label = "File Export";
    target.capabilityTag = CapabilityTagForState(
        SendTargetCapabilityState::ExportReady);
    target.preflightSummary = PreflightSummaryForState(
        SendTargetCapabilityState::ExportReady);
    target.recommendedActionLabel = RecommendedActionLabelForState(
        SendTargetCapabilityState::ExportReady);
    target.modes.push_back(MakeMode(
        SendTargetModeId::ExportFiles,
        "export_files",
        ExportLabelForSelection(ctx),
        true));
    return target;
}

} // namespace

const char* CapabilityTagForState(SendTargetCapabilityState state) {
    switch (state) {
    case SendTargetCapabilityState::DirectReady:
        return "Running - direct insert";
    case SendTargetCapabilityState::ExperimentalReady:
        return "Experimental";
    case SendTargetCapabilityState::LaunchReady:
        return "Not running";
    case SendTargetCapabilityState::PasteReady:
        return "Running - no direct insert";
    case SendTargetCapabilityState::ClipboardReady:
        return "No direct insert";
    case SendTargetCapabilityState::AdapterMissing:
        return "Adapter required";
    case SendTargetCapabilityState::HostNotReady:
        return "Host not ready";
    case SendTargetCapabilityState::BridgeOffline:
        return "Bridge offline";
    case SendTargetCapabilityState::Unavailable:
        return "Unavailable";
    case SendTargetCapabilityState::NeedsConfig:
        return "Not configured";
    case SendTargetCapabilityState::PanelReady:
        return "Transfer panel";
    case SendTargetCapabilityState::FeatureLocked:
        return "Feature locked";
    case SendTargetCapabilityState::ExportReady:
        return "Export fallback";
    }
    return "";
}

std::string PreflightSummaryForState(SendTargetCapabilityState state) {
    switch (state) {
    case SendTargetCapabilityState::DirectReady:
        return "Ready for host handoff.";
    case SendTargetCapabilityState::ExperimentalReady:
        return "Nuendo-first experimental send is available. Use it as a live host import path, but keep it marked experimental until selected-track delivery is fully proven.";
    case SendTargetCapabilityState::LaunchReady:
        return "Configured target. Launch or paste path will be used.";
    case SendTargetCapabilityState::PasteReady:
        return "Ready to hand off without direct insert.";
    case SendTargetCapabilityState::ClipboardReady:
        return "Clipboard or paste fallback will be used.";
    case SendTargetCapabilityState::AdapterMissing:
        return "This host needs its dedicated direct-insert adapter before handoff can run.";
    case SendTargetCapabilityState::HostNotReady:
        return "This host bridge is online, but the current project/track state is not ready for direct insert yet.";
    case SendTargetCapabilityState::BridgeOffline:
        return "This host adapter is installed, but the bridge is not responding yet.";
    case SendTargetCapabilityState::Unavailable:
        return "This host route is intentionally blocked from pretending launch-only handoff is a real send path.";
    case SendTargetCapabilityState::NeedsConfig:
        return "Configure this target before host handoff.";
    case SendTargetCapabilityState::PanelReady:
        return "Ready to open the Wwise transfer panel.";
    case SendTargetCapabilityState::FeatureLocked:
        return "Requires Standard edition or higher.";
    case SendTargetCapabilityState::ExportReady:
        return "Ready to open the export dialog.";
    }
    return {};
}

std::string RecommendedActionLabelForState(SendTargetCapabilityState state) {
    switch (state) {
    case SendTargetCapabilityState::ExperimentalReady:
        return "Try Experimental Send";
    case SendTargetCapabilityState::AdapterMissing:
    case SendTargetCapabilityState::HostNotReady:
    case SendTargetCapabilityState::BridgeOffline:
    case SendTargetCapabilityState::Unavailable:
        return "Use Export Fallback";
    case SendTargetCapabilityState::NeedsConfig:
        return "Configure Targets...";
    case SendTargetCapabilityState::FeatureLocked:
        return "Use Export Fallback";
    case SendTargetCapabilityState::ExportReady:
        return "Export Files";
    default:
        return "Use Host Handoff";
    }
}

SendToTargetSurfaceModel BuildSendToTargetSurface(
    const SendToTargetSurfaceContext& ctx) {
    SendToTargetSurfaceModel model;
    model.targets.reserve(
        ctx.dawTargets.size() +
        (ctx.exposeWwiseTransfer ? 1u : 0u) +
        (ctx.exportFallbackVisible ? 1u : 0u));

    for (const auto& daw : ctx.dawTargets) {
        if (!daw.visible) continue;
        model.targets.push_back(BuildDawTargetDescriptor(ctx, daw));
    }

    if (ctx.exposeWwiseTransfer) {
        model.targets.push_back(BuildWwiseTargetDescriptor(ctx));
    }

    if (ctx.exportFallbackVisible) {
        model.targets.push_back(BuildExportTargetDescriptor(ctx));
    }

    const std::size_t recentCount = std::min(
        ctx.recentHistory.size(),
        ctx.maxRecentHistory);
    model.recentHistory.assign(
        ctx.recentHistory.begin(),
        ctx.recentHistory.begin() + recentCount);

    return model;
}

const SendTargetDescriptor* FindSendTarget(
    const SendToTargetSurfaceModel& model,
    SendTargetKind kind,
    int dawIndex) {
    auto it = std::find_if(
        model.targets.begin(),
        model.targets.end(),
        [kind, dawIndex](const SendTargetDescriptor& target) {
            return target.kind == kind &&
                (kind != SendTargetKind::Daw || target.dawIndex == dawIndex);
        });
    return it == model.targets.end() ? nullptr : &(*it);
}

const SendTargetModeDescriptor* FindSendTargetMode(
    const SendTargetDescriptor& target,
    SendTargetModeId id) {
    auto it = std::find_if(
        target.modes.begin(),
        target.modes.end(),
        [id](const SendTargetModeDescriptor& mode) {
            return mode.id == id;
        });
    return it == target.modes.end() ? nullptr : &(*it);
}

} // namespace HarkenSearch::UI
