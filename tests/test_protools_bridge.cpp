#include "ProToolsBridge.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>
#include <set>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

void TestInstallProbePlan() {
    const auto plan = HarkenSearch::ProTools::BuildInstallProbePlan("C:/Program Files/Avid/Pro Tools");
    Require(Contains(plan.proToolsExePathUtf8, "ProTools.exe"), "Pro Tools exe path is modeled");
    Require(Contains(plan.ptslDllPathUtf8, "PTSL.dll"), "PTSL dll path is modeled");
    Require(Contains(plan.userSdkDirectoryUtf8, "ProToolsSdk"), "user PTSL drop directory is modeled");
    Require(Contains(plan.userPtslDllPathUtf8, "ProToolsSdk"), "legacy user PTSL dll path is modeled");
    Require(Contains(plan.userPtslDllPathUtf8, "PTSL.dll"), "legacy user PTSL dll filename is modeled");
    Require(Contains(plan.userPtslClientDllPathUtf8, "ProToolsSdk"), "user PTSL C++ wrapper drop path is modeled");
    Require(Contains(plan.userPtslClientDllPathUtf8, "PTSLC_CPP.dll"), "user PTSL C++ wrapper DLL filename is modeled");
    Require(Contains(plan.sdkGuideReadmePathUtf8, "README"), "user SDK guide README path is modeled");
    Require(Contains(plan.sdkDownloadUrl, "developer.avid.com/scripting"), "official Avid SDK URL is modeled");
    Require(Contains(plan.ptAafDllPathUtf8, "PtAAF.dll"), "PtAAF dll path is modeled");
    Require(Contains(plan.aafCoApiDllPathUtf8, "AAFCOAPI.dll"), "AAFCOAPI dll path is modeled");
    Require(plan.liveWindowClasses.size() >= 2, "live window class gate is modeled");
    Require(plan.liveWindowClasses[0] == "DigiAppWndClass", "DigiAppWndClass is the primary live host gate");
    Require(!plan.activationWindowClasses.empty(), "activation proxy window gate is modeled");
    Require(plan.activationWindowClasses[0] == "PACEEdenExperienceProxy", "PACE activation proxy class is modeled");
}

void TestUserPtslDropFolderCanSatisfyLiveCapability() {
    const auto plan = HarkenSearch::ProTools::BuildInstallProbePlan("C:/Avid/PT");
    const std::set<std::string> files = {
        plan.proToolsExePathUtf8,
        plan.userPtslClientDllPathUtf8,
    };
    const auto exists = [&](const std::string& path) { return files.count(path) != 0; };
    const auto snapshot = HarkenSearch::ProTools::EvaluateCapability(plan, exists, true, false);
    Require(snapshot.userPtslClientPresent, "user-dropped PTSLC_CPP.dll is detected separately");
    Require(snapshot.readiness == HarkenSearch::ProTools::Readiness::LivePtslPossible,
            "user-dropped PTSLC_CPP.dll can satisfy live PTSL capability");
    Require(Contains(snapshot.summary, "ProToolsSdk"),
            "capability summary names the user drop folder when it is used");
}

void TestUserSdkGuideText() {
    const auto plan = HarkenSearch::ProTools::BuildInstallProbePlan("C:/Avid/PT");
    const auto guide = HarkenSearch::ProTools::BuildUserSdkGuideReadmeText(plan);
    Require(Contains(guide, "PTSLC_CPP.dll"), "guide tells the user which built wrapper DLL to copy");
    Require(Contains(guide, plan.userSdkDirectoryUtf8), "guide includes the local drop directory");
    Require(Contains(guide, plan.sdkDownloadUrl), "guide includes the official Avid download page");
    Require(Contains(guide, "Do not copy Avid DLLs into the Pro Tools install directory"),
            "guide warns against modifying the Pro Tools install directory");
}

void TestBundledRuntimeChecklistAndSendPresentation() {
    const auto plan = HarkenSearch::ProTools::BuildInstallProbePlan("C:/Avid/PT");
    HarkenSearch::ProTools::CapabilitySnapshot missingRuntime;
    missingRuntime.proToolsExePresent = true;
    missingRuntime.proToolsProcessRunning = true;
    missingRuntime.liveHostWindowPresent = true;
    missingRuntime.readiness = HarkenSearch::ProTools::Readiness::InstalledButHostNotReady;
    missingRuntime.failure = HarkenSearch::ProTools::FailureCode::PtslMissing;
    missingRuntime.summary = "PTSLC_CPP.dll missing";

    const auto missingChecklist = HarkenSearch::ProTools::BuildRuntimeSetupChecklist(
        plan,
        missingRuntime,
        false);
    Require(missingChecklist.size() >= 5, "bundled runtime checklist exposes all user-facing setup gates");
    Require(missingChecklist[0].key == "bridge_runtime" &&
                missingChecklist[0].label == "Auro Pro Tools Bridge installed" &&
                !missingChecklist[0].ok,
            "checklist starts with bundled bridge runtime status");
    Require(Contains(missingChecklist[0].detail, "HarkenSearchProToolsBridge.exe"),
            "bridge runtime checklist detail names the missing helper");
    Require(missingChecklist[1].key == "ptsl_runtime" &&
                missingChecklist[1].label == "PTSL runtime bundled" &&
                !missingChecklist[1].ok,
            "checklist treats PTSLC_CPP.dll as bundled runtime, not user SDK work");

    const auto missingPresentation = HarkenSearch::ProTools::BuildTimelineSendPresentation(
        missingRuntime,
        false);
    Require(missingPresentation.actionLabel == "Spot to Pro Tools Timeline",
            "Pro Tools send action has timeline-specific label");
    Require(missingPresentation.capabilityTag == "SDK/runtime missing",
            "missing runtime has explicit capability tag");
    Require(!missingPresentation.sendEnabled,
            "missing runtime disables timeline send");
    Require(Contains(missingPresentation.disabledReason, "Repair Pro Tools bridge runtime"),
            "missing runtime disabled reason points to repair/setup, not manual SDK build");

    HarkenSearch::ProTools::CapabilitySnapshot notRunning;
    notRunning.proToolsExePresent = true;
    notRunning.ptslPresent = true;
    notRunning.userPtslClientPresent = true;
    notRunning.proToolsProcessRunning = false;
    notRunning.liveHostWindowPresent = false;
    notRunning.readiness = HarkenSearch::ProTools::Readiness::AafFallbackPossible;
    notRunning.failure = HarkenSearch::ProTools::FailureCode::HostWindowUnavailable;
    notRunning.summary = "Pro Tools host window is unavailable";
    const auto notRunningPresentation = HarkenSearch::ProTools::BuildTimelineSendPresentation(
        notRunning,
        true);
    Require(notRunningPresentation.capabilityTag == "Pro Tools not running",
            "closed Pro Tools has explicit not-running capability tag");
    Require(!notRunningPresentation.sendEnabled,
            "closed Pro Tools disables timeline send");
    Require(Contains(notRunningPresentation.disabledReason, "Open Pro Tools"),
            "closed Pro Tools disabled reason tells the user to open Pro Tools/session");

    HarkenSearch::ProTools::CapabilitySnapshot ready;
    ready.proToolsExePresent = true;
    ready.ptslPresent = true;
    ready.userPtslClientPresent = true;
    ready.proToolsProcessRunning = true;
    ready.liveHostWindowPresent = true;
    ready.readiness = HarkenSearch::ProTools::Readiness::LivePtslPossible;
    ready.failure = HarkenSearch::ProTools::FailureCode::None;
    ready.summary = "Pro Tools live PTSL spot path can be attempted";
    const auto readyPresentation = HarkenSearch::ProTools::BuildTimelineSendPresentation(ready, true);
    Require(readyPresentation.capabilityTag == "Live PTSL Ready",
            "live-ready state has user-facing capability tag");
    Require(readyPresentation.sendEnabled,
            "live-ready state enables timeline send");
    Require(readyPresentation.recommendedAction == "Spot to Timeline",
            "live-ready state recommends the timeline spot action");
}

void TestProToolsConnectionResetPresentation() {
    HarkenSearch::ProTools::CapabilitySnapshot notRunning;
    notRunning.proToolsExePresent = true;
    notRunning.userPtslClientPresent = true;
    notRunning.proToolsProcessRunning = false;
    notRunning.liveHostWindowPresent = false;
    notRunning.readiness = HarkenSearch::ProTools::Readiness::AafFallbackPossible;
    notRunning.failure = HarkenSearch::ProTools::FailureCode::HostWindowUnavailable;

    const auto notRunningReset = HarkenSearch::ProTools::BuildProToolsConnectionResetPresentation(
        notRunning,
        true);
    Require(notRunningReset.actionLabel == "Reset Pro Tools Connection",
            "reset action has a user-facing label");
    Require(notRunningReset.canResetHarkenSearchBridgeHelpers,
            "reset action can always clean up Auro-owned helper processes");
    Require(!notRunningReset.requiresProToolsRestart,
            "closed Pro Tools does not require a Pro Tools restart to release the listener");
    Require(!notRunningReset.canRequestProToolsClose,
            "closed Pro Tools should not offer a close-Pro-Tools action");
    Require(Contains(notRunningReset.safetySummary, "HarkenSearchProToolsBridge.exe"),
            "reset summary names the Auro-owned helper process");

    HarkenSearch::ProTools::CapabilitySnapshot running;
    running.proToolsExePresent = true;
    running.userPtslClientPresent = true;
    running.proToolsProcessRunning = true;
    running.liveHostWindowPresent = true;
    running.readiness = HarkenSearch::ProTools::Readiness::LivePtslPossible;
    running.failure = HarkenSearch::ProTools::FailureCode::None;

    const auto runningReset = HarkenSearch::ProTools::BuildProToolsConnectionResetPresentation(
        running,
        true);
    Require(runningReset.requiresProToolsRestart,
            "running Pro Tools requires a Pro Tools restart to release its PTSL listener");
    Require(runningReset.canRequestProToolsClose,
            "running Pro Tools offers an explicit user-confirmed close request");
    Require(Contains(runningReset.safetySummary, "31416") &&
                Contains(runningReset.safetySummary, "ProTools.exe"),
            "reset summary explains that the 31416 listener belongs to ProTools.exe");
    Require(Contains(runningReset.closeProToolsLabel, "after saving"),
            "close-Pro-Tools label must remind the user to save the session first");
}

void TestCapabilityMatrix() {
    const auto plan = HarkenSearch::ProTools::BuildInstallProbePlan("C:/Avid/PT");
    const auto existsNone = [](const std::string&) { return false; };
    auto snapshot = HarkenSearch::ProTools::EvaluateCapability(plan, existsNone, false, false);
    Require(snapshot.readiness == HarkenSearch::ProTools::Readiness::NotInstalled, "missing Pro Tools reports not installed");
    Require(snapshot.failure == HarkenSearch::ProTools::FailureCode::ProToolsNotInstalled, "missing Pro Tools failure is typed");

    const std::set<std::string> all = {
        plan.proToolsExePathUtf8,
        plan.ptslDllPathUtf8,
        plan.ptAafDllPathUtf8,
        plan.aafCoApiDllPathUtf8,
    };
    const auto existsAll = [&](const std::string& path) { return all.count(path) != 0; };
    snapshot = HarkenSearch::ProTools::EvaluateCapability(plan, existsAll, false, true);
    Require(snapshot.readiness == HarkenSearch::ProTools::Readiness::InstalledButHostNotReady,
            "PACE activation proxy blocks live Pro Tools readiness");
    Require(snapshot.failure == HarkenSearch::ProTools::FailureCode::ActivationGate,
            "PACE activation gate failure is typed");

    snapshot = HarkenSearch::ProTools::EvaluateCapability(plan, existsAll, true, false);
    Require(snapshot.readiness == HarkenSearch::ProTools::Readiness::LivePtslPossible,
            "PTSL plus live host window allows live PTSL attempt");
    Require(snapshot.failure == HarkenSearch::ProTools::FailureCode::None,
            "live PTSL possible has no failure");

    const std::set<std::string> aafOnly = {
        plan.proToolsExePathUtf8,
        plan.ptAafDllPathUtf8,
        plan.aafCoApiDllPathUtf8,
    };
    const auto existsAafOnly = [&](const std::string& path) { return aafOnly.count(path) != 0; };
    snapshot = HarkenSearch::ProTools::EvaluateCapability(plan, existsAafOnly, false, false);
    Require(snapshot.readiness == HarkenSearch::ProTools::Readiness::InstalledButHostNotReady,
            "missing PTSL blocks live path even if AAF components exist");
    Require(snapshot.failure == HarkenSearch::ProTools::FailureCode::PtslMissing,
            "missing PTSL failure is typed");
}

void TestNativeProbeSmokeIfAvailable() {
#ifdef _WIN32
    const auto snapshot = HarkenSearch::ProTools::ProbeInstalledCapability();
    Require(!snapshot.summary.empty(), "native Pro Tools probe returns a human-readable summary");
    if (snapshot.activationProxyVisible) {
        Require(snapshot.failure == HarkenSearch::ProTools::FailureCode::ActivationGate ||
                    snapshot.liveHostWindowPresent,
                "activation proxy cannot be treated as live host readiness");
    }
    if (snapshot.liveHostWindowPresent) {
        Require(!snapshot.matchedLiveWindowClass.empty(), "native probe preserves matched live window class");
    }
    if (snapshot.activationProxyVisible) {
        Require(!snapshot.matchedActivationWindowClass.empty(), "native probe preserves matched activation window class");
    }
#endif
}

void TestProtocolJson() {
    const auto live = HarkenSearch::ProTools::BuildLivePtslSpotMessage(
        "pt-live-1",
        "C:\\Auro Stage\\pt \"hit\".wav",
        "pt \"hit\".wav");
    const auto liveJson = HarkenSearch::ProTools::BuildProtocolJson(live);
    Require(Contains(liveJson, "\"protocol\":\"auro-protools-host-send-v1\""), "Pro Tools protocol version is serialized");
    Require(Contains(liveJson, "\"path\":\"live_ptsl_spot\""), "live PTSL path is serialized");
    Require(Contains(liveJson, "\"spotToTimeline\":true"), "live path keeps direct timeline intent");
    Require(Contains(liveJson, "pt \\\"hit\\\".wav"), "display name is JSON escaped");

    const auto aaf = HarkenSearch::ProTools::BuildAafFallbackMessage(
        "pt-aaf-1",
        "C:\\Auro Stage\\pt-aaf.wav",
        "pt-aaf.wav");
    const auto aafJson = HarkenSearch::ProTools::BuildProtocolJson(aaf);
    Require(Contains(aafJson, "\"path\":\"aaf_fallback\""), "AAF fallback path is serialized separately");
    Require(Contains(aafJson, "\"spotToTimeline\":false"), "AAF fallback does not claim live timeline spot");
}

void TestLivePtslHelperRequestJson() {
    HarkenSearch::ProTools::LivePtslSpotRequest request;
    request.correlationId = "pt-live-req-1";
    request.target = "selected_track_or_edit_cursor";
    request.files.push_back({"C:\\Auro Stage\\hit.wav", "hit.wav"});
    request.files.push_back({"C:\\Auro Stage\\rise.wav", "rise.wav"});

    const auto json = HarkenSearch::ProTools::BuildLivePtslSpotRequestJson(request);
    Require(Contains(json, "\"protocol\":\"auro-protools-host-send-v1\""),
            "live helper request carries protocol");
    Require(Contains(json, "\"path\":\"live_ptsl_spot\""),
            "live helper request carries live spot path");
    Require(Contains(json, "\"target\":\"selected_track_or_edit_cursor\""),
            "live helper request carries selected/edit target");
    Require(Contains(json, "\"files\""),
            "live helper request carries staged file list");
    Require(Contains(json, "\"stagedPathUtf8\""),
            "live helper request uses the field name consumed by HarkenSearchProToolsBridgeMain");
    Require(Contains(json, "\"displayNameUtf8\""),
            "live helper request uses the display-name field consumed by HarkenSearchProToolsBridgeMain");
    Require(!Contains(json, "\"stagedPath\":\""),
            "live helper request must not use the legacy field name that caused emptyRequest");
    Require(Contains(json, "C:\\\\Auro Stage\\\\hit.wav"),
            "live helper request serializes staged path");
}

void TestLivePtslResponseTruthContract() {
    const std::string okJson =
        "{\"ok\":true,\"path\":\"live_ptsl_spot\",\"insertedCount\":1,"
        "\"sessionName\":\"Session A\",\"trackName\":\"DX 1\","
        "\"ptslCommandTrace\":[\"RegisterConnection\",\"HostReadyCheck\",\"Import\"]}";
    const auto ok = HarkenSearch::ProTools::ParseLivePtslSpotResponseJson(okJson);
    Require(ok.parseOk, "live response parser accepts valid JSON");
    Require(ok.ok, "live response preserves ok=true");
    Require(ok.path == HarkenSearch::ProTools::DeliveryPath::LivePtslSpot,
            "live response preserves live PTSL path");
    Require(ok.insertedCount == 1, "live response preserves inserted count");
    Require(ok.sessionName == "Session A", "live response preserves session name");
    Require(ok.trackName == "DX 1", "live response preserves track name");
    Require(ok.ptslCommandTrace.size() == 3, "live response preserves PTSL command trace");

    const std::string failJson =
        "{\"ok\":false,\"path\":\"live_ptsl_spot\",\"insertedCount\":0,"
        "\"errorCode\":\"helperUnavailable\",\"errorMessage\":\"HarkenSearchProToolsBridge.exe not found\"}";
    const auto fail = HarkenSearch::ProTools::ParseLivePtslSpotResponseJson(failJson);
    Require(fail.parseOk, "failure response parser accepts valid JSON");
    Require(!fail.ok, "failure response preserves ok=false");
    Require(fail.errorCode == "helperUnavailable",
            "failure response preserves typed error code");
    Require(fail.insertedCount == 0,
            "failure response cannot imply inserted files");
}

void TestHelperUnavailableCannotBeSuccess() {
    HarkenSearch::ProTools::LivePtslDispatchResult result;
    result.helperAvailable = false;
    result.response.parseOk = true;
    result.response.ok = false;
    result.response.errorCode = "helperUnavailable";
    result.response.errorMessage = "HarkenSearchProToolsBridge.exe not found";

    Require(!HarkenSearch::ProTools::IsLivePtslSpotSuccess(result),
            "helper unavailable is never a live spot success");
}


void TestHelperSourceUsesRealPtslSdkContract() {
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    Require(!helperSource.empty(), "helper source must be readable from direct run and CTest working directories");
    Require(Contains(helperSource, "CppPTSLClient.h"),
            "helper must include the real Avid C++ PTSL client header");
    Require(Contains(helperSource, "CppPTSLRequest"),
            "helper must issue real PTSL requests through the SDK wrapper");
    Require(Contains(helperSource, "RegisterConnection"),
            "helper must register the PTSL connection before commands");
    Require(Contains(helperSource, "HostReadyCheck"),
            "helper must run host ready check before import/spot");
    Require(Contains(helperSource, "SkipHostLaunch::No"),
            "helper must preserve the historically working PTSL client launch mode from the successful send commit");
    Require(Contains(helperSource, "ImportFilesToNewTrackAtSamples"),
            "helper must keep the historically working new-track import fallback so Send to Pro Tools does not regress to no-delivery");
    Require(Contains(helperSource, "MDestination_NewTrack"),
            "helper must retain a last-resort Pro Tools delivery fallback that can create a new track when selected-track spot is not available");
    Require(Contains(helperSource, "legacyNewTrackFallback"),
            "helper must report the new-track fallback truthfully instead of claiming selected-track spot");
    Require(!Contains(helperSource, "currentSelectionImport"),
            "helper must not report typed Import completion as a confirmed timeline insertion");
    Require(Contains(helperSource, "ResolveTimelineTargetAudioTrack") &&
                Contains(helperSource, "CId_GetTrackList") &&
                Contains(helperSource, "TLFilter_All") &&
                Contains(helperSource, "has_edit_selection"),
            "helper must use SDK JSON GetTrackList all-track data to resolve the edit-selection track before placing clips");
    Require(Contains(helperSource, "singleAudioTrackFallback"),
            "helper must safely use a single eligible audio track when Pro Tools exposes no selected/edit-selection track");
    Require(Contains(helperSource, "CId_ImportAudioToClipList"),
            "helper must import audio into the clip list before selected-track placement");
    Require(Contains(helperSource, "CId_SpotClipsByID"),
            "helper must spot imported clips by ID onto the selected track");
    Require(Contains(helperSource, "ImportAudioToClipListFallback") &&
                Contains(helperSource, "MDestination_ClipList") &&
                Contains(helperSource, "clipListFallbackNoTimelineTargetAudioTrack"),
            "helper must restore basic Pro Tools send with a truthful Clip List fallback when no timeline target audio track can be resolved");
    Require(!Contains(helperSource, "selectedTrackUnsupported"),
            "helper must not permanently block the normal selected-track Pro Tools send path");
    Require(helperSource.find("ResolveTimelineTargetAudioTrack(client") < helperSource.find("ImportAudioToClipList(client"),
            "timeline target resolution must happen before importing and spotting clip IDs");
    // M5: CId_Spot IS used, but only in the mid path (Import to ClipList first, then Spot to selected track).
    // The old ban was against Import(NewTrack) + Spot, which created 2 tracks. Verify the correct usage:
    // - typed .Spot() API remains forbidden (deprecated, gives inconsistent results)
    // - CId_Spot is only allowed inside SpotSelectedClipsAt (mid path helper), never after NewTrack import
    Require(!Contains(helperSource, ".Spot("),
            "helper must not call the typed .Spot() API — use JSON SendPtsl(CId_Spot) via SpotSelectedClipsAt instead");
    const auto spotSelectedPos = helperSource.find("SpotSelectedClipsAt");
    const auto midPathPos = helperSource.find("ExecuteMidSpotPath");
    Require(spotSelectedPos != std::string::npos && midPathPos != std::string::npos,
            "CId_Spot usage must be scoped to the SpotSelectedClipsAt helper called only from ExecuteMidSpotPath");
    Require(!Contains(helperSource, "ptslAdapterNotLinked"),
            "helper must not report the SDK-linked path as adapter-not-linked");

    const std::string bridgeSource = [] {
        const char* candidates[] = {
            "src/ProToolsBridge.cpp",
            "../src/ProToolsBridge.cpp",
            "../../src/ProToolsBridge.cpp",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    Require(!bridgeSource.empty(), "ProToolsBridge source must be readable from direct run and CTest working directories");
    Require(Contains(bridgeSource, "kProToolsBridgeTimeoutMs"),
            "Auro dispatch must use a named Pro Tools helper timeout instead of a magic wait");
    Require(Contains(bridgeSource, "kProToolsBridgeTimeoutMs = 30000"),
            "Auro dispatch must preserve the previously working 30s helper wait budget instead of timing out early");
    Require(Contains(bridgeSource, "WAIT_TIMEOUT") &&
                Contains(bridgeSource, "TerminateProcess") &&
                Contains(bridgeSource, "helperTimedOut"),
            "Auro dispatch must kill and classify a stuck Pro Tools helper instead of leaving Sending... hanging");
}

void TestLivePtslSmokeContract() {
    const std::string headerSource = [] {
        const char* candidates[] = {
            "src/ProToolsBridge.h",
            "../src/ProToolsBridge.h",
            "../../src/ProToolsBridge.h",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    Require(!headerSource.empty() && !helperSource.empty(), "Pro Tools smoke contract sources must be readable");
    Require(Contains(headerSource, "DispatchLivePtslSmokeTest"),
            "ProToolsBridge must expose a real PTSL smoke dispatch API for Test Pro Tools Connection");
    Require(Contains(helperSource, "--smoke") &&
                Contains(helperSource, "ptslSmoke") &&
                Contains(helperSource, "RegisterConnection") &&
                Contains(helperSource, "HostReadyCheck"),
            "HarkenSearchProToolsBridge must support a no-audio smoke path that proves RegisterConnection/HostReadyCheck before import");
    Require(Contains(helperSource, "PTSL smoke does not import audio") &&
                Contains(helperSource, "SmokePtslConnection"),
            "smoke mode must be visibly separated from the audio import/new-track fallback path");
}

void TestPtslPersistentBridgeContract() {
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    const std::string bridgeSource = [] {
        const char* candidates[] = {
            "src/ProToolsBridge.cpp",
            "../src/ProToolsBridge.cpp",
            "../../src/ProToolsBridge.cpp",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    Require(!helperSource.empty() && !bridgeSource.empty(), "PTSL persistent bridge sources must be readable");
    Require(Contains(helperSource, "--server") &&
                Contains(helperSource, "RunPersistentQueueServer") &&
                Contains(helperSource, "ReusePersistentClient") &&
                Contains(helperSource, "ExecuteSpotRequestWithPreparedClient"),
            "helper must keep one persistent PTSL client alive for repeated sends instead of exiting after the first import");
    Require(Contains(bridgeSource, "DispatchLivePtslPersistentRequest") &&
                Contains(bridgeSource, "--server --queue") &&
                Contains(bridgeSource, "persistentHarkenSearchProToolsBridge") &&
                Contains(bridgeSource, "STILL_ACTIVE"),
            "Auro dispatch must route real sends through the persistent helper server while smoke can stay one-shot");
    Require(Contains(helperSource, "PreparePtslSession(client, response, false, false)") &&
                !Contains(helperSource, "PreparePtslSession(client, response, true, true)"),
            "cross-process SetSessionId cache reuse must stay disabled because live Pro Tools refused it on the second send");
    Require(Contains(helperSource, "ClearCachedPtslSessionId") &&
                Contains(bridgeSource, "ClearProToolsSessionCache"),
            "Auro reset must clear stale cached PTSL session state");
    Require(!Contains(helperSource, "CId_CloseSession") &&
                !Contains(helperSource, "CloseSession(") &&
                !Contains(helperSource, "save_on_close"),
            "helper must not use CloseSession as connection cleanup because it can close the user's Pro Tools session");
}

void TestProToolsStartupResetContract() {
    const std::string headerSource = [] {
        const char* candidates[] = {
            "src/ProToolsBridge.h",
            "../src/ProToolsBridge.h",
            "../../src/ProToolsBridge.h",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    const std::string bridgeSource = [] {
        const char* candidates[] = {
            "src/ProToolsBridge.cpp",
            "../src/ProToolsBridge.cpp",
            "../../src/ProToolsBridge.cpp",
        };
        for (const char* candidate : candidates) {
            std::ifstream in(candidate, std::ios::binary);
            if (in) {
                return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
        }
        return std::string{};
    }();
    Require(!headerSource.empty() && !bridgeSource.empty(), "Pro Tools startup reset sources must be readable");
    Require(Contains(headerSource, "ResetProToolsBridgeOnHarkenSearchStartup"),
            "ProToolsBridge must expose an Auro-startup bridge reset API");
    Require(Contains(bridgeSource, "ResetProToolsBridgeOnHarkenSearchStartup") &&
                Contains(bridgeSource, "ResetProToolsConnection(false)"),
            "startup reset must reuse the safe helper/cache reset path");
    Require(!Contains(bridgeSource, "ResetProToolsConnection(true) /* startup") &&
                Contains(bridgeSource, "ProTools.exe was not closed"),
            "startup reset must not request or force a Pro Tools close");
    Require(Contains(bridgeSource, "Auro startup reset Pro Tools bridge"),
            "startup reset must leave a user-visible status explaining the automatic interface restart");
}

void TestStableKeys() {
    Require(std::string(HarkenSearch::ProTools::DeliveryPathKey(HarkenSearch::ProTools::DeliveryPath::LivePtslSpot)) == "live_ptsl_spot",
            "live PTSL key is stable");
    Require(std::string(HarkenSearch::ProTools::ReadinessKey(HarkenSearch::ProTools::Readiness::LivePtslPossible)) == "livePtslPossible",
            "readiness key is stable");
    Require(std::string(HarkenSearch::ProTools::FailureCodeKey(HarkenSearch::ProTools::FailureCode::ActivationGate)) == "activationGate",
            "activation gate key is stable");
}

} // namespace

void TestSpotModeAndTrackFallbackKeys() {
    using namespace HarkenSearch::ProTools;
    Require(std::string(SpotModeKey(SpotMode::AtCursor)) == "at_cursor", "AtCursor -> at_cursor");
    Require(std::string(SpotModeKey(SpotMode::ToTimecode)) == "to_timecode", "ToTimecode -> to_timecode");
    Require(std::string(SpotModeKey(SpotMode::ToSelectionIn)) == "to_selection_in", "ToSelectionIn -> to_selection_in");
    Require(SpotModeFromKey("at_cursor") == SpotMode::AtCursor, "at_cursor round-trip");
    Require(SpotModeFromKey("to_timecode") == SpotMode::ToTimecode, "to_timecode round-trip");
    Require(SpotModeFromKey("to_selection_in") == SpotMode::ToSelectionIn, "to_selection_in round-trip");
    Require(SpotModeFromKey("unknown_junk") == SpotMode::AtCursor,
            "Unknown key falls back to AtCursor (safe default)");

    Require(std::string(TrackFallbackKey(TrackFallback::AutoCreateNew)) == "auto_create_new",
            "AutoCreateNew -> auto_create_new");
    Require(std::string(TrackFallbackKey(TrackFallback::RequireExplicit)) == "require_explicit",
            "RequireExplicit -> require_explicit");
    Require(TrackFallbackFromKey("auto_create_new") == TrackFallback::AutoCreateNew,
            "auto_create_new round-trip");
    Require(TrackFallbackFromKey("require_explicit") == TrackFallback::RequireExplicit,
            "require_explicit round-trip");
    Require(TrackFallbackFromKey("garbage") == TrackFallback::AutoCreateNew,
            "Unknown fallback key defaults to AutoCreateNew (user-requested behavior)");
}

void TestLivePtslRequestJsonCarriesV2Fields() {
    HarkenSearch::ProTools::LivePtslSpotRequest request;
    request.correlationId = "pt-live-v2-1";
    request.spotMode = HarkenSearch::ProTools::SpotMode::ToTimecode;
    request.trackFallback = HarkenSearch::ProTools::TrackFallback::AutoCreateNew;

    HarkenSearch::ProTools::LivePtslSpotFile f1;
    f1.stagedPathUtf8 = "C:\\Auro Stage\\hit.wav";
    f1.displayNameUtf8 = "hit.wav";
    f1.sourceSampleRate = 48000;
    f1.channelCount = 2;
    f1.durationSamples = 96000;
    f1.bwfTimeReferenceSamples = 4800000;
    request.files.push_back(f1);

    const auto json = HarkenSearch::ProTools::BuildLivePtslSpotRequestJson(request);
    Require(Contains(json, "\"spotMode\":\"to_timecode\""), "spotMode is serialized");
    Require(Contains(json, "\"trackFallback\":\"auto_create_new\""), "trackFallback is serialized");
    Require(Contains(json, "\"sourceSampleRate\":48000"), "sourceSampleRate is serialized per file");
    Require(Contains(json, "\"channelCount\":2"), "channelCount is serialized per file");
    Require(Contains(json, "\"durationSamples\":96000"), "durationSamples is serialized per file");
    Require(Contains(json, "\"bwfTimeReferenceSamples\":4800000"),
            "BWF time reference is serialized per file");
}

void TestLivePtslResponseV2RoundTrip() {
    HarkenSearch::ProTools::LivePtslSpotResponse original;
    original.ok = true;
    original.parseOk = true;
    original.path = HarkenSearch::ProTools::DeliveryPath::LivePtslSpot;
    original.insertedCount = 3;
    original.sessionName = "SFX-01.ptx";
    original.trackName = "SFX 01";
    original.ptslCommandTrace = {"RegisterConnection", "GetPTSLVersion",
                                  "ImportAudioToClipList", "SpotClipsByID"};
    original.ptVersionMajor = 2025;
    original.ptVersionMinor = 6;
    original.ptVersionRevision = 12;
    original.sessionSampleRate = 48000;
    original.pathTaken = "modern-2025.06+";
    original.resolvedTrackId = "{00000000-aaaa-1111}";
    original.resolvedTrackName = "SFX 01";
    original.trackWasCreated = false;
    original.perClipSampleOffsets = {480000, 528000, 576000};

    const auto json = HarkenSearch::ProTools::BuildLivePtslSpotResponseJson(original);
    const auto parsed = HarkenSearch::ProTools::ParseLivePtslSpotResponseJson(json);

    Require(parsed.parseOk, "v2 response JSON parses");
    Require(parsed.ok, "ok round-trips");
    Require(parsed.ptVersionMajor == 2025, "ptVersionMajor round-trips");
    Require(parsed.ptVersionMinor == 6, "ptVersionMinor round-trips");
    Require(parsed.ptVersionRevision == 12, "ptVersionRevision round-trips");
    Require(parsed.sessionSampleRate == 48000, "sessionSampleRate round-trips");
    Require(parsed.pathTaken == "modern-2025.06+", "pathTaken round-trips");
    Require(parsed.resolvedTrackId == "{00000000-aaaa-1111}", "resolvedTrackId round-trips");
    Require(parsed.resolvedTrackName == "SFX 01", "resolvedTrackName round-trips");
    Require(!parsed.trackWasCreated, "trackWasCreated round-trips (false)");
    Require(parsed.perClipSampleOffsets.size() == 3,
            "perClipSampleOffsets size preserved");
    Require(parsed.perClipSampleOffsets[0] == 480000 &&
            parsed.perClipSampleOffsets[1] == 528000 &&
            parsed.perClipSampleOffsets[2] == 576000,
            "perClipSampleOffsets values preserved (sequential layout)");
}

void TestMidPathContract() {
    // M1-M5: Mid tier path for PT 2023.09 ≤ v < 2025.06 uses SelectTracksByName + Import(ClipList) + Spot.
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* c : candidates) {
            std::ifstream in(c, std::ios::binary);
            if (in) return std::string(std::istreambuf_iterator<char>(in), {});
        }
        return std::string{};
    }();
    Require(!helperSource.empty(), "helper source readable for M-series mid-path contract check");

    // M1: SelectTracksByName helper
    Require(Contains(helperSource, "SelectTracksByNamePtsl"),
            "M1: helper must contain SelectTracksByNamePtsl");
    Require(Contains(helperSource, "CId_SelectTracksByName"),
            "M1: mid path must issue CId_SelectTracksByName (73) to Pro Tools");
    Require(Contains(helperSource, "SMode_Replace"),
            "M1: SelectTracksByName must use SMode_Replace to clear prior selection");

    // M2: ImportSingleFileToClipListMid helper — must use OLD CId_Import(2), NOT CId_ImportAudioToClipList(123).
    Require(Contains(helperSource, "ImportSingleFileToClipListMid"),
            "M2: helper must contain ImportSingleFileToClipListMid");
    Require(Contains(helperSource, "client.Import(request)") &&
                Contains(helperSource, "PTSLC_CPP::ImportRequest request"),
            "M2: mid path Import must use the typed legacy Import request, not CId_ImportAudioToClipList (2025.06+)");
    Require(Contains(helperSource, "MDestination_ClipList"),
            "M2: mid path Import must target MDestination_ClipList");
    Require(Contains(helperSource, "IType_Audio"),
            "M2: mid path Import must specify IType_Audio import type");

    // M3: SpotSelectedClipsAt helper — must use CId_Spot(29), NOT CId_SpotClipsByID(124).
    Require(Contains(helperSource, "SpotSelectedClipsAt"),
            "M3: helper must contain SpotSelectedClipsAt");
    Require(Contains(helperSource, "CId_Spot,"),
            "M3: mid path must issue CId_Spot (29), not CId_SpotClipsByID (2025.06+)");
    Require(Contains(helperSource, "SLType_Start"),
            "M3: Spot location must use SLType_Start");
    Require(Contains(helperSource, "TLType_Samples"),
            "M3: Spot must use TLType_Samples time base");

    // M4: ExecuteMidSpotPath main function
    Require(Contains(helperSource, "ExecuteMidSpotPath"),
            "M4: helper must contain ExecuteMidSpotPath function");
    Require(Contains(helperSource, "mid-2023.09+"),
            "M4: mid path success sets pathTaken to 'mid-2023.09+'");
    Require(Contains(helperSource, "midPathPartial"),
            "M4: mid path partial-success errorCode is 'midPathPartial' (user visibility)");

    // M5: version gate injection in main dispatch
    Require(Contains(helperSource, "isMid"),
            "M5: ExecuteSpotRequestWithPreparedClient must have isMid version gate");
    Require(Contains(helperSource, "ptVersionMajor == 2023 && response.ptVersionMinor >= 9"),
            "M5: mid path boundary must include Pro Tools 2023.09 as the low edge");
    // Order matters: in the dispatch function, mid branch must appear AFTER modern block
    // and BEFORE the S7 legacy path comment marker (which is unique in the source).
    const auto modernBranchPos = helperSource.find("if (isModern) {");
    const auto midBranchPos    = helperSource.find("if (isMid) {");
    const auto legacyCommentPos = helperSource.find("── S7 Legacy path");
    Require(modernBranchPos != std::string::npos && midBranchPos != std::string::npos && legacyCommentPos != std::string::npos,
            "M5: modern/mid/legacy branch markers must all be present in dispatch");
    Require(modernBranchPos < midBranchPos && midBranchPos < legacyCommentPos,
            "M5: dispatch order must be modern → mid → legacy (highest version first)");
}

void TestVersionAndSrFetchContract() {
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* c : candidates) {
            std::ifstream in(c, std::ios::binary);
            if (in) return std::string(std::istreambuf_iterator<char>(in), {});
        }
        return std::string{};
    }();
    Require(!helperSource.empty(), "helper source readable for S3 contract check");
    Require(Contains(helperSource, "FetchPTSLVersion"),
            "S3: helper must contain FetchPTSLVersion");
    Require(Contains(helperSource, "CId_GetPTSLVersion"),
            "S3: helper must issue CId_GetPTSLVersion to PT");
    Require(Contains(helperSource, "FetchSessionSampleRate"),
            "S3: helper must contain FetchSessionSampleRate");
    Require(Contains(helperSource, "CId_GetSessionSampleRate"),
            "S3: helper must issue CId_GetSessionSampleRate to PT");
    // Both must be called from the main spot execution path
    Require(Contains(helperSource, "FetchPTSLVersion(client, response)"),
            "S3: FetchPTSLVersion must be wired into ExecuteSpotRequestWithPreparedClient");
    Require(Contains(helperSource, "FetchSessionSampleRate(client, response)"),
            "S3: FetchSessionSampleRate must be wired into ExecuteSpotRequestWithPreparedClient");
}

void TestResolverSixTierContract() {
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* c : candidates) {
            std::ifstream in(c, std::ios::binary);
            if (in) return std::string(std::istreambuf_iterator<char>(in), {});
        }
        return std::string{};
    }();
    Require(!helperSource.empty(), "helper source readable for S5 contract check");
    // 6-tier labels present
    Require(Contains(helperSource, "editSelectionExplicit"),
            "S5: tier-1 editSelectionExplicit resolution code present");
    Require(Contains(helperSource, "selectedExplicit"),
            "S5: tier-2 selectedExplicit resolution code present");
    Require(Contains(helperSource, "editSelectionImplicit"),
            "S5: tier-3 editSelectionImplicit resolution code present");
    Require(Contains(helperSource, "TrackAttributeIsExplicit"),
            "S5: TrackAttributeIsExplicit helper must distinguish SetExplicitly from implicit");
    // Old too-strict error message gone; new message present
    Require(!Contains(helperSource, "PTSL did not expose exactly one"),
            "S5: old exact-one error message must be replaced by tier-6 fallback message");
    Require(Contains(helperSource, "TAState_SetExplicitly"),
            "S5: resolver must check TAState_SetExplicitly string");
}

void TestLivePtslLegacyResponseStaysCompatible() {
    // Older helpers won't emit v2 fields; the parser must not crash and must return sane defaults.
    const std::string legacyJson =
        "{\"ok\":true,\"path\":\"live_ptsl_spot\",\"insertedCount\":1,"
        "\"sessionName\":\"A\",\"trackName\":\"B\","
        "\"ptslCommandTrace\":[\"RegisterConnection\"]}";
    const auto parsed = HarkenSearch::ProTools::ParseLivePtslSpotResponseJson(legacyJson);
    Require(parsed.parseOk && parsed.ok, "legacy response still parses");
    Require(parsed.ptVersionMajor == 0 && parsed.ptVersionMinor == 0,
            "missing version fields default to zero");
    Require(parsed.sessionSampleRate == 0, "missing session SR defaults to zero");
    Require(parsed.pathTaken.empty(), "missing pathTaken stays empty");
    Require(!parsed.trackWasCreated, "missing trackWasCreated defaults to false");
    Require(parsed.perClipSampleOffsets.empty(), "missing offset array defaults to empty");
}

void TestTimelineBaseSelectionContract() {
    using HarkenSearch::ProTools::SelectTimelineBaseSamples;
    using HarkenSearch::ProTools::SpotMode;

    Require(SelectTimelineBaseSamples(SpotMode::AtCursor, "20363413", "15794033") == "20363413",
            "AtCursor must prefer the current non-zero edit cursor/in_time over stale play_start_marker_time");
    Require(SelectTimelineBaseSamples(SpotMode::AtCursor, "0", "15794033") == "15794033",
            "AtCursor keeps the prior guard: zero in_time falls back to play_start_marker_time");
    Require(SelectTimelineBaseSamples(SpotMode::AtCursor, "", "15794033") == "15794033",
            "AtCursor falls back to play_start_marker_time when in_time is missing");
    Require(SelectTimelineBaseSamples(SpotMode::ToSelectionIn, "20363413", "15794033") == "20363413",
            "ToSelectionIn remains bound to the edit-selection start");
}

void TestPtslImportPathEncodingContract() {
    const std::string helperSource = [] {
        const char* candidates[] = {
            "src/HarkenSearchProToolsBridgeMain.cpp",
            "../src/HarkenSearchProToolsBridgeMain.cpp",
            "../../src/HarkenSearchProToolsBridgeMain.cpp",
        };
        for (const char* c : candidates) {
            std::ifstream in(c, std::ios::binary);
            if (in) return std::string(std::istreambuf_iterator<char>(in), {});
        }
        return std::string{};
    }();
    Require(!helperSource.empty(), "helper source readable for PTSL import path encoding contract");
    Require(Contains(helperSource, "Utf8PathForPtslImport"),
            "helper must adapt UTF-8 staged paths before passing them into PTSL import commands");
    Require(!Contains(helperSource, "filesList.push_back(file.stagedPathUtf8)"),
            "helper must not pass raw UTF-8 stagedPathUtf8 directly into typed PTSL ImportRequest file lists");
    Require(Contains(helperSource, "filesList.push_back(Utf8PathForPtslImport(file.stagedPathUtf8))"),
            "typed PTSL ImportRequest file lists must use the native-codepage path adapter");
}

int main() {
    TestInstallProbePlan();
    TestUserSdkGuideText();
    TestBundledRuntimeChecklistAndSendPresentation();
    TestProToolsConnectionResetPresentation();
    TestUserPtslDropFolderCanSatisfyLiveCapability();
    TestCapabilityMatrix();
    TestNativeProbeSmokeIfAvailable();
    TestProtocolJson();
    TestLivePtslHelperRequestJson();
    TestLivePtslResponseTruthContract();
    TestHelperUnavailableCannotBeSuccess();
    TestHelperSourceUsesRealPtslSdkContract();
    TestLivePtslSmokeContract();
    TestPtslPersistentBridgeContract();
    TestProToolsStartupResetContract();
    TestStableKeys();
    TestSpotModeAndTrackFallbackKeys();
    TestLivePtslRequestJsonCarriesV2Fields();
    TestLivePtslResponseV2RoundTrip();
    TestLivePtslLegacyResponseStaysCompatible();
    TestVersionAndSrFetchContract();
    TestResolverSixTierContract();
    TestMidPathContract();
    TestTimelineBaseSelectionContract();
    TestPtslImportPathEncodingContract();
    std::cout << "TestProToolsBridge passed\n";
    return 0;
}
