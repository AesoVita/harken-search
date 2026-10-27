#include "RecordRenameWorkflow.h"

#include "AudioEngine.h"
#include "Database.h"
#include "Utils.h"
#include "platform/filesystem.h"

#include <stdexcept>

namespace aura::records {

RecordRenameOutcome RenameFileAndDatabase(
    Database& database,
    AudioEngine& audioEngine,
    const std::string& oldPath,
    const std::string& newFileName) {
    RecordRenameOutcome outcome;

    fs::path oldP = Utf8ToPath(oldPath);
    if (!Platform::FileSystem::Exists(PathToUtf8(oldP))) return outcome;

    fs::path newP = oldP.parent_path() / Utf8ToPath(newFileName);
    if (Platform::FileSystem::Exists(PathToUtf8(newP))) return outcome;

    outcome.newPath = PathToUtf8(newP);

    try {
        database.noteExpectedFileRename(oldPath, outcome.newPath);

        bool wasPlayingThis = false;
        std::string renameError;
        if (!Platform::FileSystem::Rename(oldPath, outcome.newPath, &renameError)) {
            if (audioEngine.isLoaded() &&
                audioEngine.getFileInfo().filePath == oldPath) {
                wasPlayingThis = true;
                audioEngine.stop();
                audioEngine.unload();
            }
            if (!Platform::FileSystem::Rename(oldPath, outcome.newPath, &renameError)) {
                throw std::runtime_error(
                    renameError.empty() ? "rename failed" : renameError);
            }
        }

        const bool dbUpdated =
            database.updateFilePath(oldPath, outcome.newPath, newFileName);
        if (!dbUpdated) {
            try {
                database.noteExpectedFileRename(outcome.newPath, oldPath);
                std::string rollbackError;
                if (!Platform::FileSystem::Rename(outcome.newPath, oldPath, &rollbackError)) {
                    throw std::runtime_error(
                        rollbackError.empty()
                            ? "rollback rename failed"
                            : rollbackError);
                }
            } catch (...) {
            }
            if (wasPlayingThis) audioEngine.loadFileAsync(oldPath);
            return outcome;
        }

        outcome.success = true;
        outcome.reloadRenamedAudio = wasPlayingThis;
        return outcome;
    } catch (...) {
        if (!audioEngine.isLoaded()) {
            audioEngine.loadFileAsync(oldPath);
        }
        return outcome;
    }
}

} // namespace aura::records
