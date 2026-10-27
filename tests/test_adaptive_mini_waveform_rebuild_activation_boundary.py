from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER_PATH = ROOT / "src/AdaptiveMiniWaveformRebuildActivation.h"
SOURCE_PATH = ROOT / "src/AdaptiveMiniWaveformRebuildActivation.cpp"

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


for token in (
    "struct AdaptiveMiniWaveformRebuildActivationState",
    "struct AdaptiveMiniWaveformRebuildActivationResult",
    "ActivateAdaptiveMiniWaveformRebuild(",
):
    assert token in HEADER, token

for owned in (
    "if (!startSucceeded)",
    "state.waveformRebuildMiniOnlyMode = true",
    "state.incrementalMiniOnly = true",
    "state.rebuildPending = false",
    "state.stampPending = true",
):
    assert owned in SOURCE, owned

body = function_body(
    MAIN, "static void MaybeStartAdaptiveMiniWaveformFormatUpgrade(AppState& state)"
)
assert MAIN.count("ActivateAdaptiveMiniWaveformRebuild(") == 1
assert "const bool startSucceeded = StartWaveformRebuild(" in body
assert "if (activation.activated)" in body
for removed in (
    "state.waveformRebuildMiniOnlyMode = true",
    "g_waveformRebuildRuntimeState.setIncrementalMiniOnly(true)",
    "state.adaptiveMiniWaveformFormatRebuildPending = false",
    "state.adaptiveMiniWaveformFormatStampPending = true",
):
    assert removed not in body, removed
for applied in (
    "activation.state.waveformRebuildMiniOnlyMode",
    "activation.state.incrementalMiniOnly",
    "activation.state.rebuildPending",
    "activation.state.stampPending",
):
    assert applied in body, applied
start_pos = body.index("const bool startSucceeded = StartWaveformRebuild(")
activation_pos = body.index("ActivateAdaptiveMiniWaveformRebuild(")
diag_pos = body.index("Adaptive mini format upgrade rebuild started:")
assert start_pos < activation_pos < diag_pos

assert CMAKE.count("src/AdaptiveMiniWaveformRebuildActivation.cpp") == 1
assert BEHAVIOR.count("src/AdaptiveMiniWaveformRebuildActivation.cpp") == 1
assert "TestAdaptiveMiniWaveformRebuildActivation" in BEHAVIOR
assert "NAME TestAdaptiveMiniWaveformRebuildActivationBoundary" in BEHAVIOR

for forbidden in (
    "AppState.h",
    "AsyncWorker.h",
    "Database.h",
    "DiagnosticsLogger",
    "ModuleUI.h",
    "Platform::",
    "WaveformRebuildRuntimeState.h",
    "glDelete",
    "main.cpp",
    "WindowsMain",
):
    assert forbidden not in HEADER, forbidden
    assert forbidden not in SOURCE, forbidden

print("PASS: adaptive mini waveform rebuild activation boundary contracts")
