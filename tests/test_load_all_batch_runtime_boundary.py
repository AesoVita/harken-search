from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/LoadAllBatchRuntime.h"
SOURCE = ROOT / "src/LoadAllBatchRuntime.cpp"


def test_load_all_batch_runtime_owns_feature_state_machine() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    for token in (
        "class LoadAllBatchRuntime",
        "begin(",
        "onResult(",
        "onFrame(",
    ):
        assert token in header
    for forbidden in (
        "AppState",
        "Database",
        "ImGui",
        "main.cpp",
        "WindowsMain",
        "platform/",
    ):
        assert forbidden not in header
        assert forbidden not in source
    assert "struct LoadAllJob" not in main
    assert "static LoadAllJob g_loadAllJob" not in main
    assert "aura::load_all::LoadAllBatchRuntime" in main
