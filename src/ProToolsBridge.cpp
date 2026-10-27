#include "ProToolsBridge.h"
#include "platform/executable_path.h"
#include "platform/filesystem.h"
#include "platform/process_launch.h"
#include "platform/process_query.h"
#include "platform/system.h"
#include "platform/text_encoding.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace HarkenSearch::ProTools {
namespace {

constexpr unsigned long kProToolsBridgeTimeoutMs = 30000;
constexpr const char* kProToolsBridgeHelperExeName = "HarkenSearchProToolsBridge.exe";
constexpr const char* kProToolsSessionCacheFileName = "auro-protools-ptsl-session.txt";

std::string JoinPath(std::string base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    std::replace(base.begin(), base.end(), '/', '\\');
    if (base.back() != '\\') {
        base.push_back('\\');
    }
    return base + leaf;
}

std::filesystem::path ProToolsSessionCachePath() {
#if defined(_MSC_VER)
    char* envBuffer = nullptr;
    std::size_t envLength = 0;
    if (_dupenv_s(&envBuffer, &envLength, "TEMP") == 0 && envBuffer != nullptr) {
        std::string tempRoot = envBuffer;
        free(envBuffer);
        if (!tempRoot.empty()) {
            return std::filesystem::u8path(tempRoot) / kProToolsSessionCacheFileName;
        }
    }
#else
    if (const char* tempRoot = std::getenv("TEMP")) {
        if (*tempRoot) {
            return std::filesystem::u8path(tempRoot) / kProToolsSessionCacheFileName;
        }
    }
#endif
    return std::filesystem::u8path(kProToolsSessionCacheFileName);
}

std::filesystem::path ProToolsPersistentQueueDir() {
    auto base = ProToolsSessionCachePath().parent_path();
    if (base.empty()) {
        base = std::filesystem::u8path(Platform::FileSystem::TemporaryDirectory());
    }
    return base / "auro-protools-persistent-bridge";
}

void ClearProToolsSessionCache() {
    Platform::FileSystem::RemoveFile(ProToolsSessionCachePath().u8string());
}

std::string DefaultUserSdkDirectory() {
#ifdef _WIN32
    const Platform::ExecutablePathResult executablePath = Platform::GetExecutablePath();
    if (executablePath.status == Platform::ExecutablePathStatus::Success &&
        !executablePath.pathUtf8.empty()) {
        return JoinPath(std::filesystem::u8path(executablePath.pathUtf8).parent_path().u8string(),
                        "ProToolsSdk");
    }
#endif
    return "ProToolsSdk";
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string JsonStringField(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\":\"";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return {};
    }
    std::string value;
    bool escape = false;
    for (std::size_t i = pos + marker.size(); i < json.size(); ++i) {
        const char ch = json[i];
        if (escape) {
            switch (ch) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: value.push_back(ch); break;
            }
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        value.push_back(ch);
    }
    return value;
}

bool JsonBoolField(const std::string& json, const std::string& key, bool fallback = false) {
    const std::string marker = "\"" + key + "\":";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return fallback;
    }
    const auto valuePos = pos + marker.size();
    return json.compare(valuePos, 4, "true") == 0;
}

int JsonIntField(const std::string& json, const std::string& key, int fallback = 0) {
    const std::string marker = "\"" + key + "\":";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return fallback;
    }
    const auto valuePos = pos + marker.size();
    char* end = nullptr;
    const long value = std::strtol(json.c_str() + valuePos, &end, 10);
    return end == json.c_str() + valuePos ? fallback : static_cast<int>(value);
}

std::int64_t JsonInt64Field(const std::string& json, const std::string& key, std::int64_t fallback = 0) {
    const std::string marker = "\"" + key + "\":";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return fallback;
    }
    const auto valuePos = pos + marker.size();
    char* end = nullptr;
    const long long value = std::strtoll(json.c_str() + valuePos, &end, 10);
    return end == json.c_str() + valuePos ? fallback : static_cast<std::int64_t>(value);
}

std::uint64_t JsonUInt64Field(const std::string& json, const std::string& key, std::uint64_t fallback = 0) {
    const std::string marker = "\"" + key + "\":";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return fallback;
    }
    const auto valuePos = pos + marker.size();
    char* end = nullptr;
    const unsigned long long value = std::strtoull(json.c_str() + valuePos, &end, 10);
    return end == json.c_str() + valuePos ? fallback : static_cast<std::uint64_t>(value);
}

std::vector<std::int64_t> JsonInt64ArrayField(const std::string& json, const std::string& key) {
    std::vector<std::int64_t> values;
    const std::string marker = "\"" + key + "\":[";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return values;
    }
    const auto end = json.find(']', pos + marker.size());
    if (end == std::string::npos) {
        return values;
    }
    const std::string body = json.substr(pos + marker.size(), end - (pos + marker.size()));
    std::size_t cursor = 0;
    while (cursor < body.size()) {
        while (cursor < body.size() && (body[cursor] == ',' || body[cursor] == ' ')) ++cursor;
        if (cursor >= body.size()) break;
        char* stop = nullptr;
        const long long v = std::strtoll(body.c_str() + cursor, &stop, 10);
        if (stop == body.c_str() + cursor) break;
        values.push_back(static_cast<std::int64_t>(v));
        cursor = static_cast<std::size_t>(stop - body.c_str());
    }
    return values;
}

std::vector<std::string> JsonStringArrayField(const std::string& json, const std::string& key) {
    std::vector<std::string> values;
    const std::string marker = "\"" + key + "\":[";
    const auto pos = json.find(marker);
    if (pos == std::string::npos) {
        return values;
    }
    const auto end = json.find(']', pos + marker.size());
    if (end == std::string::npos) {
        return values;
    }
    const std::string body = json.substr(pos + marker.size(), end - (pos + marker.size()));
    std::size_t cursor = 0;
    while (cursor < body.size()) {
        const auto quote = body.find('"', cursor);
        if (quote == std::string::npos) {
            break;
        }
        const auto close = body.find('"', quote + 1);
        if (close == std::string::npos) {
            break;
        }
        values.push_back(body.substr(quote + 1, close - quote - 1));
        cursor = close + 1;
    }
    return values;
}

#ifdef _WIN32
bool PathExistsWin32(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    return Platform::FileSystem::Exists(path);
}

bool EqualsIgnoreCaseAscii(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    while (*lhs && *rhs) {
        char a = *lhs++;
        char b = *rhs++;
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) {
            return false;
        }
    }
    return *lhs == '\0' && *rhs == '\0';
}

bool IsProcessRunningByImageName(const char* imageName) {
    return !Platform::FindProcessIdsByImageName(imageName).empty();
}

int TerminateProcessesByImageName(const char* imageName) {
    int terminated = 0;
    const uint64_t currentPid = Platform::CurrentProcessId();
    for (const uint64_t pid : Platform::FindProcessIdsByImageName(imageName)) {
        if (pid == 0 || pid == currentPid) {
            continue;
        }
        const auto termination = Platform::TerminateProcessById(static_cast<uint32_t>(pid), 1460, 1000);
        if (termination.status == Platform::ProcessTerminationStatus::Terminated) {
            ++terminated;
        }
    }
    return terminated;
}

struct CloseProToolsWindowContext {
    std::set<uint64_t> processIds;
    int messagesPosted = 0;
};

bool RequestCloseProToolsWindow(void* windowHandle, void* contextValue) {
    auto* context = static_cast<CloseProToolsWindowContext*>(contextValue);
    if (!context || !Platform::WindowIsValid(windowHandle)) {
        return true;
    }
    const uint64_t pid = Platform::WindowProcessId(windowHandle);
    if (context->processIds.find(pid) == context->processIds.end()) {
        return true;
    }
    if (!Platform::WindowIsVisible(windowHandle)) {
        return true;
    }
    // Never terminate Pro Tools forcibly; after confirmation, request a
    // normal close so the host retains control of session-save prompts.
    Platform::RequestWindowClose(windowHandle);
    ++context->messagesPosted;
    return true;
}

int RequestCloseProToolsAfterUserSaved() {
    CloseProToolsWindowContext context;
    const auto ids = Platform::FindProcessIdsByImageName(kProToolsExeName);
    context.processIds.insert(ids.begin(), ids.end());
    if (context.processIds.empty()) {
        return 0;
    }
    Platform::EnumerateTopLevelWindows(RequestCloseProToolsWindow, &context);
    return context.messagesPosted;
}

struct WindowProbeContext {
    const InstallProbePlan* plan = nullptr;
    bool live = false;
    bool activation = false;
    std::string liveClass;
    std::string activationClass;
};

bool MatchesAny(const std::vector<std::string>& classes, const char* className, std::string* matched) {
    for (const auto& candidate : classes) {
        if (EqualsIgnoreCaseAscii(candidate.c_str(), className)) {
            if (matched) {
                *matched = candidate;
            }
            return true;
        }
    }
    return false;
}

bool EnumProbeWindow(void* windowHandle, void* contextValue) {
    auto* context = static_cast<WindowProbeContext*>(contextValue);
    if (!context || !context->plan || !Platform::WindowIsVisible(windowHandle)) {
        return true;
    }
    char className[256] = {};
    if (Platform::WindowClassName(windowHandle, className,
                                   static_cast<int>(sizeof(className))) <= 0) {
        return true;
    }
    if (!context->live && MatchesAny(context->plan->liveWindowClasses, className, &context->liveClass)) {
        context->live = true;
    }
    if (!context->activation && MatchesAny(context->plan->activationWindowClasses, className, &context->activationClass)) {
        context->activation = true;
    }
    return true;
}
#endif

} // namespace

const char* DeliveryPathKey(DeliveryPath path) {
    switch (path) {
    case DeliveryPath::LivePtslSpot:
        return "live_ptsl_spot";
    case DeliveryPath::AafFallback:
        return "aaf_fallback";
    }
    return "unknown";
}

const char* SpotModeKey(SpotMode mode) {
    switch (mode) {
    case SpotMode::AtCursor:
        return "at_cursor";
    case SpotMode::ToTimecode:
        return "to_timecode";
    case SpotMode::ToSelectionIn:
        return "to_selection_in";
    }
    return "at_cursor";
}

SpotMode SpotModeFromKey(const std::string& key) {
    if (key == "to_timecode") return SpotMode::ToTimecode;
    if (key == "to_selection_in") return SpotMode::ToSelectionIn;
    return SpotMode::AtCursor;
}

const char* TrackFallbackKey(TrackFallback fallback) {
    switch (fallback) {
    case TrackFallback::AutoCreateNew:
        return "auto_create_new";
    case TrackFallback::RequireExplicit:
        return "require_explicit";
    }
    return "auto_create_new";
}

TrackFallback TrackFallbackFromKey(const std::string& key) {
    if (key == "require_explicit") return TrackFallback::RequireExplicit;
    return TrackFallback::AutoCreateNew;
}

const char* ReadinessKey(Readiness readiness) {
    switch (readiness) {
    case Readiness::NotInstalled:
        return "notInstalled";
    case Readiness::InstalledButHostNotReady:
        return "installedButHostNotReady";
    case Readiness::LivePtslPossible:
        return "livePtslPossible";
    case Readiness::AafFallbackPossible:
        return "aafFallbackPossible";
    }
    return "unknown";
}

const char* FailureCodeKey(FailureCode code) {
    switch (code) {
    case FailureCode::None:
        return "none";
    case FailureCode::ProToolsNotInstalled:
        return "proToolsNotInstalled";
    case FailureCode::ActivationGate:
        return "activationGate";
    case FailureCode::HostWindowUnavailable:
        return "hostWindowUnavailable";
    case FailureCode::PtslMissing:
        return "ptslMissing";
    case FailureCode::AafMissing:
        return "aafMissing";
    case FailureCode::ProtocolUnavailable:
        return "protocolUnavailable";
    }
    return "unknown";
}

InstallProbePlan BuildInstallProbePlan(const std::string& avidProToolsRootUtf8) {
    InstallProbePlan plan;
    plan.installRootUtf8 = avidProToolsRootUtf8.empty()
        ? "C:\\Program Files\\Avid\\Pro Tools"
        : avidProToolsRootUtf8;
    plan.proToolsExePathUtf8 = JoinPath(plan.installRootUtf8, kProToolsExeName);
    plan.ptslDllPathUtf8 = JoinPath(plan.installRootUtf8, kPtslDllName);
    plan.userSdkDirectoryUtf8 = DefaultUserSdkDirectory();
    plan.userPtslDllPathUtf8 = JoinPath(plan.userSdkDirectoryUtf8, kPtslDllName);
    plan.userPtslClientDllPathUtf8 = JoinPath(plan.userSdkDirectoryUtf8, kPtslClientDllName);
    plan.sdkGuideReadmePathUtf8 = JoinPath(plan.userSdkDirectoryUtf8, "README.txt");
    plan.sdkDownloadUrl = "https://developer.avid.com/scripting/";
    plan.ptAafDllPathUtf8 = JoinPath(plan.installRootUtf8, kPtAafDllName);
    plan.aafCoApiDllPathUtf8 = JoinPath(plan.installRootUtf8, kAafCoApiDllName);
    plan.liveWindowClasses = {
        "DigiAppWndClass",
        "DigiMDIWndClass",
        "AvidEditorMainWndClass",
    };
    plan.activationWindowClasses = {
        "PACEEdenExperienceProxy",
    };
    return plan;
}

CapabilitySnapshot EvaluateCapability(
    const InstallProbePlan& plan,
    const std::function<bool(const std::string&)>& pathExists,
    bool liveHostWindowPresent,
    bool activationProxyVisible) {
    CapabilitySnapshot snapshot;
    snapshot.proToolsExePresent = pathExists && pathExists(plan.proToolsExePathUtf8);
    snapshot.ptslPresent = pathExists && pathExists(plan.ptslDllPathUtf8);
    snapshot.userPtslPresent = pathExists && pathExists(plan.userPtslDllPathUtf8);
    snapshot.userPtslClientPresent = pathExists && pathExists(plan.userPtslClientDllPathUtf8);
    snapshot.ptAafPresent = pathExists && pathExists(plan.ptAafDllPathUtf8);
    snapshot.aafCoApiPresent = pathExists && pathExists(plan.aafCoApiDllPathUtf8);
    snapshot.liveHostWindowPresent = liveHostWindowPresent;
    snapshot.activationProxyVisible = activationProxyVisible;

    if (!snapshot.proToolsExePresent) {
        snapshot.readiness = Readiness::NotInstalled;
        snapshot.failure = FailureCode::ProToolsNotInstalled;
        snapshot.summary = "Pro Tools executable is not installed";
        return snapshot;
    }
    if (snapshot.activationProxyVisible && !snapshot.liveHostWindowPresent) {
        snapshot.readiness = Readiness::InstalledButHostNotReady;
        snapshot.failure = FailureCode::ActivationGate;
        snapshot.summary = "Pro Tools is stopped at the PACE activation gate";
        return snapshot;
    }
    if ((snapshot.ptslPresent || snapshot.userPtslPresent || snapshot.userPtslClientPresent) && snapshot.liveHostWindowPresent) {
        snapshot.readiness = Readiness::LivePtslPossible;
        snapshot.failure = FailureCode::None;
        snapshot.summary = (snapshot.userPtslClientPresent && !snapshot.ptslPresent)
            ? ("Pro Tools live PTSL spot path can be attempted using user PTSLC_CPP.dll from " + plan.userSdkDirectoryUtf8)
            : (snapshot.userPtslPresent && !snapshot.ptslPresent)
                ? ("Pro Tools live PTSL spot path can be attempted using user PTSL.dll from " + plan.userSdkDirectoryUtf8)
                : "Pro Tools live PTSL spot path can be attempted";
        return snapshot;
    }
    if (!snapshot.ptslPresent && !snapshot.userPtslPresent && !snapshot.userPtslClientPresent) {
        snapshot.readiness = Readiness::InstalledButHostNotReady;
        snapshot.failure = FailureCode::PtslMissing;
        snapshot.summary = "PTSLC_CPP.dll is missing; build the Avid Pro Tools Scripting SDK C++ wrapper and copy PTSLC_CPP.dll into " +
            plan.userSdkDirectoryUtf8;
        return snapshot;
    }
    if (snapshot.ptAafPresent && snapshot.aafCoApiPresent) {
        snapshot.readiness = Readiness::AafFallbackPossible;
        snapshot.failure = FailureCode::HostWindowUnavailable;
        snapshot.summary = "Pro Tools host window is unavailable; AAF fallback can be prepared";
        return snapshot;
    }

    snapshot.readiness = Readiness::InstalledButHostNotReady;
    snapshot.failure = FailureCode::AafMissing;
    snapshot.summary = "Pro Tools live host is unavailable and AAF fallback components are missing";
    return snapshot;
}

CapabilitySnapshot ProbeInstalledCapability(const InstallProbePlan& plan) {
#ifdef _WIN32
    WindowProbeContext windowContext;
    windowContext.plan = &plan;
    Platform::EnumerateTopLevelWindows(EnumProbeWindow, &windowContext);
    auto snapshot = EvaluateCapability(
        plan,
        [](const std::string& path) { return PathExistsWin32(path); },
        windowContext.live,
        windowContext.activation);
    snapshot.proToolsProcessRunning = IsProcessRunningByImageName(kProToolsExeName);
    snapshot.matchedLiveWindowClass = windowContext.liveClass;
    snapshot.matchedActivationWindowClass = windowContext.activationClass;
    if (snapshot.proToolsExePresent && snapshot.proToolsProcessRunning && !snapshot.liveHostWindowPresent &&
        !snapshot.activationProxyVisible && snapshot.summary.find("host window") == std::string::npos) {
        snapshot.summary += " (ProTools.exe is running but no live Digi/Avid host window was detected)";
    }
    return snapshot;
#else
    (void)plan;
    CapabilitySnapshot snapshot;
    snapshot.summary = "Pro Tools native capability probe is Windows-only";
    snapshot.failure = FailureCode::ProtocolUnavailable;
    return snapshot;
#endif
}

std::vector<ProToolsSetupChecklistItem> BuildRuntimeSetupChecklist(
    const InstallProbePlan& plan,
    const CapabilitySnapshot& snapshot,
    bool bridgeHelperPresent) {
    std::vector<ProToolsSetupChecklistItem> items;
    auto add = [&](std::string key,
                   std::string label,
                   bool ok,
                   bool blocking,
                   std::string detail) {
        ProToolsSetupChecklistItem item;
        item.key = std::move(key);
        item.label = std::move(label);
        item.ok = ok;
        item.blocking = blocking;
        item.detail = std::move(detail);
        items.push_back(std::move(item));
    };

    const bool ptslRuntimePresent =
        snapshot.ptslPresent ||
        snapshot.userPtslPresent ||
        snapshot.userPtslClientPresent;

    add("bridge_runtime",
        "Harken Search Pro Tools Bridge installed",
        bridgeHelperPresent,
        true,
        bridgeHelperPresent
            ? "HarkenSearchProToolsBridge.exe is available in the Harken Search install."
            : "HarkenSearchProToolsBridge.exe is missing; repair or reinstall Harken Search before using Pro Tools send.");
    add("ptsl_runtime",
        "User-supplied Avid PTSL runtime",
        ptslRuntimePresent,
        true,
        ptslRuntimePresent
            ? "Avid PTSL runtime files are available for the bridge."
            : ("PTSLC_CPP.dll is missing from the user-supplied runtime location: " + plan.userSdkDirectoryUtf8));
    add("protools_installed",
        "Pro Tools installed",
        snapshot.proToolsExePresent,
        true,
        snapshot.proToolsExePresent
            ? snapshot.summary
            : ("Expected ProTools.exe at " + plan.proToolsExePathUtf8));
    add("protools_running",
        "Pro Tools running",
        snapshot.proToolsProcessRunning,
        true,
        snapshot.proToolsProcessRunning
            ? "ProTools.exe is running."
            : "Open Pro Tools before sending to the timeline.");
    add("session_ready",
        "Pro Tools session ready",
        snapshot.liveHostWindowPresent,
        true,
        snapshot.liveHostWindowPresent
            ? ("Live host window detected" +
               (snapshot.matchedLiveWindowClass.empty()
                    ? std::string(".")
                    : (": " + snapshot.matchedLiveWindowClass)))
            : (snapshot.activationProxyVisible
                   ? "PACE activation is visible; finish activation and open a session."
                   : "Open a Pro Tools session and place the edit cursor before sending."));
    add("timeline_ready",
        "Timeline/edit cursor ready",
        bridgeHelperPresent && ptslRuntimePresent &&
            snapshot.readiness == Readiness::LivePtslPossible,
        true,
        (bridgeHelperPresent && ptslRuntimePresent &&
         snapshot.readiness == Readiness::LivePtslPossible)
            ? "Live PTSL preflight is ready; actual insertion is proven only after a successful send response."
            : "Timeline send stays disabled until the bridge helper, Avid PTSL runtime, Pro Tools, and session readiness all pass.");
    return items;
}

ProToolsTimelineSendPresentation BuildTimelineSendPresentation(
    const CapabilitySnapshot& snapshot,
    bool bridgeHelperPresent) {
    ProToolsTimelineSendPresentation presentation;
    presentation.actionLabel = "Spot to Pro Tools Timeline";

    const bool ptslRuntimePresent =
        snapshot.ptslPresent ||
        snapshot.userPtslPresent ||
        snapshot.userPtslClientPresent;

    if (!bridgeHelperPresent) {
        presentation.capabilityTag = "Bridge helper missing";
        presentation.preflightSummary =
            "HarkenSearchProToolsBridge.exe is missing from the Harken Search install.";
        presentation.recommendedAction = "Repair Bridge";
        presentation.disabledReason =
            "Repair or reinstall Harken Search before sending to the Pro Tools timeline.";
        return presentation;
    }

    if (!ptslRuntimePresent ||
        snapshot.failure == FailureCode::PtslMissing) {
        presentation.capabilityTag = "Avid PTSL runtime missing";
        presentation.preflightSummary =
            "Harken Search bridge helper is present, but the user-supplied Avid PTSL runtime was not found.";
        presentation.recommendedAction = "Install PTSL runtime";
        presentation.disabledReason =
            "Install the licensed Avid PTSL runtime before sending to the Pro Tools timeline.";
        return presentation;
    }

    if (!snapshot.proToolsExePresent ||
        snapshot.failure == FailureCode::ProToolsNotInstalled) {
        presentation.capabilityTag = "Pro Tools not installed";
        presentation.preflightSummary = "Install Pro Tools before using live PTSL timeline send.";
        presentation.recommendedAction = "Install Pro Tools";
        presentation.disabledReason = "Install Pro Tools before sending.";
        return presentation;
    }

    if (snapshot.activationProxyVisible &&
        snapshot.failure == FailureCode::ActivationGate) {
        presentation.capabilityTag = "Activation required";
        presentation.preflightSummary =
            "Pro Tools is stopped at the PACE activation gate.";
        presentation.recommendedAction = "Finish Activation";
        presentation.disabledReason =
            "Finish Pro Tools activation, then open a session before sending.";
        return presentation;
    }

    if (!snapshot.proToolsProcessRunning) {
        presentation.capabilityTag = "Pro Tools not running";
        presentation.preflightSummary =
            "Open Pro Tools and a session, then place the edit cursor before sending.";
        presentation.recommendedAction = "Open Pro Tools";
        presentation.disabledReason =
            "Open Pro Tools and a session, then place the edit cursor before sending.";
        return presentation;
    }

    if (!snapshot.liveHostWindowPresent ||
        snapshot.readiness != Readiness::LivePtslPossible) {
        presentation.capabilityTag = "Session not ready";
        presentation.preflightSummary =
            "Pro Tools is running, but no live session/timeline window is ready for PTSL spot.";
        presentation.recommendedAction = "Prepare Session";
        presentation.disabledReason =
            "Open a Pro Tools session and place the edit cursor before sending.";
        return presentation;
    }

    presentation.capabilityTag = "Live PTSL Ready";
    presentation.preflightSummary =
        "Ready to spot selected audio into the Pro Tools timeline via the Harken Search bridge and the available Avid PTSL runtime.";
    presentation.recommendedAction = "Spot to Timeline";
    presentation.sendEnabled = true;
    return presentation;
}

ProToolsConnectionResetPresentation BuildProToolsConnectionResetPresentation(
    const CapabilitySnapshot& snapshot,
    bool bridgeHelperPresent) {
    ProToolsConnectionResetPresentation presentation;
    presentation.canResetHarkenSearchBridgeHelpers = true;

    std::ostringstream summary;
    summary << "Resets Harken Search-owned Pro Tools bridge state by terminating any stuck "
            << kProToolsBridgeHelperExeName << " helper process and clearing the next readiness probe.";
    if (!bridgeHelperPresent) {
        summary << " The helper is currently missing, so reset can only refresh status.";
    }

    if (snapshot.proToolsProcessRunning) {
        presentation.requiresProToolsRestart = true;
        presentation.canRequestProToolsClose = true;
        summary << " TCP 31416 is owned by ProTools.exe while Pro Tools is running; "
                << "Harken Search cannot directly close that listener without closing Pro Tools.";
    } else {
        summary << " ProTools.exe is not running, so the PTSL listener should disappear once helper cleanup is done.";
    }

    presentation.safetySummary = summary.str();
    return presentation;
}

ProToolsConnectionResetResult ResetProToolsConnection(bool closeProToolsAfterUserSaved) {
    ProToolsConnectionResetResult result;
    result.attempted = true;
    result.helperCleanupAttempted = true;
    ClearProToolsSessionCache();
#ifdef _WIN32
    result.helperProcessesTerminated = TerminateProcessesByImageName(kProToolsBridgeHelperExeName);
    result.proToolsWasRunning = IsProcessRunningByImageName(kProToolsExeName);
    if (closeProToolsAfterUserSaved && result.proToolsWasRunning) {
        result.proToolsCloseRequested = true;
        result.proToolsCloseMessagesPosted = RequestCloseProToolsAfterUserSaved();
    }

    result.ok = true;
    if (result.proToolsCloseRequested) {
        result.status = "Pro Tools close requested";
        result.detail =
            "Harken Search cleaned HarkenSearchProToolsBridge.exe helpers and asked Pro Tools to close normally. "
            "If Pro Tools shows a save prompt, choose the desired save action; TCP 31416 disappears only after Pro Tools exits.";
    } else if (result.proToolsWasRunning) {
        result.status = "Harken Search bridge reset; Pro Tools still owns listener";
        result.detail =
            "Harken Search cleaned HarkenSearchProToolsBridge.exe helpers. ProTools.exe is still running, so its PTSL listener on TCP 31416 remains open until Pro Tools is closed or restarted.";
    } else {
        result.status = "Harken Search bridge reset";
        result.detail =
            "Harken Search cleaned HarkenSearchProToolsBridge.exe helpers. ProTools.exe is not running, so the PTSL listener should be closed.";
    }
#else
    (void)closeProToolsAfterUserSaved;
    result.ok = false;
    result.status = "Reset unavailable";
    result.detail = "Pro Tools PTSL reset is only implemented on Windows.";
#endif
    return result;
}

ProToolsConnectionResetResult ResetProToolsBridgeOnHarkenSearchStartup() {
    auto result = ResetProToolsConnection(false);
    if (result.ok) {
        result.status = "Harken Search startup reset Pro Tools bridge";
        if (result.proToolsWasRunning) {
            result.detail =
                "Pro Tools bridge auto-reset at Harken Search startup: Harken Search cleared cached PTSL session state "
                "and terminated stuck HarkenSearchProToolsBridge.exe helpers. ProTools.exe was not closed; "
                "if Pro Tools still rejects RegisterConnection, save and restart Pro Tools to release TCP 31416.";
        } else {
            result.detail =
                "Pro Tools bridge auto-reset at Harken Search startup: Harken Search cleared cached PTSL session state "
                "and terminated stuck HarkenSearchProToolsBridge.exe helpers. ProTools.exe was not running.";
        }
    } else if (result.status.empty()) {
        result.status = "Harken Search startup Pro Tools bridge reset unavailable";
        result.detail =
            "Pro Tools bridge auto-reset at Harken Search startup could not run on this platform. ProTools.exe was not closed.";
    }
    return result;
}

ProtocolMessage BuildLivePtslSpotMessage(const std::string& correlationId,
                                          const std::string& stagedPathUtf8,
                                          const std::string& displayNameUtf8) {
    ProtocolMessage message;
    message.path = DeliveryPath::LivePtslSpot;
    message.correlationId = correlationId;
    message.stagedPathUtf8 = stagedPathUtf8;
    message.displayNameUtf8 = displayNameUtf8;
    message.spotToTimeline = true;
    return message;
}

ProtocolMessage BuildAafFallbackMessage(const std::string& correlationId,
                                         const std::string& stagedPathUtf8,
                                         const std::string& displayNameUtf8) {
    ProtocolMessage message;
    message.path = DeliveryPath::AafFallback;
    message.correlationId = correlationId;
    message.stagedPathUtf8 = stagedPathUtf8;
    message.displayNameUtf8 = displayNameUtf8;
    message.spotToTimeline = false;
    return message;
}

std::string BuildProtocolJson(const ProtocolMessage& message) {
    std::string json;
    json += "{";
    json += "\"protocol\":\"";
    json += kProtocolVersion;
    json += "\",\"path\":\"";
    json += DeliveryPathKey(message.path);
    json += "\",\"correlationId\":\"";
    json += JsonEscape(message.correlationId);
    json += "\",\"stagedPath\":\"";
    json += JsonEscape(message.stagedPathUtf8);
    json += "\",\"displayName\":\"";
    json += JsonEscape(message.displayNameUtf8);
    json += "\",\"spotToTimeline\":";
    json += message.spotToTimeline ? "true" : "false";
    json += "}";
    return json;
}

std::string BuildUserSdkGuideReadmeText(const InstallProbePlan& plan) {
    std::ostringstream oss;
    oss << "Harken Search Pro Tools PTSL setup\n\n";
    oss << "Harken Search cannot redistribute Avid PTSL client binaries by default. Download/build the latest Pro Tools Scripting SDK from:\n";
    oss << plan.sdkDownloadUrl << "\n\n";
    oss << "Build the C++ client wrapper, then place PTSLC_CPP.dll in this user-owned runtime folder:\n";
    oss << plan.userSdkDirectoryUtf8 << "\n\n";
    oss << "Expected runtime file:\n";
    oss << plan.userPtslClientDllPathUtf8 << "\n\n";
    oss << "Developer import library, if you are compiling against the SDK locally:\n";
    oss << JoinPath(plan.userSdkDirectoryUtf8, "PTSLC_CPP.lib") << "\n\n";
    oss << "Do not copy Avid DLLs into the Pro Tools install directory and do not overwrite Pro Tools files.\n";
    oss << "After staging PTSLC_CPP.dll, restart Harken Search and run Detect Pro Tools support again.\n";
    return oss.str();
}

std::string BuildLivePtslSpotRequestJson(const LivePtslSpotRequest& request) {
    std::string json;
    json += "{";
    json += "\"protocol\":\"";
    json += kProtocolVersion;
    json += "\",\"path\":\"";
    json += DeliveryPathKey(DeliveryPath::LivePtslSpot);
    json += "\",\"correlationId\":\"";
    json += JsonEscape(request.correlationId);
    json += "\",\"target\":\"";
    json += JsonEscape(request.target.empty() ? "selected_track_or_edit_cursor" : request.target);
    json += "\",\"spotMode\":\"";
    json += SpotModeKey(request.spotMode);
    json += "\",\"trackFallback\":\"";
    json += TrackFallbackKey(request.trackFallback);
    json += "\",\"files\":[";
    for (std::size_t i = 0; i < request.files.size(); ++i) {
        if (i > 0) {
            json += ",";
        }
        const auto& f = request.files[i];
        json += "{\"stagedPathUtf8\":\"";
        json += JsonEscape(f.stagedPathUtf8);
        json += "\",\"displayNameUtf8\":\"";
        json += JsonEscape(f.displayNameUtf8);
        json += "\",\"sourceSampleRate\":";
        json += std::to_string(f.sourceSampleRate);
        json += ",\"channelCount\":";
        json += std::to_string(f.channelCount);
        json += ",\"durationSamples\":";
        json += std::to_string(f.durationSamples);
        json += ",\"bwfTimeReferenceSamples\":";
        json += std::to_string(f.bwfTimeReferenceSamples);
        json += "}";
    }
    json += "]}";
    return json;
}

LivePtslSpotResponse ParseLivePtslSpotResponseJson(const std::string& json) {
    LivePtslSpotResponse response;
    if (json.empty() || json.front() != '{') {
        return response;
    }
    response.parseOk = true;
    response.ok = JsonBoolField(json, "ok", false);
    response.insertedCount = JsonIntField(json, "insertedCount", 0);
    response.sessionName = JsonStringField(json, "sessionName");
    response.trackName = JsonStringField(json, "trackName");
    response.errorCode = JsonStringField(json, "errorCode");
    response.errorMessage = JsonStringField(json, "errorMessage");
    response.ptslCommandTrace = JsonStringArrayField(json, "ptslCommandTrace");
    const auto path = JsonStringField(json, "path");
    response.path = path == DeliveryPathKey(DeliveryPath::AafFallback)
        ? DeliveryPath::AafFallback
        : DeliveryPath::LivePtslSpot;

    // v2 fields (older helpers omit them; defaults survive):
    response.ptVersionMajor = JsonIntField(json, "ptVersionMajor", 0);
    response.ptVersionMinor = JsonIntField(json, "ptVersionMinor", 0);
    response.ptVersionRevision = JsonIntField(json, "ptVersionRevision", 0);
    response.sessionSampleRate = JsonIntField(json, "sessionSampleRate", 0);
    response.pathTaken = JsonStringField(json, "pathTaken");
    response.resolvedTrackId = JsonStringField(json, "resolvedTrackId");
    response.resolvedTrackName = JsonStringField(json, "resolvedTrackName");
    response.trackWasCreated = JsonBoolField(json, "trackWasCreated", false);
    response.perClipSampleOffsets = JsonInt64ArrayField(json, "perClipSampleOffsets");
    return response;
}

std::string BuildLivePtslSpotResponseJson(const LivePtslSpotResponse& response) {
    std::string json;
    json += "{";
    json += "\"ok\":";
    json += response.ok ? "true" : "false";
    json += ",\"path\":\"";
    json += DeliveryPathKey(response.path);
    json += "\",\"insertedCount\":";
    json += std::to_string(response.insertedCount);
    json += ",\"sessionName\":\"";
    json += JsonEscape(response.sessionName);
    json += "\",\"trackName\":\"";
    json += JsonEscape(response.trackName);
    json += "\",\"errorCode\":\"";
    json += JsonEscape(response.errorCode);
    json += "\",\"errorMessage\":\"";
    json += JsonEscape(response.errorMessage);
    json += "\",\"ptslCommandTrace\":[";
    for (std::size_t i = 0; i < response.ptslCommandTrace.size(); ++i) {
        if (i > 0) {
            json += ",";
        }
        json += "\"";
        json += JsonEscape(response.ptslCommandTrace[i]);
        json += "\"";
    }
    json += "]";
    // v2 fields:
    json += ",\"ptVersionMajor\":";
    json += std::to_string(response.ptVersionMajor);
    json += ",\"ptVersionMinor\":";
    json += std::to_string(response.ptVersionMinor);
    json += ",\"ptVersionRevision\":";
    json += std::to_string(response.ptVersionRevision);
    json += ",\"sessionSampleRate\":";
    json += std::to_string(response.sessionSampleRate);
    json += ",\"pathTaken\":\"";
    json += JsonEscape(response.pathTaken);
    json += "\",\"resolvedTrackId\":\"";
    json += JsonEscape(response.resolvedTrackId);
    json += "\",\"resolvedTrackName\":\"";
    json += JsonEscape(response.resolvedTrackName);
    json += "\",\"trackWasCreated\":";
    json += response.trackWasCreated ? "true" : "false";
    json += ",\"perClipSampleOffsets\":[";
    for (std::size_t i = 0; i < response.perClipSampleOffsets.size(); ++i) {
        if (i > 0) json += ",";
        json += std::to_string(response.perClipSampleOffsets[i]);
    }
    json += "]}";
    return json;
}

bool IsLivePtslSpotSuccess(const LivePtslDispatchResult& result) {
    // exitCode 0 = one-shot helper exited cleanly.
    // exitCode 259 = STILL_ACTIVE (persistent helper stays alive for repeated sends).
    // Both are valid success exit codes; actual outcome is in response.ok.
    const bool validExitCode = result.exitCode == 0 || result.exitCode == 259;
    return result.helperAvailable &&
           validExitCode &&
           result.response.parseOk &&
           result.response.ok &&
           result.response.path == DeliveryPath::LivePtslSpot &&
           result.response.insertedCount > 0 &&
           !result.response.ptslCommandTrace.empty();
}

std::string ResolveProToolsBridgeHelperPath() {
#if defined(_MSC_VER)
    char* envBuffer = nullptr;
    std::size_t envLength = 0;
    if (_dupenv_s(&envBuffer, &envLength, "AURO_PROTOOLS_BRIDGE_EXE") == 0 && envBuffer != nullptr) {
        std::string overridePath = envBuffer;
        free(envBuffer);
        if (!overridePath.empty()) {
            return overridePath;
        }
    }
#else
    if (const char* overridePath = std::getenv("AURO_PROTOOLS_BRIDGE_EXE")) {
        if (*overridePath) {
            return overridePath;
        }
    }
#endif
#ifdef _WIN32
    const Platform::ExecutablePathResult executablePath = Platform::GetExecutablePath();
    if (executablePath.status == Platform::ExecutablePathStatus::Success &&
        !executablePath.pathUtf8.empty()) {
        std::filesystem::path exePath = std::filesystem::u8path(executablePath.pathUtf8);
        exePath = exePath.parent_path() / "HarkenSearchProToolsBridge.exe";
        return exePath.u8string();
    }
#endif
    return "HarkenSearchProToolsBridge.exe";
}

bool StartPersistentProToolsBridgeServer(const std::string& helperPathUtf8,
                                         const std::filesystem::path& queueDir,
                                         std::string& detail) {
#ifdef _WIN32
    if (IsProcessRunningByImageName(kProToolsBridgeHelperExeName)) {
        return true;
    }
    Platform::FileSystem::CreateDirectories(queueDir.u8string());
    const std::string args = "--server --queue \"" + queueDir.u8string() + "\"";
    if (!Platform::LaunchProcess(helperPathUtf8, args)) {
        detail = "HarkenSearchProToolsBridge.exe persistent server failed to start";
        return false;
    }
    detail = "HarkenSearchProToolsBridge persistent server started";
    return true;
#else
    (void)helperPathUtf8;
    (void)queueDir;
    detail = "HarkenSearchProToolsBridge persistent server is Windows-only";
    return false;
#endif
}

bool WaitForProToolsBridgeResponse(const std::filesystem::path& responsePath,
                                   unsigned long timeoutMs,
                                   std::string& json) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (Platform::FileSystem::Exists(responsePath.u8string())) {
            json.clear();
            if (!Platform::FileSystem::ReadFileText(responsePath.u8string(), json)) return false;
            return !json.empty();
        }
#ifdef _WIN32
        Platform::SleepMilliseconds(50);
#endif
    }
    return false;
}

LivePtslDispatchResult DispatchLivePtslPersistentRequest(const LivePtslSpotRequest& request) {
    LivePtslDispatchResult result;
    result.helperPathUtf8 = ResolveProToolsBridgeHelperPath();
    if (!Platform::FileSystem::Exists(result.helperPathUtf8)) {
        result.detail = "HarkenSearchProToolsBridge.exe not found";
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.path = DeliveryPath::LivePtslSpot;
        result.response.errorCode = "helperUnavailable";
        result.response.errorMessage = result.detail;
        return result;
    }
    result.helperAvailable = true;

    const auto queueDir = ProToolsPersistentQueueDir();
    Platform::FileSystem::CreateDirectories(queueDir.u8string());
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string baseName = "auro-protools-live-" + std::to_string(static_cast<long long>(now));
    const auto requestPath = queueDir / (baseName + "-request.json");
    const auto responsePath = queueDir / (baseName + "-response.json");
    result.requestPathUtf8 = requestPath.u8string();
    result.responsePathUtf8 = responsePath.u8string();

    Platform::FileSystem::WriteFileText(
        requestPath.u8string(), BuildLivePtslSpotRequestJson(request));

    std::string startDetail;
    if (!StartPersistentProToolsBridgeServer(result.helperPathUtf8, queueDir, startDetail)) {
        result.detail = startDetail;
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.path = DeliveryPath::LivePtslSpot;
        result.response.errorCode = "helperLaunchFailed";
        result.response.errorMessage = result.detail;
        return result;
    }
    result.detail = "persistentHarkenSearchProToolsBridge";

    std::string responseJson;
    if (!WaitForProToolsBridgeResponse(responsePath, kProToolsBridgeTimeoutMs, responseJson)) {
#ifdef _WIN32
        TerminateProcessesByImageName(kProToolsBridgeHelperExeName);
#endif
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.path = DeliveryPath::LivePtslSpot;
        result.response.insertedCount = 0;
        result.response.errorCode = "helperTimedOut";
        result.response.errorMessage =
            "Persistent Pro Tools bridge did not answer before the timeout. "
            "Harken Search reset the helper; retry once after Pro Tools finishes processing.";
        return result;
    }
    result.exitCode = 259; // STILL_ACTIVE: persistent helper remains alive for repeated sends.
    result.response = ParseLivePtslSpotResponseJson(responseJson);
    if (!result.response.parseOk) {
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.errorCode = "responseMissing";
        result.response.errorMessage = "HarkenSearchProToolsBridge persistent server did not write a parseable response";
    }
    return result;
}

LivePtslDispatchResult DispatchLivePtslHelper(const LivePtslSpotRequest* request, bool smokeOnly) {
    LivePtslDispatchResult result;
    result.helperPathUtf8 = ResolveProToolsBridgeHelperPath();

    if (!Platform::FileSystem::Exists(result.helperPathUtf8)) {
        result.detail = "HarkenSearchProToolsBridge.exe not found";
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.path = DeliveryPath::LivePtslSpot;
        result.response.errorCode = "helperUnavailable";
        result.response.errorMessage = result.detail;
        return result;
    }
    result.helperAvailable = true;

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tempRoot = std::filesystem::u8path(Platform::FileSystem::TemporaryDirectory());
    const std::string baseName = smokeOnly
        ? "auro-protools-smoke-" + std::to_string(static_cast<long long>(now))
        : "auro-protools-" + std::to_string(static_cast<long long>(now));
    const auto requestPath = tempRoot / (baseName + "-request.json");
    const auto responsePath = tempRoot / (baseName + "-response.json");
    result.requestPathUtf8 = smokeOnly ? std::string{} : requestPath.u8string();
    result.responsePathUtf8 = responsePath.u8string();

    if (!smokeOnly && request) {
        Platform::FileSystem::WriteFileText(
            requestPath.u8string(), BuildLivePtslSpotRequestJson(*request));
    }

#ifdef _WIN32
    std::string commandLineUtf8 = "\"" + result.helperPathUtf8 + "\" ";
    if (smokeOnly) {
        commandLineUtf8 += "--smoke ";
    } else {
        commandLineUtf8 += "--request \"" + result.requestPathUtf8 + "\" ";
    }
    commandLineUtf8 += "--response \"" + result.responsePathUtf8 + "\"";
    const std::wstring applicationName =
        Platform::TextEncoding::Utf8ToWidePermissive(result.helperPathUtf8);
    const std::wstring commandLine =
        Platform::TextEncoding::Utf8ToWidePermissive(commandLineUtf8);
    Platform::ProcessLaunchOptions launchOptions;
    const auto run = Platform::RunProcessAndWait(
        applicationName, commandLine, launchOptions,
        kProToolsBridgeTimeoutMs, 1460);
    if (run.status == Platform::ProcessRunStatus::LaunchFailed) {
        result.detail = "HarkenSearchProToolsBridge.exe failed to start";
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.errorCode = "helperLaunchFailed";
        result.response.errorMessage = result.detail;
        return result;
    }
    result.exitCode = run.exitCode;
    if (run.status == Platform::ProcessRunStatus::TimedOut) {
        result.detail = "HarkenSearchProToolsBridge.exe timed out while waiting for Pro Tools PTSL readiness";
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.path = DeliveryPath::LivePtslSpot;
        result.response.insertedCount = 0;
        result.response.errorCode = "helperTimedOut";
        result.response.errorMessage =
            "Pro Tools did not become available to PTSL quickly enough. "
            "Wait until Pro Tools is fully loaded with a session open, then retry.";
    } else if (run.status == Platform::ProcessRunStatus::WaitFailed) {
        result.detail = "HarkenSearchProToolsBridge.exe wait failed";
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.path = DeliveryPath::LivePtslSpot;
        result.response.errorCode = "helperWaitFailed";
        result.response.errorMessage = result.detail;
    }
#else
    result.exitCode = 1;
    result.detail = "HarkenSearchProToolsBridge dispatch is Windows-only";
#endif

    std::string responseJson;
    Platform::FileSystem::ReadFileText(responsePath.u8string(), responseJson);
    if (result.response.errorCode == "helperTimedOut" && responseJson.empty()) {
        return result;
    }
    result.response = ParseLivePtslSpotResponseJson(responseJson);
    if (!result.response.parseOk) {
        result.response.parseOk = true;
        result.response.ok = false;
        result.response.errorCode = "responseMissing";
        result.response.errorMessage = "HarkenSearchProToolsBridge.exe did not write a parseable response";
    }
    return result;
}

LivePtslDispatchResult DispatchLivePtslSmokeTest() {
    return DispatchLivePtslHelper(nullptr, true);
}

LivePtslDispatchResult DispatchLivePtslSpotRequest(const LivePtslSpotRequest& request) {
    return DispatchLivePtslPersistentRequest(request);
}

namespace {

bool IsZeroOrEmptyTimelineSample(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    for (const char c : value) {
        if (c != '0' && c != ' ' && c != '\t') {
            return false;
        }
    }
    return true;
}

} // namespace

std::string SelectTimelineBaseSamples(SpotMode spotMode,
                                      const std::string& timelineInTime,
                                      const std::string& timelinePlayStartMarker)
{
    if (spotMode == SpotMode::ToSelectionIn) {
        return timelineInTime;
    }

    // AtCursor/ToTimecode fallback semantics:
    // Pro Tools may keep play_start_marker_time at an older transport start
    // after the user moves the edit cursor. A non-zero in_time is the current
    // edit insertion/selection start, so it must win. The play-start marker is
    // fallback when in_time is zero but the transport cursor is not.
    // in_time is reported as 0 while the transport cursor is non-zero.
    if (!IsZeroOrEmptyTimelineSample(timelineInTime)) {
        return timelineInTime;
    }
    if (!IsZeroOrEmptyTimelineSample(timelinePlayStartMarker)) {
        return timelinePlayStartMarker;
    }
    return timelineInTime.empty() ? timelinePlayStartMarker : timelineInTime;
}

// S4: Sequential or timecode-based per-clip position layout.
std::vector<std::int64_t> ComputeSpotPositions(
    const std::vector<LivePtslSpotFile>& files,
    SpotMode spotMode,
    std::int64_t basePosition,
    std::int32_t sessionSampleRate)
{
    std::vector<std::int64_t> positions;
    positions.reserve(files.size());

    if (spotMode == SpotMode::ToTimecode) {
        // Each clip lands at its own BWF-derived absolute position; no sequential layout.
        for (const auto& f : files) {
            if (f.bwfTimeReferenceSamples == 0 || f.sourceSampleRate == 0 || sessionSampleRate == 0) {
                positions.push_back(basePosition);
            } else {
                const std::int64_t pos = static_cast<std::int64_t>(
                    static_cast<double>(f.bwfTimeReferenceSamples)
                    * static_cast<double>(sessionSampleRate)
                    / static_cast<double>(f.sourceSampleRate)
                    + 0.5);
                positions.push_back(pos);
            }
        }
        return positions;
    }

    // AtCursor / ToSelectionIn: butt-joint sequential layout from basePosition.
    std::int64_t cursor = basePosition;
    for (const auto& f : files) {
        positions.push_back(cursor);
        if (f.durationSamples == 0) {
            continue;
        }
        if (f.sourceSampleRate == 0 || sessionSampleRate == 0 ||
            f.sourceSampleRate == static_cast<std::uint32_t>(sessionSampleRate)) {
            cursor += static_cast<std::int64_t>(f.durationSamples);
        } else {
            const std::int64_t scaled = static_cast<std::int64_t>(
                static_cast<double>(f.durationSamples)
                * static_cast<double>(sessionSampleRate)
                / static_cast<double>(f.sourceSampleRate)
                + 0.5);
            cursor += scaled;
        }
    }
    return positions;
}

} // namespace HarkenSearch::ProTools
