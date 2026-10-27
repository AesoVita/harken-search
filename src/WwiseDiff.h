#pragma once
// Aris - Send to Wwise (F-034 S5) - Preview Diff algorithm.
//
// Takes planned imports (from expandStructure in S4) + snapshots of the
// existing Wwise-side state + the user's conflict policy, and produces the
// hierarchical WwiseDiffNode tree the S7 UI renders in the Preview panel.
//
// Pure function. No WAAPI calls in this TU — fetching the existing-objects
// snapshot is the next slice's job (S5-2, via WaapiClient + ak.wwise.core.
// object.get). Keeping compute vs. fetch separated lets the algorithm be
// unit-tested against hand-built fixtures.

#include "WwiseStructureTemplate.h"
#include "WwiseTransferConfig.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aura {
namespace wwise {

// @stage7-contract
// API contract: computeDiff is a pure synchronous transformation. It borrows
// all input containers for the call and returns an owned tree; no WAAPI,
// filesystem, global state, or caller-owned memory is retained. Callers must
// provide the expanded import paths and matching snapshots; unresolved input
// tokens are reported in the returned nodes rather than thrown.

// Per-node outcome after comparing plan vs. current state.
enum class DiffStatus : std::uint8_t {
    NoChange,   // Object exists, types match, name matches — transfer leaves it alone.
    New,        // Object is missing on the Wwise side and will be created.
    Replaced,   // Object exists but policy forces overwrite OR type differs from plan.
    Unresolved, // Wildcards failed to resolve a name in this layer — blocks transfer.
};

const char* toString(DiffStatus);

// Leaf-only WAV outcome. Non-leaf nodes leave this empty.
enum class WavStatus : std::uint8_t {
    None,      // Non-leaf (no WAV associated).
    New,       // Not currently under Originals — will be copied fresh.
    Exists,    // Present + policy says keep (UseExisting) — skipped.
    Overwrite, // Present + policy says replace.
};

const char* toString(WavStatus);

struct WwiseDiffNode {
    std::string name;                           // Display name (last path component after expansion).
    std::string fullPath;                       // Full Wwise object path from project root.
    WwiseObjectType type = WwiseObjectType::VirtualFolder;
    DiffStatus status = DiffStatus::NoChange;
    std::vector<std::string> unresolvedTokens;  // Populated only when status == Unresolved.
    std::vector<WwiseDiffNode> children;        // Tree form; children are unique per parent.
    WavStatus wavStatus = WavStatus::None;      // Leaf-only.
    std::string sourceWav;                      // Leaf-only: absolute path to the source WAV on disk.
};

// Snapshot inputs fed into computeDiff. Both containers are treated as
// immutable views — callers own the memory.

// Key = full Wwise object path (matching what expandStructure produces);
// value = the type Wwise reports for that object. A mismatch between the
// planned type and the reported type is what triggers a Replaced status
// on non-leaf layers.
using ExistingObjectMap = std::unordered_map<std::string, WwiseObjectType>;

// Set of "Originals-relative" WAV paths (relative to the Wwise project's
// Originals/SFX/ directory — same shape as WwiseImportItem::originalsRelPath +
// leaf filename). Presence toggles between WavStatus::Exists/Overwrite/New.
using ExistingOriginalsSet = std::unordered_set<std::string>;

// Build the Preview Diff tree rooted at `importDestination`. The root node is
// always NoChange (the destination must already exist for imports to land).
// Each planned import contributes one spine under the root; spines sharing
// prefixes get merged so the UI shows one node per unique object path.
//
// `conflictPolicy` decides, for both non-leaf and leaf, whether "already
// exists" means NoChange (UseExisting / CreateNew) or Replaced (Replace).
// For CreateNew, computeDiff still reports the conflict as NoChange on the
// existing object and appends a sibling New node with an auto-suffixed name
// — that way the UI makes the duplicate visible rather than hiding it.
WwiseDiffNode computeDiff(
    const std::string& importDestination,
    const std::vector<WwiseImportItem>& plannedImports,
    const ExistingObjectMap& existingObjects,
    const ExistingOriginalsSet& existingOriginals,
    ConflictPolicy conflictPolicy);

} // namespace wwise
}
