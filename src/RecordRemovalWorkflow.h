#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Database;
class AudioEngine;

namespace aura::records {

struct FileDeletionOutcome {
    std::vector<std::string> filesystemFailures;
};

// @stage7-contract
// API contract: all collaborators and input collections are borrowed for the
// synchronous call. Database-only removal owns transaction/cache ordering.
void RemoveRecordsFromDatabase(
    Database& database,
    const std::vector<int64_t>& ids);

// @stage7-contract
// API contract: unloads an exact-path audio owner before physical deletion,
// removes database rows only for files already absent or successfully deleted,
// and returns owned failure paths without throwing filesystem failures.
FileDeletionOutcome DeleteFilesFromSystem(
    Database& database,
    AudioEngine& audioEngine,
    const std::vector<int64_t>& ids,
    const std::vector<std::string>& paths);

} // namespace aura::records
