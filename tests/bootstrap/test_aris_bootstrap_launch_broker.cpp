#include "bootstrap/WindowsBootstrapLaunchBroker.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace aris::bootstrap;

class ScopedHandle final {
public:
    explicit ScopedHandle(HANDLE handle = nullptr) noexcept : handle_(handle) {}
    ~ScopedHandle() { reset(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.release()) {}
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    HANDLE get() const noexcept { return handle_; }
    HANDLE release() noexcept {
        HANDLE value = handle_;
        handle_ = nullptr;
        return value;
    }
    void reset(HANDLE handle = nullptr) noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_ = nullptr;
};

class ScopedAttributeList final {
public:
    ~ScopedAttributeList() {
        if (list_ != nullptr) DeleteProcThreadAttributeList(list_);
    }
    bool initialize(const DWORD count) {
        SIZE_T bytes = 0U;
        static_cast<void>(InitializeProcThreadAttributeList(
            nullptr, count, 0U, &bytes));
        if (bytes == 0U) return false;
        storage_.resize(bytes);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        return InitializeProcThreadAttributeList(
                   list_, count, 0U, &bytes) != FALSE;
    }
    LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept { return list_; }

private:
    std::vector<std::uint8_t> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

std::wstring currentModulePath() {
    std::vector<wchar_t> path(512U);
    for (;;) {
        const DWORD length = GetModuleFileNameW(
            nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0U) return {};
        if (length < path.size() - 1U) return {path.data(), length};
        if (path.size() >= 32768U) return {};
        path.resize(path.size() * 2U);
    }
}

std::wstring finalPath(HANDLE file) {
    std::vector<wchar_t> path(512U);
    for (;;) {
        const DWORD length = GetFinalPathNameByHandleW(
            file, path.data(), static_cast<DWORD>(path.size()),
            FILE_NAME_NORMALIZED);
        if (length == 0U) return {};
        if (length < path.size() - 1U) return {path.data(), length};
        if (path.size() >= 32768U) return {};
        path.resize(path.size() * 2U);
    }
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string output(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), output.data(),
                            required, nullptr, nullptr) != required) {
        return {};
    }
    return output;
}

bool captureIdentity(HANDLE process, BootstrapProcessIdentity& identity) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return false;
    ULARGE_INTEGER creation{};
    creation.LowPart = created.dwLowDateTime;
    creation.HighPart = created.dwHighDateTime;
    std::vector<wchar_t> image(512U);
    DWORD imageLength = static_cast<DWORD>(image.size());
    while (!QueryFullProcessImageNameW(
        process, 0U, image.data(), &imageLength)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            image.size() >= 32768U) {
            return false;
        }
        image.resize(image.size() * 2U);
        imageLength = static_cast<DWORD>(image.size());
    }
    const std::wstring requested(image.data(), imageLength);
    ScopedHandle file(CreateFileW(
        requested.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) return false;
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    FILE_ID_INFO fileIdentity{};
    std::wstring canonical = finalPath(file.get());
    if (GetFileInformationByHandleEx(file.get(), FileAttributeTagInfo,
                                     &attributes, sizeof(attributes)) == FALSE ||
        GetFileInformationByHandleEx(file.get(), FileIdInfo, &fileIdentity,
                                     sizeof(fileIdentity)) == FALSE ||
        (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
        canonical.empty()) {
        return false;
    }
    if (canonical.rfind(L"\\\\?\\", 0U) == 0U) canonical.erase(0U, 4U);
    for (wchar_t& character : canonical) {
        if (character == L'\\') character = L'/';
    }
    identity.valid = true;
    identity.processId = GetProcessId(process);
    identity.creationTime = creation.QuadPart;
    identity.imagePath = wideToUtf8(canonical);
    identity.imageVolumeSerial = fileIdentity.VolumeSerialNumber;
    std::copy(std::begin(fileIdentity.FileId.Identifier),
              std::end(fileIdentity.FileId.Identifier),
              identity.imageFileId.begin());
    return identity.processId != 0U && !identity.imagePath.empty();
}

std::wstring uniquePipeName() {
    return L"\\\\.\\pipe\\aris-bootstrap-launch-broker-test-" +
           std::to_wstring(GetCurrentProcessId()) + L"-" +
           std::to_wstring(GetTickCount64());
}

std::filesystem::path sibling(const wchar_t* name) {
    const std::wstring module = currentModulePath();
    return module.empty()
        ? std::filesystem::path{}
        : std::filesystem::path(module).parent_path() / name;
}

bool startBroker(WindowsBootstrapLaunchBrokerCapability& capability) {
    const std::wstring pipeName = uniquePipeName();
    ScopedHandle controllerPipe(CreateNamedPipeW(
        pipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
            PIPE_REJECT_REMOTE_CLIENTS,
        1U, 64U * 1024U, 64U * 1024U, 0U, nullptr));
    if (controllerPipe.get() == INVALID_HANDLE_VALUE) return false;
    SECURITY_ATTRIBUTES inherited{};
    inherited.nLength = sizeof(inherited);
    inherited.bInheritHandle = TRUE;
    ScopedHandle brokerPipe(CreateFileW(
        pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0U, &inherited,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (brokerPipe.get() == INVALID_HANDLE_VALUE) return false;
    if (ConnectNamedPipe(controllerPipe.get(), nullptr) == FALSE &&
        GetLastError() != ERROR_PIPE_CONNECTED) {
        return false;
    }

    ScopedHandle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    if (job.get() == nullptr ||
        SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                &limits, sizeof(limits)) == FALSE) {
        return false;
    }

    ScopedAttributeList attributes;
    if (!attributes.initialize(2U)) return false;
    HANDLE inheritedPipe = brokerPipe.get();
    HANDLE inheritedJob = job.get();
    if (UpdateProcThreadAttribute(
            attributes.get(), 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            &inheritedPipe, sizeof(inheritedPipe), nullptr, nullptr) == FALSE ||
        UpdateProcThreadAttribute(attributes.get(), 0U,
                                  PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                  &inheritedJob, sizeof(inheritedJob), nullptr,
                                  nullptr) == FALSE) {
        return false;
    }

    const std::wstring executable = currentModulePath();
    if (executable.empty()) return false;
    std::wstring command = L"\"" + executable +
                           L"\" --aris-bootstrap-launch-broker";
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = brokerPipe.get();
    startup.StartupInfo.hStdOutput = brokerPipe.get();
    startup.StartupInfo.hStdError = brokerPipe.get();
    startup.lpAttributeList = attributes.get();
    PROCESS_INFORMATION process{};
    if (CreateProcessW(
            executable.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
            CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT |
                EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr, &startup.StartupInfo, &process) == FALSE) {
        return false;
    }
    ScopedHandle brokerProcess(process.hProcess);
    ScopedHandle brokerThread(process.hThread);
    BootstrapProcessIdentity identity;
    if (!captureIdentity(brokerProcess.get(), identity) ||
        ResumeThread(brokerThread.get()) == static_cast<DWORD>(-1)) {
        return false;
    }
    brokerPipe.reset();
    capability.channelHandle = reinterpret_cast<std::uintptr_t>(
        controllerPipe.release());
    capability.brokerProcessHandle = reinterpret_cast<std::uintptr_t>(
        brokerProcess.release());
    capability.containmentJobHandle = reinterpret_cast<std::uintptr_t>(
        job.release());
    capability.brokerIdentity = identity;
    capability.launchTimeoutMs = 5000U;
    return true;
}

bool testMissingCapabilityIsRejected() {
    WindowsBootstrapLaunchBrokerCapability capability;
    const auto process = CreateBrokeredNativeBootstrapProcessPort(capability);
    return process == nullptr && !capability.valid();
}

bool testExactPreestablishedCapabilityIsAccepted() {
    WindowsBootstrapLaunchBrokerCapability capability;
    if (!startBroker(capability)) return false;
    const auto process = CreateBrokeredNativeBootstrapProcessPort(capability);
    return process != nullptr && !capability.valid();
}

bool testCopiedCapabilityCannotReplayTheBrokerSession() {
    WindowsBootstrapLaunchBrokerCapability capability;
    if (!startBroker(capability)) return false;
    WindowsBootstrapLaunchBrokerCapability replay = capability;
    const auto accepted = CreateBrokeredNativeBootstrapProcessPort(capability);
    const auto rejected = CreateBrokeredNativeBootstrapProcessPort(replay);
    return accepted != nullptr && rejected == nullptr && !capability.valid() &&
           !replay.valid();
}

bool testBrokerSessionSupportsUpdatedChildAndRollbackRelaunch() {
    WindowsBootstrapLaunchBrokerCapability capability;
    const std::filesystem::path fixture =
        sibling(L"aris_bootstrap_sleep_fixture.exe");
    if (fixture.empty() || !startBroker(capability)) return false;
    const auto process = CreateBrokeredNativeBootstrapProcessPort(capability);
    if (process == nullptr) return false;

    BootstrapProcessResult first = process->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    if (!first.ok() || !first.process.valid() ||
        WaitForSingleObject(
            reinterpret_cast<HANDLE>(first.process.waitToken), 0U) !=
            WAIT_TIMEOUT) {
        return false;
    }
    BootstrapOwnedProcess updatedChild = first.process;
    if (!process->TerminateOwned(updatedChild, 0xE101U).ok()) return false;

    BootstrapProcessResult second = process->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    if (!second.ok() || !second.process.valid() ||
        WaitForSingleObject(
            reinterpret_cast<HANDLE>(second.process.waitToken), 0U) !=
            WAIT_TIMEOUT) {
        return false;
    }
    BootstrapOwnedProcess restoredApplication = second.process;
    return process->TerminateOwned(restoredApplication, 0xE102U).ok();
}

bool testBrokerCrashBetweenRequestsRejectsRollbackRelaunch() {
    WindowsBootstrapLaunchBrokerCapability capability;
    const std::filesystem::path fixture =
        sibling(L"aris_bootstrap_sleep_fixture.exe");
    if (fixture.empty() ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_TEST_BROKER_CUT",
                                L"crash-after-first") == FALSE) {
        return false;
    }
    const bool started = startBroker(capability);
    static_cast<void>(SetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_TEST_BROKER_CUT", nullptr));
    if (!started) return false;
    const auto process = CreateBrokeredNativeBootstrapProcessPort(capability);
    if (process == nullptr) return false;
    BootstrapProcessResult first = process->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    if (!first.ok() || !first.process.valid()) return false;
    BootstrapOwnedProcess updatedChild = first.process;
    if (!process->TerminateOwned(updatedChild, 0xE105U).ok()) return false;
    Sleep(50U);
    const BootstrapProcessResult second = process->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    return second.status == BootstrapProcessStatus::LaunchFailed &&
           !second.process.valid();
}

DWORD readPublishedProcessId(const std::filesystem::path& marker) {
    ScopedHandle file(CreateFileW(
        marker.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) return 0U;
    DWORD processId = 0U;
    DWORD read = 0U;
    return ReadFile(file.get(), &processId, sizeof(processId), &read,
                    nullptr) != FALSE &&
            read == sizeof(processId)
        ? processId
        : 0U;
}

bool processIsTerminal(const DWORD processId) {
    ScopedHandle process(OpenProcess(SYNCHRONIZE, FALSE, processId));
    return process.get() == nullptr ||
           WaitForSingleObject(process.get(), 0U) == WAIT_OBJECT_0;
}

bool testReleasedBrokerChildRebindsItsCommittedProcessTree() {
    WindowsBootstrapLaunchBrokerCapability capability;
    const std::filesystem::path fixture =
        sibling(L"aris_bootstrap_sleep_fixture.exe");
    const std::filesystem::path marker =
        std::filesystem::temp_directory_path() /
        ("aris-bootstrap-broker-recovery-tree-" +
         std::to_string(GetCurrentProcessId()) + ".bin");
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    const std::wstring markerText = marker.wstring();
    if (fixture.empty() ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_SLEEP_MS", L"10000") ==
            FALSE ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_DESCENDANT_MARKER",
                                markerText.c_str()) == FALSE) {
        return false;
    }
    const bool started = startBroker(capability);
    static_cast<void>(SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_SLEEP_MS",
                                               nullptr));
    static_cast<void>(SetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_DESCENDANT_MARKER", nullptr));
    if (!started) return false;
    auto owner = CreateBrokeredNativeBootstrapProcessPort(capability);
    auto recovery = CreateNativeBootstrapProcessPort();
    if (owner == nullptr || recovery == nullptr) return false;
    BootstrapProcessResult launched = owner->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    if (!launched.ok() || !launched.process.valid()) return false;
    BootstrapOwnedProcess child = launched.process;
    const BootstrapProcessIdentity durableIdentity = child.identity;
    if (!owner->ResumeOwned(child).ok()) return false;
    DWORD descendantId = 0U;
    for (DWORD waited = 0U; waited < 5000U && descendantId == 0U;
         waited += 10U) {
        descendantId = readPublishedProcessId(marker);
        if (descendantId == 0U) Sleep(10U);
    }
    if (descendantId == 0U || !owner->PrepareOwnedForCommit(child).ok() ||
        !owner->ReleaseOwned(child).ok()) {
        return false;
    }
    BootstrapProcessResult rebound =
        recovery->RebindOwnedChild(durableIdentity);
    bool terminated = false;
    if (rebound.ok() && rebound.process.valid()) {
        BootstrapOwnedProcess recoveredChild = rebound.process;
        terminated = recovery->TerminateOwned(recoveredChild, 0xE107U).ok();
    }
    const bool markerReleased = DeleteFileW(markerText.c_str()) != FALSE;
    if (!markerReleased) {
        ScopedHandle descendant(OpenProcess(
            PROCESS_TERMINATE | SYNCHRONIZE, FALSE, descendantId));
        if (descendant.get() != nullptr) {
            static_cast<void>(TerminateProcess(descendant.get(), 0xE107U));
            static_cast<void>(WaitForSingleObject(descendant.get(), 5000U));
        }
        std::filesystem::remove(marker, ignored);
    }
    return terminated && markerReleased;
}

bool testRootAbsentReleasedTreeIsRecoveryAmbiguity() {
    WindowsBootstrapLaunchBrokerCapability capability;
    const std::filesystem::path fixture =
        sibling(L"aris_bootstrap_sleep_fixture.exe");
    const std::filesystem::path marker =
        std::filesystem::temp_directory_path() /
        ("aris-bootstrap-broker-absent-root-tree-" +
         std::to_string(GetCurrentProcessId()) + ".bin");
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    const std::wstring markerText = marker.wstring();
    if (fixture.empty() ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_SLEEP_MS", L"500") == FALSE ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_DESCENDANT_MARKER",
                                markerText.c_str()) == FALSE) {
        return false;
    }
    const bool started = startBroker(capability);
    static_cast<void>(SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_SLEEP_MS",
                                               nullptr));
    static_cast<void>(SetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_DESCENDANT_MARKER", nullptr));
    if (!started) return false;
    auto owner = CreateBrokeredNativeBootstrapProcessPort(capability);
    auto recovery = CreateNativeBootstrapProcessPort();
    if (owner == nullptr || recovery == nullptr) return false;
    BootstrapProcessResult launched = owner->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    if (!launched.ok() || !launched.process.valid()) return false;
    BootstrapOwnedProcess child = launched.process;
    const BootstrapProcessIdentity durableIdentity = child.identity;
    if (!owner->ResumeOwned(child).ok()) return false;
    DWORD descendantId = 0U;
    for (DWORD waited = 0U; waited < 5000U && descendantId == 0U;
         waited += 10U) {
        descendantId = readPublishedProcessId(marker);
        if (descendantId == 0U) Sleep(10U);
    }
    if (descendantId == 0U || !owner->PrepareOwnedForCommit(child).ok() ||
        !owner->ReleaseOwned(child).ok()) {
        return false;
    }
    Sleep(1000U);
    const BootstrapProcessResult rebound =
        recovery->RebindOwnedChild(durableIdentity);
    ScopedHandle descendant(OpenProcess(
        PROCESS_TERMINATE | SYNCHRONIZE, FALSE, descendantId));
    const bool descendantWasLive = descendant.get() != nullptr &&
        WaitForSingleObject(descendant.get(), 0U) == WAIT_TIMEOUT;
    if (descendant.get() != nullptr) {
        static_cast<void>(TerminateProcess(descendant.get(), 0xE108U));
        static_cast<void>(WaitForSingleObject(descendant.get(), 5000U));
    }
    const bool markerReleased = DeleteFileW(markerText.c_str()) != FALSE;
    const bool passed =
        rebound.status == BootstrapProcessStatus::IdentityMismatch &&
           descendantWasLive && markerReleased;
    if (!passed) {
        std::cerr << "absent-root status=" << static_cast<int>(rebound.status)
                  << " native=" << rebound.nativeError
                  << " descendant-live=" << descendantWasLive
                  << " marker-released=" << markerReleased << '\n';
    }
    return passed;
}

bool brokerRequestTimeoutLeavesNoLateChild(const std::uint32_t sequence) {
    WindowsBootstrapLaunchBrokerCapability capability;
    const std::filesystem::path fixture =
        sibling(L"aris_bootstrap_sleep_fixture.exe");
    const std::filesystem::path marker =
        std::filesystem::temp_directory_path() /
        ("aris-bootstrap-broker-timeout-" + std::to_string(sequence) + '-' +
         std::to_string(GetCurrentProcessId()) + ".bin");
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    const std::wstring markerText = marker.wstring();
    const std::wstring cut =
        L"stall-after-create-" + std::to_wstring(sequence);
    if (fixture.empty() ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_TEST_BROKER_CUT",
                                cut.c_str()) == FALSE ||
        SetEnvironmentVariableW(L"ARIS_BOOTSTRAP_TEST_BROKER_CHILD_PID",
                                markerText.c_str()) == FALSE) {
        return false;
    }
    const bool started = startBroker(capability);
    static_cast<void>(SetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_TEST_BROKER_CUT", nullptr));
    static_cast<void>(SetEnvironmentVariableW(
        L"ARIS_BOOTSTRAP_TEST_BROKER_CHILD_PID", nullptr));
    if (!started) return false;
    capability.launchTimeoutMs = 100U;
    const auto process = CreateBrokeredNativeBootstrapProcessPort(capability);
    if (process == nullptr) return false;
    if (sequence == 2U) {
        BootstrapProcessResult first = process->LaunchSuspended(
            fixture.u8string(), fixture.parent_path().u8string(), {});
        if (!first.ok() || !first.process.valid()) return false;
        BootstrapOwnedProcess updatedChild = first.process;
        if (!process->TerminateOwned(updatedChild, 0xE106U).ok()) return false;
    }
    const ULONGLONG startedAt = GetTickCount64();
    const BootstrapProcessResult launched = process->LaunchSuspended(
        fixture.u8string(), fixture.parent_path().u8string(), {});
    const ULONGLONG elapsed = GetTickCount64() - startedAt;
    const DWORD childId = readPublishedProcessId(marker);
    const bool terminal = childId != 0U && processIsTerminal(childId);
    const bool removed = std::filesystem::remove(marker, ignored);
    const bool passed = launched.status == BootstrapProcessStatus::LaunchFailed &&
           launched.nativeError == ERROR_TIMEOUT && !launched.process.valid() &&
           elapsed < 2000U && terminal && removed && !ignored;
    if (!passed) {
        std::cerr << "broker-timeout sequence=" << sequence << " status="
                  << static_cast<int>(launched.status)
                  << " native=" << launched.nativeError
                  << " elapsed=" << elapsed << " child=" << childId
                  << " terminal=" << terminal << " removed=" << removed
                  << " cleanup=" << ignored.value() << '\n';
    }
    return passed;
}

bool testFirstBrokerRequestTimeoutLeavesNoLateChild() {
    return brokerRequestTimeoutLeavesNoLateChild(1U);
}

bool testSecondBrokerRequestTimeoutLeavesNoLateChild() {
    return brokerRequestTimeoutLeavesNoLateChild(2U);
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc == 2 &&
        std::wstring_view(argv[1]) ==
            L"--aris-bootstrap-launch-broker") {
        return RunWindowsBootstrapLaunchBroker(
            reinterpret_cast<std::uintptr_t>(GetStdHandle(STD_INPUT_HANDLE)));
    }
    if (!testMissingCapabilityIsRejected()) {
        std::cerr << "FAIL: missing broker capability must be consumed and rejected\n";
        return 1;
    }
    if (!testExactPreestablishedCapabilityIsAccepted()) {
        std::cerr << "FAIL: exact pre-established broker capability must be accepted\n";
        return 1;
    }
    if (!testCopiedCapabilityCannotReplayTheBrokerSession()) {
        std::cerr << "FAIL: copied broker capability must not replay the session\n";
        return 1;
    }
    if (!testBrokerSessionSupportsUpdatedChildAndRollbackRelaunch()) {
        std::cerr << "FAIL: one broker session must support the updated child and rollback relaunch\n";
        return 1;
    }
    if (!testBrokerCrashBetweenRequestsRejectsRollbackRelaunch()) {
        std::cerr << "FAIL: broker crash between requests must reject rollback relaunch\n";
        return 1;
    }
    if (!testFirstBrokerRequestTimeoutLeavesNoLateChild()) {
        std::cerr << "FAIL: first broker request timeout must drain its late child\n";
        return 1;
    }
    if (!testSecondBrokerRequestTimeoutLeavesNoLateChild()) {
        std::cerr << "FAIL: second broker request timeout must drain its late child\n";
        return 1;
    }
    if (!testReleasedBrokerChildRebindsItsCommittedProcessTree()) {
        std::cerr << "FAIL: released broker child must rebind and drain its committed tree\n";
        return 1;
    }
    if (!testRootAbsentReleasedTreeIsRecoveryAmbiguity()) {
        std::cerr << "FAIL: root-absent released tree must quarantine as ambiguous\n";
        return 1;
    }
    std::cout << "HarkenSearchBootstrap launch broker tests passed\n";
    return 0;
}
