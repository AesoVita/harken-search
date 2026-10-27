from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/BrowserSystemFolderSearchRuntime.h"
SOURCE_PATH = ROOT / "src/BrowserSystemFolderSearchRuntime.cpp"

assert HEADER_PATH.is_file()
assert SOURCE_PATH.is_file()

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
RULES = (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
    encoding="utf-8"
)
STAGE6 = (ROOT / "tests/test_stage6_platform_boundary.py").read_text(
    encoding="utf-8"
)
STAGE7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)

for required in (
    "struct SystemFolderSearchRoot",
    "struct SystemFolderSearchSnapshot",
    "class SystemFolderSearchRuntime",
    "kSystemFolderSearchPublishInterval{120}",
    "uint64_t Start(",
    "void Cancel();",
    "bool Poll(SystemFolderSearchSnapshot& snapshot);",
    "size_t VisitedCount() const;",
    "bool Busy() const;",
):
    assert required in HEADER

for forbidden in (
    "AppState",
    "ImGui",
    "Localization",
    "Database",
    "AudioEngine",
    "WindowsMain",
    "main.cpp",
    "windows.h",
    "filesystem_win32",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

assert '#include "platform/filesystem.h"' in SOURCE
assert '#include "BrowserSystemFolderSearchPolicy.h"' in HEADER
assert "Platform::FileSystem::DirectoryIterator" in SOURCE
assert "std::thread(" in SOURCE and "}).detach();" in SOURCE
assert "std::memory_order_release" in SOURCE
assert "std::memory_order_acquire" in SOURCE
assert "std::memory_order_acq_rel" in SOURCE
assert "kSystemFolderSearchPublishInterval" in SOURCE

for removed in (
    "struct BrowserSystemFolderSearchTask",
    "g_browserSystemFolderSearchCancel",
    "g_browserSystemFolderSearchTask",
    "g_browserSystemFolderSearchGeneration",
    "g_browserSystemFolderSearchAppliedGeneration",
    "std::thread([query, roots, cancel, task]",
):
    assert removed not in MAIN

for retained in (
    "g_browserSystemFolderSearchSubmittedQuery",
    "g_browserSystemFolderSearchSubmittedFocusPath",
    "BrowserSystemFolderSearchRoots(",
    "g_browserSystemFolderSearchNeedsFirstHitReveal",
    "g_browserSystemFolderTreeScrollTarget",
    "RenderBrowserSystemFolderSearchTree",
):
    assert retained in MAIN

assert '#include "BrowserSystemFolderSearchRuntime.h"' in MAIN
assert "aura::browser::SystemFolderSearchRuntime" in MAIN
assert ".Start(query," in MAIN
assert ".Poll(snapshot)" in MAIN
assert ".Cancel()" in MAIN
assert CMAKE.count("src/BrowserSystemFolderSearchRuntime.cpp") == 1
assert "NAME TestBrowserSystemFolderSearchRuntime" in BEHAVIOR
assert "NAME TestBrowserSystemFolderSearchRuntimeBoundary" in BEHAVIOR
assert '"domain_service"' in RULES
assert '"src/BrowserSystemFolderSearchRuntime.cpp"' in RULES
assert "test_browser_system_folder_search_runtime_owns_async_session" in STAGE6
assert (
    '"src/BrowserSystemFolderSearchRuntime.h": '
    '"tests/test_browser_system_folder_search_runtime_boundary.py"'
    in STAGE7
)
