#pragma once

#include <string>

class Database;
class AudioEngine;

namespace aura::records {

struct RecordRenameOutcome {
    bool success = false;
    bool reloadRenamedAudio = false;
    std::string newPath;
};

// @stage7-contract
// API contract: collaborators and strings are borrowed for the synchronous
// call. The returned path is owned. Failure is reported without throwing after
// path validation, with best-effort filesystem and playback recovery.
RecordRenameOutcome RenameFileAndDatabase(
    Database& database,
    AudioEngine& audioEngine,
    const std::string& oldPath,
    const std::string& newFileName);

} // namespace aura::records
