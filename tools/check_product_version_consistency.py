from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


DEFAULT_REPO_ROOT = Path(__file__).resolve().parents[1]

VERSION_PART_RE = {
    "MAJOR": re.compile(r"#define\s+SOUNDMANAGER_VERSION_MAJOR\s+([0-9]+)"),
    "MINOR": re.compile(r"#define\s+SOUNDMANAGER_VERSION_MINOR\s+([0-9]+)"),
    "PATCH": re.compile(r"#define\s+SOUNDMANAGER_VERSION_PATCH\s+([0-9]+)"),
}
VERSION_STRING_RE = re.compile(r'SOUNDMANAGER_VERSION_STRING\s+"([0-9]+\.[0-9]+\.[0-9]+)"')
CMAKE_PROJECT_RE = re.compile(r"project\(\s*HarkenSearch\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)")
ISS_VERSION_RE = re.compile(r'#define\s+MyAppVersion\s+"([0-9]+\.[0-9]+\.[0-9]+)"')
RC_COMMA_RE = re.compile(r"\b(FILEVERSION|PRODUCTVERSION)\s+([0-9]+),([0-9]+),([0-9]+),([0-9]+)")
RC_STRING_RE = re.compile(r'VALUE\s+"(FileVersion|ProductVersion)"\s*,\s*"([0-9]+\.[0-9]+\.[0-9]+)"')


def _read_text(path: Path) -> str | None:
    if not path.is_file():
        return None
    return path.read_text(encoding="utf-8")


def parse_version_header(text: str) -> tuple[str | None, dict[str, str | None]]:
    parts = {
        name: (match.group(1) if (match := pattern.search(text)) else None)
        for name, pattern in VERSION_PART_RE.items()
    }
    composed = None
    if all(parts.values()):
        composed = ".".join(parts[name] for name in ("MAJOR", "MINOR", "PATCH"))
    string_match = VERSION_STRING_RE.search(text)
    return (string_match.group(1) if string_match else None), parts | {"composed": composed}


def parse_cmake_project_version(text: str) -> str | None:
    match = CMAKE_PROJECT_RE.search(text)
    return match.group(1) if match else None


def parse_installer_version(text: str) -> str | None:
    match = ISS_VERSION_RE.search(text)
    return match.group(1) if match else None


def parse_resources_rc_versions(text: str) -> dict[str, str | None]:
    values: dict[str, str | None] = {
        "FILEVERSION": None,
        "PRODUCTVERSION": None,
        "FileVersion": None,
        "ProductVersion": None,
    }
    for match in RC_COMMA_RE.finditer(text):
        name, major, minor, patch, build = match.groups()
        if build != "0":
            values[name] = f"{major}.{minor}.{patch}.{build}"
        else:
            values[name] = f"{major}.{minor}.{patch}"
    for match in RC_STRING_RE.finditer(text):
        values[match.group(1)] = match.group(2)
    return values


def _issue(path: str, detail: str) -> str:
    return f"{path}: {detail}"


def check_product_version_consistency(repo_root: Path | None = None) -> list[str]:
    root = Path(repo_root) if repo_root is not None else DEFAULT_REPO_ROOT
    issues: list[str] = []

    version_header_path = root / "src" / "Version.h"
    cmake_path = root / "CMakeLists.txt"
    installer_path = root / "installer" / "HarkenSearch_Setup.iss"
    resources_path = root / "resources.rc"

    header_text = _read_text(version_header_path)
    if header_text is None:
        issues.append(_issue("src/Version.h", "missing product version header"))
        source_version = None
    else:
        string_version, parts = parse_version_header(header_text)
        composed = parts.get("composed")
        if composed is None:
            issues.append(
                _issue(
                    "src/Version.h",
                    "SOUNDMANAGER_VERSION_MAJOR/MINOR/PATCH could not be parsed",
                )
            )
        if string_version is None:
            issues.append(
                _issue("src/Version.h", "SOUNDMANAGER_VERSION_STRING could not be parsed")
            )
        elif composed is not None and string_version != composed:
            issues.append(
                _issue(
                    "src/Version.h",
                    f"SOUNDMANAGER_VERSION_STRING is {string_version}, "
                    f"expected {composed} from MAJOR/MINOR/PATCH",
                )
            )
        source_version = composed or string_version

    cmake_text = _read_text(cmake_path)
    if cmake_text is None:
        issues.append(_issue("CMakeLists.txt", "missing CMake project file"))
    else:
        cmake_version = parse_cmake_project_version(cmake_text)
        if cmake_version is None:
            issues.append(
                _issue("CMakeLists.txt", "project(HarkenSearch VERSION …) could not be parsed")
            )
        elif source_version is not None and cmake_version != source_version:
            issues.append(
                _issue(
                    "CMakeLists.txt",
                    f"project VERSION is {cmake_version}, expected {source_version} from src/Version.h",
                )
            )

    installer_text = _read_text(installer_path)
    if installer_text is None:
        issues.append(_issue("installer/HarkenSearch_Setup.iss", "missing installer script"))
    else:
        installer_version = parse_installer_version(installer_text)
        if installer_version is None:
            issues.append(
                _issue(
                    "installer/HarkenSearch_Setup.iss",
                    "MyAppVersion could not be parsed",
                )
            )
        elif source_version is not None and installer_version != source_version:
            issues.append(
                _issue(
                    "installer/HarkenSearch_Setup.iss",
                    f"MyAppVersion is {installer_version}, expected {source_version} from src/Version.h",
                )
            )

    resources_text = _read_text(resources_path)
    if resources_text is None:
        issues.append(_issue("resources.rc", "missing Windows version resource"))
    else:
        rc_versions = parse_resources_rc_versions(resources_text)
        for field, actual in rc_versions.items():
            if actual is None:
                issues.append(_issue("resources.rc", f"{field} could not be parsed"))
            elif source_version is not None and actual != source_version:
                issues.append(
                    _issue(
                        "resources.rc",
                        f"{field} is {actual}, expected {source_version} from src/Version.h",
                    )
                )

    return issues


def _current_product_version() -> str:
    header = _read_text(DEFAULT_REPO_ROOT / "src" / "Version.h") or ""
    string_version, parts = parse_version_header(header)
    return str(parts.get("composed") or string_version or "")


CURRENT_PRODUCT_VERSION = _current_product_version()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Fail when product version declaration points drift from src/Version.h."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=DEFAULT_REPO_ROOT,
        help="Repository root to scan. Defaults to this checkout.",
    )
    args = parser.parse_args(argv)
    issues = check_product_version_consistency(args.repo_root)
    if issues:
        print("PRODUCT VERSION CONSISTENCY CHECK FAILED")
        for issue in issues:
            print(f" - {issue}")
        return 1
    print("PRODUCT VERSION CONSISTENCY CHECK PASSED")
    print(f"All current product version declaration points match {CURRENT_PRODUCT_VERSION}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
