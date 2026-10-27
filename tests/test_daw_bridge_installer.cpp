// ============================================================================
// test_daw_bridge_installer.cpp — REAPER-only registry characterization
// ============================================================================
// Runtime proves the sole public entry forwards probe and both install modes
// to the REAPER Doctor boundary while preserving exact returned values.
// ============================================================================

#include "DawBridgeInstaller.h"
#include <cstdio>
#include <string>

namespace DawBridgeInstallerTestStubs {
extern int reaperInstallCalls;
extern int reaperProbeCalls;
extern bool reaperInstallDryRun;
extern HarkenSearch::DawBridge::InstallStatus reaperProbeResult;
extern HarkenSearch::DawBridge::InstallResult reaperDryRunResult;
extern HarkenSearch::DawBridge::InstallResult reaperLiveResult;
void Reset();
} // namespace DawBridgeInstallerTestStubs

static int g_pass = 0;
static int g_fail = 0;

static void Check(bool cond, const char* desc) {
    if (cond) {
        std::printf("PASS: %s\n", desc);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", desc);
        ++g_fail;
    }
}

static void Test_RegistryHasExpectedEntries() {
    const auto reg = HarkenSearch::DawBridge::BuildInstallerRegistry();
    Check(reg.size() == 1, "Registry has exactly one reviewed entry");
    Check(reg.size() == 1 && reg[0].targetId == "reaper-extension",
          "Registry contains only REAPER");
}

static void Test_ReaperDoesNotRequireAdmin() {
    const auto reg = HarkenSearch::DawBridge::BuildInstallerRegistry();
    for (const auto& inst : reg) {
        if (inst.targetId == "reaper-extension") {
            Check(inst.requiresAdmin == false, "reaper-extension: requiresAdmin == false");
            return;
        }
    }
    Check(false, "reaper-extension entry found for requiresAdmin check");
}

static void Test_ReaperCallbacksForwardToDoctor() {
    using namespace DawBridgeInstallerTestStubs;
    DawBridgeInstallerTestStubs::Reset();
    const auto reg = HarkenSearch::DawBridge::BuildInstallerRegistry();
    Check(reg.size() == 1, "REAPER forwarding entry exists");
    if (reg.size() != 1) return;

    reaperProbeResult.state = HarkenSearch::DawBridge::InstallState::FileInstalled;
    reaperProbeResult.detail = "probe-sentinel";
    reaperDryRunResult.ok = true;
    reaperDryRunResult.failureCode = "dry-sentinel";
    reaperLiveResult.changed = true;
    reaperLiveResult.installedPath = "live-sentinel";

    const auto probe = reg[0].probe();
    Check(reaperProbeCalls == 1 &&
              probe.state == HarkenSearch::DawBridge::InstallState::FileInstalled &&
              probe.detail == "probe-sentinel",
          "REAPER probe forwards exact Doctor result");

    const auto dry = reg[0].install(true);
    Check(reaperInstallCalls == 1 && reaperInstallDryRun &&
              dry.ok && dry.failureCode == "dry-sentinel",
          "REAPER install(true) forwards argument and exact result");

    const auto live = reg[0].install(false);
    Check(reaperInstallCalls == 2 && !reaperInstallDryRun &&
              live.changed && live.installedPath == "live-sentinel",
          "REAPER install(false) forwards argument and exact result");
}

static void Test_InstallerMetadataIsComplete() {
    const auto reg = HarkenSearch::DawBridge::BuildInstallerRegistry();
    for (const auto& inst : reg) {
        Check(!inst.displayName.empty(),
              (std::string("displayName non-empty for: ") + inst.targetId).c_str());
        Check(!inst.descriptionLocalizationKey.empty(),
              (std::string("localization key non-empty for: ") + inst.targetId).c_str());
        Check(static_cast<bool>(inst.probe) && static_cast<bool>(inst.install),
              (std::string("callbacks populated for: ") + inst.targetId).c_str());
    }
}

int main() {
    std::printf("=== TestDawBridgeInstaller ===\n");
    Test_RegistryHasExpectedEntries();
    Test_ReaperDoesNotRequireAdmin();
    Test_ReaperCallbacksForwardToDoctor();
    Test_InstallerMetadataIsComplete();
    std::printf("\n--- Results: %d passed, %d failed ---\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
