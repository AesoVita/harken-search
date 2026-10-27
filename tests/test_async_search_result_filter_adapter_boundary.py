from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AsyncSearchResultFilterAdapter.h"
SOURCE = ROOT / "src/AsyncSearchResultFilterAdapter.cpp"
MAIN = ROOT / "src/main.cpp"
FACTORY = ROOT / "src/AsyncSearchResultEffectFactory.cpp"
APPLICATION = ROOT / "src/AsyncSearchResultApplicationAdapter.cpp"


def test_filter_adapter_owns_playlist_and_file_type_projection() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "ApplyPlaylistScopedFilters(" in header
    assert "FilterResultsByFileTypePolicy(" in header
    for owned in (
        "HandlePlaylistResultFilter(",
        "BuildPlaylistFolderTreeFromRecords(",
        "FileTypeVisibility",
        "DIAG_ACTION_FMT",
    ):
        assert owned in source
    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_application_root_uses_filter_adapter_without_local_definitions() -> None:
    main = MAIN.read_text(encoding="utf-8")
    factory = FACTORY.read_text(encoding="utf-8")
    application = APPLICATION.read_text(encoding="utf-8")
    assert '#include "AsyncSearchResultFilterAdapter.h"' in main
    assert "ApplyPlaylistScopedFilters" in factory
    assert "FilterResultsByFileTypePolicy" in factory
    assert "static void ApplyPlaylistScopedFilters(" not in main
    assert "static void FilterResultsByFileTypePolicy(" not in main
    assert "ApplyPlaylistScopedFilters," not in application
    assert "FilterResultsByFileTypePolicy," not in application


def test_filter_adapter_is_built_and_governed() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    assert "src/AsyncSearchResultFilterAdapter.cpp" in cmake
    assert "TestAsyncSearchResultFilterAdapterBoundary" in behavior
