#include "FilePlayerDatabaseOpenWorkflow.h"

#include "Database.h"
#include "platform/harken_user_data_layout.h"

#include <utility>

namespace aura::file_player {

FilePlayerDatabaseOpenResult OpenFilePlayerDatabaseWithFallback(
    Database& database,
    std::string primaryPathUtf8,
    std::string fallbackPathUtf8) {
    const bool readOnlyFallback = Platform::HarkenUserData::EnsureLayout().readOnly;
    const auto attempt = [&database, readOnlyFallback](const std::string& pathUtf8) {
        const bool opened = readOnlyFallback
            ? database.openReadOnly(pathUtf8)
            : database.open(pathUtf8, true);
        return DatabaseOpenAttemptResult{
            opened,
            opened ? std::string{} : database.getLastError()};
    };
    return OpenFilePlayerDatabaseWithFallback(
        std::move(primaryPathUtf8), std::move(fallbackPathUtf8), attempt);
}

} // namespace aura::file_player
