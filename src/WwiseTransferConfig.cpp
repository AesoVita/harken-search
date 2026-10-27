// Aris - Send to Wwise (F-034 S3) - WwiseTransferConfig JSON round-trip.

#include "WwiseTransferConfig.h"

#include "nlohmann_json.hpp"

#include <stdexcept>
#include <string>

namespace aura {
namespace wwise {

namespace {
using nlohmann::json;

// Single source of truth for enum names — keep in sync with toString/fromString
// below. If you add a new WwiseObjectType, adding the pair here is all the
// JSON layer needs.
struct EnumPair {
    const char* name;
    int value;
};

constexpr EnumPair kObjectTypes[] = {
    {"WorkUnit",           static_cast<int>(WwiseObjectType::WorkUnit)},
    {"VirtualFolder",      static_cast<int>(WwiseObjectType::VirtualFolder)},
    {"ActorMixer",         static_cast<int>(WwiseObjectType::ActorMixer)},
    {"RandomContainer",    static_cast<int>(WwiseObjectType::RandomContainer)},
    {"SequenceContainer",  static_cast<int>(WwiseObjectType::SequenceContainer)},
    {"SwitchContainer",    static_cast<int>(WwiseObjectType::SwitchContainer)},
    {"BlendContainer",     static_cast<int>(WwiseObjectType::BlendContainer)},
    {"SoundSFX",           static_cast<int>(WwiseObjectType::SoundSFX)},
};

constexpr EnumPair kConflictPolicies[] = {
    {"UseExisting", static_cast<int>(ConflictPolicy::UseExisting)},
    {"Replace",     static_cast<int>(ConflictPolicy::Replace)},
    {"CreateNew",   static_cast<int>(ConflictPolicy::CreateNew)},
};

constexpr EnumPair kTemplateApply[] = {
    {"NewOnly", static_cast<int>(TemplateApply::NewOnly)},
    {"Always",  static_cast<int>(TemplateApply::Always)},
};

template <std::size_t N>
std::string enumToString(const EnumPair (&table)[N], int value) {
    for (const auto& e : table) {
        if (e.value == value) return e.name;
    }
    return {};
}

template <typename Enum, std::size_t N>
bool enumFromString(const EnumPair (&table)[N], const std::string& s, Enum& out) {
    for (const auto& e : table) {
        if (s == e.name) {
            out = static_cast<Enum>(e.value);
            return true;
        }
    }
    return false;
}

} // namespace

std::string toString(WwiseObjectType v) {
    return enumToString(kObjectTypes, static_cast<int>(v));
}
bool fromString(const std::string& s, WwiseObjectType& out) {
    return enumFromString(kObjectTypes, s, out);
}
std::string toString(ConflictPolicy v) {
    return enumToString(kConflictPolicies, static_cast<int>(v));
}
bool fromString(const std::string& s, ConflictPolicy& out) {
    return enumFromString(kConflictPolicies, s, out);
}
std::string toString(TemplateApply v) {
    return enumToString(kTemplateApply, static_cast<int>(v));
}
bool fromString(const std::string& s, TemplateApply& out) {
    return enumFromString(kTemplateApply, s, out);
}

std::string toJson(const WwiseTransferConfig& cfg, bool pretty) {
    json j;
    j["version"] = 1; // Wire version — bump on breaking schema change.
    j["projectPath"]        = cfg.projectPath;
    j["waapiHost"]          = cfg.waapiHost;
    j["waapiPort"]          = cfg.waapiPort;
    j["originalsSubfolder"] = cfg.originalsSubfolder;
    j["importDestination"]  = cfg.importDestination;
    j["conflictPolicy"]     = toString(cfg.conflictPolicy);
    j["templateApply"]      = toString(cfg.templateApply);

    json structure = json::array();
    for (const auto& n : cfg.structure) {
        structure.push_back({
            {"type",                 toString(n.type)},
            {"nameTemplate",         n.nameTemplate},
            {"propertyTemplatePath", n.propertyTemplatePath},
            {"useTemplate",          n.useTemplate},
        });
    }
    j["structure"] = std::move(structure);

    json userVars = json::object();
    for (const auto& kv : cfg.userVars) {
        userVars[kv.first] = kv.second;
    }
    j["userVars"] = std::move(userVars);

    return pretty ? j.dump(2) : j.dump();
}

bool fromJson(const std::string& jsonText,
              WwiseTransferConfig& out,
              std::string* errorOut) {
    auto fail = [&](const char* msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };
    auto failDetail = [&](const std::string& msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };

    json j;
    try {
        j = json::parse(jsonText);
    } catch (const std::exception& e) {
        return failDetail(std::string("JSON parse failed: ") + e.what());
    }
    if (!j.is_object()) return fail("JSON root must be an object");

    WwiseTransferConfig tmp; // Parse into temp; only commit on full success.

    auto getStr = [&](const char* key, std::string& dst) {
        if (j.contains(key) && j[key].is_string()) dst = j[key].get<std::string>();
    };
    auto getInt = [&](const char* key, int& dst) {
        if (j.contains(key) && j[key].is_number_integer()) dst = j[key].get<int>();
    };

    getStr("projectPath",        tmp.projectPath);
    getStr("waapiHost",          tmp.waapiHost);
    getInt("waapiPort",          tmp.waapiPort);
    getStr("originalsSubfolder", tmp.originalsSubfolder);
    getStr("importDestination",  tmp.importDestination);

    if (j.contains("conflictPolicy") && j["conflictPolicy"].is_string()) {
        if (!fromString(j["conflictPolicy"].get<std::string>(), tmp.conflictPolicy)) {
            return failDetail("Unknown conflictPolicy: " + j["conflictPolicy"].get<std::string>());
        }
    }
    if (j.contains("templateApply") && j["templateApply"].is_string()) {
        if (!fromString(j["templateApply"].get<std::string>(), tmp.templateApply)) {
            return failDetail("Unknown templateApply: " + j["templateApply"].get<std::string>());
        }
    }

    if (j.contains("structure")) {
        if (!j["structure"].is_array()) return fail("`structure` must be an array");
        for (const auto& node : j["structure"]) {
            if (!node.is_object()) return fail("Each structure node must be an object");
            WwiseStructureNode n;
            if (node.contains("type") && node["type"].is_string()) {
                if (!fromString(node["type"].get<std::string>(), n.type)) {
                    return failDetail("Unknown WwiseObjectType: " + node["type"].get<std::string>());
                }
            }
            if (node.contains("nameTemplate") && node["nameTemplate"].is_string()) {
                n.nameTemplate = node["nameTemplate"].get<std::string>();
            }
            if (node.contains("propertyTemplatePath") && node["propertyTemplatePath"].is_string()) {
                n.propertyTemplatePath = node["propertyTemplatePath"].get<std::string>();
            }
            if (node.contains("useTemplate") && node["useTemplate"].is_boolean()) {
                n.useTemplate = node["useTemplate"].get<bool>();
            }
            tmp.structure.push_back(std::move(n));
        }
    }

    if (j.contains("userVars")) {
        if (!j["userVars"].is_object()) return fail("`userVars` must be an object");
        for (auto it = j["userVars"].begin(); it != j["userVars"].end(); ++it) {
            if (!it.value().is_string()) return fail("userVars values must be strings");
            tmp.userVars[it.key()] = it.value().get<std::string>();
        }
    }

    out = std::move(tmp);
    return true;
}

} // namespace wwise
}
