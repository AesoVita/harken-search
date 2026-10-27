from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/DatabaseSearchFilterBuilder.h"
SOURCE = ROOT / "src/DatabaseSearchFilterBuilder.cpp"


def test_database_filter_builder_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "SearchFilter BuildCurrentDbFilter(const AppState& state)",
        "CompileSearchBufferKeywordForFilter",
        "NormalizeFolderPathForDb",
        "ApplyFileTypePolicyToFilter",
        "activeUserCollectionIdx",
        "genrePillState",
        "instrumentPillState",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_only_consumes_database_filter_builder():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "DatabaseSearchFilterBuilder.h"' in main
    assert "static SearchFilter BuildCurrentDbFilter(" not in main
    assert "using aura::search::BuildCurrentDbFilter;" in main
