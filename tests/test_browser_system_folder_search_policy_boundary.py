from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/BrowserSystemFolderSearchPolicy.h"
SOURCE_PATH = ROOT / "src/BrowserSystemFolderSearchPolicy.cpp"

assert HEADER_PATH.is_file()
assert SOURCE_PATH.is_file()

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
RUNTIME = (ROOT / "src/BrowserSystemFolderSearchRuntime.cpp").read_text(
    encoding="utf-8"
)
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
    "struct SystemFolderSearchHit",
    "struct SystemFolderSearchTreeNode",
    "bool MatchesSystemFolderSearchQuery(",
    "bool IsSystemFolderAncestorOf(",
    "BuildSystemFolderPathChain(",
    "BuildSystemFolderSearchTree(",
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
    "Platform::",
    "platform/",
    "windows.h",
    "std::thread",
    "std::mutex",
    "std::atomic",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

for removed in (
    "struct BrowserSystemFolderSearchHit",
    "struct BrowserSystemFolderSearchTreeNode",
    "static bool BrowserSystemFolderMatchesQuery(",
    "static bool IsFolderAncestorOf(",
    "static std::vector<std::pair<std::string, std::string>> BuildBrowserSystemFolderPathChain(",
    "static std::map<std::string, BrowserSystemFolderSearchTreeNode> BuildBrowserSystemFolderSearchTree(",
):
    assert removed not in MAIN

assert "MatchesSystemFolderSearchQuery(" in RUNTIME

for qualified_call in (
    "aura::browser::IsSystemFolderAncestorOf(",
    "aura::browser::BuildSystemFolderPathChain(",
    "aura::browser::BuildSystemFolderSearchTree(",
):
    assert qualified_call in MAIN

assert '#include "BrowserSystemFolderSearchPolicy.h"' in MAIN
assert "std::vector<aura::browser::SystemFolderSearchHit>" in MAIN
assert "const aura::browser::SystemFolderSearchTreeNode& node" in MAIN
assert CMAKE.count("src/BrowserSystemFolderSearchPolicy.cpp") == 1
assert "NAME TestBrowserSystemFolderSearchPolicy" in BEHAVIOR
assert "NAME TestBrowserSystemFolderSearchPolicyBoundary" in BEHAVIOR
assert "tests/test_browser_system_folder_search_policy_boundary.py" in BEHAVIOR
assert '"src/BrowserSystemFolderSearchPolicy.cpp"' in RULES
assert '"src/BrowserSystemFolderSearchPolicy.h"' in RULES
assert "test_browser_system_folder_search_policy_is_platform_independent" in STAGE6
assert (
    '"src/BrowserSystemFolderSearchPolicy.h": '
    '"tests/test_browser_system_folder_search_policy_boundary.py"'
    in STAGE7
)
