from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/DatabaseSessionMediaCacheReset.h"
SOURCE_PATH = ROOT / "src/DatabaseSessionMediaCacheReset.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


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
assert "void ResetDatabaseSessionMediaCaches(AppState& state);" in HEADER
assert '#include "AppState.h"' in SOURCE
assert MAIN.count(
    "aura::database_session::ResetDatabaseSessionMediaCaches(state);"
) == 1
assert CMAKE.count("src/DatabaseSessionMediaCacheReset.cpp") == 1
assert BEHAVIOR.count("src/DatabaseSessionMediaCacheReset.cpp") == 1
assert "NAME TestDatabaseSessionMediaCacheResetBoundary" in BEHAVIOR

for forbidden in (
    "Database.h",
    "AsyncWorker.h",
    "CoverModule.h",
    "glDeleteTextures",
    "DeleteMiniWaveTexture",
    "Platform::",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER
    assert forbidden not in SOURCE

reset_body = function_body(MAIN, "void ResetDatabaseState(AppState& state)")
for owned in (
    "state.coverWallDatabaseInvalidationRequested = true",
    "state.invalidateCoverWallAfterDatabaseContentChange()",
    "state.preloadedCoverWallEntries.clear()",
    "state.preloadedCoverWallFullyDiscovered = false",
    "state.coverWall.reset()",
    "state.waveformCache.clear()",
    "state.clearPendingWaveforms()",
    "state.waveformFailed.clear()",
    "state.visibleFilePaths.clear()",
):
    assert owned in SOURCE, owned
    assert owned not in reset_body, owned

for retained in (
    "aura::database_session::ResetDatabaseSessionViewState(state)",
    "aura::database_session::ResetDatabaseSessionScanState(state)",
    "glDeleteTextures",
    "state.spectrogramFilePath.clear()",
    "g_miniWaveTextureCache.clear()",
    "StartDeferredSidebarBootstrap(state, true, true)",
):
    assert retained in reset_body, retained

print("PASS: database session media cache reset boundary contracts")
