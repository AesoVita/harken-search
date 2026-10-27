#include "FilePlayerDeferredDbWarmupRuntime.h"

#include "Database.h"
#include "platform/system.h"

namespace aura::file_player {

bool WarmFilePlayerDatabaseReadOnly(const std::string& dbPathUtf8) {
    Platform::SetThreadBackgroundPriority();
    Database warmupDb;
    const bool ok = warmupDb.openReadOnly(dbPathUtf8);
    if (ok) {
        (void)warmupDb.getFileCount();
        warmupDb.warmupCoverIndexes();
        warmupDb.close();
    }
    return ok;
}

} // namespace aura::file_player
