#include "FilePlayerDatabaseOpenWorkflow.h"

#include <utility>

namespace aura::file_player {

FilePlayerDatabaseOpenResult OpenFilePlayerDatabaseWithFallback(
    std::string primaryPathUtf8,
    std::string fallbackPathUtf8,
    const DatabaseOpenAttempt& attempt) {
    FilePlayerDatabaseOpenResult result;
    result.activePathUtf8 = std::move(primaryPathUtf8);

    const auto primary = attempt(result.activePathUtf8);
    if (primary.opened) {
        result.opened = true;
        return result;
    }
    result.error = primary.error;

    if (fallbackPathUtf8 == result.activePathUtf8) return result;

    result.fallbackAttempted = true;
    const auto fallback = attempt(fallbackPathUtf8);
    if (fallback.opened) {
        result.opened = true;
        result.activePathUtf8 = std::move(fallbackPathUtf8);
        result.error.clear();
    } else {
        result.error = fallback.error;
    }
    return result;
}

} // namespace aura::file_player
