#pragma once

#include "Database.h"

#include <string>

namespace aura::cover_cache {

struct PersistenceContext {
    Database& database;
    std::string databasePathUtf8;
    bool persistenceEnabled = true;
};

// Drains cover-key updates and persists them on the caller's (UI) thread.
// The workflow owns only database synchronization; presentation remains above.
void PumpPersistence(PersistenceContext context);

}  // namespace aura::cover_cache
