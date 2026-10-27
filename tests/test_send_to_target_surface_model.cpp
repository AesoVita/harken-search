#include "src/SendToTargetSurfaceModel.h"

#include <iostream>

static int failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "PASS: " << msg << "\n";
    } else {
        std::cout << "FAIL: " << msg << "\n";
        ++failed;
    }
}

static void testDirectReadyDawTarget() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    ctx.selectedCount = 1;
    ctx.dawTargets.push_back({
        0,
        "REAPER",
        HarkenSearch::UI::SendTargetCapabilityState::DirectReady,
        true
    });

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* reaper = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        0);
    check(reaper != nullptr, "direct-ready DAW target is present");
    check(reaper && reaper->capabilityTag == "Running - direct insert",
          "direct-ready DAW target exposes capability tag");
    check(reaper && reaper->preflightSummary == "Ready for host handoff.",
          "direct-ready DAW target exposes preflight summary");
    const auto* hostMode = reaper
        ? HarkenSearch::UI::FindSendTargetMode(*reaper, HarkenSearch::UI::SendTargetModeId::HostHandoff)
        : nullptr;
    check(hostMode && hostMode->enabled,
          "direct-ready DAW target enables host handoff");
}

static void testNeedsConfigDawTarget() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    ctx.dawTargets.push_back({
        2,
        "Nuendo",
        HarkenSearch::UI::SendTargetCapabilityState::NeedsConfig,
        true
    });

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* nuendo = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        2);
    check(nuendo != nullptr, "unconfigured DAW target remains visible");
    check(nuendo && nuendo->recommendedActionLabel == "Configure Targets...",
          "unconfigured DAW target recommends configuration");
    const auto* hostMode = nuendo
        ? HarkenSearch::UI::FindSendTargetMode(*nuendo, HarkenSearch::UI::SendTargetModeId::HostHandoff)
        : nullptr;
    const auto* configMode = nuendo
        ? HarkenSearch::UI::FindSendTargetMode(*nuendo, HarkenSearch::UI::SendTargetModeId::ConfigureTargets)
        : nullptr;
    check(hostMode && !hostMode->enabled,
          "unconfigured DAW target disables host handoff");
    check(hostMode && hostMode->disabledReason == "Configure this target first",
          "unconfigured DAW target carries explicit disabled reason");
    check(configMode && configMode->enabled,
          "unconfigured DAW target keeps configure mode enabled");
}

static void testAdapterMissingDawTarget() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    ctx.dawTargets.push_back({
        3,
        "Cubase",
        HarkenSearch::UI::SendTargetCapabilityState::AdapterMissing,
        true
    });

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* cubase = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        3);
    check(cubase != nullptr, "adapter-missing Cubase target is present");
    check(cubase && cubase->capabilityTag == "Adapter required",
          "adapter-missing Cubase target exposes explicit capability tag");
    check(cubase &&
              cubase->preflightSummary ==
                  "This host needs its dedicated direct-insert adapter before handoff can run.",
          "adapter-missing Cubase target exposes honest preflight");
    check(cubase && cubase->recommendedActionLabel == "Use Export Fallback",
          "adapter-missing Cubase target recommends export fallback");
    const auto* hostMode = cubase
        ? HarkenSearch::UI::FindSendTargetMode(*cubase, HarkenSearch::UI::SendTargetModeId::HostHandoff)
        : nullptr;
    check(hostMode && !hostMode->enabled,
          "adapter-missing Cubase target disables host handoff");
    check(hostMode && hostMode->disabledReason == "Direct insert adapter is not available yet",
          "adapter-missing Cubase target carries explicit disabled reason");
}

static void testBridgeOfflineDawTargetWithOverride() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    HarkenSearch::UI::SendTargetDawContext cubase;
    cubase.dawIndex = 4;
    cubase.label = "Cubase";
    cubase.capability = HarkenSearch::UI::SendTargetCapabilityState::BridgeOffline;
    cubase.visible = true;
    cubase.capabilityTagOverride = "Bridge offline";
    cubase.preflightSummaryOverride =
        "Cubase adapter is installed, but the host bridge is not responding yet.";
    cubase.recommendedActionOverride = "Use Export Fallback";
    cubase.hostModeDisabledReasonOverride =
        "Direct insert bridge is installed but not responding yet";
    ctx.dawTargets.push_back(cubase);

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* target = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        4);
    check(target != nullptr, "bridge-offline Cubase target is present");
    check(target && target->capabilityTag == "Bridge offline",
          "bridge-offline Cubase target exposes override capability tag");
    check(target &&
              target->preflightSummary ==
                  "Cubase adapter is installed, but the host bridge is not responding yet.",
          "bridge-offline Cubase target exposes override preflight summary");
    const auto* hostMode = target
        ? HarkenSearch::UI::FindSendTargetMode(*target, HarkenSearch::UI::SendTargetModeId::HostHandoff)
        : nullptr;
    check(hostMode && !hostMode->enabled,
          "bridge-offline Cubase target disables host handoff");
    check(hostMode && hostMode->disabledReason ==
          "Direct insert bridge is installed but not responding yet",
          "bridge-offline Cubase target carries explicit disabled reason");
}

static void testExperimentalNuendoTargetWithOverride() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    HarkenSearch::UI::SendTargetDawContext nuendo;
    nuendo.dawIndex = 6;
    nuendo.label = "Nuendo";
    nuendo.capability = HarkenSearch::UI::SendTargetCapabilityState::ExperimentalReady;
    nuendo.visible = true;
    nuendo.capabilityTagOverride = "Experimental";
    nuendo.preflightSummaryOverride =
        "Nuendo-first experimental send is available for live import validation.";
    nuendo.recommendedActionOverride = "Try Experimental Send";
    nuendo.hasHostModeEnabledOverride = true;
    nuendo.hostModeEnabledOverride = true;
    nuendo.exposeTransferActionSplit = true;
    nuendo.bringModeLabelOverride = "Bring Into Host";
    nuendo.spotModeLabelOverride = "Spot To Host";
    ctx.dawTargets.push_back(nuendo);

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* target = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        6);
    check(target != nullptr, "experimental Nuendo target is present");
    check(target && target->capabilityTag == "Experimental",
          "experimental Nuendo target exposes explicit capability tag");
    check(target && target->recommendedActionLabel == "Try Experimental Send",
          "experimental Nuendo target recommends experimental send");
    const auto* bringMode = target
        ? HarkenSearch::UI::FindSendTargetMode(*target, HarkenSearch::UI::SendTargetModeId::BringIntoHost)
        : nullptr;
    const auto* spotMode = target
        ? HarkenSearch::UI::FindSendTargetMode(*target, HarkenSearch::UI::SendTargetModeId::SpotToHost)
        : nullptr;
    check(bringMode && bringMode->enabled,
          "experimental Nuendo target exposes enabled Bring-into-host mode");
    check(bringMode && bringMode->label == "Bring Into Host",
          "experimental Nuendo target relabels bring mode explicitly");
    check(spotMode && spotMode->enabled,
          "experimental Nuendo target exposes enabled Spot-to-host mode");
    check(spotMode && spotMode->label == "Spot To Host",
          "experimental Nuendo target relabels spot mode explicitly");
}

static void testUnavailableCubaseTargetWithOverride() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    HarkenSearch::UI::SendTargetDawContext cubase;
    cubase.dawIndex = 7;
    cubase.label = "Cubase";
    cubase.capability = HarkenSearch::UI::SendTargetCapabilityState::Unavailable;
    cubase.visible = true;
    cubase.capabilityTagOverride = "Unavailable";
    cubase.preflightSummaryOverride =
        "Cubase launch-only send is intentionally blocked from pretending it can insert into the host.";
    cubase.recommendedActionOverride = "Use Export Fallback";
    cubase.hasHostModeEnabledOverride = true;
    cubase.hostModeEnabledOverride = false;
    cubase.hostModeLabelOverride = "Unavailable";
    cubase.hostModeDisabledReasonOverride =
        "Cubase launch-only send is intentionally blocked from pretending it can insert into the host";
    ctx.dawTargets.push_back(cubase);

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* target = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        7);
    check(target != nullptr, "unavailable Cubase target is present");
    check(target && target->capabilityTag == "Unavailable",
          "unavailable Cubase target exposes explicit capability tag");
    const auto* hostMode = target
        ? HarkenSearch::UI::FindSendTargetMode(*target, HarkenSearch::UI::SendTargetModeId::HostHandoff)
        : nullptr;
    check(hostMode && !hostMode->enabled,
          "unavailable Cubase target disables host handoff");
    check(hostMode && hostMode->label == "Unavailable",
          "unavailable Cubase target exposes unavailable host mode label");
}

static void testHostNotReadyDawTargetWithOverride() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    HarkenSearch::UI::SendTargetDawContext cubase;
    cubase.dawIndex = 5;
    cubase.label = "Cubase";
    cubase.capability = HarkenSearch::UI::SendTargetCapabilityState::HostNotReady;
    cubase.visible = true;
    cubase.capabilityTagOverride = "Host not ready";
    cubase.preflightSummaryOverride =
        "Cubase bridge is online, but the current host project/track state is not ready for direct insert.";
    cubase.recommendedActionOverride = "Prepare Session";
    cubase.hostModeDisabledReasonOverride =
        "Direct insert bridge is online, but the current Cubase project/track is not ready yet";
    ctx.dawTargets.push_back(cubase);

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* target = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Daw,
        5);
    check(target != nullptr, "host-not-ready Cubase target is present");
    check(target && target->capabilityTag == "Host not ready",
          "host-not-ready Cubase target exposes override capability tag");
    check(target &&
              target->preflightSummary ==
                  "Cubase bridge is online, but the current host project/track state is not ready for direct insert.",
          "host-not-ready Cubase target exposes override preflight summary");
    check(target && target->recommendedActionLabel == "Prepare Session",
          "host-not-ready Cubase target recommends session preparation");
    const auto* hostMode = target
        ? HarkenSearch::UI::FindSendTargetMode(*target, HarkenSearch::UI::SendTargetModeId::HostHandoff)
        : nullptr;
    check(hostMode && !hostMode->enabled,
          "host-not-ready Cubase target disables host handoff");
    check(hostMode && hostMode->disabledReason ==
          "Direct insert bridge is online, but the current Cubase project/track is not ready yet",
          "host-not-ready Cubase target carries explicit disabled reason");
}

static void testWwiseAndExportTargets() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    ctx.isMulti = true;
    ctx.selectedCount = 3;
    ctx.exposeWwiseTransfer = true;
    ctx.wwiseEnabled = false;
    ctx.exportFallbackVisible = true;

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* wwise = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Wwise);
    const auto* exportTarget = HarkenSearch::UI::FindSendTarget(
        model,
        HarkenSearch::UI::SendTargetKind::Export);
    check(wwise != nullptr, "Wwise target is exposed when feature surface is enabled");
    check(wwise && wwise->preflightSummary == "Requires Standard edition or higher.",
          "edition-gated Wwise target exposes honest preflight");
    const auto* wwiseMode = wwise
        ? HarkenSearch::UI::FindSendTargetMode(*wwise, HarkenSearch::UI::SendTargetModeId::OpenTransferPanel)
        : nullptr;
    check(wwiseMode && !wwiseMode->enabled,
          "edition-gated Wwise panel mode is disabled");
    check(exportTarget != nullptr, "export fallback target is always present in v1");
    const auto* exportMode = exportTarget
        ? HarkenSearch::UI::FindSendTargetMode(*exportTarget, HarkenSearch::UI::SendTargetModeId::ExportFiles)
        : nullptr;
    check(exportMode && exportMode->label == "Export Selected",
          "multi-select export target uses plural label");
}

static void testRecentHistoryCapped() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    ctx.maxRecentHistory = 5;
    for (int i = 0; i < 6; ++i) {
        HarkenSearch::UI::SendTargetHistoryItem item;
        item.targetKind = HarkenSearch::UI::SendTargetKind::Daw;
        item.modeId = HarkenSearch::UI::SendTargetModeId::HostHandoff;
        item.dawIndex = i;
        item.targetLabel = "DAW " + std::to_string(i);
        item.modeLabel = "Host Handoff";
        item.resultSummary = "Started";
        item.resultDetail = "project=Demo | selected=Track";
        item.timestampUtc = i;
        ctx.recentHistory.push_back(item);
    }

    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    check(model.recentHistory.size() == 5,
          "recent history is capped to the configured limit");
    check(model.recentHistory.front().targetLabel == "DAW 0",
          "recent history preserves newest-first order from context");
    check(model.recentHistory.front().resultDetail == "project=Demo | selected=Track",
          "recent history preserves structured detail copy");
}

static void testSameNamedTargetsKeepSeparateIdentityAndRouting() {
    HarkenSearch::UI::SendToTargetSurfaceContext ctx;
    ctx.exportFallbackVisible = false;
    ctx.dawTargets.push_back({7, "Same DAW", HarkenSearch::UI::SendTargetCapabilityState::DirectReady, true});
    ctx.dawTargets.push_back({2, "Same DAW", HarkenSearch::UI::SendTargetCapabilityState::NeedsConfig, true});
    const auto model = HarkenSearch::UI::BuildSendToTargetSurface(ctx);
    const auto* first = HarkenSearch::UI::FindSendTarget(model, HarkenSearch::UI::SendTargetKind::Daw, 7);
    const auto* second = HarkenSearch::UI::FindSendTarget(model, HarkenSearch::UI::SendTargetKind::Daw, 2);
    check(model.targets.size() == 2 && first && second, "same display names keep both configured targets");
    if (!first || !second) return;
    check(first->label == second->label, "target identity does not rename visible labels");
    check(first->key != second->key, "same-named targets supply distinct menu scope keys");
    const auto* firstMode = HarkenSearch::UI::FindSendTargetMode(*first, HarkenSearch::UI::SendTargetModeId::HostHandoff);
    const auto* secondMode = HarkenSearch::UI::FindSendTargetMode(*second, HarkenSearch::UI::SendTargetModeId::HostHandoff);
    check(firstMode && secondMode && firstMode->key != secondMode->key, "child actions retain separate target identities");
    check(firstMode && firstMode->enabled && secondMode && !secondMode->enabled,
          "same names do not leak the ready target state into the unconfigured target");
    check(first->dawIndex == 7 && second->dawIndex == 2, "send routing uses the configured index, not display order or name");
}

int main() {
    testSameNamedTargetsKeepSeparateIdentityAndRouting();
    testDirectReadyDawTarget();
    testNeedsConfigDawTarget();
    testAdapterMissingDawTarget();
    testBridgeOfflineDawTargetWithOverride();
    testExperimentalNuendoTargetWithOverride();
    testUnavailableCubaseTargetWithOverride();
    testHostNotReadyDawTargetWithOverride();
    testWwiseAndExportTargets();
    testRecentHistoryCapped();

    if (failed != 0) {
        std::cerr << "Total failures: " << failed << "\n";
        return 1;
    }
    std::cout << "All send-to-target surface model tests passed\n";
    return 0;
}
