from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchAppendResultStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchAppendResultStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_append_state_adapter_owns_result_mutation_and_effect_bridge() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchAppendResultStateAdapter final" in header
    assert "void apply();" in header
    assert "HandleAsyncSearchAppendResult(" in source
    assert "state_.searchResults.insert" in source
    assert "applyCoverKeyFilter_" in source


def test_append_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_append_root_only_composes_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AppAsyncSearchAppendEffects" not in source
    assert "AsyncSearchAppendResultStateAdapter" in body
    assert "appendResultAdapter.apply();" in body
    assert "HandleAsyncSearchAppendResult(" not in body
