from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchFacetResultStateAdapter.h"
SOURCE = ROOT / "src/AsyncSearchFacetResultStateAdapter.cpp"
MAIN = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_facet_result_state_adapter_owns_app_state_effect_bridge() -> None:
    assert HEADER.exists()
    assert SOURCE.exists()
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AsyncSearchFacetResultStateAdapter final" in header
    assert "void apply();" in header
    assert "HandleAsyncSearchFacetResult(" in source
    assert "void AsyncSearchFacetResultStateAdapter::updateTotalCount(" in source
    assert "void AsyncSearchFacetResultStateAdapter::replaceFacets(" in source


def test_facet_result_state_adapter_has_one_way_dependencies() -> None:
    combined = "\n".join(
        path.read_text(encoding="utf-8") for path in (HEADER, SOURCE)
    )
    for forbidden in ("main.cpp", "WindowsMain", "src/platform/"):
        assert forbidden not in combined


def test_facet_result_root_only_composes_state_adapter() -> None:
    source = MAIN.read_text(encoding="utf-8")
    body = source
    assert "AppAsyncSearchFacetResultEffects" not in source
    assert "AsyncSearchFacetResultStateAdapter facetResultAdapter(" in body
    assert "facetResultAdapter.apply();" in body
    assert "HandleAsyncSearchFacetResult(" not in body
