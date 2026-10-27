// Aris - Send to Wwise (F-034 S5-2) - Snapshot inputs for Preview Diff.

#include "WwiseQuery.h"
#include "platform/filesystem.h"

#include "nlohmann_json.hpp"

#include <filesystem>

namespace aura {
namespace wwise {

namespace {
using nlohmann::json;

// Converts `foo/bar\baz.wav` → `foo/bar/baz.wav` so the set keys match the
// shape expandStructure produces (which always uses forward slashes).
std::string toForwardSlashes(std::string s) {
    for (auto& c : s) {
        if (c == '\\') c = '/';
    }
    return s;
}

} // namespace

bool parseExistingObjectsResponse(const std::string& jsonText,
                                  ExistingObjectMap& outMap,
                                  std::string* errorOut) {
    outMap.clear();
    auto fail = [&](const std::string& msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };

    json j;
    try {
        j = json::parse(jsonText);
    } catch (const std::exception& e) {
        return fail(std::string("WAAPI JSON parse failed: ") + e.what());
    }
    if (!j.is_object()) return fail("WAAPI response root must be an object");

    // WAAPI returns results under "return" — a JSON array of object records.
    // Some older/newer call shapes wrap this under a top-level "objects" key;
    // accept either to stay forward-compatible.
    const json* arr = nullptr;
    if (j.contains("return") && j["return"].is_array()) arr = &j["return"];
    else if (j.contains("objects") && j["objects"].is_array()) arr = &j["objects"];
    else return fail("WAAPI response missing `return`/`objects` array");

    for (const auto& entry : *arr) {
        if (!entry.is_object()) continue; // Skip malformed entries; don't fail whole.
        if (!entry.contains("path") || !entry["path"].is_string()) continue;
        if (!entry.contains("type") || !entry["type"].is_string()) continue;

        const std::string path = entry["path"].get<std::string>();
        const std::string typeStr = entry["type"].get<std::string>();
        WwiseObjectType type;
        if (!fromString(typeStr, type)) {
            // Unknown type (Event, Bus, Effect, ...) — WAAPI can traverse
            // into kinds outside our transfer model. Silently skip so a
            // mixed-kind subtree doesn't fail the whole snapshot.
            continue;
        }
        outMap.emplace(path, type);
    }
    return true;
}

bool scanOriginalsDirectory(const std::string& originalsSfxRoot,
                            ExistingOriginalsSet& outSet,
                            std::string* errorOut) {
    outSet.clear();
    namespace fs = std::filesystem;

    if (!Platform::FileSystem::Exists(originalsSfxRoot) ||
        !Platform::FileSystem::IsDirectory(originalsSfxRoot)) {
        // Not an error per se — this commonly happens the first time a user
        // points at a brand-new Wwise project. Return false so the caller
        // knows the scan found nothing, but don't populate errorOut.
        return false;
    }

    Platform::FileSystem::DirectoryIterator iterator;
    if (!iterator.Open(originalsSfxRoot, true, true)) return false;
    for (;;) {
        Platform::FileSystem::DirectoryEntry entry;
        std::string walkError;
        const auto status = iterator.Next(entry, &walkError);
        if (status == Platform::FileSystem::DirectoryReadStatus::End) break;
        if (status == Platform::FileSystem::DirectoryReadStatus::Error) {
            if (errorOut) *errorOut = "Directory walk partial failure: " + walkError;
            continue; // Skip the problematic entry, keep walking siblings.
        }
        if (!entry.isRegularFile) continue;
        const fs::path full = fs::u8path(entry.pathUtf8);
        const std::string ext = full.extension().string();
        // Case-insensitive .wav check — some exporters emit .WAV.
        if (ext.size() != 4) continue;
        if ((ext[0] != '.') ||
            (ext[1] != 'w' && ext[1] != 'W') ||
            (ext[2] != 'a' && ext[2] != 'A') ||
            (ext[3] != 'v' && ext[3] != 'V')) continue;

        const std::string rel = Platform::FileSystem::RelativePath(entry.pathUtf8, originalsSfxRoot);
        if (rel.empty()) continue;
        outSet.emplace(toForwardSlashes(rel));
    }
    return true;
}

// fetchExistingObjects lives in WwiseQueryClient.cpp — split out so unit
// tests can link WwiseQuery.cpp alone without pulling in ixwebsocket.

} // namespace wwise
}
