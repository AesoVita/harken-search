#!/usr/bin/env python3
"""Check Aris behavior contract docs for required BDD/TDD gate sections."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BEHAVIOR_DIR = ROOT / "docs" / "behavior"
MODULE_DIR = BEHAVIOR_DIR / "modules"
INDEX_PATH = BEHAVIOR_DIR / "module-index.md"
SYMBOL_SCAN_DIRS = (ROOT / "src", ROOT / "tests")

REQUIRED_HEADINGS = [
    "## Product Behavior",
    "## 中文语义",
    "## BDD Scenarios",
    "## Interface Binding",
    "## Graphify Binding",
    "## TDD / QA Proof",
    "## Missing Coverage",
    "## Change Gate",
    "## Decision Log",
]

ALLOWED_INTERFACE_KINDS = {"UI", "API", "worker", "DB", "diagnostics", "QA"}
ALLOWED_STATUSES = {"Draft", "Active", "Needs Audit", "Superseded"}
CJK_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]")


def module_paths(index_text: str) -> list[Path]:
    paths: list[Path] = []
    for match in re.finditer(r"`(modules/[^`]+\.md)`", index_text):
        path = BEHAVIOR_DIR / match.group(1)
        if path not in paths:
            paths.append(path)
    return paths


def registered_test_names() -> set[str]:
    cmake_paths = [ROOT / "CMakeLists.txt", *sorted((ROOT / "cmake").rglob("*.cmake"))]
    names: set[str] = set()
    for cmake_path in cmake_paths:
        if not cmake_path.exists():
            continue
        text = cmake_path.read_text(encoding="utf-8", errors="replace")
        # A build target is not a registered test.  Only add_test() establishes
        # something ctest -R can execute; conflating add_executable() with this
        # set is what allowed the Cubase contract's zero-match false green.
        names.update(
            match.group(1)
            for match in re.finditer(r"add_test\s*\(\s*NAME\s+([A-Za-z0-9_]+)", text, re.I | re.S)
        )
        names.update(
            match.group(1)
            for match in re.finditer(
                r"add_test\s*\(\s*(?!NAME\b)([A-Za-z0-9_]+)\s+",
                text,
                re.I | re.S,
            )
        )
    return names


def _section(text: str, heading: str) -> str:
    match = re.search(
        rf"{re.escape(heading)}\s*\n(?P<body>.*?)(?:\n## |\Z)",
        text,
        re.S,
    )
    return match.group("body") if match else ""


def _split_markdown_table_row(line: str) -> list[str]:
    """Split a Markdown row while treating an escaped pipe as cell text."""
    raw = line.strip()
    if raw.startswith("|"):
        raw = raw[1:]
    if raw.endswith("|") and not raw.endswith(r"\|"):
        raw = raw[:-1]

    cells: list[str] = []
    current: list[str] = []
    escaped = False
    for char in raw:
        if escaped:
            current.append(char)
            escaped = False
        elif char == "\\":
            current.append(char)
            escaped = True
        elif char == "|":
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    cells.append("".join(current).strip())
    return cells


def _proof_rows(text: str) -> list[tuple[int, str, list[str]]]:
    proof = _section(text, "## TDD / QA Proof")
    rows: list[tuple[int, str, list[str]]] = []
    for line_number, line in enumerate(proof.splitlines(), 1):
        stripped = line.strip()
        if not stripped.startswith("|"):
            continue
        if "Artifact / command / event" in stripped or re.fullmatch(r"[|:\- ]+", stripped):
            continue
        rows.append((line_number, line, _split_markdown_table_row(line)))
    return rows


def _ctest_patterns(artifact: str) -> list[str]:
    patterns: list[str] = []
    regex = re.compile(
        r"\bctest(?:\.exe)?\b[^`\n]*?\s-R\s+(?:\"([^\"]+)\"|'([^']+)'|([^\s`]+))",
        re.I,
    )
    for match in regex.finditer(artifact):
        patterns.append(next(group for group in match.groups() if group is not None))
    return patterns


def proof_test_names(text: str) -> set[str]:
    names: set[str] = set()
    for _, _, cells in _proof_rows(text):
        if len(cells) != 5:
            continue
        for segment in re.findall(r"`([^`]+)`", cells[2]):
            candidate = segment.split("::", 1)[0].strip()
            if re.fullmatch(r"Test[A-Za-z0-9_]+", candidate):
                names.add(candidate)
    return names


def section_test_names(text: str, heading: str) -> set[str]:
    """Return exact backtick-delimited CTest names from one contract section."""
    names: set[str] = set()
    for segment in re.findall(r"`([^`]+)`", _section(text, heading)):
        candidate = segment.split("::", 1)[0].strip()
        if re.fullmatch(r"Test[A-Za-z0-9_]+", candidate):
            names.add(candidate)
    return names


def interface_symbols(text: str) -> set[str]:
    interface_table = re.search(
        r"## Interface Binding\s*\n(?P<table>.*?)(?:\n## |\Z)",
        text,
        re.S,
    )
    if not interface_table:
        return set()

    symbols: set[str] = set()
    for line in interface_table.group("table").splitlines():
        if not line.startswith("|") or "---" in line or "Kind" in line:
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 2:
            continue
        kind = cells[1]
        if kind == "diagnostics":
            continue
        for segment in re.findall(r"`([^`]+)`", line):
            for raw_part in re.split(r"\s*(?:/|->)\s*", segment):
                part = raw_part.strip()
                if not part:
                    continue
                symbols.add(part)
    return symbols


def _candidate_symbol_needles(symbol: str) -> list[str]:
    clean = symbol.strip()
    if "*" in clean:
        return []
    no_parens = clean.replace("()", "").replace("(...)","")
    tail = no_parens.split("::")[-1]
    candidates = [clean, no_parens, tail]
    return [item for item in dict.fromkeys(candidates) if item]


def symbol_exists(symbol: str) -> bool:
    needles = _candidate_symbol_needles(symbol)
    for directory in SYMBOL_SCAN_DIRS:
        if not directory.exists():
            continue
        for path in directory.rglob("*"):
            if path.suffix.lower() not in {".h", ".hpp", ".cpp", ".cxx", ".cc", ".inl"}:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            if any(needle in text for needle in needles):
                return True
    return False


def check_contract(path: Path) -> list[str]:
    errors: list[str] = []
    if not path.exists():
        return [f"missing contract file: {path}"]
    text = path.read_text(encoding="utf-8")
    for heading in REQUIRED_HEADINGS:
        if heading not in text:
            errors.append(f"{path}: missing heading {heading}")
    status_match = re.search(r"^\*\*Status:\*\*\s*(.*?)\s*$", text, re.M)
    if not status_match:
        errors.append(f"{path}: missing Status header")
    else:
        status = status_match.group(1)
        if status not in ALLOWED_STATUSES:
            allowed = ", ".join(sorted(ALLOWED_STATUSES))
            errors.append(f"{path}: invalid Status `{status}`; allowed: {allowed}")

    bdd_counts = {"Given": 0, "When": 0, "Then": 0}
    bdd = _section(text, "## BDD Scenarios")
    for line_number, line in enumerate(bdd.splitlines(), 1):
        match = re.match(r"^\s*-\s*(Given|When|Then)\b\s*(.*)$", line)
        if not match:
            continue
        keyword, description = match.groups()
        bdd_counts[keyword] += 1
        if not CJK_RE.search(description):
            errors.append(
                f"{path}: BDD {keyword} description must contain Chinese "
                f"(BDD section line {line_number})"
            )
    for keyword, count in bdd_counts.items():
        if count == 0:
            errors.append(f"{path}: missing {keyword} scenario")
    if "UNKNOWN_BLOCKING" not in text:
        errors.append(f"{path}: missing UNKNOWN_BLOCKING review rule")
    if "用户承诺：" not in text:
        errors.append(f"{path}: missing Chinese user promise")
    if "核心流程：" not in text:
        errors.append(f"{path}: missing Chinese workflow")
    if "非目标：" not in text:
        errors.append(f"{path}: missing Chinese non-goals")
    interface_table = re.search(
        r"## Interface Binding\s*\n(?P<table>.*?)(?:\n## |\Z)",
        text,
        re.S,
    )
    if interface_table:
        for line in interface_table.group("table").splitlines():
            if not line.startswith("|") or "---" in line or "Kind" in line:
                continue
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            if len(cells) < 2:
                continue
            kind = cells[1]
            if kind not in ALLOWED_INTERFACE_KINDS:
                allowed = ", ".join(sorted(ALLOWED_INTERFACE_KINDS))
                errors.append(f"{path}: invalid Interface Binding Kind `{kind}`; allowed: {allowed}")
    for symbol in sorted(interface_symbols(text)):
        if not _candidate_symbol_needles(symbol):
            continue
        if not symbol_exists(symbol):
            errors.append(f"{path}: Interface Binding symbol not found in src/tests `{symbol}`")
    proof_rows = _proof_rows(text)
    for line_number, _, cells in proof_rows:
        if len(cells) != 5:
            errors.append(
                f"{path}: TDD / QA Proof row must have exactly 5 columns; "
                f"got {len(cells)} (proof line {line_number})"
            )
            continue
        if not cells[0]:
            errors.append(f"{path}: TDD / QA Proof row has empty Proof ID (proof line {line_number})")
        if not cells[3]:
            errors.append(f"{path}: TDD / QA Proof row has empty pass signal (proof line {line_number})")
        if not cells[4]:
            errors.append(f"{path}: TDD / QA Proof row has empty Status (proof line {line_number})")

    registered = registered_test_names()
    for test_name in sorted(proof_test_names(text)):
        if test_name not in registered:
            errors.append(f"{path}: proof references unregistered test `{test_name}`")
    for test_name in sorted(section_test_names(text, "## Change Gate")):
        if test_name not in registered:
            errors.append(f"{path}: Change Gate references unregistered test `{test_name}`")
    for _, _, cells in proof_rows:
        if len(cells) != 5:
            continue
        for pattern in _ctest_patterns(cells[2]):
            try:
                matched = any(re.search(pattern, name) for name in registered)
            except re.error as exc:
                errors.append(f"{path}: invalid ctest -R pattern `{pattern}`: {exc}")
                continue
            if not matched:
                errors.append(
                    f"{path}: ctest -R pattern matches zero registered tests `{pattern}`"
                )
    return errors


def check_all() -> list[str]:
    errors: list[str] = []
    if not INDEX_PATH.exists():
        return [f"missing module index: {INDEX_PATH}"]
    if not MODULE_DIR.exists():
        errors.append(f"missing module directory: {MODULE_DIR}")
    index_text = INDEX_PATH.read_text(encoding="utf-8")
    paths = module_paths(index_text)
    if not paths:
        errors.append(f"{INDEX_PATH}: no module contract paths found")
    for path in paths:
        errors.extend(check_contract(path))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()

    errors = check_all()
    if errors:
        print("Behavior contract check failed:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Behavior contract check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
