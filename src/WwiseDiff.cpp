// Aris - Send to Wwise (F-034 S5) - Preview Diff algorithm.

#include "WwiseDiff.h"

#include <algorithm>
#include <unordered_map>

namespace aura {
namespace wwise {

const char* toString(DiffStatus s) {
    switch (s) {
        case DiffStatus::NoChange:   return "NoChange";
        case DiffStatus::New:        return "New";
        case DiffStatus::Replaced:   return "Replaced";
        case DiffStatus::Unresolved: return "Unresolved";
    }
    return "";
}

const char* toString(WavStatus s) {
    switch (s) {
        case WavStatus::None:      return "";
        case WavStatus::New:       return "New";
        case WavStatus::Exists:    return "Exists";
        case WavStatus::Overwrite: return "Overwrite";
    }
    return "";
}

namespace {

// Join an import destination path with a child node name. Wwise paths use
// backslash separators ("\Actor-Mixer Hierarchy\Default Work Unit\AMBIENCE").
// Treats a trailing backslash on the parent as a no-op so callers don't have
// to normalise first.
std::string joinPath(const std::string& parent, const std::string& name) {
    if (parent.empty()) return name;
    if (parent.back() == '\\') return parent + name;
    return parent + "\\" + name;
}

// Build or locate a child node with the given `name` under `parent`. Mutates
// the tree in place to keep spines deduplicated (one planned import per
// spine would explode the tree otherwise).
WwiseDiffNode* upsertChild(WwiseDiffNode& parent,
                           const std::string& name,
                           WwiseObjectType type,
                           const std::string& parentPath) {
    for (auto& child : parent.children) {
        if (child.name == name && child.type == type) {
            return &child;
        }
    }
    parent.children.push_back(WwiseDiffNode{});
    WwiseDiffNode& child = parent.children.back();
    child.name = name;
    child.type = type;
    child.fullPath = joinPath(parentPath, name);
    child.status = DiffStatus::NoChange; // Caller re-scores after lookup.
    return &child;
}

// Classify a non-leaf layer against the Wwise snapshot. "Non-leaf" means the
// type is structural (Container/Folder/WorkUnit/Mixer), not a SoundSFX.
DiffStatus classifyLayer(const std::string& fullPath,
                         WwiseObjectType plannedType,
                         const ExistingObjectMap& existing) {
    auto it = existing.find(fullPath);
    if (it == existing.end()) return DiffStatus::New;
    // Type mismatch on an intermediate container forces Replace — the user's
    // template genuinely wants a different object kind at that slot.
    if (it->second != plannedType) return DiffStatus::Replaced;
    return DiffStatus::NoChange;
}

// Classify a leaf SoundSFX and compute its WavStatus based on the Originals
// snapshot. Separate from classifyLayer because conflictPolicy interacts
// differently with SoundSFX (all three policies have distinct outcomes).
void classifyLeaf(WwiseDiffNode& leaf,
                  const std::string& fullPath,
                  const std::string& originalsPath,
                  const ExistingObjectMap& existing,
                  const ExistingOriginalsSet& existingOriginals,
                  ConflictPolicy conflictPolicy) {
    const bool objExists = existing.find(fullPath) != existing.end();
    const bool wavExists = existingOriginals.find(originalsPath) != existingOriginals.end();

    if (!objExists) {
        leaf.status = DiffStatus::New;
        leaf.wavStatus = wavExists ? WavStatus::Overwrite : WavStatus::New;
        return;
    }

    switch (conflictPolicy) {
        case ConflictPolicy::UseExisting:
            leaf.status = DiffStatus::NoChange;
            leaf.wavStatus = wavExists ? WavStatus::Exists : WavStatus::New;
            break;
        case ConflictPolicy::Replace:
            leaf.status = DiffStatus::Replaced;
            leaf.wavStatus = wavExists ? WavStatus::Overwrite : WavStatus::New;
            break;
        case ConflictPolicy::CreateNew:
            // UI-level concern: Wwise will auto-suffix. For the Preview tree,
            // we surface the existing object as NoChange AND add a sibling
            // marker so the batch-total count reads correctly.
            leaf.status = DiffStatus::NoChange;
            leaf.wavStatus = wavExists ? WavStatus::Exists : WavStatus::New;
            break;
    }
}

// Augments the tree with a "create-new sibling" node when the conflict policy
// forces uniqueness. Called only for leaves that already exist under
// ConflictPolicy::CreateNew. The suffix is a placeholder — actual numeric
// suffixing happens at transfer execution (S6) via WAAPI.
void addCreateNewSibling(WwiseDiffNode& parent,
                         const WwiseDiffNode& existingLeaf,
                         const std::string& sourceWav) {
    WwiseDiffNode sibling;
    sibling.name = existingLeaf.name + " (new)";
    sibling.fullPath = joinPath(parent.fullPath, sibling.name);
    sibling.type = existingLeaf.type;
    sibling.status = DiffStatus::New;
    sibling.wavStatus = WavStatus::New;
    sibling.sourceWav = sourceWav;
    parent.children.push_back(std::move(sibling));
}

} // namespace

WwiseDiffNode computeDiff(
    const std::string& importDestination,
    const std::vector<WwiseImportItem>& plannedImports,
    const ExistingObjectMap& existingObjects,
    const ExistingOriginalsSet& existingOriginals,
    ConflictPolicy conflictPolicy) {

    WwiseDiffNode root;
    root.fullPath = importDestination;
    // Last segment of the destination path, for display.
    if (const auto pos = importDestination.find_last_of('\\');
        pos != std::string::npos && pos + 1 < importDestination.size()) {
        root.name = importDestination.substr(pos + 1);
    } else {
        root.name = importDestination;
    }
    root.type = WwiseObjectType::WorkUnit; // Best-effort — most destinations are WUs.
    root.status = DiffStatus::NoChange;

    for (const auto& imp : plannedImports) {
        // Upfront: if any layer or the originals path failed to resolve, the
        // whole spine is Unresolved — don't create intermediate children
        // because their paths are nonsense.
        if (!imp.unresolvedTokens.empty()) {
            WwiseDiffNode spine;
            spine.name = imp.objectPath.empty() ? "<unresolved>" : imp.objectPath.back();
            spine.fullPath = "";
            spine.type = imp.objectTypes.empty() ? WwiseObjectType::SoundSFX
                                                 : imp.objectTypes.back();
            spine.status = DiffStatus::Unresolved;
            spine.unresolvedTokens = imp.unresolvedTokens;
            spine.sourceWav = imp.sourceWav;
            root.children.push_back(std::move(spine));
            continue;
        }

        WwiseDiffNode* cursor = &root;
        for (std::size_t layer = 0; layer < imp.objectPath.size(); ++layer) {
            const std::string& layerName = imp.objectPath[layer];
            const WwiseObjectType layerType = imp.objectTypes[layer];
            const std::string parentPath = cursor->fullPath;
            WwiseDiffNode* next = upsertChild(*cursor, layerName, layerType, parentPath);

            const bool isLeaf = (layer == imp.objectPath.size() - 1);
            if (isLeaf) {
                const std::string originalsPath = imp.originalsRelPath + layerName + ".wav";
                classifyLeaf(*next, next->fullPath, originalsPath,
                             existingObjects, existingOriginals, conflictPolicy);
                next->sourceWav = imp.sourceWav;
                if (conflictPolicy == ConflictPolicy::CreateNew &&
                    existingObjects.find(next->fullPath) != existingObjects.end()) {
                    addCreateNewSibling(*cursor, *next, imp.sourceWav);
                }
            } else {
                // Only re-score the node the first time we meet it — later
                // imports sharing this prefix shouldn't flap its status.
                if (next->status == DiffStatus::NoChange && next->fullPath != "") {
                    next->status = classifyLayer(next->fullPath, layerType, existingObjects);
                }
            }
            cursor = next;
        }
    }

    return root;
}

} // namespace wwise
}
