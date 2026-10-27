// Aris - Send to Wwise (F-034 S3 + preset manager) - Persistence hook.

#include "WwiseTransferPersist.h"
#include "platform/filesystem.h"

#include "nlohmann_json.hpp"

#include <algorithm>
#include <filesystem>

namespace aura {
namespace wwise {

namespace {

// Process-global. Defaults are sensible out-of-the-box: localhost WAAPI on
// port 8765 (Wwise's default), ImportDestination at
// "\Actor-Mixer Hierarchy\Default Work Unit" (the path that exists in every
// fresh .wproj), no structure template (user must add one before Transfer).
WwiseTransferConfig g_active;
std::vector<WwisePreset> g_presets;
std::string g_activePresetName;  // Empty = unsaved scratch work.

// Current schema version emitted on save. Loaders tolerate any version <= this.
constexpr int kPersistSchemaVersion = 2;

// Convert "<settings dir>/settings.ini" → "<settings dir>/wwise_transfer.json"
// Keeps the JSON sibling-file in whatever directory AppSettings lives in, so
// per-user / portable / roaming install modes all work without extra plumbing.
std::filesystem::path siblingJsonPath(const std::string& appSettingsPathUtf8) {
    std::filesystem::path p(appSettingsPathUtf8);
    if (p.has_parent_path()) {
        return p.parent_path() / "wwise_transfer.json";
    }
    return std::filesystem::path("wwise_transfer.json");
}

std::string readAll(const std::filesystem::path& p, std::string* errorOut) {
    std::string text;
    if (!Platform::FileSystem::ReadFileText(p.u8string(), text)) {
        if (errorOut) *errorOut = "cannot open for read";
        return {};
    }
    return text;
}

bool writeAll(const std::filesystem::path& p, const std::string& s, std::string* errorOut) {
    if (!p.parent_path().empty()) {
        Platform::FileSystem::CreateDirectories(p.parent_path().u8string());
    }
    if (!Platform::FileSystem::WriteFileText(p.u8string(), s)) {
        if (errorOut) *errorOut = "cannot open for write";
        return false;
    }
    return true;
}

} // namespace

WwiseTransferConfig& activeTransferConfig() {
    return g_active;
}

std::vector<WwisePreset>& presetList() {
    return g_presets;
}

std::string& activePresetName() {
    return g_activePresetName;
}

namespace {
auto findPreset(const std::string& name) {
    return std::find_if(g_presets.begin(), g_presets.end(),
                        [&](const WwisePreset& p) { return p.name == name; });
}
} // namespace

bool savePresetFromActive(const std::string& name) {
    if (name.empty()) return false;
    auto it = findPreset(name);
    if (it == g_presets.end()) {
        g_presets.push_back({name, g_active});
    } else {
        it->config = g_active; // overwrite in place
    }
    g_activePresetName = name;
    return true;
}

bool loadPresetIntoActive(const std::string& name) {
    auto it = findPreset(name);
    if (it == g_presets.end()) return false;
    g_active = it->config;
    g_activePresetName = name;
    return true;
}

bool deletePreset(const std::string& name) {
    auto it = findPreset(name);
    if (it == g_presets.end()) return false;
    g_presets.erase(it);
    if (g_activePresetName == name) g_activePresetName.clear();
    return true;
}

bool renamePreset(const std::string& from, const std::string& to) {
    if (to.empty() || from == to) return false;
    if (findPreset(to) != g_presets.end()) return false; // collision
    auto it = findPreset(from);
    if (it == g_presets.end()) return false;
    it->name = to;
    if (g_activePresetName == from) g_activePresetName = to;
    return true;
}

bool loadActiveTransferConfig(const std::string& appSettingsPathUtf8,
                              std::string* errorOut) {
    if (appSettingsPathUtf8.empty()) {
        if (errorOut) *errorOut = "appSettingsPath is empty";
        return false;
    }
    const auto jsonPath = siblingJsonPath(appSettingsPathUtf8);

    if (!Platform::FileSystem::Exists(jsonPath.u8string())) {
        // First-run path — not an error. Leave g_active / g_presets at defaults.
        return false;
    }

    std::string readErr;
    const std::string text = readAll(jsonPath, &readErr);
    if (text.empty() && !readErr.empty()) {
        if (errorOut) *errorOut = readErr;
        return false;
    }

    // Detect schema version. The v1 format is just a bare WwiseTransferConfig
    // JSON object with fields like "projectPath" at the root. The v2 format
    // wraps the active config + presets[] + activePresetName under dedicated
    // keys. We look for the "presets" key as the distinguishing marker so the
    // v1→v2 upgrade path is automatic the first time the user saves.
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = std::string("JSON parse failed: ") + e.what();
        return false;
    }
    if (!root.is_object()) {
        if (errorOut) *errorOut = "JSON root must be an object";
        return false;
    }

    // v1: presets key absent → treat the entire object as the active config.
    if (!root.contains("presets")) {
        WwiseTransferConfig tmp;
        std::string parseErr;
        if (!fromJson(text, tmp, &parseErr)) {
            if (errorOut) *errorOut = "v1 parse failed: " + parseErr;
            return false;
        }
        g_active = std::move(tmp);
        g_presets.clear();
        g_activePresetName.clear();
        return true;
    }

    // v2: explicit presets + active fields.
    g_presets.clear();
    g_activePresetName.clear();

    if (root.contains("active") && root["active"].is_object()) {
        std::string parseErr;
        if (!fromJson(root["active"].dump(), g_active, &parseErr)) {
            if (errorOut) *errorOut = "active parse failed: " + parseErr;
            return false;
        }
    }
    if (root.contains("activePresetName") && root["activePresetName"].is_string()) {
        g_activePresetName = root["activePresetName"].get<std::string>();
    }

    if (root["presets"].is_array()) {
        for (const auto& p : root["presets"]) {
            if (!p.is_object()) continue;
            WwisePreset wp;
            if (p.contains("name") && p["name"].is_string()) {
                wp.name = p["name"].get<std::string>();
            }
            if (wp.name.empty()) continue; // drop malformed entries, don't fail the whole load
            if (p.contains("config") && p["config"].is_object()) {
                std::string parseErr;
                if (!fromJson(p["config"].dump(), wp.config, &parseErr)) {
                    // One bad preset shouldn't block the rest.
                    continue;
                }
            }
            g_presets.push_back(std::move(wp));
        }
    }
    return true;
}

bool saveActiveTransferConfig(const std::string& appSettingsPathUtf8,
                              std::string* errorOut) {
    if (appSettingsPathUtf8.empty()) {
        if (errorOut) *errorOut = "appSettingsPath is empty";
        return false;
    }
    const auto jsonPath = siblingJsonPath(appSettingsPathUtf8);

    nlohmann::json root;
    root["version"] = kPersistSchemaVersion;
    root["activePresetName"] = g_activePresetName;
    // Active config as a nested object. We round-trip through its own toJson
    // so the active's `version` field + nested structure[] stay consistent.
    root["active"] = nlohmann::json::parse(toJson(g_active, /*pretty=*/false));

    nlohmann::json presetsArr = nlohmann::json::array();
    for (const auto& p : g_presets) {
        nlohmann::json entry;
        entry["name"] = p.name;
        entry["config"] = nlohmann::json::parse(toJson(p.config, /*pretty=*/false));
        presetsArr.push_back(std::move(entry));
    }
    root["presets"] = std::move(presetsArr);

    return writeAll(jsonPath, root.dump(2), errorOut);
}

} // namespace wwise
}
