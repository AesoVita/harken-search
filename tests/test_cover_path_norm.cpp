// TDD: 封面墙 Folder key 路径规范化（根因文档 2026-03-17）
// 仅依赖 Utils，无 Database/ImGui，便于先红后绿。
#include "Utils.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    int failed = 0;
#define ASSERT_EQ(a, b) do { \
    std::string got = NormalizeFolderPathFromKeyForLookup(a); \
    if (got != (b)) { \
        std::cerr << "FAIL: NormalizeFolderPathFromKeyForLookup(\"" << (a) << "\") == \"" << got << "\", expected \"" << (b) << "\"\n"; \
        failed++; \
    } else std::cout << "OK: \"" << (a) << "\" -> \"" << got << "\"\n"; \
} while(0)

    ASSERT_EQ("F|\\\\?\\H:\\Sound effect\\foo", "H:\\Sound effect\\foo");
    ASSERT_EQ("F|H|39aa5222810909f2|42674", "");
    ASSERT_EQ("F|H:\\bar", "H:\\bar");
    ASSERT_EQ("F|\\?\\H:\\baz", "H:\\baz");
    ASSERT_EQ("", "");
    ASSERT_EQ("E|00068c588eb97aa9|317132", "");

    // 定位高亮谓词（Bug: 定位之后没有高亮显示）
    int64_t now = 1000, until = 1500;
    if (!ShouldHighlightFolderNodeForLocate("/a/b", "/a/b", until, now)) { std::cerr << "FAIL: locate highlight should be true when path match and now<until\n"; failed++; }
    else std::cout << "OK: locate highlight true when path match and now<until\n";
    if (ShouldHighlightFolderNodeForLocate("/a/c", "/a/b", until, now)) { std::cerr << "FAIL: locate highlight should be false when path mismatch\n"; failed++; }
    else std::cout << "OK: locate highlight false when path mismatch\n";
    if (ShouldHighlightFolderNodeForLocate("/a/b", "/a/b", until, 2000)) { std::cerr << "FAIL: locate highlight should be false when now>=until\n"; failed++; }
    else std::cout << "OK: locate highlight false when now>=until\n";
    if (ShouldHighlightFolderNodeForLocate("/a/b", "", until, now)) { std::cerr << "FAIL: locate highlight should be false when highlightPath empty\n"; failed++; }
    else std::cout << "OK: locate highlight false when highlightPath empty\n";

    // 导出防爆音：首尾淡入淡出（拖入 DAW 有时 click）
    {
        std::vector<float> buf(20, 1.0f);
        ApplyExportDeclickFade(buf.data(), 20, 1, 0, 20, 5);
        if (buf[0] <= 0.0f || buf[0] > 0.5f) { std::cerr << "FAIL: export fade-in first sample should be (0,0.5]\n"; failed++; }
        else std::cout << "OK: export fade-in first sample in range\n";
        if (buf[4] <= 0.7f || buf[4] > 1.01f) { std::cerr << "FAIL: export fade-in last fade frame ~1.0\n"; failed++; }
        else std::cout << "OK: export fade-in last fade frame ~1.0\n";
        buf.assign(20, 1.0f);
        ApplyExportDeclickFade(buf.data(), 10, 1, 10, 20, 5);
        if (buf[9] <= 0.0f || buf[9] > 0.5f) { std::cerr << "FAIL: export fade-out last sample should be (0,0.5]\n"; failed++; }
        else std::cout << "OK: export fade-out last sample in range\n";
    }

    // 开发模式切库持久化：显式切过且文件仍存在的 DB 路径，不应在下次启动时被强制打回默认库。
    {
        namespace fs = std::filesystem;
        fs::path tempRoot = fs::temp_directory_path() / "aura_dev_mode_db_persistence_test";
        std::error_code ec;
        fs::remove_all(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        fs::path dbPath = tempRoot / "external_dev_db" / "main.db";
        fs::create_directories(dbPath.parent_path(), ec);
        std::ofstream(dbPath.string(), std::ios::binary).put('\n');

        const std::string dbPathUtf8 = dbPath.u8string();
        const std::vector<std::string> recentPaths = {
            dbPathUtf8,
            "C:\\ignored\\other.db"
        };
        if (!ShouldKeepDevModeCurrentDbPath(dbPathUtf8, recentPaths)) {
            std::cerr << "FAIL: existing current DB in recent list should be preserved in dev mode\n";
            failed++;
        } else {
            std::cout << "OK: existing current DB in recent list is preserved in dev mode\n";
        }

        const std::vector<std::string> otherRecentPaths = {
            "C:\\ignored\\other.db"
        };
        if (ShouldKeepDevModeCurrentDbPath(dbPathUtf8, otherRecentPaths)) {
            std::cerr << "FAIL: DB outside recent list should not be preserved automatically in dev mode\n";
            failed++;
        } else {
            std::cout << "OK: DB outside recent list is not preserved automatically in dev mode\n";
        }

        fs::remove(dbPath, ec);
        if (ShouldKeepDevModeCurrentDbPath(dbPathUtf8, recentPaths)) {
            std::cerr << "FAIL: missing current DB should not be preserved in dev mode\n";
            failed++;
        } else {
            std::cout << "OK: missing current DB is not preserved in dev mode\n";
        }

        fs::remove_all(tempRoot, ec);
    }

    // AUR-P0-001: 库选择器必须区分 active / missing / disconnected，
    // 避免用户删除 DB 后仍显示为正常可切换库。
    {
        namespace fs = std::filesystem;
        fs::path tempRoot = fs::temp_directory_path() / "auro_library_state_test";
        std::error_code ec;
        fs::remove_all(tempRoot, ec);
        fs::create_directories(tempRoot, ec);
        fs::path activeDb = tempRoot / "Active" / "main.db";
        fs::create_directories(activeDb.parent_path(), ec);
        std::ofstream(activeDb.string(), std::ios::binary).put('\n');

        const std::string activePath = activeDb.u8string();
        const std::string missingPath = (tempRoot / "Missing" / "main.db").u8string();

        if (ClassifyDatabaseLibraryPath(activePath) != DatabaseLibraryState::Active) {
            std::cerr << "FAIL: existing database should be Active\n";
            failed++;
        } else {
            std::cout << "OK: existing database is Active\n";
        }
        if (ClassifyDatabaseLibraryPath(missingPath) != DatabaseLibraryState::Missing) {
            std::cerr << "FAIL: missing database should be Missing\n";
            failed++;
        } else {
            std::cout << "OK: missing database is Missing\n";
        }
        if (ClassifyDatabaseLibraryPath("") != DatabaseLibraryState::Disconnected) {
            std::cerr << "FAIL: empty database path should be Disconnected\n";
            failed++;
        } else {
            std::cout << "OK: empty database path is Disconnected\n";
        }
        if (!IsDatabaseLibrarySelectable(DatabaseLibraryState::Active)) {
            std::cerr << "FAIL: Active library should be selectable\n";
            failed++;
        } else {
            std::cout << "OK: Active library is selectable\n";
        }
        if (IsDatabaseLibrarySelectable(DatabaseLibraryState::Missing)) {
            std::cerr << "FAIL: Missing library should not be selectable\n";
            failed++;
        } else {
            std::cout << "OK: Missing library is not selectable\n";
        }
        std::string missingLabel = BuildDatabaseLibraryDisplayLabelUtf8(missingPath);
        if (missingLabel.find("[Missing]") == std::string::npos) {
            std::cerr << "FAIL: missing label should include [Missing], got " << missingLabel << "\n";
            failed++;
        } else {
            std::cout << "OK: missing label includes [Missing]\n";
        }
        std::string activeLabel = BuildDatabaseLibraryDisplayLabelUtf8(activePath);
        if (activeLabel.find("[Missing]") != std::string::npos ||
            activeLabel.find("[Disconnected]") != std::string::npos) {
            std::cerr << "FAIL: active label should not include missing/disconnected suffix, got " << activeLabel << "\n";
            failed++;
        } else {
            std::cout << "OK: active label has no warning suffix\n";
        }

        fs::remove_all(tempRoot, ec);
    }

    // AUR-P0-001 Slice 2: 恢复动作的列表语义必须可预测。
    {
        std::vector<std::string> recent = {"A.db", "B.db", "C.db"};
        if (!RemoveRecentDatabasePath(recent, "B.db") || recent.size() != 2 ||
            recent[0] != "A.db" || recent[1] != "C.db") {
            std::cerr << "FAIL: removing a recent DB should remove only the matching entry\n";
            failed++;
        } else {
            std::cout << "OK: remove recent DB path\n";
        }
        if (RemoveRecentDatabasePath(recent, "Missing.db")) {
            std::cerr << "FAIL: removing absent recent DB should return false\n";
            failed++;
        } else {
            std::cout << "OK: absent recent DB remove returns false\n";
        }

        recent = {"Old.db", "Existing.db", "Tail.db"};
        if (!ReplaceRecentDatabasePath(recent, "Old.db", "New.db") || recent.size() != 3 ||
            recent[0] != "New.db" || recent[1] != "Existing.db" || recent[2] != "Tail.db") {
            std::cerr << "FAIL: reconnect should replace old path in place\n";
            failed++;
        } else {
            std::cout << "OK: reconnect replaces old path in place\n";
        }

        recent = {"Old.db", "Existing.db", "Tail.db"};
        if (!ReplaceRecentDatabasePath(recent, "Old.db", "Existing.db") || recent.size() != 2 ||
            recent[0] != "Existing.db" || recent[1] != "Tail.db") {
            std::cerr << "FAIL: reconnect to an existing path should deduplicate\n";
            failed++;
        } else {
            std::cout << "OK: reconnect deduplicates existing path\n";
        }
    }

#undef ASSERT_EQ
    if (failed) {
        std::cerr << failed << " assertion(s) failed.\n";
        std::cout << "\nPress Enter to exit...\n";
        std::cin.get();
        return 1;
    }
    std::cout << "All tests passed.\n";
    std::cout << "\nPress Enter to exit...\n";
    std::cin.get();
    return 0;
}
