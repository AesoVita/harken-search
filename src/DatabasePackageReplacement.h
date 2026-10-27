#pragma once
#include <string>

namespace aura::database_switch {

// Runs only after all connections to the current database have closed.
// Keep an existing package intact until the replacement has opened successfully.
class DatabasePackageReplacement {
public:
    bool Prepare(const std::string& databasePath, bool overwrite);
    bool Commit();
    bool Rollback();
    const std::string& Error() const { return error_; }
    std::string RecoveryDatabasePath() const;
private:
    std::string root_;
    std::string backup_;
    std::string error_;
    bool created_ = false;
};
}
