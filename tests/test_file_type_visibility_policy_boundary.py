from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/FileTypeVisibilityPolicy.h"
SOURCE = ROOT / "src/FileTypeVisibilityPolicy.cpp"


def test_file_type_visibility_policy_owns_extension_semantics() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"
    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    for token in ("struct FileTypeVisibility", "BuildPolicy(", "isVisible("):
        assert token in header
    for forbidden in ("AppState", "Database", "AudioRecord", "ImGui", "main.cpp", "WindowsMain"):
        assert forbidden not in header
        assert forbidden not in source
    assert "ApplyCurrentFileTypeVisibilityToFilter" not in main
    assert "FilterBatchByFileType" not in main
    assert "FileTypeVisibilityPolicy" in main
