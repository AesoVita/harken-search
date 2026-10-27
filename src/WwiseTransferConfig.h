#pragma once
// Aris - Send to Wwise (F-034 S3) - Transfer configuration types
//
// Pure data. No ImGui, no WAAPI, no IXWebSocket - safe to include from unit
// tests. JSON round-trip via nlohmann (implementation-side only; header stays
// dependency-free to keep compile times manageable for callers like main.cpp).

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aura {
namespace wwise {

// @stage7-contract
// API contract: Wwise transfer configuration is a value-only description of
// validated paths, nodes, and property choices. It owns its strings/vectors and
// does not perform transport or mutate authoring state; callers handle WAAPI
// lifecycle and report validation failures before execution.

// Wwise object kinds supported in a structure template. Ordering matches the
// Wwise Authoring UI's Project Explorer progression.
enum class WwiseObjectType : std::uint8_t {
    WorkUnit,
    VirtualFolder,
    ActorMixer,
    RandomContainer,
    SequenceContainer,
    SwitchContainer,
    BlendContainer,
    SoundSFX,
};

// What to do when Wwise already has a Sound SFX at the resolved path.
enum class ConflictPolicy : std::uint8_t {
    UseExisting, // Leave the existing object untouched; skip import for that file.
    Replace,     // Overwrite Originals WAV + reset Sound SFX to point at it.
    CreateNew,   // Append a numeric suffix to create a sibling.
};

// When to (re)apply Property Template inheritance.
enum class TemplateApply : std::uint8_t {
    NewOnly, // Only copy properties when creating the object for the first time.
    Always,  // Also overwrite properties on already-existing objects.
};

// One layer of the structure template. A full template is a stack of these,
// from root (outermost container) to leaf (Sound SFX).
struct WwiseStructureNode {
    WwiseObjectType type = WwiseObjectType::VirtualFolder;
    std::string nameTemplate;          // Supports wildcards — see WwiseStructureTemplate.h.
    std::string propertyTemplatePath;  // Empty = no inheritance. Wwise object path, e.g. "\Actor-Mixer Hierarchy\Templates\SFX_Footstep".
    bool useTemplate = false;
};

// Full transfer configuration. Lives outside AppSettings because its nested
// shape (structure[], userVars{}) doesn't fit the legacy flat key-value
// settings.ini format; it round-trips through JSON into its own file.
struct WwiseTransferConfig {
    std::string projectPath;                                            // Absolute path to .wproj (for display / validation only; WAAPI operates on the live project).
    std::string waapiHost = "127.0.0.1";
    int waapiPort = 8765;
    std::string originalsSubfolder;                                     // Wildcards-enabled; written under <ProjectRoot>/Originals/SFX/.
    std::string importDestination = "\\Actor-Mixer Hierarchy\\Default Work Unit";
    std::vector<WwiseStructureNode> structure;
    ConflictPolicy conflictPolicy = ConflictPolicy::UseExisting;
    TemplateApply templateApply = TemplateApply::NewOnly;
    std::map<std::string, std::string> userVars;                        // Resolution source for $user(NAME).
};

// Enum <-> string conversions. Stable wire format (kebab-ish names chosen to
// match WAAPI's own conventions where feasible). *FromString returns false
// when the input is unrecognised; callers should surface the error rather
// than silently fall back.
std::string toString(WwiseObjectType);
bool fromString(const std::string& s, WwiseObjectType& out);
std::string toString(ConflictPolicy);
bool fromString(const std::string& s, ConflictPolicy& out);
std::string toString(TemplateApply);
bool fromString(const std::string& s, TemplateApply& out);

// JSON (de)serialisation. `toJson` is lossless; `fromJson` tolerates missing
// optional fields (falls back to defaults) but rejects unknown enum values
// and malformed JSON. Error detail goes to *errorOut when provided.
std::string toJson(const WwiseTransferConfig&, bool pretty = true);
bool fromJson(const std::string& jsonText,
              WwiseTransferConfig& out,
              std::string* errorOut = nullptr);

} // namespace wwise
}
