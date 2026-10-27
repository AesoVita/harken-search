#pragma once
// Aris - Send to Wwise (F-034 S6) - Transfer execution.
//
// Takes the planned WwiseImportItem batch (from S4 expandStructure) and
// actually pushes it to Wwise: copies each source WAV into the project's
// Originals/SFX tree, then fires batched ak.wwise.core.audio.import calls.
// Property Template inheritance, if any layer requests it, runs as a second
// pass via ak.wwise.core.object.pasteProperties so the newly-created objects
// pick up Output Bus / Attenuation / etc. from the template.
//
// Network side lives in this TU (not split like WwiseQuery/Client) because
// there's no meaningful pure-function portion to unit-test independently —
// the WAV copy is a one-liner wrapper over std::filesystem and the rest is
// all WAAPI calls.

#include "WaapiClient.h"
#include "WwiseStructureTemplate.h"
#include "WwiseTransferConfig.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace aura {
namespace wwise {

// @stage7-contract
// API contract: executeTransfer is a synchronous, caller-thread operation
// that borrows client/config/imports for the duration of the call and owns
// the returned reports. It requires a connected client and valid project
// directory; progressCb is optional, invoked on the caller thread, and must
// not be used to re-enter transfer execution. Failures are represented in
// TransferResult and do not change the public ABI or batching semantics.

// Per-item outcome. Preserved in TransferResult.items so the UI can show a
// row-by-row report in the result dialog.
enum class ItemStatus : std::uint8_t {
    Pending,         // Haven't reached this item yet (progressCb sees this).
    WavCopied,       // Source WAV successfully placed under Originals.
    Imported,        // Wwise accepted the audio.import call for this item.
    TemplateApplied, // pasteProperties from Property Template succeeded.
    SkippedExisting, // ConflictPolicy::UseExisting + object already in Wwise.
    FailedWavCopy,   // Filesystem copy failed (permission / disk / path).
    FailedImport,    // WAAPI rejected the import.
    FailedTemplate,  // audio.import succeeded but pasteProperties failed.
};

const char* toString(ItemStatus);

using ImportedObjectPathMap = std::map<std::string, std::string>;

// Parse ak.wwise.core.audio.import result objects back to Originals-relative
// WAV paths. Keys are normalized to lowercase backslash paths.
bool parseAudioImportResultPaths(const std::string& resultJson,
                                 ImportedObjectPathMap& out,
                                 std::string* errorOut = nullptr);

struct ItemReport {
    std::string sourceWav;
    std::string targetObjectPath; // Expanded Wwise path, as handed to WAAPI.
    ItemStatus  status = ItemStatus::Pending;
    std::string errorMessage;     // Populated on any Failed* status.
};

struct TransferResult {
    int total = 0;
    int imported = 0;
    int skipped = 0;
    int failed = 0;
    std::vector<ItemReport> items;
    std::string aggregateError; // Non-empty if an early abort prevented per-item progress.
};

// Progress callback shape. `done` counts items fully resolved (success OR
// terminal failure); `total` is the batch size. Invoked from the same thread
// that called executeTransfer — callers wanting to drive an ImGui progress
// bar should marshal to the UI thread themselves.
using ProgressCallback = std::function<void(int done, int total, const ItemReport& latest)>;

// Execute the transfer. Preconditions (failure → aggregateError, no items
// touched):
//   - client.isConnected() returns true
//   - config.projectPath non-empty and points at a real directory
//   - imports non-empty
//
// Batching: audio.import calls are chunked into groups of `kImportBatchSize`
// (default 300) to avoid stalling Wwise's UI thread on single giant calls.
// Each chunk is a separate WAAPI request; the callback fires after each
// item's resolution inside a chunk, not once per chunk.
constexpr int kImportBatchSize = 300;

TransferResult executeTransfer(
    waapi::WaapiClient& client,
    const WwiseTransferConfig& config,
    const std::vector<WwiseImportItem>& imports,
    ProgressCallback progressCb = nullptr);

} // namespace wwise
}
