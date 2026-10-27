#include "platform/filesystem.h"
#include "platform/filesystem_probe_path_test_hooks.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

static int g_fails = 0;

static bool Check(bool cond, const char* msg) {
    std::cout << (cond ? "PASS: " : "FAIL: ") << msg << std::endl;
    if (!cond) ++g_fails;
    return cond;
}

int main() {
    using Platform::FileSystem::PathStatus;
    using Platform::FileSystem::ProbePath;
    using Platform::FileSystem::IsNetworkPath;

    Check(IsNetworkPath(R"(\\nas\share\hit.wav)"), "UNC is network");
    Check(IsNetworkPath(R"(//nas/share/hit.wav)"), "forward-slash UNC is network");
    Check(IsNetworkPath(R"(\\?\UNC\nas\share\hit.wav)"), "long UNC is network");
    Check(!IsNetworkPath(R"(C:\Users\a.wav)"), "local drive is not network");
    Check(!IsNetworkPath(R"(\\?\C:\Users\a.wav)"), "long local path is not network");

    Platform::FileSystem::Testing::DriveTypeScript drive{};
    drive.overrideDriveType = true;
    drive.driveType = DRIVE_REMOTE;
    Platform::FileSystem::Testing::SetDriveTypeScript(&drive);
    Check(IsNetworkPath(R"(Z:\share\a.wav)"), "mapped drive hook is network");
    Check(drive.calls > 0, "mapped drive used GetDriveType hook not live IO");
    Platform::FileSystem::Testing::SetDriveTypeScript(nullptr);

    const fs::path fixtureRoot = fs::u8path("D:/Aris-QA/test-databases/RQ-20261010-NONDOCK-PREVIEW-nas/probe");
    fs::create_directories(fixtureRoot);
    const fs::path present = fixtureRoot / "present.wav";
    const fs::path missing = fixtureRoot / "missing.wav";
    {
        std::ofstream out(present, std::ios::binary);
        out.write("RIFF", 4);
    }
    std::error_code ec;
    fs::remove(missing, ec);

    Check(ProbePath(present.u8string()) == PathStatus::Present, "local present file");
    Check(ProbePath(missing.u8string()) == PathStatus::Missing, "local truly missing file");

    Platform::FileSystem::Testing::ProbePathScript script{};
    script.attributes = INVALID_FILE_ATTRIBUTES;
    script.lastError = ERROR_BAD_NETPATH;
    Platform::FileSystem::Testing::SetProbePathScript(&script);
    const PathStatus unreachable = ProbePath(R"(\\nas\share\offline.wav)");
    Check(unreachable == PathStatus::Unreachable, "network error is Unreachable not Missing");
    Check(unreachable != PathStatus::Missing, "Unreachable is not Missing");
    Check(script.calls > 0, "ProbePath hook was used");
    Platform::FileSystem::Testing::SetProbePathScript(nullptr);

    Check(ProbePath(missing.u8string()) == PathStatus::Missing, "after hook clear, local missing still Missing");

    std::cout << (g_fails == 0 ? "PROBE_ALL_PASS" : "PROBE_HAS_FAIL") << std::endl;
    return g_fails == 0 ? 0 : 1;
}
