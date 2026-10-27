#!/usr/bin/env python3
"""Fail when a loop uses a variable ImGui label without PushID or ##."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


WIDGETS = ("Selectable", "Checkbox", "MenuItem")
WIDGET_RE = re.compile(
    r"\bImGui::(?P<widget>" + "|".join(WIDGETS) + r")\s*\(",
)
FOR_RE = re.compile(r"\bfor\s*\(")


@dataclass(frozen=True)
class Finding:
    line: int
    widget: str
    arg: str
    snippet: str


def _strip_line_comment(line: str) -> str:
    in_str = False
    quote = ""
    escape = False
    i = 0
    while i < len(line):
        ch = line[i]
        if in_str:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                in_str = False
            i += 1
            continue
        if ch in "\"'":
            in_str = True
            quote = ch
            i += 1
            continue
        if ch == "/" and i + 1 < len(line) and line[i + 1] == "/":
            return line[:i]
        i += 1
    return line


def _mask_strings_and_comments(src: str) -> str:
    out: list[str] = []
    i = 0
    n = len(src)
    while i < n:
        if src.startswith("//", i):
            nl = src.find("\n", i)
            if nl < 0:
                out.append(" " * (n - i))
                break
            out.append(" " * (nl - i))
            i = nl
            continue
        if src.startswith("/*", i):
            end = src.find("*/", i + 2)
            if end < 0:
                out.append(" " * (n - i))
                break
            end += 2
            chunk = src[i:end]
            out.append("".join("\n" if ch == "\n" else " " for ch in chunk))
            i = end
            continue
        if src[i] in "\"'":
            quote = src[i]
            j = i + 1
            escape = False
            while j < n:
                ch = src[j]
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == quote:
                    j += 1
                    break
                j += 1
            chunk = src[i:j]
            out.append("".join("\n" if ch == "\n" else " " for ch in chunk))
            i = j
            continue
        out.append(src[i])
        i += 1
    return "".join(out)


def _matching_paren(src: str, open_idx: int) -> int:
    depth = 0
    i = open_idx
    n = len(src)
    while i < n:
        ch = src[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _matching_brace(src: str, open_idx: int) -> int:
    depth = 0
    i = open_idx
    n = len(src)
    while i < n:
        ch = src[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _line_number(src: str, idx: int) -> int:
    return src[:idx].count("\n") + 1


def _first_arg(call_src: str) -> str:
    depth = 0
    in_str = False
    quote = ""
    escape = False
    for i, ch in enumerate(call_src):
        if in_str:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == quote:
                in_str = False
            continue
        if ch in "\"'":
            in_str = True
            quote = ch
            continue
        if ch in "({[<":
            depth += 1
        elif ch in ")}]>":
            depth -= 1
        elif ch == "," and depth == 0:
            return call_src[:i].strip()
    return call_src.strip()


def _is_fixed_enum_label(arg: str) -> bool:
    compact = re.sub(r"\s+", "", arg)
    if compact.startswith("L("):
        return True
    if compact.startswith('"') or compact.startswith("u8\""):
        return True
    if re.search(r"\.(first|second)(\.c_str\(\))?$", compact):
        return True
    return False


_IDENT_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\b")


def _arg_idents(arg: str) -> list[str]:
    compact = re.sub(r"\s+", "", arg)
    compact = re.sub(r"\.c_str\(\)$", "", compact)
    return _IDENT_RE.findall(compact)


def _has_explicit_id(arg: str, loop_prefix: str) -> bool:
    if "##" in arg:
        return True
    if "PushID" in loop_prefix:
        return True
    compact_arg = re.sub(r"\s+", "", arg)
    if "selectableId" in compact_arg and "MakeOutputDeviceComboItem" in loop_prefix:
        return True
    for ident in _arg_idents(arg):
        if re.search(
            rf"\b{re.escape(ident)}\s*=\s*[^;]*##",
            loop_prefix,
        ):
            return True
        if re.search(
            rf"\bsnprintf\s*\(\s*{re.escape(ident)}\b[\s\S]*?##",
            loop_prefix,
        ):
            return True
    return False


def find_naked_loop_labels(src: str) -> list[Finding]:
    masked = _mask_strings_and_comments(src)
    findings: list[Finding] = []
    for match in FOR_RE.finditer(masked):
        paren_open = match.end() - 1
        paren_close = _matching_paren(masked, paren_open)
        if paren_close < 0:
            continue
        body_open = masked.find("{", paren_close)
        header_gap = masked[paren_close + 1 : body_open if body_open >= 0 else paren_close + 1]
        if body_open < 0 or not header_gap.strip() == "":
            continue
        body_close = _matching_brace(masked, body_open)
        if body_close < 0:
            continue
        loop_src = src[body_open : body_close + 1]
        loop_masked = masked[body_open : body_close + 1]
        for widget in WIDGET_RE.finditer(loop_masked):
            call_open = body_open + widget.end() - 1
            call_close = _matching_paren(masked, call_open)
            if call_close < 0:
                continue
            arg = _first_arg(src[call_open + 1 : call_close])
            if not arg or _is_fixed_enum_label(arg):
                continue
            prefix = loop_src[: widget.start()]
            if _has_explicit_id(arg, prefix):
                continue
            line = _line_number(src, body_open + widget.start())
            snippet_line = _strip_line_comment(src.splitlines()[line - 1]).strip()
            findings.append(
                Finding(
                    line=line,
                    widget=widget.group("widget"),
                    arg=arg,
                    snippet=snippet_line,
                )
            )
    return findings


def scan_file(path: Path) -> list[tuple[Path, Finding]]:
    text = path.read_text(encoding="utf-8")
    return [(path, finding) for finding in find_naked_loop_labels(text)]


# First-tier user-made duplicate lists + Combo/ComboMulti. Whole-file scans
# still catch out-of-contract loops (output-device combo, DAW menus, UCS).
CONTRACT_SITES = (
    (
        "src/PlaylistQuickBar.cpp",
        "PLQ_ChooseTargetFolder",
        "PLQ_MoveSubCancelBtn",
        "p.name.c_str()",
    ),
    (
        "src/ModuleUI.cpp",
        'BeginCombo("##WFLoad"',
        "ImGui::EndCombo();",
        "wf.name.c_str()",
    ),
    (
        "src/ModuleUI.cpp",
        'BeginCombo("##NamingPreset"',
        "ImGui::EndCombo();",
        "presets[i].name.c_str()",
    ),
    (
        "src/WwiseTransferPanel.cpp",
        'BeginCombo("##preset"',
        "ImGui::EndCombo();",
        "p.name.c_str()",
    ),
    (
        "src/main.cpp",
        "VALUE_PICK content: list value candidates",
        "WordSuggest content",
        "val.c_str()",
    ),
    (
        "src/main.cpp",
        "WordSuggest content",
        "ImGui::EndPopup();",
        "disp.c_str()",
    ),
    (
        "src/main.cpp",
        "for (int i = 0; i < (int)items.size(); ++i)",
        "ImGui::EndChild();",
        "it.c_str()",
    ),
    (
        "src/HarkenSearchWidgets.cpp",
        "bool Combo(const char* label, int* currentIndex,",
        "return changed;",
        "items[i]",
    ),
    (
        "src/HarkenSearchWidgets.cpp",
        "bool ComboMulti(const char* label, std::vector<bool>* selections,",
        "return changed;",
        "items[i]",
    ),
)


def extract_region(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    stop = text.index(end, begin)
    return text[begin:stop]


def scan_contract_sites(root: Path) -> list[tuple[str, Finding | None, str]]:
    leftovers: list[tuple[str, Finding | None, str]] = []
    for rel, start, end, needle in CONTRACT_SITES:
        text = (root / rel).read_text(encoding="utf-8")
        region = extract_region(text, start, end)
        if needle not in region:
            leftovers.append((f"{rel}: missing {needle}", None, region))
            continue
        findings = find_naked_loop_labels("void Site() {\n" + region + "\n}\n")
        if findings:
            leftovers.append((rel, findings[0], region))
        elif "PushID" not in region and "##" not in region:
            leftovers.append((f"{rel}: {needle} has no PushID/##", None, region))
    return leftovers


DEFAULT_PATHS = (
    "src/main.cpp",
    "src/ModuleUI.cpp",
    "src/WwiseTransferPanel.cpp",
    "src/HarkenSearchWidgets.cpp",
    "src/PlaylistQuickBar.cpp",
)


def iter_protected_sources(root: Path) -> list[Path]:
    src = root / "src"
    return sorted(path for path in src.glob("*.cpp") if path.is_file())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "paths",
        nargs="*",
        help="Optional source files. Default scans every src/*.cpp file.",
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    failed = False
    if args.paths:
        paths = [
            root / rel if not Path(rel).is_absolute() else Path(rel) for rel in args.paths
        ]
    else:
        leftovers = scan_contract_sites(root)
        for label, finding, _region in leftovers:
            failed = True
            if finding is None:
                print(label)
            else:
                print(f"{label}:{finding.line}: {finding.widget}({finding.arg})")
        paths = iter_protected_sources(root)
    for path in paths:
        for _, finding in scan_file(path):
            failed = True
            print(f"{path}:{finding.line}: {finding.widget}({finding.arg})")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
