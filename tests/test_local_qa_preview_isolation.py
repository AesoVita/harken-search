from pathlib import Path


ROOT = Path(__file__).parents[1]


def test_current_qa_launcher_builds_with_a_pinned_preview_root():
    script = (ROOT / "scripts/local_current_qa_oneclick.ps1").read_text(encoding="utf-8")
    assert '"-DARIS_PREVIEW_ISOLATION=ON"' in script
    assert '"-DARIS_PREVIEW_ISOLATION_ROOT=$dataRoot"' in script
    assert "-BuildDir $buildPath" in script
    assert script.index('"-DARIS_PREVIEW_ISOLATION=ON"') < script.index("package_qa_exe.ps1")
    assert script.index("CMakeCache.txt") < script.index("Start-Process -FilePath $exe")


def test_remote_qa_launcher_builds_with_a_pinned_preview_root():
    script = (ROOT / "scripts/local_qa_oneclick.ps1").read_text(encoding="utf-8")
    assert '"-DARIS_PREVIEW_ISOLATION=ON"' in script
    assert '"-DARIS_PREVIEW_ISOLATION_ROOT=$dataRoot"' in script
    assert "-BuildDir $buildPath" in script
    assert script.index('"-DARIS_PREVIEW_ISOLATION=ON"') < script.index("package_qa_exe.ps1")
    assert script.index("CMakeCache.txt") < script.index("Start-Process -FilePath $exe")
