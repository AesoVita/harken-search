#include "DatabasePackageReplacement.h"
#include "platform/filesystem.h"
#include <chrono>
#include <filesystem>

namespace aura::database_switch {
namespace fs = std::filesystem;
namespace files = Platform::FileSystem;

bool DatabasePackageReplacement::Prepare(const std::string& databasePath, bool overwrite) {
    const auto path = fs::u8path(databasePath).lexically_normal();
    if (path.filename() != "main.db" || !path.is_absolute() ||
        path.parent_path() == path.root_path()) {
        error_ = "New databases require a dedicated package directory";
        return false;
    }
    root_ = path.parent_path().u8string();
    const auto status = files::ProbePath(root_, &error_);
    if (status != files::PathStatus::Missing && status != files::PathStatus::Present)
        return false;
    if (status == files::PathStatus::Present) {
        if (!overwrite) {
            error_ = "Database already exists; confirm overwrite or choose another name";
            return false;
        }
        // Rename, never delete, the complete package (including sidecars).
        const std::string backup = root_ + ".before-create-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        if (files::ProbePath(backup) != files::PathStatus::Missing ||
            !files::Rename(root_, backup, &error_)) {
            if (error_.empty()) error_ = "Could not preserve the existing database";
            return false;
        }
        backup_ = backup;
    }
    created_ = files::CreateDirectories(root_);
    if (!created_) {
        error_ = "Could not create the new database directory";
        return false;
    }
    return true;
}

bool DatabasePackageReplacement::Commit() {
    if (backup_.empty()) return true;
    if (!files::RemoveAll(backup_, &error_)) return false;
    backup_.clear();
    return true;
}

bool DatabasePackageReplacement::Rollback() {
    std::string cleanupError;
    if (created_ && !files::RemoveAll(root_, &cleanupError)) {
        error_ += "; rollback: " + cleanupError;
        return false;
    }
    created_ = false;
    if (!backup_.empty()) {
        if (!files::Rename(backup_, root_, &cleanupError)) {
            error_ += "; restore: " + cleanupError;
            return false;
        }
        backup_.clear();
    }
    return true;
}

std::string DatabasePackageReplacement::RecoveryDatabasePath() const {
    return backup_.empty() ? std::string() :
        (fs::u8path(backup_) / "main.db").u8string();
}
}
