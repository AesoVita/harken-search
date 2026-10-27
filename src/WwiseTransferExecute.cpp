// Aris - Send to Wwise (F-034 S6) - Transfer execution.

#include "WwiseTransferExecute.h"
#include "platform/filesystem.h"

#include "nlohmann_json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace aura {
namespace wwise {

namespace {
using nlohmann::json;
namespace fs = std::filesystem;

fs::path utf8Path(const std::string& value) {
    return fs::u8path(value);
}

std::string pathToUtf8(const fs::path& path) {
#if defined(__cpp_char8_t)
    const auto text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.c_str()), text.size());
#else
    return path.u8string();
#endif
}

// Wwise's audio.import API expects objectPath encoded with inline type tags:
//   \<ParentType>ParentName\<ChildType>ChildName
// Root segments (WorkUnit / VirtualFolder under the Actor-Mixer Hierarchy)
// already exist in the destination, so we only tag the NEW layers our
// structure template is creating. The destination prefix is passed through
// as-is.
std::string wwiseTagFor(WwiseObjectType t) {
    switch (t) {
        case WwiseObjectType::WorkUnit:          return "WorkUnit";
        case WwiseObjectType::VirtualFolder:     return "Folder";
        case WwiseObjectType::ActorMixer:        return "ActorMixer";
        case WwiseObjectType::RandomContainer:   return "RandomSequenceContainer"; // Random/Sequence share a type class in WAAPI.
        case WwiseObjectType::SequenceContainer: return "RandomSequenceContainer";
        case WwiseObjectType::SwitchContainer:   return "SwitchContainer";
        case WwiseObjectType::BlendContainer:    return "BlendContainer";
        case WwiseObjectType::SoundSFX:          return "Sound";
    }
    return "";
}

std::string buildTaggedObjectPath(const std::string& destination,
                                  const std::vector<std::string>& names,
                                  const std::vector<WwiseObjectType>& types) {
    std::string out = destination;
    for (std::size_t i = 0; i < names.size(); ++i) {
        out += "\\<";
        out += wwiseTagFor(types[i]);
        out += ">";
        out += names[i];
    }
    return out;
}

// Maps our ConflictPolicy to WAAPI's `importOperation` string.
std::string toImportOperation(ConflictPolicy p) {
    switch (p) {
        case ConflictPolicy::UseExisting: return "useExisting";
        case ConflictPolicy::Replace:     return "replaceExisting";
        case ConflictPolicy::CreateNew:   return "createNew";
    }
    return "useExisting";
}

// Public WAAPI request builders. The field names and shapes are pinned to the
// Audiokinetic documentation listed in docs/process/WWISE_WAAPI_PROVENANCE.md.
// Keeping construction here makes the provenance boundary reviewable and
// prevents UI wording or third-party plugin behavior from becoming an API
// contract by accident.
json buildUndoBeginArguments() {
    json args;
    args["displayName"] = "Harken Search Transfer";
    return args;
}

json buildAudioImportArguments(const std::string& importOperation, json imports) {
    json args;
    args["importOperation"] = importOperation;
    args["default"]["importLanguage"] = "SFX";
    args["imports"] = std::move(imports);
    return args;
}

json buildPastePropertiesArguments(const std::string& source,
                                    const std::string& target) {
    json args;
    args["source"] = source;
    args["targets"] = json::array({target});
    return args;
}

// Derive the Originals/SFX directory from the project path. Wwise's default
// layout puts it at `<project dir>/Originals/SFX/` — we honour that here and
// treat customised Originals roots as an S9 follow-up if users request it.
fs::path originalsSfxRoot(const std::string& projectPath) {
    fs::path p = utf8Path(projectPath);
    // Wproj files live at the root of the project directory in Wwise's
    // layout; walk up to the parent to reach the peer Originals/ folder.
    if (!Platform::FileSystem::IsDirectory(pathToUtf8(p))) {
        p = p.parent_path();
    }
    return p / "Originals" / "SFX";
}

// Copy `src` to `dst`, creating parent directories as needed. Returns empty
// string on success; otherwise the OS error message. overwrite=true for
// Replace policy, false otherwise (UseExisting / CreateNew both leave the
// existing WAV alone — Wwise will reuse it or auto-suffix a new import).
std::string copyWav(const fs::path& src, const fs::path& dst, bool overwrite) {
    const std::string srcUtf8 = pathToUtf8(src);
    const std::string dstUtf8 = pathToUtf8(dst);
    Platform::FileSystem::CreateDirectories(pathToUtf8(dst.parent_path()));
    std::string error;
    if (!Platform::FileSystem::CopyFile(srcUtf8, dstUtf8, overwrite, &error)) {
        return "copy " + srcUtf8 + " → " + dstUtf8 + ": " + error;
    }
    return {};
}

std::string copyWavUtf8(const std::string& srcUtf8, const fs::path& dst, bool overwrite) {
    const std::string dstUtf8 = pathToUtf8(dst);
    Platform::FileSystem::CreateDirectories(pathToUtf8(dst.parent_path()));
    std::string error;
    if (!Platform::FileSystem::CopyFile(srcUtf8, dstUtf8, overwrite, &error)) {
        return "copy " + srcUtf8 + " -> " + dstUtf8 + ": " + error;
    }
    return {};
}

std::string normalizeOriginalRelativePath(std::string value) {
    for (char& ch : value) {
        if (ch == '/') {
            ch = '\\';
        } else {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }
    return value;
}

} // namespace

const char* toString(ItemStatus s) {
    switch (s) {
        case ItemStatus::Pending:         return "Pending";
        case ItemStatus::WavCopied:       return "WavCopied";
        case ItemStatus::Imported:        return "Imported";
        case ItemStatus::TemplateApplied: return "TemplateApplied";
        case ItemStatus::SkippedExisting: return "SkippedExisting";
        case ItemStatus::FailedWavCopy:   return "FailedWavCopy";
        case ItemStatus::FailedImport:    return "FailedImport";
        case ItemStatus::FailedTemplate:  return "FailedTemplate";
    }
    return "";
}

bool parseAudioImportResultPaths(const std::string& resultJson,
                                 ImportedObjectPathMap& out,
                                 std::string* errorOut) {
    out.clear();
    try {
        const json root = json::parse(resultJson);
        if (!root.is_object()) {
            if (errorOut) *errorOut = "audio.import result root is not an object";
            return false;
        }
        auto objectsIt = root.find("objects");
        if (objectsIt == root.end() || !objectsIt->is_array()) {
            if (errorOut) *errorOut = "audio.import result missing objects array";
            return false;
        }
        for (const auto& item : *objectsIt) {
            if (!item.is_object()) {
                continue;
            }
            const auto pathIt = item.find("path");
            const auto relIt = item.find("originalRelativeFilePath");
            if (pathIt == item.end() || relIt == item.end() ||
                !pathIt->is_string() || !relIt->is_string()) {
                continue;
            }
            const std::string normalizedRel = normalizeOriginalRelativePath(relIt->get<std::string>());
            const std::string objectPath = pathIt->get<std::string>();
            if (normalizedRel.empty() || objectPath.empty()) {
                continue;
            }
            auto existing = out.find(normalizedRel);
            if (existing == out.end() || objectPath.size() < existing->second.size()) {
                out[normalizedRel] = objectPath;
            }
        }
        return true;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = e.what();
        return false;
    }
}

TransferResult executeTransfer(
    waapi::WaapiClient& client,
    const WwiseTransferConfig& config,
    const std::vector<WwiseImportItem>& imports,
    ProgressCallback progressCb) {

    TransferResult result;
    result.total = static_cast<int>(imports.size());

    if (!client.isConnected()) {
        result.aggregateError = "WAAPI client not connected";
        return result;
    }
    if (config.projectPath.empty()) {
        result.aggregateError = "Wwise project path is empty";
        return result;
    }
    if (imports.empty()) {
        result.aggregateError = "Nothing to transfer (imports list is empty)";
        return result;
    }

    // Published WAAPI undo groups let the user remove the complete Aris batch
    // with one Wwise undo action.
    struct UndoGroupGuard {
        waapi::WaapiClient& c;
        bool active = false;
        ~UndoGroupGuard() {
            if (active) c.call("ak.wwise.core.undo.endGroup", "{}");
        }
    } undoGuard{client};
    {
        const json undoArgs = buildUndoBeginArguments();
        // Failure is non-fatal — Wwise may not support undo groups in all configs.
        if (client.call("ak.wwise.core.undo.beginGroup", undoArgs.dump()).ok)
            undoGuard.active = true;
    }

    const fs::path originalsRoot = originalsSfxRoot(config.projectPath);
    const bool overwriteWavs = (config.conflictPolicy == ConflictPolicy::Replace);

    // Stage 1: copy WAVs into Originals. We build ItemReports up-front so
    // the progress callback sees the full batch size from the first tick.
    result.items.reserve(imports.size());
    for (const auto& imp : imports) {
        ItemReport r;
        r.sourceWav = imp.sourceWav;
        r.targetObjectPath = buildTaggedObjectPath(
            config.importDestination, imp.objectPath, imp.objectTypes);
        result.items.push_back(std::move(r));
    }

    for (std::size_t i = 0; i < imports.size(); ++i) {
        const auto& imp = imports[i];
        auto& report = result.items[i];

        // Unresolved imports skip the whole pipeline — surface as failure so
        // the user can't silently ship a transfer with `<unresolved>` tokens.
        if (!imp.unresolvedTokens.empty()) {
            report.status = ItemStatus::FailedWavCopy;
            report.errorMessage = "Unresolved wildcards: ";
            for (std::size_t k = 0; k < imp.unresolvedTokens.size(); ++k) {
                report.errorMessage += (k ? ", " : "") + imp.unresolvedTokens[k];
            }
            ++result.failed;
            if (progressCb) progressCb(static_cast<int>(i + 1), result.total, report);
            continue;
        }

        // Leaf filename comes from the last layer's expanded name, plus ".wav"
        // to match what Wwise's importer expects.
        if (imp.objectPath.empty()) {
            report.status = ItemStatus::FailedWavCopy;
            report.errorMessage = "Empty objectPath — structure template produced no layers";
            ++result.failed;
            if (progressCb) progressCb(static_cast<int>(i + 1), result.total, report);
            continue;
        }
        const std::string leafName = imp.objectPath.back();
        const fs::path dstPath = originalsRoot / utf8Path(imp.originalsRelPath) / utf8Path(leafName + ".wav");

        const std::string copyErr = copyWavUtf8(imp.sourceWav, dstPath, overwriteWavs);
        if (!copyErr.empty()) {
            report.status = ItemStatus::FailedWavCopy;
            report.errorMessage = copyErr;
            ++result.failed;
            if (progressCb) progressCb(static_cast<int>(i + 1), result.total, report);
            continue;
        }
        report.status = ItemStatus::WavCopied;
    }

    // Stage 2: batch audio.import calls. Collect only the items that
    // succeeded at stage 1.
    std::vector<std::size_t> importable;
    importable.reserve(result.items.size());
    for (std::size_t i = 0; i < result.items.size(); ++i) {
        if (result.items[i].status == ItemStatus::WavCopied) importable.push_back(i);
    }

    const std::string importOp = toImportOperation(config.conflictPolicy);

    for (std::size_t chunkStart = 0; chunkStart < importable.size();
         chunkStart += kImportBatchSize) {
        const std::size_t chunkEnd = std::min(chunkStart + static_cast<std::size_t>(kImportBatchSize),
                                              importable.size());

        json importsArr = json::array();
        for (std::size_t k = chunkStart; k < chunkEnd; ++k) {
            const std::size_t i = importable[k];
            const auto& imp = imports[i];
            const std::string leafName = imp.objectPath.back();
            const fs::path dstPath = originalsRoot / utf8Path(imp.originalsRelPath) / utf8Path(leafName + ".wav");

            json entry;
            entry["audioFile"] = pathToUtf8(dstPath);
            entry["objectPath"] = result.items[i].targetObjectPath;
            entry["notes"] = "Imported from Harken Search";
            importsArr.push_back(std::move(entry));
        }

        const json args = buildAudioImportArguments(importOp, std::move(importsArr));

        waapi::CallResult rc = client.call("ak.wwise.core.audio.import", args.dump());
        if (!rc.ok) {
            // Whole chunk failed. Mark every item in this chunk as failed.
            for (std::size_t k = chunkStart; k < chunkEnd; ++k) {
                const std::size_t i = importable[k];
                result.items[i].status = ItemStatus::FailedImport;
                result.items[i].errorMessage = "audio.import chunk failed: " + rc.errorMessage;
                ++result.failed;
                if (progressCb) progressCb(static_cast<int>(i + 1), result.total, result.items[i]);
            }
            continue;
        }
        // Chunk succeeded. Mark every item Imported.
        for (std::size_t k = chunkStart; k < chunkEnd; ++k) {
            const std::size_t i = importable[k];
            result.items[i].status = ItemStatus::Imported;
            ++result.imported;
            if (progressCb) progressCb(static_cast<int>(i + 1), result.total, result.items[i]);
        }
    }

    // Stage 3: Property Template — only runs for items that got Imported and
    // whose structure layers flagged useTemplate. copyProperties operates on
    // a single (source, target) pair, so we call it per item here; batching
    // is an S2-7-era optimisation if needed.
    for (std::size_t i = 0; i < result.items.size(); ++i) {
        if (result.items[i].status != ItemStatus::Imported) continue;
        const auto& imp = imports[i];

        // Find the first layer with a non-empty propertyTemplatePath. Multi-
        // layer templates would require per-layer calls; design doc keeps v0
        // at "leaf template only" since that's where the interesting property
        // inheritance (Output Bus, Attenuation) actually lives.
        std::string templatePath;
        for (const auto& path : imp.propertyTemplatePaths) {
            if (!path.empty()) { templatePath = path; break; }
        }
        if (templatePath.empty()) continue;
        // v0: always applies when useTemplate is set on any layer. The
        // NewOnly-vs-Always distinction requires knowing whether each
        // target object pre-existed — that's an ExistingObjectMap we don't
        // currently thread into execute(). Pushed to a follow-up commit
        // once the UI panel (S7) is populating that map for the Diff.
        (void)config.templateApply;

        // The published pasteProperties contract takes one source path and a
        // targets array. Aris applies the template to one imported object per
        // request so each failure remains attributable to a single item.
        const json args = buildPastePropertiesArguments(
            templatePath,
            result.items[i].targetObjectPath);

        waapi::CallResult rc = client.call("ak.wwise.core.object.pasteProperties", args.dump());
        if (!rc.ok) {
            result.items[i].status = ItemStatus::FailedTemplate;
            result.items[i].errorMessage = "copyProperties failed: " + rc.errorMessage;
            // Don't undo the audio.import — the object is in Wwise, just
            // without its template inheritance. User can re-apply manually.
            ++result.failed;
            --result.imported; // reclassify from Imported → Failed
            if (progressCb) progressCb(static_cast<int>(i + 1), result.total, result.items[i]);
        } else {
            result.items[i].status = ItemStatus::TemplateApplied;
        }
    }

    return result;
}

} // namespace wwise
}
