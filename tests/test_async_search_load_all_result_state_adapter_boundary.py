from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchLoadAllResultStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchLoadAllResultStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_load_all_result_state_adapter_owns_view_projection() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "class AsyncSearchLoadAllResultStateAdapter final" in header
    assert "bool acceptsResult() const;" in header
    assert "void observeResult(" in header
    assert "runtime_.acceptsResult" in source
    assert "runtime_.onResult" in source
    assert "state_.viewMode" in source
    body = main
    assert "g_loadAllBatchRuntime.acceptsResult" not in body
    assert "g_loadAllBatchRuntime.onResult" not in body


def test_load_all_result_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_apply_root_only_composes_load_all_result_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AsyncSearchLoadAllResultStateAdapter" in body
    assert "loadAllResultAdapter.acceptsResult()" in body
    assert "loadAllResultAdapter.observeResult(" in body
