from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHECKER_PATH = ROOT / "tools" / "check_product_version_consistency.py"


ALIGNED_VERSION_H = """
#define SOUNDMANAGER_VERSION_MAJOR 1
#define SOUNDMANAGER_VERSION_MINOR 0
#define SOUNDMANAGER_VERSION_PATCH 31
#ifndef ARIS_UPDATE_LOCAL_DEMO_BUILD_VERSION
    #define SOUNDMANAGER_VERSION_STRING "1.0.31"
#else
    #define SOUNDMANAGER_VERSION_STRING ARIS_UPDATE_LOCAL_DEMO_BUILD_VERSION
#endif
"""

ALIGNED_CMAKE = 'project(HarkenSearch VERSION 1.0.31 LANGUAGES C CXX)\n'
ALIGNED_ISS = '#ifndef MyAppVersion\n#define MyAppVersion "1.0.31"\n#endif\n'
ALIGNED_RC = """
1 VERSIONINFO
 FILEVERSION 1,0,31,0
 PRODUCTVERSION 1,0,31,0
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "FileVersion", "1.0.31"
            VALUE "ProductVersion", "1.0.31"
        END
    END
END
"""


def load_checker():
    spec = importlib.util.spec_from_file_location(
        "check_product_version_consistency", CHECKER_PATH
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _write_aligned(root: Path) -> None:
    (root / "src").mkdir(parents=True, exist_ok=True)
    (root / "installer").mkdir(parents=True, exist_ok=True)
    (root / "src" / "Version.h").write_text(ALIGNED_VERSION_H, encoding="utf-8")
    (root / "CMakeLists.txt").write_text(ALIGNED_CMAKE, encoding="utf-8")
    (root / "installer" / "HarkenSearch_Setup.iss").write_text(ALIGNED_ISS, encoding="utf-8")
    (root / "resources.rc").write_text(ALIGNED_RC, encoding="utf-8")


def test_aligned_declaration_points_pass(tmp_path: Path) -> None:
    _write_aligned(tmp_path)
    checker = load_checker()
    assert checker.check_product_version_consistency(tmp_path) == []


def test_installer_drift_fails_and_names_iss(tmp_path: Path) -> None:
    _write_aligned(tmp_path)
    (tmp_path / "installer" / "HarkenSearch_Setup.iss").write_text(
        '#ifndef MyAppVersion\n#define MyAppVersion "1.0.30"\n#endif\n',
        encoding="utf-8",
    )
    issues = load_checker().check_product_version_consistency(tmp_path)
    assert issues
    assert any("installer/HarkenSearch_Setup.iss" in issue for issue in issues)
    assert any("1.0.30" in issue for issue in issues)


def test_cmake_drift_fails_and_names_cmakelists(tmp_path: Path) -> None:
    _write_aligned(tmp_path)
    (tmp_path / "CMakeLists.txt").write_text(
        "project(HarkenSearch VERSION 9.9.9 LANGUAGES C CXX)\n",
        encoding="utf-8",
    )
    issues = load_checker().check_product_version_consistency(tmp_path)
    assert issues
    assert any("CMakeLists.txt" in issue for issue in issues)
    assert any("9.9.9" in issue for issue in issues)


def test_resources_rc_drift_fails_and_names_rc(tmp_path: Path) -> None:
    _write_aligned(tmp_path)
    (tmp_path / "resources.rc").write_text(
        ALIGNED_RC.replace("1,0,31,0", "1,0,30,0").replace('"1.0.31"', '"1.0.30"'),
        encoding="utf-8",
    )
    issues = load_checker().check_product_version_consistency(tmp_path)
    assert issues
    assert any("resources.rc" in issue for issue in issues)


def test_version_header_internal_mismatch_fails(tmp_path: Path) -> None:
    _write_aligned(tmp_path)
    (tmp_path / "src" / "Version.h").write_text(
        ALIGNED_VERSION_H.replace(
            '#define SOUNDMANAGER_VERSION_STRING "1.0.31"',
            '#define SOUNDMANAGER_VERSION_STRING "1.0.99"',
        ),
        encoding="utf-8",
    )
    issues = load_checker().check_product_version_consistency(tmp_path)
    assert issues
    assert any("src/Version.h" in issue for issue in issues)


def test_live_repo_product_version_declarations_match_current_source() -> None:
    checker = load_checker()
    issues = checker.check_product_version_consistency(ROOT)
    assert issues == []
    assert checker.CURRENT_PRODUCT_VERSION
    assert checker.CURRENT_PRODUCT_VERSION == checker.parse_cmake_project_version(
        (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    )
