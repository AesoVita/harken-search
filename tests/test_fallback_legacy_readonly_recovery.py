from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"


def test_database_switch_failure_recovery_preserves_fallback_read_only_mode() -> None:
    source = MAIN.read_text(encoding="utf-8")
    recovery = source.split("void PumpDatabaseSwitchIfPending(AppState& state)", 1)[1].split("// 快速切换数据库", 1)[0]

    assert "const auto dataLayout = Platform::HarkenUserData::EnsureLayout();" in recovery
    assert "dataLayout.readOnly" in recovery
    assert "state.database.openReadOnly(oldPath)" in recovery
    read_only_pos = recovery.index("if (dataLayout.readOnly)")
    read_only_open_pos = recovery.index("state.database.openReadOnly(oldPath)")
    writable_open_pos = recovery.index("state.database.open(oldPath, true)")
    assert read_only_pos < read_only_open_pos < writable_open_pos
