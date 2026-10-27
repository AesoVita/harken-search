#include "ProToolsBridge.h"
#include "platform/executable_path.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <utility>

namespace {

Platform::ExecutablePathResult g_executablePathResult;

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

std::string JoinWindowsPath(std::string base, const char* leaf) {
    std::replace(base.begin(), base.end(), '/', '\\');
    if (!base.empty() && base.back() != '\\') {
        base.push_back('\\');
    }
    return base + leaf;
}

} // namespace

namespace Platform {

ExecutablePathResult GetExecutablePath() {
    return g_executablePathResult;
}

} // namespace Platform

int main() {
    if (_putenv_s("AURO_PROTOOLS_BRIDGE_EXE", "") != 0) {
        std::cerr << "FAIL: could not clear helper override\n";
        return 1;
    }

    g_executablePathResult.status = Platform::ExecutablePathStatus::Success;
    g_executablePathResult.pathUtf8 = u8"C:/Auro 测试/bin/HarkenSearch.exe";
    const std::string expected =
        (std::filesystem::u8path(g_executablePathResult.pathUtf8).parent_path() /
         "HarkenSearchProToolsBridge.exe").u8string();
    if (!Check(HarkenSearch::ProTools::ResolveProToolsBridgeHelperPath() == expected,
               "helper path must be resolved beside the PAL executable path")) {
        return 1;
    }
    const std::string expectedSdkDirectory = JoinWindowsPath(
        std::filesystem::u8path(g_executablePathResult.pathUtf8).parent_path().u8string(),
        "ProToolsSdk");
    if (!Check(HarkenSearch::ProTools::BuildInstallProbePlan({}).userSdkDirectoryUtf8 ==
                   expectedSdkDirectory,
               "SDK directory must be resolved beside the PAL executable path")) {
        return 1;
    }

    g_executablePathResult.status = Platform::ExecutablePathStatus::QueryFailed;
    g_executablePathResult.pathUtf8.clear();
    if (!Check(HarkenSearch::ProTools::ResolveProToolsBridgeHelperPath() ==
                   "HarkenSearchProToolsBridge.exe",
               "query failure must retain the helper-name fallback")) {
        return 1;
    }
    if (!Check(HarkenSearch::ProTools::BuildInstallProbePlan({}).userSdkDirectoryUtf8 ==
                   "ProToolsSdk",
               "query failure must retain the SDK directory fallback")) {
        return 1;
    }

    g_executablePathResult.status = Platform::ExecutablePathStatus::Success;
    if (!Check(HarkenSearch::ProTools::ResolveProToolsBridgeHelperPath() ==
                   "HarkenSearchProToolsBridge.exe",
               "empty success result must retain the helper-name fallback")) {
        return 1;
    }
    if (!Check(HarkenSearch::ProTools::BuildInstallProbePlan({}).userSdkDirectoryUtf8 ==
                   "ProToolsSdk",
               "empty success result must retain the SDK directory fallback")) {
        return 1;
    }

    std::cout << "PASS: Pro Tools bridge executable path PAL contract\n";
    return 0;
}
