#include "WwiseTransferPersist.h"

#include <filesystem>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    using namespace aura::wwise;

    const fs::path root = fs::temp_directory_path() / "aris_stage6_wwise_persist";
    const fs::path settings = root / "nested" / "settings.ini";
    std::error_code ec;
    fs::remove_all(root, ec);

    activeTransferConfig() = WwiseTransferConfig{};
    activeTransferConfig().projectPath = "E:/Audio/Game.wproj";
    activeTransferConfig().originalsSubfolder = "Foley/$user(kind)";
    activeTransferConfig().userVars["kind"] = "Footsteps";
    presetList().clear();
    activePresetName().clear();
    if (!savePresetFromActive("Footsteps")) {
        std::cerr << "FAIL: preset snapshot should succeed\n";
        return 1;
    }

    std::string error;
    if (!saveActiveTransferConfig(settings.u8string(), &error)) {
        std::cerr << "FAIL: Wwise persistence save failed: " << error << "\n";
        return 1;
    }

    activeTransferConfig() = WwiseTransferConfig{};
    presetList().clear();
    activePresetName().clear();
    if (!loadActiveTransferConfig(settings.u8string(), &error) ||
        activeTransferConfig().projectPath != "E:/Audio/Game.wproj" ||
        activeTransferConfig().userVars["kind"] != "Footsteps" ||
        presetList().size() != 1 || presetList()[0].name != "Footsteps" ||
        activePresetName() != "Footsteps") {
        std::cerr << "FAIL: Wwise persistence PAL roundtrip changed config or presets\n";
        fs::remove_all(root, ec);
        return 1;
    }

    fs::remove_all(root, ec);
    std::cout << "PASS: Wwise persistence preserves config and presets through PAL\n";
    return 0;
}
