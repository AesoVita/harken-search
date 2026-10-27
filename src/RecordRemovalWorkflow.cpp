#include "RecordRemovalWorkflow.h"

#include "AudioEngine.h"
#include "Database.h"
#include "platform/filesystem.h"

namespace aura::records {

void RemoveRecordsFromDatabase(
    Database& database,
    const std::vector<int64_t>& ids) {
    if (ids.empty()) return;

    database.executeRawQuery("PRAGMA busy_timeout = 5000;");
    database.beginTransaction();
    for (int64_t id : ids) {
        database.removeFile(id);
    }
    database.commitTransaction();
    database.invalidateFacetQueryCache();
    database.clearFolderIndex();
}

FileDeletionOutcome DeleteFilesFromSystem(
    Database& database,
    AudioEngine& audioEngine,
    const std::vector<int64_t>& ids,
    const std::vector<std::string>& paths) {
    FileDeletionOutcome outcome;
    if (ids.empty() || paths.empty()) return outcome;

    for (const auto& path : paths) {
        if (audioEngine.getLoadedFilePath() == path) {
            audioEngine.unload();
            break;
        }
    }

    database.executeRawQuery("PRAGMA busy_timeout = 5000;");
    database.beginTransaction();
    for (size_t i = 0; i < ids.size() && i < paths.size(); ++i) {
        bool filesystemOk = true;
        try {
            const auto pathStatus = Platform::FileSystem::ProbePath(paths[i]);
            if (pathStatus == Platform::FileSystem::PathStatus::Error ||
                (pathStatus == Platform::FileSystem::PathStatus::Present &&
                 !Platform::FileSystem::RemoveFile(paths[i]))) {
                outcome.filesystemFailures.push_back(paths[i]);
                filesystemOk = false;
            }
        } catch (...) {
            outcome.filesystemFailures.push_back(paths[i]);
            filesystemOk = false;
        }
        if (filesystemOk) {
            database.removeFile(ids[i]);
        }
    }
    database.commitTransaction();
    database.invalidateFacetQueryCache();
    database.clearFolderIndex();
    return outcome;
}

} // namespace aura::records
