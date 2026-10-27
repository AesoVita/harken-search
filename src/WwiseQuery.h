#pragma once
// Aris - Send to Wwise (F-034 S5-2) - Snapshot inputs for Preview Diff.
//
// Two sources feed the diff algorithm from S5-1:
//   1. ExistingObjectMap — obtained by calling ak.wwise.core.object.get on
//      the import destination's descendants. parseExistingObjectsResponse
//      turns the WAAPI return payload into the map; fetchExistingObjects is
//      the thin orchestrator that wraps a WaapiClient call + parse.
//   2. ExistingOriginalsSet — filesystem walk under <ProjectRoot>/Originals/
//      SFX/. Pure filesystem work; doesn't touch WAAPI.
//
// The parser + scanner are split from the orchestrator so they can be unit-
// tested against hand-built inputs without needing a live Wwise or a mock
// WebSocket server.

#include "WwiseDiff.h"          // ExistingObjectMap / ExistingOriginalsSet
#include "WaapiClient.h"        // For fetchExistingObjects orchestrator

#include <string>

namespace aura {
namespace wwise {

// @stage7-contract
// API contract: parser/scanner helpers synchronously fill caller-owned output
// containers and never retain JSON, path, or error-string references.
// fetchExistingObjects additionally borrows a connected WaapiClient and may
// block on RPC/filesystem IO; false means the output is empty and errorOut,
// when supplied, receives diagnostic text.

// Parses the JSON text payload WAAPI returns from `ak.wwise.core.object.get`
// with a "transform":[{"select":["descendants"]}] query and an "options":
// {"return":["type","path"]} clause. Only the "type" and "path" fields are
// consumed — other fields pass through harmlessly.
//
// Returns true on success with `outMap` populated. On malformed JSON the
// map is cleared and errorOut (if provided) carries detail. Unknown type
// strings are *skipped* rather than failing the whole response — Wwise
// regularly returns types we don't model (Event, Bus, ...) when the query
// traverses mixed-kind trees, and those are simply irrelevant to the diff.
bool parseExistingObjectsResponse(const std::string& jsonText,
                                  ExistingObjectMap& outMap,
                                  std::string* errorOut = nullptr);

// Recursively enumerates every *.wav file under <originalsSfxRoot> and emits
// their paths *relative to that root* with forward-slash separators — the
// same shape WwiseImportItem::originalsRelPath produces. Non-WAV files are
// ignored; symlink cycles are detected via std::filesystem's own guards.
//
// Returns true if the directory exists and was walked; false if the path
// doesn't exist or isn't a directory (outSet is left empty in that case).
// Errors during individual file inspection are logged via errorOut (if
// provided) but don't abort the walk — a single unreadable file shouldn't
// poison the whole snapshot.
bool scanOriginalsDirectory(const std::string& originalsSfxRoot,
                            ExistingOriginalsSet& outSet,
                            std::string* errorOut = nullptr);

// Orchestrator — calls `ak.wwise.core.object.get` over the given client to
// fetch descendants of `importDestination`, then hands off to
// parseExistingObjectsResponse. Returns the empty map on any failure
// (client not connected, RPC error, parse error); errorOut carries detail.
//
// This is the production entry point; unit tests should exercise the parser
// directly with canned JSON rather than standing up a WaapiClient.
bool fetchExistingObjects(waapi::WaapiClient& client,
                          const std::string& importDestination,
                          ExistingObjectMap& outMap,
                          std::string* errorOut = nullptr);

} // namespace wwise
}
