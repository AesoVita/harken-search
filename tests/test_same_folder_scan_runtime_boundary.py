from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "SameFolderScanRuntime.h"
SOURCE = ROOT / "src" / "SameFolderScanRuntime.cpp"
MAIN = ROOT / "src" / "main.cpp"
CMAKE = ROOT / "CMakeLists.txt"
BEHAVIOR_CMAKE = ROOT / "cmake" / "harken_search_behavior_tests.cmake"
RULES = ROOT / "docs" / "architecture" / "stage-4-dependency-rules.json"
STAGE6 = ROOT / "tests" / "test_stage6_platform_boundary.py"
STAGE7 = ROOT / "tools" / "stage7_api_contract_audit.py"


def require(text: str, needle: str, source: str) -> None:
    assert needle in text, f"missing {needle!r} in {source}"


def reject(text: str, needle: str, source: str) -> None:
    assert needle not in text, f"forbidden {needle!r} remains in {source}"


def main() -> None:
    assert HEADER.exists(), f"missing production header: {HEADER}"
    assert SOURCE.exists(), f"missing production source: {SOURCE}"

    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    switch_adapter = (
        ROOT / "src" / "DatabaseSwitchBlockingOperationAdapter.cpp"
    ).read_text(encoding="utf-8")
    root = MAIN.read_text(encoding="utf-8")
    cmake = CMAKE.read_text(encoding="utf-8")
    behavior_cmake = BEHAVIOR_CMAKE.read_text(encoding="utf-8")
    rules = RULES.read_text(encoding="utf-8")
    stage6 = STAGE6.read_text(encoding="utf-8")
    stage7 = STAGE7.read_text(encoding="utf-8")

    for needle in (
        "namespace aura::same_folder",
        "struct SameFolderScanRequest",
        "uint64_t generation = 0;",
        "std::string folderPathUtf8;",
        "std::string missingDirectoryErrorUtf8;",
        "struct SameFolderScanChunk",
        "std::vector<std::string> filePaths;",
        "bool done = false;",
        "std::string error;",
        "class SameFolderScanRuntime final",
        "SameFolderScanRuntime(const SameFolderScanRuntime&) = delete;",
        "void submitLatest(const SameFolderScanRequest& request);",
        "bool tryPopChunk(SameFolderScanChunk& chunk);",
        "void stop();",
        "std::unique_ptr<Impl> impl_;",
    ):
        require(header, needle, HEADER.name)

    for needle in (
        "std::thread",
        "std::atomic<bool>",
        "std::atomic<uint64_t>",
        "std::condition_variable",
        "std::optional<SameFolderScanRequest>",
        "std::deque<SameFolderScanChunk>",
        "compare_exchange_strong",
        "pendingRequest.reset()",
        "pendingRequest = request",
        "memory_order_release",
        "memory_order_acquire",
        "Platform::FileSystem::ProbePath",
        "Platform::FileSystem::PathStatus::Present",
        "Platform::FileSystem::IsDirectory",
        "Platform::FileSystem::DirectoryIterator",
        "entry.isRegularFile",
        '".wav", ".wave", ".mp3", ".flac", ".ogg", ".aiff", ".aif", ".m4a", ".wma"',
        "chunk.filePaths.size() >= 128",
        "isCanceled(request.generation)",
        "request.missingDirectoryErrorUtf8",
        "notify_all",
        "notify_one",
        "joinable()",
        "join()",
    ):
        require(source, needle, SOURCE.name)

    for forbidden in (
        "AppState",
        "Database",
        "WindowsMain",
        "ImGui",
        "glDelete",
        "Localization",
        "filesystem_std.cpp",
        "filesystem_win32.cpp",
    ):
        reject(header + source, forbidden, "runtime files")

    require(root, '#include "SameFolderScanRuntime.h"', MAIN.name)
    require(root, "aura::same_folder::SameFolderScanRuntime g_sameFolderScanner", MAIN.name)
    require(root, "g_sameFolderScanner,", MAIN.name)
    reject(root, "aura::same_folder::SameFolderScanRequest req;", MAIN.name)
    reject(root, "g_sameFolderScanner.submitLatest(req);", MAIN.name)
    reject(root, "g_sameFolderScanner.tryPopChunk(chunk)", MAIN.name)
    require(switch_adapter, "sameFolderScanner_.stop();", "switch adapter")
    reject(root, "class SameFolderAsyncScanner", MAIN.name)
    reject(root, "struct SameFolderScanRequest", MAIN.name)
    reject(root, "struct SameFolderScanChunk", MAIN.name)
    reject(root, "static void StartSameFolderScan(", MAIN.name)
    reject(root, "static void PumpSameFolderScan(", MAIN.name)

    require(cmake, "src/SameFolderScanRuntime.cpp", CMAKE.name)
    require(behavior_cmake, "TestSameFolderScanRuntime", BEHAVIOR_CMAKE.name)
    require(behavior_cmake, "TestSameFolderScanRuntimeBoundary", BEHAVIOR_CMAKE.name)
    require(rules, '"src/SameFolderScanRuntime.cpp"', RULES.name)
    require(rules, '"src/SameFolderScanRuntime.h"', RULES.name)
    require(stage6, "test_same_folder_scan_runtime_uses_path_probe_pal", STAGE6.name)
    require(stage7, '"src/SameFolderScanRuntime.h"', STAGE7.name)

    print("PASS: same folder scan runtime boundary contracts")


if __name__ == "__main__":
    main()
