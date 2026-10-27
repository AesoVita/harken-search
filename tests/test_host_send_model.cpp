#include "src/HostSendModel.h"

#include <iostream>
#include <string>

static int g_failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "PASS: " << msg << "\n";
    } else {
        std::cout << "FAIL: " << msg << "\n";
        ++g_failed;
    }
}

static void testNuendoDefaultsToBridgeImportPath() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::Nuendo;
    request.mode = HarkenSearch::HostSend::DefaultDeliveryModeForHost(request.host);
    request.items.push_back({
        "D:/Library/Impacts/Big Hit.wav",
        "Big Hit",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto job = HarkenSearch::HostSend::BuildJob(
        request,
        "C:/Temp/HarkenSearchHostSend");
    check(job.adapterPath == HarkenSearch::HostSend::AdapterPath::SteinbergBridge,
          "Nuendo defaults to Steinberg bridge adapter");
    check(job.mode == HarkenSearch::HostSend::DeliveryMode::BringIntoHost,
          "Nuendo defaults to bring-into-host mode");
    check(job.stagingPlan.files.size() == 1,
          "Nuendo host-send job creates one staging file");
    check(job.stagingPlan.files[0].stagedPathUtf8.find("/nuendo/") != std::string::npos ||
              job.stagingPlan.files[0].stagedPathUtf8.find("\\nuendo\\") != std::string::npos,
          "Nuendo staging plan is namespaced by host");
}

static void testProToolsDefaultsToLivePtslSpotPath() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::ProTools;
    request.mode = HarkenSearch::HostSend::DefaultDeliveryModeForHost(request.host);
    request.items.push_back({
        "D:/Library/Foley/cloth rustle.WAV",
        "cloth rustle",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto job = HarkenSearch::HostSend::BuildJob(
        request,
        "C:/Temp/HarkenSearchHostSend");
    check(job.adapterPath == HarkenSearch::HostSend::AdapterPath::ProToolsPtslBridge,
          "Pro Tools defaults to live PTSL bridge adapter");
    check(job.mode == HarkenSearch::HostSend::DeliveryMode::SpotToHost,
          "Pro Tools defaults to live spot-to-host mode");
    check(job.stagingPlan.files[0].extensionLower == ".wav",
          "staging plan lowercases file extension");
    check(job.stagingPlan.files[0].stagedFileName == "cloth rustle.WAV",
          "Pro Tools staging preserves the original single-file basename for clip naming");
}

static void testProToolsStagingPreservesUnicodeSourceName() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::ProTools;
    request.mode = HarkenSearch::HostSend::DeliveryMode::SpotToHost;
    request.items.push_back({
        "D:/Library/中文音效/Inchi Monster Low Grow Long 09.wav",
        "audio-01",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto plan = HarkenSearch::HostSend::BuildStagingPlan(
        request,
        "C:/Temp/HarkenSearchHostSend");
    check(plan.files.size() == 1,
          "Pro Tools staging plan builds one file for one source item");
    check(plan.files[0].stagedFileName == "Inchi Monster Low Grow Long 09.wav",
          "Pro Tools clip naming uses the source filename instead of Auro's temporary display label");
}

static void testHostSendCanStripHarkenSearchProcessingSuffixesForAllDaws() {
    HarkenSearch::HostSend::Request ptRequest;
    ptRequest.host = HarkenSearch::HostSend::HostId::ProTools;
    ptRequest.mode = HarkenSearch::HostSend::DeliveryMode::SpotToHost;
    ptRequest.keepHarkenSearchProcessingSuffix = false;
    ptRequest.items.push_back({
        "D:/HarkenSearchTemp/Inchi_Monster_Low_Grow_Short_31_Org_vol.wav",
        "0_audio-01",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto ptPlan = HarkenSearch::HostSend::BuildStagingPlan(
        ptRequest,
        "C:/Temp/HarkenSearchHostSend");
    check(ptPlan.files[0].stagedFileName == "Inchi_Monster_Low_Grow_Short_31.wav",
          "Pro Tools staging can strip Auro processing suffixes from clip names");

    HarkenSearch::HostSend::Request nuendoRequest;
    nuendoRequest.host = HarkenSearch::HostSend::HostId::Nuendo;
    nuendoRequest.keepHarkenSearchProcessingSuffix = false;
    nuendoRequest.items.push_back({
        "D:/HarkenSearchTemp/Hit_Org_pitch_vol.wav",
        "Hit_Org_pitch_vol",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto nuendoPlan = HarkenSearch::HostSend::BuildStagingPlan(
        nuendoRequest,
        "C:/Temp/HarkenSearchHostSend");
    check(nuendoPlan.files[0].stagedFileName == "0_Hit.wav",
          "Nuendo staging can strip the same Auro processing suffixes before DAW import");
}

static void testHostSendKeepsHarkenSearchProcessingSuffixesByDefault() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::ProTools;
    request.mode = HarkenSearch::HostSend::DeliveryMode::SpotToHost;
    request.items.push_back({
        "D:/HarkenSearchTemp/Hit_Org_pitch_vol.wav",
        "Hit_Org_pitch_vol",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto plan = HarkenSearch::HostSend::BuildStagingPlan(
        request,
        "C:/Temp/HarkenSearchHostSend");
    check(plan.files[0].stagedFileName == "Hit_Org_pitch_vol.wav",
          "HostSend keeps Auro processing suffixes by default for compatibility");
}

static void testStagingPlanSanitizesDisplayLabel() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::Nuendo;
    request.mode = HarkenSearch::HostSend::DefaultDeliveryModeForHost(request.host);
    request.items.push_back({
        "D:/Library/Whoosh/! Weird Name ?.wav",
        "! Weird Name ?",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto plan = HarkenSearch::HostSend::BuildStagingPlan(
        request,
        "C:/Temp/HarkenSearchHostSend");
    check(plan.files.size() == 1,
          "staging plan builds one file for one source item");
    check(plan.files[0].stagedFileName.find("Weird_Name") != std::string::npos,
          "staging file name sanitizes display label for filesystem safety");
}

static void testPayloadJsonCarriesHostAdapterAndSelectionTruth() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::ProTools;
    request.mode = HarkenSearch::HostSend::DeliveryMode::SpotToHost;
    request.copyToProject = true;
    request.items.push_back({
        "D:/Library/Clips/impact.wav",
        "impact",
        HarkenSearch::HostSend::SourceSelectionKind::RegionRender
    });

    const auto job = HarkenSearch::HostSend::BuildJob(
        request,
        "C:/Temp/HarkenSearchHostSend");
    const std::string& payload = job.payloadJson;
    check(payload.find("\"host\":\"protools\"") != std::string::npos,
          "payload encodes target host");
    check(payload.find("\"adapter\":\"protools_ptsl_bridge\"") != std::string::npos,
          "payload encodes live PTSL adapter path");
    check(payload.find("\"mode\":\"spot_to_host\"") != std::string::npos,
          "payload encodes delivery mode");
    check(payload.find("\"selection\":\"region_render\"") != std::string::npos,
          "payload encodes source-selection semantics");
    check(payload.find("\"copyToProject\":true") != std::string::npos,
          "payload preserves copy-to-project flag");
}

static void testDiagnosticsContractUsesStableEventNames() {
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::Nuendo;
    request.items.push_back({
        "D:/Library/Clips/impact.wav",
        "impact",
        HarkenSearch::HostSend::SourceSelectionKind::WholeFile
    });

    const auto job = HarkenSearch::HostSend::BuildJob(
        request,
        "C:/Temp/HarkenSearchHostSend");
    check(job.diagnostics.category == "HostSend",
          "host-send diagnostics use stable category");
    check(job.diagnostics.requestEvent == "HostSend.Requested",
          "host-send diagnostics expose request event");
    check(job.diagnostics.stageReadyEvent == "HostSend.StageReady",
          "host-send diagnostics expose stage-ready event");
    check(job.diagnostics.failEvent == "HostSend.Fail",
          "host-send diagnostics expose fail event");
}

// S8: SourceItem must carry per-file audio metadata for Pro Tools spot positioning.
static void testSourceItemCarriesSpotMetadata() {
    HarkenSearch::HostSend::SourceItem item;
    item.sourcePathUtf8 = "D:/Library/Foley/cloth rustle.WAV";
    item.displayLabel = "cloth rustle";
    item.selectionKind = HarkenSearch::HostSend::SourceSelectionKind::WholeFile;
    // Simulate metadata that would be populated from DB + MetadataReader.
    item.sourceSampleRate = 48000;
    item.channelCount = 2;
    item.durationSamples = 96000;
    item.bwfTimeReferenceSamples = 1440000; // 30s into session at 48kHz

    check(item.sourceSampleRate == 48000, "S8: SourceItem carries sourceSampleRate");
    check(item.channelCount == 2, "S8: SourceItem carries channelCount");
    check(item.durationSamples == 96000, "S8: SourceItem carries durationSamples (1 clip = 2s at 48k)");
    check(item.bwfTimeReferenceSamples == 1440000, "S8: SourceItem carries bwfTimeReferenceSamples");

    // Items with metadata round-trip through BuildJob without corruption.
    HarkenSearch::HostSend::Request request;
    request.host = HarkenSearch::HostSend::HostId::ProTools;
    request.mode = HarkenSearch::HostSend::DeliveryMode::SpotToHost;
    request.items.push_back(item);
    const auto job = HarkenSearch::HostSend::BuildJob(request, "C:/Temp/HarkenSearchHostSend");
    check(job.items.size() == 1, "S8: BuildJob preserves item count");
    check(job.items[0].sourceSampleRate == 48000, "S8: BuildJob preserves sourceSampleRate");
    check(job.items[0].durationSamples == 96000, "S8: BuildJob preserves durationSamples");
    check(job.items[0].bwfTimeReferenceSamples == 1440000, "S8: BuildJob preserves bwfTimeReferenceSamples");

    // Default metadata is zero (safe backward-compat).
    HarkenSearch::HostSend::SourceItem bare;
    bare.sourcePathUtf8 = "D:/test.wav";
    bare.displayLabel = "test";
    check(bare.sourceSampleRate == 0 &&
          bare.channelCount == 0 &&
          bare.durationSamples == 0 &&
          bare.bwfTimeReferenceSamples == 0,
          "S8: SourceItem metadata defaults to zero (backward compat)");
}

int main() {
    testNuendoDefaultsToBridgeImportPath();
    testProToolsDefaultsToLivePtslSpotPath();
    testProToolsStagingPreservesUnicodeSourceName();
    testHostSendCanStripHarkenSearchProcessingSuffixesForAllDaws();
    testHostSendKeepsHarkenSearchProcessingSuffixesByDefault();
    testStagingPlanSanitizesDisplayLabel();
    testPayloadJsonCarriesHostAdapterAndSelectionTruth();
    testDiagnosticsContractUsesStableEventNames();
    testSourceItemCarriesSpotMetadata();
    return g_failed == 0 ? 0 : 1;
}
