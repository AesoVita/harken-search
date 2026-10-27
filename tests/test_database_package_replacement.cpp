#include "DatabasePackageReplacement.h"
#include "platform/filesystem.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace files = Platform::FileSystem;
using aura::database_switch::DatabasePackageReplacement;
int failures = 0;
void Expect(bool ok, const char* label) {
    if (!ok) { std::cerr << "FAIL: " << label << "\n"; ++failures; }
}
std::string Read(const fs::path& path) {
    std::string result; files::ReadFileText(path.u8string(), result); return result;
}
void Seed(const fs::path& root) {
    files::CreateDirectories(root.u8string());
    files::WriteFileText((root/"main.db").u8string(), "original-main");
    files::WriteFileText((root/"waveforms.db").u8string(), "original-waveforms");
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto root = fs::absolute(fs::u8path(argv[1])) /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    files::CreateDirectories(root.u8string());
    {
        const auto package = root/"fresh";
        DatabasePackageReplacement tx;
        Expect(tx.Prepare((package/"main.db").u8string(), false), "fresh prepare");
        Expect(!files::Exists((package/"main.db").u8string()), "fresh package is empty");
        files::WriteFileText((package/"main.db").u8string(), "new-main");
        Expect(tx.Commit(), "fresh commit");
        Expect(Read(package/"main.db") == "new-main", "fresh content retained");
    }
    {
        const auto package = root/"existing"; Seed(package);
        DatabasePackageReplacement denied;
        Expect(!denied.Prepare((package/"main.db").u8string(), false), "overwrite needs confirmation");
        Expect(denied.Rollback(), "denied prepare rollback is harmless");
        Expect(Read(package/"main.db") == "original-main", "denial preserves original");
        DatabasePackageReplacement tx;
        Expect(tx.Prepare((package/"main.db").u8string(), true), "overwrite prepare");
        const auto backupMain = fs::u8path(tx.RecoveryDatabasePath());
        Expect(Read(backupMain) == "original-main", "original preserved until success");
        Expect(Read(backupMain.parent_path()/"waveforms.db") == "original-waveforms", "sidecars preserved");
        Expect(!files::Exists((package/"main.db").u8string()), "replacement never reopens old content");
        files::WriteFileText((package/"main.db").u8string(), "new-main");
        Expect(tx.Commit(), "overwrite commit");
        Expect(Read(package/"main.db") == "new-main", "commit publishes new package");
        Expect(!files::Exists(backupMain.u8string()), "successful commit cleans backup");
    }
    {
        const auto package = root/"failed-open"; Seed(package);
        DatabasePackageReplacement tx;
        Expect(tx.Prepare((package/"main.db").u8string(), true), "rollback prepare");
        files::WriteFileText((package/"main.db").u8string(), "partial-new-main");
        Expect(tx.Rollback(), "failed open restores package");
        Expect(Read(package/"main.db") == "original-main", "rollback restores main bytes");
        Expect(Read(package/"waveforms.db") == "original-waveforms", "rollback restores sidecars");
    }
#ifdef _WIN32
    {
        const auto package = root/"locked"; Seed(package);
        HANDLE lock = CreateFileW(package.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        Expect(lock != INVALID_HANDLE_VALUE, "lock existing package");
        DatabasePackageReplacement tx;
        Expect(!tx.Prepare((package/"main.db").u8string(), true), "locked package is rejected");
        Expect(tx.Rollback(), "rejected rename leaves package alone");
        Expect(Read(package/"main.db") == "original-main", "locked original is untouched");
        CloseHandle(lock);
    }
    {
        const auto package = root/"blocked-rollback"; Seed(package);
        DatabasePackageReplacement tx;
        Expect(tx.Prepare((package/"main.db").u8string(), true), "blocked rollback prepare");
        const auto backupMain = fs::u8path(tx.RecoveryDatabasePath());
        const auto partial = package/"main.db";
        files::WriteFileText(partial.u8string(), "partial-new-main");
        HANDLE lock = CreateFileW(partial.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        Expect(lock != INVALID_HANDLE_VALUE, "lock partial package");
        Expect(!tx.Rollback(), "blocked rollback reports failure");
        Expect(Read(backupMain) == "original-main", "failed rollback retains recovery copy");
        CloseHandle(lock);
        Expect(tx.Rollback(), "rollback can finish after lock is released");
        Expect(Read(package/"main.db") == "original-main", "retry restores original");
    }
#endif
    {
        DatabasePackageReplacement tx;
        Expect(!tx.Prepare((root/"not-a-package.db").u8string(), true), "reject non-package target");
        Expect(!tx.Prepare("main.db", true), "reject relative target");
    }
    if (failures) return 1;
    std::cout << "PASS: create, overwrite, rollback and locked-package preservation\n";
    return 0;
}
