#include "BrowserSystemFolderSearchRuntime.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

bool WaitForReady(
    aura::browser::SystemFolderSearchRuntime& runtime,
    aura::browser::SystemFolderSearchSnapshot& snapshot) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (runtime.Poll(snapshot) && snapshot.ready) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

void Touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << "fixture";
}

} // namespace

int main() {
    using aura::browser::SystemFolderSearchRoot;
    using aura::browser::SystemFolderSearchRuntime;
    using aura::browser::SystemFolderSearchSnapshot;

    if (!Check(
            aura::browser::kSystemFolderSearchPublishInterval.count() == 120,
            "progressive publication interval should remain 120 ms")) return 1;

    const fs::path root = fs::temp_directory_path() /
        ("aris-browser-runtime-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "HitFolder");
    fs::create_directories(root / "miss");
    Touch(root / "ParentFromFile" / "hit-one.wav");
    Touch(root / "ParentFromFile" / "hit-two.wav");
    Touch(root / "SecondParent" / "second.wav");
    Touch(root / "miss" / "other.wav");

    SystemFolderSearchRuntime runtime;
    const std::vector<SystemFolderSearchRoot> roots{{root.u8string(), "Fixture"}};
    const uint64_t firstGeneration = runtime.Start("hit", roots);
    if (!Check(firstGeneration > 0 && runtime.Busy(),
               "start should create a busy generation")) return 1;

    SystemFolderSearchSnapshot first;
    if (!Check(WaitForReady(runtime, first),
               "first search should publish a ready snapshot")) return 1;
    if (!Check(first.generation == firstGeneration && !runtime.Busy(),
               "ready polling should finish the active generation")) return 1;
    if (!Check(first.visited >= 6,
               "recursive PAL traversal should account for visited entries")) return 1;
    if (!Check(first.results.size() == 2,
               "folder and duplicate file-parent hits should collapse to two folders")) return 1;
    if (!Check(
            first.results[0].displayName == "HitFolder" &&
                first.results[1].displayName == "ParentFromFile",
            "final hits should retain deterministic case-insensitive name ordering")) return 1;
    if (!Check(
            !first.results[0].isFile && !first.results[1].isFile &&
                first.results[1].path == (root / "ParentFromFile").u8string(),
            "matching files should publish their parent as a folder-only hit")) return 1;

    const uint64_t cancelledGeneration = runtime.Start("hit", roots);
    const uint64_t secondGeneration = runtime.Start("second", roots);
    if (!Check(secondGeneration > cancelledGeneration,
               "restart should advance the generation")) return 1;
    SystemFolderSearchSnapshot second;
    if (!Check(WaitForReady(runtime, second),
               "replacement search should publish a ready snapshot")) return 1;
    if (!Check(
            second.generation == secondGeneration &&
                second.results.size() == 1 &&
                second.results.front().displayName == "SecondParent",
            "polling should expose only the replacement generation")) return 1;

    runtime.Cancel();
    if (!Check(!runtime.Busy(), "cancel should clear busy state without blocking")) return 1;

    fs::remove_all(root);
    std::cout << "PASS: browser system folder search runtime contracts\n";
    return 0;
}
