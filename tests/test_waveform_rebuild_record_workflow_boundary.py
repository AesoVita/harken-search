from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "WaveformRebuildRecordWorkflow.h"
SOURCE = ROOT / "src" / "WaveformRebuildRecordWorkflow.cpp"
MAIN = ROOT / "src" / "main.cpp"


def _read_required(path: Path, label: str) -> str:
    assert path.exists(), f"missing {label}: {path.relative_to(ROOT)}"
    return path.read_text(encoding="utf-8")


def _pump_body() -> str:
    main = MAIN.read_text(encoding="utf-8")
    match = re.search(
        r"static void PumpWaveformRebuild\(AppState& state\)\s*\{(.*?)\n\}",
        main,
        re.S,
    )
    assert match, "PumpWaveformRebuild body missing"
    return match.group(1)


def test_record_workflow_files_and_registration_exist():
    _read_required(HEADER, "header")
    _read_required(SOURCE, "source")
    assert (ROOT / "tests" / "test_waveform_rebuild_record_workflow.cpp").exists()

    product = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    assert "src/WaveformRebuildRecordWorkflow.cpp" in product
    assert "TestWaveformRebuildRecordWorkflow" in behavior
    assert "tests/test_waveform_rebuild_record_workflow.cpp" in behavior


def test_record_workflow_owns_the_complete_per_record_transition():
    body = _pump_body()
    assert body.count("ProcessNextWaveformRebuildRecord(") == 1

    for assignment in (
        "state.waveformRebuildBatchIdx = recordResult.state.batchIndex",
        "state.waveformRebuildOffset = recordResult.state.offset",
        "state.waveformRebuildProgress = recordResult.state.progress",
        "state.waveformRebuildFailed = recordResult.state.failed",
        "state.waveformRebuildSkipped = recordResult.state.skipped",
        "pending = recordResult.state.pending",
    ):
        assert assignment in body

    loop = re.search(
        r"for \(int n = 0; n < WAVEFORM_REBUILD_PER_FRAME.*?\n\s*\}",
        body,
        re.S,
    )
    assert loop, "bounded waveform rebuild loop missing"
    loop_body = loop.group(0)
    assert "WAVEFORM_REBUILD_QUEUE_THRESHOLD" in loop_body
    for forbidden in (
        "waveformRebuildQueuedIds.count(",
        "waveformRebuildQueuedIds.insert(",
        "PersistPreviousWaveformRebuildFailure(",
        "SubmitWaveformRebuildRequest(",
        "waveformRebuildProgress++",
        "waveformRebuildFailed++",
        "waveformRebuildSkipped++",
        "pending++",
    ):
        assert forbidden not in loop_body


def test_record_workflow_has_one_way_dependencies():
    header = _read_required(HEADER, "header")
    source = _read_required(SOURCE, "source")
    combined = header + "\n" + source

    assert "WaveformRebuildFailurePersistence.h" in header
    assert "WaveformRebuildRequestSubmission.h" in header
    assert "#include <set>" in header
    assert "unordered_set" not in header
    for forbidden in (
        "AppState",
        "main.cpp",
        "WindowsMain",
        "platform/",
        "ModuleUI",
        "UIRenderer",
        "DiagnosticsLogger",
        "DIAG_",
    ):
        assert forbidden not in combined

    assert not re.search(r"\bDatabase\s*[&*]", combined)
    assert not re.search(r"\bWaveformWorker\s*[&*]", combined)
