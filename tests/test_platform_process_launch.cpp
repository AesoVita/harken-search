#include "platform/process_launch.h"
#include "tests/process_launch_test_seam.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace Platform;
using Platform::ProcessLaunchTesting::BackendOps;

struct State {
    bool createOk = true;
    bool closeResult = true;
    DWORD error = ERROR_SUCCESS;
    DWORD processId = 7331;
    HANDLE process = reinterpret_cast<HANDLE>(0x101);
    HANDLE thread = reinterpret_cast<HANDLE>(0x202);
    int createCalls = 0;
    std::wstring application;
    std::wstring command;
    LPSECURITY_ATTRIBUTES processAttributes = reinterpret_cast<LPSECURITY_ATTRIBUTES>(1);
    LPSECURITY_ATTRIBUTES threadAttributes = reinterpret_cast<LPSECURITY_ATTRIBUTES>(1);
    BOOL inheritHandles = TRUE;
    DWORD flags = 0;
    LPVOID environment = reinterpret_cast<LPVOID>(1);
    LPCWSTR currentDirectory = reinterpret_cast<LPCWSTR>(1);
    DWORD startupCb = 0;
    std::vector<HANDLE> closed;
} g;
int failures = 0;

void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void Reset(bool createOk = true, DWORD error = ERROR_SUCCESS) {
    g = {};
    g.createOk = createOk; g.error = error; g.processId = 7331;
    g.process = reinterpret_cast<HANDLE>(0x101);
    g.thread = reinterpret_cast<HANDLE>(0x202);
}
BOOL Create(LPCWSTR application, LPWSTR command,
            LPSECURITY_ATTRIBUTES processAttributes,
            LPSECURITY_ATTRIBUTES threadAttributes, BOOL inheritHandles,
            DWORD flags, LPVOID environment, LPCWSTR currentDirectory,
            LPSTARTUPINFOW startup, LPPROCESS_INFORMATION processInfo) {
    ++g.createCalls;
    g.application = application ? application : L"";
    g.command = command ? command : L"";
    g.processAttributes = processAttributes; g.threadAttributes = threadAttributes;
    g.inheritHandles = inheritHandles; g.flags = flags; g.environment = environment;
    g.currentDirectory = currentDirectory; g.startupCb = startup ? startup->cb : 0;
    processInfo->dwProcessId = g.processId;
    processInfo->hProcess = g.process; processInfo->hThread = g.thread;
    return g.createOk ? TRUE : FALSE;
}
DWORD LastError() { return g.error; }
BOOL Close(HANDLE handle) {
    g.closed.push_back(handle);
    return g.closeResult ? TRUE : FALSE;
}
const BackendOps kOps{Create, LastError, Close};
}

int main() {
    Platform::ProcessLaunchTesting::SetBackendOps(&kOps);
    ProcessLaunchOptions options;

    std::wstring nulApplication = L"C:/helper.exe";
    nulApplication.push_back(L'\0'); nulApplication += L"shadow";
    std::wstring nulCommand = L"--pipe ok";
    nulCommand.push_back(L'\0'); nulCommand += L"shadow";
    for (const auto& invalid : std::vector<std::pair<std::wstring_view, std::wstring_view>>{
             {L"", L""}, {nulApplication, L""}, {L"C:/helper.exe", nulCommand}}) {
        Reset();
        const auto result = LaunchProcess(invalid.first, invalid.second, options);
        Check(result.status == ProcessLaunchStatus::InvalidArgument &&
                  result.processId == 0 && result.systemError == 0 && g.createCalls == 0,
              "invalid arguments reject before native create");
    }

    Reset();
    auto result = LaunchProcess(L"C:/Bridge Dir/妗ユ帴 Helper.exe",
                                L"\"C:/Bridge Dir/妗ユ帴 Helper.exe\" --token 浠ょ墝馃帶",
                                options);
    Check(result.status == ProcessLaunchStatus::Ok && result.processId == 7331 &&
              result.systemError == 0, "success returns PID without system error");
    Check(g.createCalls == 1 && g.application == L"C:/Bridge Dir/妗ユ帴 Helper.exe" &&
              g.command == L"\"C:/Bridge Dir/妗ユ帴 Helper.exe\" --token 浠ょ墝馃帶",
          "success forwards exact application and mutable command");
    Check(g.processAttributes == nullptr && g.threadAttributes == nullptr &&
              g.inheritHandles == FALSE && g.flags == CREATE_NO_WINDOW &&
              g.environment == nullptr && g.currentDirectory == nullptr &&
              g.startupCb == sizeof(STARTUPINFOW),
          "default options preserve native create parameters");
    Check(g.closed == std::vector<HANDLE>{g.thread, g.process},
          "success closes thread then process exactly once");

    Reset();
    g.processId = 0;
    g.process = nullptr;
    g.thread = INVALID_HANDLE_VALUE;
    g.closeResult = false;
    result = LaunchProcess(L"C:/helper.exe", L"\"C:/helper.exe\"", options);
    Check(result.status == ProcessLaunchStatus::Ok && result.processId == 0 &&
              result.systemError == 0,
          "anomalous native success remains successful with zero PID");
    Check(g.closed == std::vector<HANDLE>{INVALID_HANDLE_VALUE, nullptr},
          "anomalous native success attempts both failing closes in order");

    Reset(); options.noWindow = false; options.inheritHandles = true;
    result = LaunchProcess(L"C:/helper.exe", L"", options);
    Check(result.status == ProcessLaunchStatus::Ok && g.command.empty() &&
              g.inheritHandles == TRUE && g.flags == 0,
          "empty command and alternate options reach native create");

    for (DWORD error : {ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND, ERROR_ACCESS_DENIED}) {
        Reset(false, error);
        result = LaunchProcess(L"C:/helper.exe", L"\"C:/helper.exe\"", options);
        Check(result.status == ProcessLaunchStatus::Failed && result.processId == 0 &&
                  result.systemError == error && g.createCalls == 1 && g.closed.empty(),
              "native failure preserves error and never closes poisoned handles");
    }
    Platform::ProcessLaunchTesting::SetBackendOps(nullptr);
    return failures == 0 ? 0 : 1;
}
