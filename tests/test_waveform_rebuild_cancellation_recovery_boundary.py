from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/WaveformRebuildCancellationRecovery.h"
SOURCE = ROOT / "src/WaveformRebuildCancellationRecovery.cpp"


def required(path: Path, label: str) -> str:
    assert path.exists(), f"missing {label}: {path.relative_to(ROOT)}"
    return path.read_text(encoding="utf-8")


def braced_block(text: str, marker: str) -> str:
    marker_at = text.index(marker)
    open_at = text.index("{", marker_at)
    depth = 0
    for index in range(open_at, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[open_at + 1 : index]
    raise AssertionError(f"unterminated block after {marker}")


def test_cancellation_recovery_files_and_registration_exist():
    required(HEADER, "header")
    required(SOURCE, "source")
    product = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(
        encoding="utf-8"
    )
    assert product.count("src/WaveformRebuildCancellationRecovery.cpp") == 1
    assert "TestWaveformRebuildCancellationRecovery" in behavior
    assert "tests/test_waveform_rebuild_cancellation_recovery.cpp" in behavior
    assert "NAME TestWaveformRebuildCancellationRecoveryBoundary" in behavior


def test_cancellation_recovery_owns_the_ordered_cancel_transaction():
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    adapter = required(
        ROOT / "src/WaveformRebuildCancellationRecoveryStateAdapter.cpp",
        "state adapter source",
    )
    pump = braced_block(main, "static void PumpWaveformRebuild(AppState& state)")
    cancel = braced_block(pump, "if (state.waveformRebuildCancel)")

    assert pump.index("if (!state.waveformRebuildActive) return") < pump.index(
        "if (state.waveformRebuildCancel)"
    )
    assert "AppWaveformRebuildCancellationRecoveryEffects" not in main
    assert adapter.count("RecoverCancelledWaveformRebuild(") == 1
    assert cancel.count("recoveryAdapter.recover(") == 1
    assert "RecoverCancelledWaveformRebuild(" not in cancel
    assert cancel.rstrip().endswith("return;")
    for removed in (
        "FinalizeAdaptiveMiniWaveformFormat(",
        "state.adaptiveMiniWaveformFormatRebuildPending =",
        "state.adaptiveMiniWaveformFormatStampPending =",
        "RestartWaveformWorkerForForegroundRebuild(state)",
        "g_waveformRebuildRuntimeState.Reset(state, false)",
    ):
        assert removed not in cancel, removed


def test_cancellation_recovery_has_one_way_dependencies():
    header = required(HEADER, "header")
    source = required(SOURCE, "source")
    combined = header + "\n" + source
    assert "AdaptiveMiniWaveformFormatFinalization.h" in header
    for forbidden in (
        "AppState",
        "Database",
        "AsyncWorker",
        "WaveformWorker",
        "WaveformRebuildRuntimeState",
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
    test_cancellation_recovery_files_and_registration_exist()
    test_cancellation_recovery_owns_the_ordered_cancel_transaction()
    test_cancellation_recovery_has_one_way_dependencies()
    print("PASS: waveform rebuild cancellation recovery boundary contracts")
