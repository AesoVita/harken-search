#pragma once

#include <functional>
#include <string>

class Database;

namespace aura::file_player {

struct DatabaseOpenAttemptResult {
    bool opened = false;
    std::string error;
};

using DatabaseOpenAttempt =
    std::function<DatabaseOpenAttemptResult(const std::string& pathUtf8)>;

struct FilePlayerDatabaseOpenResult {
    bool opened = false;
    bool fallbackAttempted = false;
    std::string activePathUtf8;
    std::string error;
};

FilePlayerDatabaseOpenResult OpenFilePlayerDatabaseWithFallback(
    std::string primaryPathUtf8,
    std::string fallbackPathUtf8,
    const DatabaseOpenAttempt& attempt);

FilePlayerDatabaseOpenResult OpenFilePlayerDatabaseWithFallback(
    Database& database,
    std::string primaryPathUtf8,
    std::string fallbackPathUtf8);

} // namespace aura::file_player
