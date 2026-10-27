#pragma once
// Aris - Send to Wwise (F-034 S3) - Persistence hook.
//
// The WwiseTransferConfig has a nested shape (structure[], userVars{}) that
// doesn't fit the flat key-value settings.ini format Aris uses for AppSettings.
// So we keep it in a sibling JSON file next to settings.ini and expose a
// single process-global instance that the S7 UI panel and the S6 transfer
// executor both read/write.
//
// Thread model: all callers run on the main (ImGui) thread. If that ever
// stops being true, wrap the getter/setter in a mutex — for now, direct
// access is fine.

#include "WwiseTransferConfig.h"

#include <string>
#include <vector>

namespace aura {
namespace wwise {

// @stage7-contract
// API contract: persistence reads/writes the sibling JSON representation of
// WwiseTransferConfig on the main thread as documented. It owns serialized
// values and reports malformed/missing files through existing status/results;
// it does not execute WAAPI operations.

// Current in-process config. Mutated by the UI panel; read by the executor.
// Initialised to defaults before the first load.
WwiseTransferConfig& activeTransferConfig();

// Saved preset = named snapshot of a WwiseTransferConfig. Users save their
// working config into a preset, switch between presets, and load them back
// into the active config. Empty list = no presets saved yet.
struct WwisePreset {
    std::string name;
    WwiseTransferConfig config;
};

std::vector<WwisePreset>& presetList();
// Name of the preset the active config was last loaded from or saved to.
// Empty string means the active config is unsaved scratch work — the
// panel surfaces this via a "(unsaved)" tag on the preset dropdown.
std::string& activePresetName();

// Preset operations. All in-memory; persistence happens at save*TransferConfig()
// time via the usual SaveAppSettings hook. Returns true on success.
bool savePresetFromActive(const std::string& name);   // creates or overwrites
bool loadPresetIntoActive(const std::string& name);
bool deletePreset(const std::string& name);
bool renamePreset(const std::string& from, const std::string& to);

// Derive the JSON path from the AppSettings path (sits alongside as
// `<settings dir>/wwise_transfer.json`), load the contents into the
// active config + presets list, and return true on success. Returns
// false when the file is absent (first-run) or when parse fails; errorOut
// (if provided) carries detail for the malformed case. Backwards-compatible
// with the v1 (pre-presets) single-config schema.
bool loadActiveTransferConfig(const std::string& appSettingsPathUtf8,
                              std::string* errorOut = nullptr);

// Write both the active config AND the presets list to the sibling JSON
// path (schema v2). Returns false on I/O error; errorOut carries detail.
bool saveActiveTransferConfig(const std::string& appSettingsPathUtf8,
                              std::string* errorOut = nullptr);

} // namespace wwise
}
