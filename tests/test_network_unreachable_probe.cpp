#include "platform/filesystem.h"

#ifdef _WIN32
#include "platform/filesystem_probe_path_test_hooks.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef CopyFile
#undef CopyFile
#endif
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const char* message) {
    if (cond) return;
    ++g_failures;
    std::cerr << "FAIL: " << message << "\n";
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "aris_network_unreachable_probe";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    const fs::path present = root / "present.bin";
    {
        std::ofstream out(present, std::ios::binary);
        out << "ok";
    }
    const fs::path missing = root / "deleted.bin";

    const auto presentStatus = Platform::FileSystem::ProbePath(present.u8string());
    check(presentStatus == Platform::FileSystem::PathStatus::Present,
          "local existing path is Present");

    const auto missingStatus = Platform::FileSystem::ProbePath(missing.u8string());
    check(missingStatus == Platform::FileSystem::PathStatus::Missing,
          "local truly deleted path is Missing");

    check(Platform::FileSystem::IsNetworkPath("\\\\nas\\share\\hit.wav"),
          "UNC is network");
    check(Platform::FileSystem::IsNetworkPath("//nas/share/hit.wav"),
          "forward-slash UNC is network");
    check(!Platform::FileSystem::IsNetworkPath("C:\\Users\\a.wav"),
          "local drive is not network");
    check(!Platform::FileSystem::IsNetworkPath("\\\\?\\C:\\Users\\a.wav"),
          "long local path is not network");

#ifdef _WIN32
    const struct Case {
        DWORD error;
        const char* path;
        const char* name;
    } kUnreachableCases[] = {
        {ERROR_BAD_NETPATH, "\\\\nas\\share\\hit.wav", "ERROR_BAD_NETPATH"},
        {ERROR_BAD_NET_NAME, "\\\\nas\\share\\hit.wav", "ERROR_BAD_NET_NAME"},
        {ERROR_NETWORK_UNREACHABLE, "\\\\nas\\share\\hit.wav", "ERROR_NETWORK_UNREACHABLE"},
        {ERROR_NO_NET_OR_BAD_PATH, "\\\\nas\\share\\hit.wav", "ERROR_NO_NET_OR_BAD_PATH"},
        {ERROR_LOGON_FAILURE, "\\\\nas\\share\\hit.wav", "ERROR_LOGON_FAILURE"},
        {ERROR_ACCESS_DENIED, "\\\\nas\\share\\hit.wav", "ERROR_ACCESS_DENIED network credentials"},
        {ERROR_UNEXP_NET_ERR, "\\\\nas\\share\\hit.wav", "ERROR_UNEXP_NET_ERR"},
        {ERROR_NOT_READY, "Z:\\mapped\\hit.wav", "ERROR_NOT_READY mapped drive"},
        {ERROR_PATH_NOT_FOUND, "\\\\nas\\share", "ERROR_PATH_NOT_FOUND share root"},
        {ERROR_BAD_NETPATH, "Z:\\mapped\\hit.wav", "ERROR_BAD_NETPATH mapped drive"},
    };

    for (const auto& testCase : kUnreachableCases) {
        Platform::FileSystem::Testing::ProbePathScript script;
        script.attributes = INVALID_FILE_ATTRIBUTES;
        script.lastError = testCase.error;
        Platform::FileSystem::Testing::SetProbePathScript(&script);
        const auto status = Platform::FileSystem::ProbePath(testCase.path);
        Platform::FileSystem::Testing::SetProbePathScript(nullptr);
        check(status == Platform::FileSystem::PathStatus::Unreachable,
              testCase.name);
        check(status != Platform::FileSystem::PathStatus::Missing,
              "network disconnect must not be Missing");
        check(script.calls == 1, "hook is used exactly once");
    }

    {
        Platform::FileSystem::Testing::ProbePathScript script;
        script.attributes = INVALID_FILE_ATTRIBUTES;
        script.lastError = ERROR_FILE_NOT_FOUND;
        Platform::FileSystem::Testing::SetProbePathScript(&script);
        const auto status = Platform::FileSystem::ProbePath(
            (root / "local-missing.bin").u8string());
        Platform::FileSystem::Testing::SetProbePathScript(nullptr);
        check(status == Platform::FileSystem::PathStatus::Missing,
              "local ERROR_FILE_NOT_FOUND remains Missing");
    }

    {
        Platform::FileSystem::Testing::ProbePathScript script;
        script.attributes = INVALID_FILE_ATTRIBUTES;
        script.lastError = ERROR_ACCESS_DENIED;
        Platform::FileSystem::Testing::SetProbePathScript(&script);
        const auto status = Platform::FileSystem::ProbePath(present.u8string());
        Platform::FileSystem::Testing::SetProbePathScript(nullptr);
        check(status == Platform::FileSystem::PathStatus::Error,
              "local ERROR_ACCESS_DENIED is Error, not Unreachable or Missing");
        check(status != Platform::FileSystem::PathStatus::Missing,
              "local access denied must not be treated as deleted");
    }
#else
    (void)missingStatus;
#endif

    fs::remove_all(root, ec);
    return g_failures == 0 ? 0 : 1;
}
