from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/WaveformRebuildBatchSelector.h"
SOURCE_PATH = ROOT / "src/WaveformRebuildBatchSelector.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "src/WaveformRebuildBatchLifecycle.cpp").read_text(
    encoding="utf-8"
)
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


for token in (
    "struct WaveformRebuildBatchRequest",
    "struct WaveformRebuildBatchSelection",
    "class WaveformRebuildBatchSource",
    "class DatabaseWaveformRebuildBatchSource final",
    "WaveformRebuildBatchSelection SelectWaveformRebuildBatch(",
):
    assert token in HEADER, token

for owned in (
    "source.getAllFiles(request.limit, request.offset)",
    "source.usesMiniOnlyCacheMode()",
    "source.getFilesNeedingWaveformCache(request.limit)",
    "source.getFilesNeedingDetailCache(request.limit)",
    "source.getFilesByIds(ids)",
):
    assert owned in SOURCE, owned

for adapter_call in (
    "database_.getAllFiles(limit, offset)",
    "database_.getWaveformCacheMode() == WaveformCachePolicy::Mode::MiniOnly",
    "database_.getFilesNeedingWaveformCache(limit)",
    "database_.getFilesNeedingDetailCache(limit)",
    "database_.getFilesByIds(ids)",
):
    assert adapter_call in SOURCE, adapter_call

pump = function_body(MAIN, "static void PumpWaveformRebuild(AppState& state)")
assert MAIN.count("SelectWaveformRebuildBatch(") == 0
assert LIFECYCLE.count("SelectWaveformRebuildBatch(") == 1
assert "DatabaseWaveformRebuildBatchSource source(state.database);" in pump
assert "LoadWaveformRebuildBatchLifecycle(" in pump
assert "state.waveformRebuildBatch = std::move(lifecycle.records);" in pump
assert "lifecycle.batchMiniOnly" in pump
for removed in (
    "state.database.getAllFiles(",
    "state.database.getWaveformCacheMode()",
    "state.database.getFilesNeedingWaveformCache(",
    "state.database.getFilesNeedingDetailCache(",
    "state.database.getFilesByIds(",
):
    assert removed not in pump, removed

assert CMAKE.count("src/WaveformRebuildBatchSelector.cpp") == 1
assert BEHAVIOR.count("src/WaveformRebuildBatchSelector.cpp") == 2
assert "NAME TestWaveformRebuildBatchSelectorBoundary" in BEHAVIOR
assert "TestWaveformRebuildBatchSelector" in BEHAVIOR

for forbidden in (
    "AppState.h",
    "AsyncWorker.h",
    "DiagnosticsLogger",
    "ModuleUI.h",
    "Platform::",
    "glDelete",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

print("PASS: waveform rebuild batch selector boundary contracts")
