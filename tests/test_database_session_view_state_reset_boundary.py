from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/DatabaseSessionViewStateReset.h"
SOURCE_PATH = ROOT / "src/DatabaseSessionViewStateReset.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

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


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"unterminated function: {signature}")


assert "struct AppState;" in HEADER
assert "namespace aura::database_session" in HEADER
assert "void ResetDatabaseSessionViewState(AppState& state);" in HEADER
assert '#include "AppState.h"' in SOURCE
assert MAIN.count("aura::database_session::ResetDatabaseSessionViewState(state);") == 1
assert CMAKE.count("src/DatabaseSessionViewStateReset.cpp") == 1
assert BEHAVIOR.count("src/DatabaseSessionViewStateReset.cpp") == 1
assert "NAME TestDatabaseSessionViewStateResetBoundary" in BEHAVIOR
assert RULES.count('"src/DatabaseSessionViewStateReset.h"') == 2
assert RULES.count('"src/DatabaseSessionViewStateReset.cpp"') == 2
assert "test_database_session_view_state_reset_owns_pure_ui_state" in STAGE6
assert (
    '"src/DatabaseSessionViewStateReset.h": '
    '"tests/test_database_session_view_state_reset_boundary.py"'
    in STAGE7
)

for forbidden in (
    "Database.h",
    "AsyncWorker.h",
    "CoverModule.h",
    "glDeleteTextures",
    "SearchWorker",
    "WaveformWorker",
    "BackfillWorkerPool",
    "Platform::",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

reset_body = function_body(MAIN, "void ResetDatabaseState(AppState& state)")
for moved in (
    "state.searchResults.clear()",
    "state.clearFolderHierarchyState()",
    "state.playlists.clear()",
    "state.listTabs",
    "state.sameFolderRecycleBuffer.clear()",
):
    assert moved not in reset_body, moved

for retained in (
    "state.searchWorker.stop()",
    "aura::database_session::ResetDatabaseSessionScanState(state)",
    "aura::database_session::ResetDatabaseSessionMediaCaches(state)",
    "glDeleteTextures",
    "StartDeferredSidebarBootstrap(state, true, true)",
):
    assert retained in reset_body, retained

print("PASS: database session view state reset boundary contracts")
