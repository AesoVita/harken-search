// Test: Send-to-DAW Named Pipe protocol validation
// Self-contained — uses a mock pipe server thread, no REAPER needed.
// Pattern: print PASS/FAIL per test, return 0 on all-pass, 1 on any failure.

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static int g_failed = 0;

#define TEST(name) std::cout << "TEST: " << name << " ... ";
#define PASS() std::cout << "PASS\n";
#define FAIL(msg) do { std::cout << "FAIL: " << msg << "\n"; g_failed++; } while (0)

// Pipe name must match Auro's protocol (test-only name to avoid real instance)
#define AURO_PIPE_NAME L"\\\\.\\pipe\\HarkenSearchReaperBridgeTest"

// ============================================================================
// Mock pipe server — simulates reaper_harken.dll behavior
// ============================================================================

static std::atomic<bool> g_serverRunning{false};
static std::atomic<int> g_requestsHandled{0};

// Response mode for the mock server
enum class MockMode {
    Normal,     // Reply OK for PING, OK for INSERT
    InsertFail, // Reply ERR for INSERT
    NoResponse, // Don't respond (timeout test)
};
static std::atomic<MockMode> g_mockMode{MockMode::Normal};

static DWORD WINAPI MockPipeServer(LPVOID) {
    while (g_serverRunning.load()) {
        HANDLE hPipe = CreateNamedPipeW(
            AURO_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096, 100, NULL);
        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(50);
            continue;
        }

        if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            continue;
        }

        char buf[4096] = {};
        DWORD bytesRead = 0;
        if (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            std::string msg(buf);
            while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
                msg.pop_back();

            MockMode mode = g_mockMode.load();

            if (mode == MockMode::NoResponse) {
                // Don't write anything back
            } else if (msg == "PING") {
                const char* resp = "OK|42\n";
                DWORD w;
                WriteFile(hPipe, resp, (DWORD)strlen(resp), &w, NULL);
            } else if (msg.size() >= 7 && msg.substr(0, 7) == "INSERT|") {
                if (mode == MockMode::InsertFail) {
                    const char* resp = "ERR|InsertMedia failed\n";
                    DWORD w;
                    WriteFile(hPipe, resp, (DWORD)strlen(resp), &w, NULL);
                } else {
                    const char* resp = "OK\n";
                    DWORD w;
                    WriteFile(hPipe, resp, (DWORD)strlen(resp), &w, NULL);
                }
            } else {
                const char* resp = "ERR|Unknown command\n";
                DWORD w;
                WriteFile(hPipe, resp, (DWORD)strlen(resp), &w, NULL);
            }
            FlushFileBuffers(hPipe);
            g_requestsHandled++;
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
    return 0;
}

// ============================================================================
// Minimal pipe client — mirrors Auro's HarkenSearchPipeSend logic
// ============================================================================

static bool TestPipeSend(const std::string& message, std::string& response, int timeoutMs = 2000) {
    // Server recreates the pipe each iteration; brief retry avoids racing CreateNamedPipe.
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    const DWORD connectDeadline = GetTickCount() + 2000;
    while (GetTickCount() < connectDeadline) {
        hPipe = CreateFileW(AURO_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
                             0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE)
            break;
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_BUSY || err == ERROR_FILE_NOT_FOUND) {
            Sleep(10);
            continue;
        }
        response = "ERR|Cannot connect";
        return false;
    }
    if (hPipe == INVALID_HANDLE_VALUE) {
        response = "ERR|Cannot connect";
        return false;
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, NULL, NULL);

    DWORD written = 0;
    if (!WriteFile(hPipe, message.c_str(), (DWORD)message.size(), &written, NULL)) {
        CloseHandle(hPipe);
        response = "ERR|Write failed";
        return false;
    }

    char buf[1024] = {};
    DWORD bytesRead = 0;
    DWORD startTick = GetTickCount();
    bool gotResponse = false;
    while ((GetTickCount() - startTick) < (DWORD)timeoutMs) {
        DWORD avail = 0;
        if (PeekNamedPipe(hPipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            if (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                response = buf;
                gotResponse = true;
                break;
            }
        }
        Sleep(10);
    }
    CloseHandle(hPipe);
    if (!gotResponse) {
        response = "ERR|Timeout";
        return false;
    }
    while (!response.empty() && (response.back() == '\n' || response.back() == '\r'))
        response.pop_back();
    return true;
}

// ============================================================================
// Tests
// ============================================================================

static void TestPingProtocol() {
    TEST("PING protocol");
    std::string resp;
    if (!TestPipeSend("PING\n", resp)) {
        FAIL(resp);
        return;
    }
    if (resp.substr(0, 2) != "OK") {
        FAIL("Expected OK, got: " + resp);
        return;
    }
    if (resp.find("42") == std::string::npos) {
        FAIL("Expected action ID 42 in response: " + resp);
        return;
    }
    PASS();
}

static void TestInsertSingleFile() {
    TEST("INSERT single file");
    g_mockMode.store(MockMode::Normal);
    std::string resp;
    std::string msg = "INSERT|E:\\Audio\\test.wav\n";
    if (!TestPipeSend(msg, resp)) {
        FAIL(resp);
        return;
    }
    if (resp != "OK") {
        FAIL("Expected OK, got: " + resp);
        return;
    }
    PASS();
}

static void TestInsertMultipleFiles() {
    TEST("INSERT multiple files sequential");
    g_mockMode.store(MockMode::Normal);
    const char* files[] = {"E:\\Audio\\a.wav", "E:\\Audio\\b.wav", "E:\\Audio\\c.wav"};
    int sent = 0;
    for (auto& f : files) {
        std::string resp;
        std::string msg = std::string("INSERT|") + f + "\n";
        if (!TestPipeSend(msg, resp)) {
            FAIL(std::string("File ") + f + ": " + resp);
            return;
        }
        if (resp != "OK") {
            FAIL(std::string("File ") + f + ": expected OK, got " + resp);
            return;
        }
        sent++;
    }
    if (sent != 3) {
        FAIL("Expected 3 sent, got " + std::to_string(sent));
        return;
    }
    PASS();
}

static void TestInsertFailure() {
    TEST("INSERT failure response");
    g_mockMode.store(MockMode::InsertFail);
    std::string resp;
    if (!TestPipeSend("INSERT|E:\\bad.wav\n", resp)) {
        FAIL("Connection failed: " + resp);
        return;
    }
    if (resp.substr(0, 3) != "ERR") {
        FAIL("Expected ERR, got: " + resp);
        return;
    }
    g_mockMode.store(MockMode::Normal);
    PASS();
}

static void TestTimeout() {
    TEST("Pipe timeout");
    g_mockMode.store(MockMode::NoResponse);
    std::string resp;
    bool ok = TestPipeSend("PING\n", resp, 500); // short timeout
    if (ok) {
        FAIL("Expected timeout, got response: " + resp);
        g_mockMode.store(MockMode::Normal);
        return;
    }
    if (resp.find("Timeout") == std::string::npos) {
        FAIL("Expected timeout error, got: " + resp);
    } else {
        PASS();
    }
    g_mockMode.store(MockMode::Normal);
}

static void TestUnknownCommand() {
    TEST("Unknown command");
    g_mockMode.store(MockMode::Normal);
    std::string resp;
    if (!TestPipeSend("BADCMD\n", resp)) {
        FAIL(resp);
        return;
    }
    if (resp.substr(0, 3) != "ERR") {
        FAIL("Expected ERR, got: " + resp);
        return;
    }
    PASS();
}

static void TestPathNormalization() {
    TEST("Path with forward slashes normalized");
    g_mockMode.store(MockMode::Normal);
    // Auro normalizes paths before sending: / -> backslash
    std::string path = "E:/Audio/sub/test.wav";
    std::replace(path.begin(), path.end(), '/', '\\');
    std::string msg = "INSERT|" + path + "\n";
    std::string resp;
    if (!TestPipeSend(msg, resp)) {
        FAIL(resp);
        return;
    }
    if (resp != "OK") {
        FAIL("Expected OK, got: " + resp);
        return;
    }
    if (path.find('/') != std::string::npos) {
        FAIL("Path still contains forward slashes");
        return;
    }
    PASS();
}

static void TestIsReaperNamedPipeTarget() {
    TEST("IsReaperNamedPipeTarget name matching");
    // Simulate the logic from main.cpp (name must contain "reaper", case-insensitive)
    auto check = [](const std::string& name) -> bool {
        std::string lower = name;
        for (auto& c : lower)
            c = (char)tolower((unsigned char)c);
        return lower.find("reaper") != std::string::npos;
    };

    if (!check("REAPER")) {
        FAIL("Should match 'REAPER'");
        return;
    }
    if (!check("Reaper")) {
        FAIL("Should match 'Reaper'");
        return;
    }
    if (!check("My REAPER")) {
        FAIL("Should match 'My REAPER'");
        return;
    }
    if (check("Pro Tools")) {
        FAIL("Should NOT match 'Pro Tools'");
        return;
    }
    if (check("Cubase")) {
        FAIL("Should NOT match 'Cubase'");
        return;
    }
    if (check("")) {
        FAIL("Should NOT match empty string");
        return;
    }
    PASS();
}

static void TestMessageFormatting() {
    TEST("Message format correctness");
    // INSERT message must be: "INSERT|<filepath>\n"
    std::string filepath = "C:\\Audio\\impact_metal_01.wav";
    std::string msg = "INSERT|" + filepath + "\n";

    if (msg.substr(0, 7) != "INSERT|") {
        FAIL("Bad prefix");
        return;
    }
    if (msg.back() != '\n') {
        FAIL("Missing newline");
        return;
    }
    std::string extracted = msg.substr(7, msg.size() - 8); // strip "INSERT|" and "\n"
    if (extracted != filepath) {
        FAIL("Path mismatch: " + extracted);
        return;
    }
    PASS();
}

static std::string LoadMainSourceForContract() {
    const char* candidates[] = {
        "..\\src\\main.cpp",
        "..\\..\\src\\main.cpp",
        "src\\main.cpp",
    };
    for (const char* path : candidates) {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    return {};
}

static std::string LoadCubaseAdapterSourceForContract() {
    const char* candidates[] = {
        "..\\src\\CubaseAdapter.cpp",
        "..\\..\\src\\CubaseAdapter.cpp",
        "src\\CubaseAdapter.cpp",
    };
    for (const char* path : candidates) {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    return {};
}

static std::string LoadSourceFile(const char* relativePath) {
    const char* roots[] = {
        "..\\",
        "..\\..\\",
        "",
    };
    for (const char* root : roots) {
        std::string path = std::string(root) + relativePath;
        std::replace(path.begin(), path.end(), '/', '\\');
        std::ifstream in(path, std::ios::binary);
        if (!in)
            continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    return {};
}

static void TestSteinbergLaunchTargetsHaveDirectHandoffContract() {
    TEST("Cubase/Nuendo LaunchExe guardrail contract");
    std::string source = LoadMainSourceForContract();
    if (source.empty()) {
        FAIL("Could not load src/main.cpp");
        return;
    }
    if (source.find("IsSteinbergLaunchTarget") == std::string::npos) {
        FAIL("Missing Steinberg target classifier");
        return;
    }
    if (source.find("SendFilesToSteinbergLaunchTarget") == std::string::npos) {
        FAIL("Missing Steinberg LaunchExe handoff dispatcher");
        return;
    }
    if (source.find("direct send via launch arguments is not supported") == std::string::npos ||
        source.find("use drag-and-drop into the timeline") == std::string::npos) {
        FAIL("Steinberg handoff must block unsupported launch-argument import and redirect users to drag-and-drop");
        return;
    }
    if (source.find("FindInstalledSteinbergExecutable") == std::string::npos ||
        source.find("GetSteinbergExecutableCandidates") == std::string::npos ||
        source.find("auto-detected") == std::string::npos) {
        FAIL("Steinberg handoff must auto-detect installed Cubase/Nuendo executables when exePath is empty");
        return;
    }
    if (source.find("Cubase") == std::string::npos ||
        source.find("Nuendo") == std::string::npos ||
        source.find("Steinberg") == std::string::npos) {
        FAIL("Contract must name Cubase, Nuendo, and Steinberg");
        return;
    }
    if (source.find("add(\"Nuendo\"") == std::string::npos) {
        FAIL("Default DAW target list must include Nuendo");
        return;
    }
    if (source.find("ShortcutAction::SendToDAW4") == std::string::npos ||
        source.find("SendToDAW(state, 3)") == std::string::npos) {
        FAIL("Nuendo default slot must expose the fourth DAW shortcut path");
        return;
    }
    PASS();
}

static void TestSteinbergCapabilityTagUsesAutodetectContract() {
    TEST("Steinberg capability tag source contract");
    std::string source = LoadMainSourceForContract();
    if (source.empty()) {
        FAIL("Could not load src/main.cpp");
        return;
    }
    if (source.find("DawCapability GetDawCapability") == std::string::npos) {
        FAIL("Missing capability classifier");
        return;
    }
    if (source.find("FindInstalledSteinbergExecutable(dt).empty()") == std::string::npos) {
        FAIL("Capability classifier must recognize Steinberg autodetect before reporting unconfigured");
        return;
    }
    if (source.find("return DawCapability::Unconfigured;") == std::string::npos ||
        source.find("return DawCapability::CanLaunch;") == std::string::npos) {
        FAIL("Capability classifier contract looks incomplete");
        return;
    }
    PASS();
}

static void TestSteinbergRunningStateUsesVersionedProcessCandidatesContract() {
    TEST("Steinberg running-state source contract");
    std::string source = LoadMainSourceForContract();
    if (source.empty()) {
        FAIL("Could not load src/main.cpp");
        return;
    }
    if (source.find("UpdateDAWRunningState") == std::string::npos ||
        source.find("RefreshDawRunningStateNow") == std::string::npos) {
        FAIL("Missing DAW running-state refresh helpers");
        return;
    }
    if (source.find("GetSteinbergExecutableCandidates(dt)") == std::string::npos ||
        source.find("GetSteinbergExecutableCandidates(target)") == std::string::npos) {
        FAIL("Steinberg running-state detection must consult versioned executable candidates");
        return;
    }
    if (source.find("GetRunningProcessPathAny(GetSteinbergExecutableCandidates(target))") == std::string::npos ||
        source.find("GetRunningProcessPathAny(GetSteinbergExecutableCandidates(dt))") == std::string::npos) {
        FAIL("Steinberg running-state detection must resolve the live Nuendo/Cubase process by candidate names");
        return;
    }
    PASS();
}

static void TestExperimentalCapabilityUsesGreenStatusDotContract() {
    TEST("experimental capability uses green status dot");
    std::string panelSource = LoadSourceFile("src/DAWPanel.cpp");
    if (panelSource.empty()) {
        FAIL("Could not load src/DAWPanel.cpp");
        return;
    }
    const auto functionStart = panelSource.find("static ImU32 GetStatusDotColor");
    const auto functionEnd = panelSource.find("struct Pal", functionStart == std::string::npos ? 0 : functionStart);
    if (functionStart == std::string::npos || functionEnd == std::string::npos || functionEnd <= functionStart) {
        FAIL("Could not isolate GetStatusDotColor");
        return;
    }
    const auto body = panelSource.substr(functionStart, functionEnd - functionStart);
    if (body.find("case DawCapability::Experimental") == std::string::npos) {
        FAIL("Experimental-ready targets must not fall through to the default grey dot");
        return;
    }
    const auto experimentalCase = body.find("case DawCapability::Experimental");
    const auto nextCase = body.find("case DawCapability::", experimentalCase + 1);
    const auto caseBody = body.substr(experimentalCase, nextCase == std::string::npos ? std::string::npos : nextCase - experimentalCase);
    if (caseBody.find("IM_COL32(80, 210, 80") == std::string::npos) {
        FAIL("Experimental-ready status dot must use the same green ready colour as direct insert");
        return;
    }
    PASS();
}
static void TestProToolsTargetUsesPtslBridgeAndNoLaunchSuccessContract() {
    TEST("Pro Tools live PTSL route source contract");
    std::string source = LoadMainSourceForContract();
    std::string bridgeSource = LoadSourceFile("src/ProToolsBridge.cpp");
    std::string modelSource = LoadSourceFile("src/HostSendModel.cpp");
    if (source.empty() || bridgeSource.empty() || modelSource.empty()) {
        FAIL("Could not load src/main.cpp, src/ProToolsBridge.cpp, or src/HostSendModel.cpp");
        return;
    }
    if (modelSource.find("ProToolsPtslBridge") == std::string::npos ||
        modelSource.find("protools_ptsl_bridge") == std::string::npos ||
        modelSource.find("DeliveryMode::SpotToHost") == std::string::npos) {
        FAIL("Pro Tools HostSend defaults must point at the live PTSL spot bridge");
        return;
    }
    if (source.find("#include \"ProToolsBridge.h\"") == std::string::npos ||
        source.find("SendFilesToProToolsTarget") == std::string::npos ||
        source.find("ProTools::DispatchLivePtslSpotRequest") == std::string::npos) {
        FAIL("SendToDAW must route Pro Tools targets through the ProTools bridge client");
        return;
    }
    const auto functionStart = source.find("static bool SendFilesToProToolsTarget");
    const auto functionEnd = source.find("#endif", functionStart == std::string::npos ? 0 : functionStart);
    if (functionStart == std::string::npos || functionEnd == std::string::npos || functionEnd <= functionStart) {
        FAIL("Could not isolate the Windows Pro Tools send function body");
        return;
    }
    const auto functionBody = source.substr(functionStart, functionEnd - functionStart);
    if (functionBody.find("HostSend::BuildRequestFromFiles") == std::string::npos ||
        functionBody.find("HostSend::MaterializeStagingPlan") == std::string::npos ||
        functionBody.find("HostSend.StageReady") == std::string::npos) {
        FAIL("Pro Tools route must use shared staging and HostSend diagnostics");
        return;
    }
    if (functionBody.find("live_ptsl_spot") == std::string::npos ||
        functionBody.find("helperUnavailable") == std::string::npos ||
        functionBody.find("LIVE_PROTOOLS_HUMAN_REQUIRED") == std::string::npos) {
        FAIL("Pro Tools route must keep live PTSL truth separate from helper/config success");
        return;
    }
    // S10: The toast now uses r.trackWasCreated to detect the new-track fallback case.
    // Both the error-code check (legacy compat) and trackWasCreated check must be present.
    const bool hasLegacyFallbackCode = functionBody.find("legacyNewTrackFallback") != std::string::npos ||
        functionBody.find("trackWasCreated") != std::string::npos;
    const bool hasLegacyFallbackMsg = functionBody.find("legacy new-track fallback") != std::string::npos ||
        functionBody.find("no track was selected") != std::string::npos ||
        functionBody.find("Created new track") != std::string::npos;
    if (!hasLegacyFallbackCode || !hasLegacyFallbackMsg) {
        FAIL("Pro Tools route must surface the no-regression new-track fallback truthfully");
        return;
    }
    if (functionBody.find("CopyPathsToClipboard") != std::string::npos ||
        functionBody.find("ShellExecute") != std::string::npos ||
        functionBody.find("SendFilesToSteinbergLaunchTarget") != std::string::npos) {
        FAIL("Pro Tools route must not claim launch/clipboard/fallback as Send success");
        return;
    }
    if (bridgeSource.find("HarkenSearchProToolsBridge.exe") == std::string::npos ||
        bridgeSource.find("--request") == std::string::npos ||
        bridgeSource.find("--response") == std::string::npos ||
        bridgeSource.find("DispatchLivePtslPersistentRequest") == std::string::npos ||
        bridgeSource.find("--server --queue") == std::string::npos ||
        bridgeSource.find("persistentHarkenSearchProToolsBridge") == std::string::npos) {
        FAIL("Pro Tools bridge client must keep smoke one-shot but route real sends through a persistent helper request/response queue");
        return;
    }
    PASS();
}

static void TestDawSettingsAllowManualExecutablePathContract() {
    TEST("DAW settings manual executable path contract");
    std::string source = LoadMainSourceForContract();
    if (source.empty()) {
        FAIL("Could not load src/main.cpp");
        return;
    }
    if (source.find("OpenExecutableFileDialogUtf8") == std::string::npos) {
        FAIL("Missing executable picker dialog helper");
        return;
    }
    if (source.find("##DawExePath") == std::string::npos ||
        source.find("Browse...##DawExePath") == std::string::npos ||
        source.find("Clear##DawExePath") == std::string::npos) {
        FAIL("DAW settings must expose manual exe path edit, browse, and clear controls");
        return;
    }
    if (source.find("If auto-detect misses the install") == std::string::npos) {
        FAIL("DAW settings must explain manual exe-path fallback when autodetect misses");
        return;
    }
    PASS();
}

static void TestProToolsSdkGuideSurfaceContract() {
    TEST("Pro Tools bundled runtime checklist surface contract");
    std::string source = LoadMainSourceForContract();
    std::string bridgeHeader = LoadSourceFile("src/ProToolsBridge.h");
    std::string bridgeSource = LoadSourceFile("src/ProToolsBridge.cpp");
    if (source.empty() || bridgeHeader.empty() || bridgeSource.empty()) {
        FAIL("Could not load Pro Tools guide source files");
        return;
    }
    if (bridgeHeader.find("BuildRuntimeSetupChecklist") == std::string::npos ||
        bridgeHeader.find("BuildTimelineSendPresentation") == std::string::npos ||
        bridgeHeader.find("ProToolsSetupChecklistItem") == std::string::npos ||
        bridgeHeader.find("ProToolsTimelineSendPresentation") == std::string::npos ||
        bridgeHeader.find("userSdkDirectoryUtf8") == std::string::npos ||
        bridgeHeader.find("userPtslClientDllPathUtf8") == std::string::npos ||
        bridgeHeader.find("userPtslClientPresent") == std::string::npos ||
        bridgeHeader.find("sdkDownloadUrl") == std::string::npos ||
        bridgeSource.find("https://developer.avid.com/scripting/") == std::string::npos) {
        FAIL("ProToolsBridge must model bundled runtime checklist and the local PTSL runtime path");
        return;
    }
    if (source.find("Pro Tools Integration") == std::string::npos ||
        source.find("Test Pro Tools Connection") == std::string::npos ||
        source.find("DispatchLivePtslSmokeTest") == std::string::npos ||
        source.find("Repair Bridge") == std::string::npos ||
        source.find("Auro Pro Tools Bridge installed") == std::string::npos ||
        source.find("PTSL runtime bundled") == std::string::npos ||
        source.find("lastProToolsConnectionStatus") == std::string::npos ||
        source.find("BuildRuntimeSetupChecklist") == std::string::npos ||
        source.find("BuildTimelineSendPresentation") == std::string::npos) {
        FAIL("DAW settings must expose bundled runtime checklist, test button, repair action, and persistent status");
        return;
    }
    const auto buttonStart = source.find("ImGui::Button(\"Test Pro Tools Connection\")");
    const auto buttonEnd = source.find("ImGui::SameLine()", buttonStart == std::string::npos ? 0 : buttonStart);
    if (buttonStart == std::string::npos || buttonEnd == std::string::npos || buttonEnd <= buttonStart) {
        FAIL("Could not isolate the Test Pro Tools Connection button body");
        return;
    }
    const std::string buttonBody = source.substr(buttonStart, buttonEnd - buttonStart);
    if (buttonBody.find("DispatchLivePtslSmokeTest") == std::string::npos ||
        buttonBody.find("ptslRegisterConnectionFailed") == std::string::npos ||
        buttonBody.find("ptPresentation.sendEnabled") != std::string::npos) {
        FAIL("Test Pro Tools Connection must run the real PTSL smoke helper and surface RegisterConnection failure instead of trusting native preflight");
        return;
    }
    if (source.find("Open Avid SDK page") != std::string::npos ||
        source.find("Build the Pro Tools Scripting SDK C++ wrapper, then copy") != std::string::npos) {
        FAIL("Pro Tools setup surface must not lead with manual SDK download/build instructions");
        return;
    }
    if (source.find("Spot to Pro Tools Timeline") == std::string::npos ||
        source.find("Live PTSL Ready") == std::string::npos ||
        source.find("Pro Tools not running") == std::string::npos ||
        source.find("SDK/runtime missing") == std::string::npos ||
        source.find("Open Pro Tools and a session, then place the edit cursor before sending") == std::string::npos) {
        FAIL("Send-to-Editor menu must use Pro Tools-specific timeline label, readiness tags, and disabled reasons");
        return;
    }
    PASS();
}

static void TestProToolsResetHostSurfaceContract() {
    TEST("Pro Tools reset host surface contract");
    std::string source = LoadMainSourceForContract();
    std::string appStateHeader = LoadSourceFile("src/AppState.h");
    std::string bridgeHeader = LoadSourceFile("src/ProToolsBridge.h");
    std::string bridgeSource = LoadSourceFile("src/ProToolsBridge.cpp");
    if (source.empty() || appStateHeader.empty() || bridgeHeader.empty() || bridgeSource.empty()) {
        FAIL("Could not load Pro Tools reset source files");
        return;
    }
    if (bridgeHeader.find("BuildProToolsConnectionResetPresentation") == std::string::npos ||
        bridgeHeader.find("ResetProToolsConnection") == std::string::npos ||
        bridgeHeader.find("ProToolsConnectionResetPresentation") == std::string::npos ||
        bridgeHeader.find("ProToolsConnectionResetResult") == std::string::npos) {
        FAIL("ProToolsBridge must expose a typed reset presentation and reset result API");
        return;
    }
    if (bridgeHeader.find("Reset Pro Tools Connection") == std::string::npos ||
        bridgeHeader.find("Close Pro Tools after saving") == std::string::npos ||
        source.find("BuildProToolsConnectionResetPresentation") == std::string::npos ||
        source.find("ResetProToolsConnection(false)") == std::string::npos ||
        source.find("ResetProToolsConnection(true)") == std::string::npos ||
        source.find("showProToolsCloseConfirm") == std::string::npos ||
        appStateHeader.find("showProToolsCloseConfirm") == std::string::npos) {
        FAIL("DAW settings must expose reset and user-confirmed close-Pro-Tools controls");
        return;
    }
    if (bridgeSource.find("HarkenSearchProToolsBridge.exe") == std::string::npos ||
        bridgeSource.find("TerminateProcessesByImageName(kProToolsBridgeHelperExeName") == std::string::npos ||
        bridgeSource.find("PostMessageW(hwnd, WM_CLOSE") == std::string::npos) {
        FAIL("Reset must clean Auro-owned helpers and only request a normal Pro Tools close");
        return;
    }
    if (bridgeSource.find("TerminateProcessesByImageName(kProToolsExeName") != std::string::npos ||
        bridgeSource.find("Auro never force-kills ProTools.exe") == std::string::npos) {
        FAIL("Reset must not force-kill ProTools.exe; user data safety requires a normal close request only");
        return;
    }
    PASS();
}

static void TestProToolsStartupAutoResetSurfaceContract() {
    TEST("Pro Tools startup auto-reset surface contract");
    std::string windowsMainSource = LoadSourceFile("src/WindowsMain.cpp");
    std::string bridgeHeader = LoadSourceFile("src/ProToolsBridge.h");
    if (windowsMainSource.empty() || bridgeHeader.empty()) {
        FAIL("Could not load WindowsMain or ProToolsBridge header for startup reset contract");
        return;
    }
    if (bridgeHeader.find("ResetProToolsBridgeOnHarkenSearchStartup") == std::string::npos) {
        FAIL("ProToolsBridge must expose a dedicated Auro-startup reset API");
        return;
    }
    if (windowsMainSource.find("ResetProToolsBridgeOnHarkenSearchStartup") == std::string::npos ||
        windowsMainSource.find("Pro Tools bridge auto-reset at Auro startup") == std::string::npos ||
        windowsMainSource.find("startupReset.status") == std::string::npos ||
        windowsMainSource.find("lastProToolsConnectionStatus = startupReset.status") == std::string::npos ||
        windowsMainSource.find("lastProToolsConnectionDetail = startupReset.detail") == std::string::npos) {
        FAIL("Auro startup must automatically reset Auro-owned Pro Tools bridge state and surface the result in settings");
        return;
    }
    if (windowsMainSource.find("ResetProToolsConnection(true)") != std::string::npos ||
        windowsMainSource.find("TerminateProcessesByImageName(kProToolsExeName") != std::string::npos) {
        FAIL("Auro startup auto-reset must not close or force-kill ProTools.exe");
        return;
    }
    PASS();
}

static void TestSendToDawProcessingSuffixPreferenceContract() {
    TEST("Auro output processing suffix preference contract");
    const std::string mainSource = LoadMainSourceForContract();
    const std::string appStateHeader = LoadSourceFile("src/AppState.h");
    const std::string hostSendModel = LoadSourceFile("src/HostSendModel.cpp");
    if (mainSource.empty() || appStateHeader.empty() || hostSendModel.empty()) {
        FAIL("Could not load source files for Auro output suffix preference contract");
        return;
    }
    if (appStateHeader.find("keepHarkenSearchOutputProcessingSuffix") == std::string::npos ||
        mainSource.find("keepHarkenSearchOutputProcessingSuffix") == std::string::npos) {
        FAIL("Personal preferences must persist a global Auro output processing suffix option");
        return;
    }
    if (mainSource.find("Auro output: keep processing suffixes") == std::string::npos ||
        mainSource.find("auro output export drag send daw filename name suffix") == std::string::npos) {
        FAIL("Settings UI/search must expose the global Auro output suffix option");
        return;
    }
    if (mainSource.find("BuildTransferSuffix(const AudioEngine& engine, bool hasRegion, bool keepHarkenSearchProcessingSuffix)") == std::string::npos ||
        mainSource.find("StartRegionDrag(state.audioEngine, state.uiState.regionFadeInNorm, state.uiState.regionFadeOutNorm, state.settings.keepHarkenSearchOutputProcessingSuffix)") == std::string::npos ||
        mainSource.find("state.settings.keepHarkenSearchOutputProcessingSuffix,") == std::string::npos) {
        FAIL("All Auro-generated transfer/drag temp files must use the global output suffix option before HostSend sees them");
        return;
    }
    if (hostSendModel.find("StripHarkenSearchProcessingSuffixesFromStem") == std::string::npos ||
        hostSendModel.find("_vol") == std::string::npos ||
        hostSendModel.find("_pitch") == std::string::npos ||
        hostSendModel.find("_region") == std::string::npos) {
        FAIL("Shared HostSend staging must strip Auro processing suffix tokens when the option is disabled");
        return;
    }
    const auto nuendoStart = mainSource.find("static bool SendFilesToNuendo" "ExperimentalTarget");
    const auto proToolsStart = mainSource.find("static bool SendFilesToProToolsTarget");
    const auto proToolsEnd = mainSource.find("#endif", proToolsStart == std::string::npos ? 0 : proToolsStart);
    if (nuendoStart == std::string::npos || proToolsStart == std::string::npos || proToolsEnd == std::string::npos) {
        FAIL("Could not isolate Nuendo/ProTools Send-to-DAW functions");
        return;
    }
    const std::string nuendoBody = mainSource.substr(nuendoStart, proToolsStart - nuendoStart);
    const std::string proToolsBody = mainSource.substr(proToolsStart, proToolsEnd - proToolsStart);
    if (nuendoBody.find("keepHarkenSearchProcessingSuffix") == std::string::npos ||
        proToolsBody.find("keepHarkenSearchProcessingSuffix") == std::string::npos ||
        mainSource.find("DispatchToDaw(targetCopy, files, status, transferAction, keepHarkenSearchProcessingSuffix)") == std::string::npos ||
        mainSource.find("const bool keepHarkenSearchProcessingSuffix = state.settings.keepHarkenSearchOutputProcessingSuffix") == std::string::npos) {
        FAIL("All live HostSend DAW routes must pass the shared processing-suffix preference into BuildRequestFromFiles");
        return;
    }
    PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Send-to-DAW Protocol Tests ===\n\n";

    g_serverRunning.store(true);
    HANDLE hThread = CreateThread(NULL, 0, MockPipeServer, NULL, 0, NULL);
    Sleep(100); // let server create first pipe instance

    TestPingProtocol();
    TestInsertSingleFile();
    TestInsertMultipleFiles();
    TestInsertFailure();
    TestTimeout();
    TestUnknownCommand();
    TestPathNormalization();
    TestIsReaperNamedPipeTarget();
    TestMessageFormatting();
    TestSteinbergLaunchTargetsHaveDirectHandoffContract();
    TestSteinbergCapabilityTagUsesAutodetectContract();
    TestSteinbergRunningStateUsesVersionedProcessCandidatesContract();
    TestExperimentalCapabilityUsesGreenStatusDotContract();
    TestProToolsTargetUsesPtslBridgeAndNoLaunchSuccessContract();
    TestDawSettingsAllowManualExecutablePathContract();
    TestProToolsSdkGuideSurfaceContract();
    TestProToolsResetHostSurfaceContract();
    TestProToolsStartupAutoResetSurfaceContract();
    TestSendToDawProcessingSuffixPreferenceContract();

    g_serverRunning.store(false);
    // Unblock server if it is waiting on ConnectNamedPipe, then send data so ReadFile completes
    HANDLE hTmp = CreateFileW(AURO_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hTmp != INVALID_HANDLE_VALUE) {
        DWORD pmode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(hTmp, &pmode, NULL, NULL);
        const char wake[] = "PING\n";
        DWORD w = 0;
        WriteFile(hTmp, wake, (DWORD)(sizeof(wake) - 1), &w, NULL);
        CloseHandle(hTmp);
    }
    WaitForSingleObject(hThread, 3000);
    CloseHandle(hThread);

    std::cout << "\n=== Results: " << (g_failed == 0 ? "ALL PASSED" : "FAILURES") << " (failures: " << g_failed
              << ") ===\n";
    return g_failed == 0 ? 0 : 1;
}
