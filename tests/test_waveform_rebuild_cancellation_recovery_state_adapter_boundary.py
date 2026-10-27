from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/WaveformRebuildCancellationRecoveryStateAdapter.h"
SOURCE = ROOT / "src/WaveformRebuildCancellationRecoveryStateAdapter.cpp"
MAIN = ROOT / "src/main.cpp"


def test_cancellation_recovery_state_adapter_owns_concrete_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "class WaveformRebuildCancellationRecoveryStateAdapter final" in header
    assert "recover(" in header
    assert "RecoverCancelledWaveformRebuild(" in source
    assert "state_.waveformWorker.stop();" in source
    assert "runtimeState_.Reset(state_, false);" in source
    assert "AppWaveformRebuildCancellationRecoveryEffects" not in main


def test_cancellation_recovery_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_pump_only_composes_cancellation_recovery_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    pump = source.split("static void PumpWaveformRebuild", 1)[1].split(
        "static void", 1
    )[0]
    assert "WaveformRebuildCancellationRecoveryStateAdapter" in pump
    assert "recoveryAdapter.recover(" in pump
    assert "RecoverCancelledWaveformRebuild(" not in pump
    assert "AppWaveformRebuildCancellationRecoveryEffects" not in pump
