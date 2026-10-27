from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_main_delegates_file_type_semantics_to_policy() -> None:
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    result_filter = (
        ROOT / "src/AsyncSearchResultFilterAdapter.cpp"
    ).read_text(encoding="utf-8")
    file_type_adapter = (
        ROOT / "src/SearchFilterFileTypeAdapter.cpp"
    ).read_text(encoding="utf-8")
    assert "ApplyCurrentFileTypeVisibilityToFilter" not in main
    assert "FilterBatchByFileType" not in main
    assert file_type_adapter.count("aura::file_types::BuildPolicy(") == 1
    assert result_filter.count("aura::file_types::BuildPolicy(") == 1
    assert "aura::file_types::isVisible(" in result_filter
