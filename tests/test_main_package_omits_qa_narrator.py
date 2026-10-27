"""Keep the local QA narration window outside the public main package."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_main_package_has_no_qa_narration_runtime_or_build_switch():
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    assert "ARIS_LOCAL_QA_NARRATOR" not in cmake
    assert "ARIS_LOCAL_QA_NARRATOR" not in main
    assert "localQaNarrator" not in main
    assert "qa-narration.json" not in main
    assert not any((ROOT / "qa_only/narrator").rglob("*"))
