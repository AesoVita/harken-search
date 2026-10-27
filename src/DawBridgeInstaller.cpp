#include "DawBridgeInstaller.h"
#include <vector>

namespace HarkenSearch::DawBridge {
static Installer MakeReaperExtensionInstaller() {
    Installer inst;
    inst.targetId                   = "reaper-extension";
    inst.displayName                = "REAPER 发送组件";
    inst.descriptionLocalizationKey = "Install_ReaperExtension";
    inst.requiresAdmin              = false;
    inst.probe = [] { return ProbeReaperBridge(); };
    inst.install = [](bool dryRun) { return InstallReaperBridge({}, dryRun); };

    return inst;
}

// BuildInstallerRegistry
std::vector<Installer> BuildInstallerRegistry() {
    std::vector<Installer> registry;
    registry.push_back(MakeReaperExtensionInstaller());
    return registry;
}

} // namespace HarkenSearch::DawBridge
