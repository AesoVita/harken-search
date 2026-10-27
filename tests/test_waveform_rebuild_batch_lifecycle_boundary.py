from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/WaveformRebuildBatchLifecycle.h"
SOURCE = ROOT / "src/WaveformRebuildBatchLifecycle.cpp"


def required(path: Path, label: str) -> str:
    assert path.exists(), f"missing {label}: {path.relative_to(ROOT)}"
    return path.read_text(encoding="utf-8")


def pump_body() -> str:
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    match = re.search(
        r"static void PumpWaveformRebuild\(AppState& state\)\s*\{(.*?)\n\}",
        main,
        re.S,
    )
    assert match
    return match.group(1)


def test_batch_lifecycle_files_and_registration_exist():
    required(HEADER, "header")
    required(SOURCE, "source")
    product = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    assert "src/WaveformRebuildBatchLifecycle.cpp" in product
    assert "TestWaveformRebuildBatchLifecycle" in behavior
    assert "tests/test_waveform_rebuild_batch_lifecycle.cpp" in behavior


def test_batch_lifecycle_owns_load_and_completion_transaction():
    body = pump_body()
    assert body.count("LoadWaveformRebuildBatchLifecycle(") == 1
    for applied in (
        "state.waveformRebuildBatch = std::move(lifecycle.records)",
        "g_waveformRebuildRuntimeState.setBatchMiniOnly(",
        "lifecycle.batchMiniOnly)",
        "state.waveformRebuildBatchIdx = lifecycle.batchIndex",
        "lifecycle.finalization.state.rebuildPending",
        "lifecycle.finalization.state.stampPending",
    ):
        assert applied in body
    load_branch = body.split("WaveformRebuildBatchGateDecision::LoadBatch", 1)[1]
    for removed in (
        "waveformRebuildQueuedIds.clear()",
        "SelectWaveformRebuildBatch(",
        "FinalizeAdaptiveMiniWaveformFormat(",
    ):
        assert removed not in load_branch
    assert load_branch.index("lifecycle.finalization.stampAttempted") < load_branch.index(
        'DIAG_SYSTEM_FMT("Database"'
    ) < load_branch.index("g_waveformRebuildRuntimeState.Reset(state, false)")


def test_batch_lifecycle_has_one_way_dependencies():
    header = required(HEADER, "header")
    source = required(SOURCE, "source")
    combined = header + "\n" + source
    assert "WaveformRebuildBatchSelector.h" in header
    assert "AdaptiveMiniWaveformFormatFinalization.h" in header
    for forbidden in (
        "AppState",
        "main.cpp",
        "WindowsMain",
        "platform/",
        "DiagnosticsLogger",
        "DIAG_",
        "WaveformRebuildRuntimeState",
        "ModuleUI",
        "UIRenderer",
    ):
        assert forbidden not in combined
    assert not re.search(r"\bDatabase\s*[&*]", combined)
    assert not re.search(r"\bWaveformWorker\s*[&*]", combined)
