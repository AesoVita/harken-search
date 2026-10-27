from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchResultLifecycleStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchResultLifecycleStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_lifecycle_state_adapter_owns_app_state_effects() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = MAIN.read_text(encoding="utf-8")
    assert "class AsyncSearchResultLifecycleStateAdapter final" in header
    assert "bool apply();" in header
    assert "HandleAsyncSearchResultLifecycle(" in source
    assert "state_.asyncSearchInProgress = false;" in source
    assert 'state_.filterQueryError = "async search failed";' in source
    assert "AppAsyncSearchResultLifecycleEffects" not in main


def test_lifecycle_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_apply_root_only_composes_lifecycle_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "lifecycleAdapter.apply()" in body
    assert "AppAsyncSearchResultLifecycleEffects" not in body
    assert "HandleAsyncSearchResultLifecycle(" not in body
