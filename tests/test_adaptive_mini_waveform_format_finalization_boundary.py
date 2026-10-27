from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/AdaptiveMiniWaveformFormatFinalization.h"
SOURCE_PATH = ROOT / "src/AdaptiveMiniWaveformFormatFinalization.cpp"

assert HEADER_PATH.is_file(), f"missing header: {HEADER_PATH}"
assert SOURCE_PATH.is_file(), f"missing source: {SOURCE_PATH}"

HEADER = HEADER_PATH.read_text(encoding="utf-8")
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "src/WaveformRebuildBatchLifecycle.cpp").read_text(
    encoding="utf-8"
)
RECOVERY = (ROOT / "src/WaveformRebuildCancellationRecovery.cpp").read_text(
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
    "enum class AdaptiveMiniFormatFinalizationReason",
    "struct AdaptiveMiniFormatFinalizationState",
    "struct AdaptiveMiniFormatFinalizationResult",
    "class AdaptiveMiniFormatStampPort",
    "class DatabaseAdaptiveMiniFormatStampPort final",
    "AdaptiveMiniFormatFinalizationResult FinalizeAdaptiveMiniWaveformFormat(",
):
    assert token in HEADER, token

for owned in (
    "AdaptiveMiniFormatFinalizationReason::NoWork",
    "AdaptiveMiniFormatFinalizationReason::RebuildCompleted",
    "AdaptiveMiniFormatFinalizationReason::Cancelled",
    "port.markUpgradeComplete()",
    "database_.markAdaptiveMiniWaveformFormatUpgradeComplete()",
):
    assert owned in SOURCE, owned

start = function_body(
    MAIN, "static void MaybeStartAdaptiveMiniWaveformFormatUpgrade(AppState& state)"
)
pump = function_body(MAIN, "static void PumpWaveformRebuild(AppState& state)")
assert MAIN.count("FinalizeAdaptiveMiniWaveformFormat(") == 1
assert LIFECYCLE.count("FinalizeAdaptiveMiniWaveformFormat(") == 1
assert RECOVERY.count("FinalizeAdaptiveMiniWaveformFormat(") == 1
assert "AdaptiveMiniFormatFinalizationReason::NoWork" in start
assert "AdaptiveMiniFormatFinalizationReason::Cancelled" in RECOVERY
assert "AdaptiveMiniFormatFinalizationReason::RebuildCompleted" in LIFECYCLE
for body in (start, pump):
    assert "state.database.markAdaptiveMiniWaveformFormatUpgradeComplete()" not in body
assert MAIN.count("state.adaptiveMiniWaveformFormatRebuildPending =") >= 3
assert MAIN.count("finalization.state.rebuildPending;") == 2
assert MAIN.count("state.adaptiveMiniWaveformFormatStampPending =") >= 3
assert MAIN.count("finalization.state.stampPending;") == 2

assert CMAKE.count("src/AdaptiveMiniWaveformFormatFinalization.cpp") == 1
assert BEHAVIOR.count("src/AdaptiveMiniWaveformFormatFinalization.cpp") == 3
assert "TestAdaptiveMiniWaveformFormatFinalization" in BEHAVIOR
assert "NAME TestAdaptiveMiniWaveformFormatFinalizationBoundary" in BEHAVIOR

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

print("PASS: adaptive mini waveform format finalization boundary contracts")
