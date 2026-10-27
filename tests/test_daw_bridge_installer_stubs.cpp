#include "DawBridgeInstaller.h"

namespace DawBridgeInstallerTestStubs {
int reaperInstallCalls = 0;
int reaperProbeCalls = 0;
bool reaperInstallDryRun = false;
HarkenSearch::DawBridge::InstallStatus reaperProbeResult{};
HarkenSearch::DawBridge::InstallResult reaperDryRunResult{};
HarkenSearch::DawBridge::InstallResult reaperLiveResult{};

void Reset() {
    reaperInstallCalls = 0;
    reaperProbeCalls = 0;
    reaperInstallDryRun = false;
    reaperProbeResult = {};
    reaperDryRunResult = {};
    reaperLiveResult = {};
}
} // namespace DawBridgeInstallerTestStubs

namespace HarkenSearch::DawBridge {

InstallStatus ProbeReaperBridge(const ReaperBridgeProbeOptions&) {
    ++DawBridgeInstallerTestStubs::reaperProbeCalls;
    return DawBridgeInstallerTestStubs::reaperProbeResult;
}

InstallResult InstallReaperBridge(const ReaperBridgeProbeOptions&,
                                  bool dryRun,
                                  ReaperBridgeInstallObserver) {
    ++DawBridgeInstallerTestStubs::reaperInstallCalls;
    DawBridgeInstallerTestStubs::reaperInstallDryRun = dryRun;
    return dryRun ? DawBridgeInstallerTestStubs::reaperDryRunResult
                  : DawBridgeInstallerTestStubs::reaperLiveResult;
}

} // namespace HarkenSearch::DawBridge
