#pragma once

// @stage7-contract
// API contract: Wwise structure templates are value descriptions used to build
// requests. They own strings/containers, perform no network or authoring I/O,
// and preserve property/path spelling for the downstream WAAPI layer.
// Aris - Send to Wwise (F-034 S4) - Wildcards parser + structure expander.
//
// Takes WwiseStructureNode templates (with $wildcards) + per-file metadata
// contexts, and produces the flat WwiseImportItem list that the transfer
// execution layer (S6) hands off to WAAPI's ak.wwise.core.audio.import.
//
// Pure functions — no filesystem, no WAAPI calls. Directly unit-tested.

#include "WwiseTransferConfig.h"

#include <map>
#include <string>
#include <vector>

namespace aura {
namespace wwise {

// Per-file context fed into wildcard resolution. Members are populated from
// the UI/search selection; empty strings signal "data missing for this file".
struct MetadataContext {
    std::string filenameBase;    // $filename_base — stem, no extension
    std::string filenameFull;    // $filename_full — with extension
    std::string ucsCategory;     // $ucs_cat
    std::string ucsSubCategory;  // $ucs_subcat
    std::string ucsCatId;        // $ucs_catid
    std::string library;         // $library
    std::string designer;        // $designer
    std::string composer;        // $composer
    std::string description;     // Source for $description_first / $description_slug
    int index = 0;               // $index — 1-based position in the batch
    int count = 0;               // $count — batch total (drives $index zero-padding)
    std::map<std::string, std::string> userVars; // Snapshot of WwiseTransferConfig::userVars
};

// Result of expanding a single template string.
struct ExpandResult {
    std::string value;                         // Best-effort output. Unresolved-but-known tokens emit an empty run.
    std::vector<std::string> unresolvedTokens; // Known wildcards that couldn't produce a value (e.g. $user(FOO) with no FOO binding).
};

// Expand `tmpl` against `ctx`. Unknown `$foo` tokens pass through as literal
// "$foo" — this way, user text like "price: $5" with an unused wildcard name
// stays visible instead of silently disappearing.
ExpandResult expandWildcards(const std::string& tmpl, const MetadataContext& ctx);

// One flat import target — the expanded address a Sound SFX will land at.
struct WwiseImportItem {
    std::string sourceWav;                         // Absolute path to the source WAV on disk.
    std::string originalsRelPath;                  // Expanded, relative to <Wwise Project>/Originals/SFX/.
    std::vector<std::string> objectPath;           // Per-layer expanded object name (length == config.structure.size()).
    std::vector<std::string> propertyTemplatePaths; // Parallel to objectPath; empty string = no template for that layer.
    std::vector<WwiseObjectType> objectTypes;      // Parallel to objectPath.
    std::vector<std::string> unresolvedTokens;     // Union of all layers' unresolved tokens + originals subfolder.
};

// Expand the full config (originals subfolder + structure layers) across a
// batch of source files. `contexts` and `sourceWavs` must be the same length;
// index/count are auto-injected (index = 1..N, count = N).
std::vector<WwiseImportItem> expandStructure(
    const WwiseTransferConfig& config,
    const std::vector<MetadataContext>& contexts,
    const std::vector<std::string>& sourceWavs);

} // namespace wwise
}
