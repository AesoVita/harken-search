from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/ScanWaveformBackfillDrainWorkflow.h"
SOURCE = ROOT / "src/ScanWaveformBackfillDrainWorkflow.cpp"


def required(path: Path, label: str) -> str:
    assert path.exists(), f"missing {label}: {path.relative_to(ROOT)}"
    return path.read_text(encoding="utf-8")


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


def test_backfill_drain_files_and_registration_exist():
    required(HEADER, "header")
    required(SOURCE, "source")
    product = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    assert product.count("src/ScanWaveformBackfillDrainWorkflow.cpp") == 1
    assert "TestScanWaveformBackfillDrainWorkflow" in behavior
    assert "tests/test_scan_waveform_backfill_drain_workflow.cpp" in behavior
    assert "NAME TestScanWaveformBackfillDrainWorkflowBoundary" in behavior


def test_backfill_drain_owns_the_complete_state_machine():
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    pump = function_body(main, "static void PumpScanWaveformBackfillQueue(AppState& state)")

    assert pump.index("if (!state.database.isOpen()) return") < pump.index(
        "if (state.waveformRebuildActive) return"
    ) < pump.index("foregroundCompletionActive")
    assert "AURA_ENABLE_WAVEFORM_BACKFILL_DRAIN" in pump
    assert main.count("class AppScanWaveformBackfillDrainPorts") == 1
    assert pump.count("DrainScanWaveformBackfillQueue(") == 1
    for removed in (
        "backfillQueue.snapshot()",
        "backfillQueue.tryDequeueKind(",
        "database.getFilesByIds(",
        "backfillQueue.reportOutcome(",
        "backfillQueue.itemState(",
        "waveformWorker.submitBackfillDetailRequest(",
        "database.markScanForegroundMiniTerminal(",
        "database.clearBackfillPending(",
    ):
        assert removed not in pump, removed


def test_backfill_drain_has_one_way_dependencies():
    header = required(HEADER, "header")
    source = required(SOURCE, "source")
    combined = header + "\n" + source
    assert "BackfillQueue.h" in header
    for forbidden in (
        "AppState",
        "Database",
        "AsyncWorker",
        "WaveformWorker",
        "DiagnosticsLogger",
        "DIAG_",
        "ModuleUI",
        "UIRenderer",
        "platform/",
        "main.cpp",
        "WindowsMain",
    ):
        assert forbidden not in combined, forbidden


if __name__ == "__main__":
    test_backfill_drain_files_and_registration_exist()
    test_backfill_drain_owns_the_complete_state_machine()
    test_backfill_drain_has_one_way_dependencies()
    print("PASS: scan waveform backfill drain workflow boundary contracts")
