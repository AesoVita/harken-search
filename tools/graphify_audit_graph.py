#!/usr/bin/env python3
"""Build an auditable call graph for Aris source code.

The existing interface graph is optimized for fast impact lookups. This tool
adds an audit layer: every call-like token is classified so unresolved edges
are explicit instead of silently missing.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = ROOT / "src"
MAIN_OUT_DIR = ROOT / "graphify-out" / "main"
AUDIT_GRAPH_PATH = MAIN_OUT_DIR / "audit-call-graph.json"
AUDIT_REPORT_PATH = MAIN_OUT_DIR / "AUDIT_CALL_GRAPH.md"
UNKNOWN_LEDGER_PATH = MAIN_OUT_DIR / "UNKNOWN_CALLS.md"
DYNAMIC_LEDGER_PATH = MAIN_OUT_DIR / "DYNAMIC_CALLS.md"

SOURCE_EXTS = {".h", ".hpp", ".hh", ".cpp", ".cc", ".cxx"}
EXCLUDED_PARTS = {"webui", "generated", "__pycache__"}
KEYWORDS = {
    "__except",
    "alignof",
    "catch",
    "const_cast",
    "defined",
    "delete",
    "dynamic_cast",
    "for",
    "if",
    "new",
    "reinterpret_cast",
    "return",
    "sizeof",
    "static_cast",
    "switch",
    "while",
}
EXTERNAL_NAMES = {
    "assert",
    "fprintf",
    "printf",
    "snprintf",
    "sprintf",
    "sscanf",
    "static_assert",
    "wprintf",
}
EXTERNAL_PREFIXES = {
    "ImGui::",
    "MA_",
    "SDL_",
    "gl",
    "ma_",
    "sqlite3_",
    "std::",
}
EXTERNAL_NAME_PREFIXES = (
    "Im",
    "ma_",
    "sqlite3_",
    "stbi_",
    "whisper_",
)
EXTERNAL_WINDOWS_NAMES = {
    "CloseHandle",
    "CoCreateInstance",
    "CoInitializeEx",
    "CoTaskMemFree",
    "CoUninitialize",
    "CreateFileW",
    "GetModuleFileNameW",
    "GetTempPathW",
    "HttpOpenRequestW",
    "HttpQueryInfoW",
    "HttpSendRequestW",
    "InternetCloseHandle",
    "InternetConnectW",
    "InternetOpenUrlW",
    "InternetOpenW",
    "InternetReadFile",
    "InternetSetOptionW",
    "LCMapStringEx",
    "MultiByteToWideChar",
    "SUCCEEDED",
    "WideCharToMultiByte",
    "WriteFile",
}

FUNC_DEF_RE = re.compile(
    r"""^\s*
    (?:template\s*<[^>]+>\s*)?
    (?:(?:static|inline|constexpr|virtual|extern|friend|explicit)\s+)*
    (?:[\w:<>,~*&\[\]\s]+\s+)?
    (?P<name>(?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*
    \([^;{}()]*\)\s*
    (?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?
    (?:->\s*[\w:<>,~*&\[\]\s]+)?\s*
    \{
    """,
    re.VERBOSE,
)
FUNC_DECL_RE = re.compile(
    r"""^\s*
    (?:(?:static|inline|constexpr|virtual|extern|friend|explicit)\s+)*
    (?:(?:[\w:<>,~*&\[\]]+)\s+)+
    (?P<name>(?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*
    \([^;{}()]*\)\s*
    (?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?
    (?:->\s*[\w:<>,~*&\[\]\s]+)?\s*
    ;
    """,
    re.VERBOSE,
)
CALL_RE = re.compile(r"\b(?P<name>[A-Za-z_]\w*)\s*\(")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')
LINE_COMMENT_RE = re.compile(r"//.*$")


def rel(path: Path, repo_root: Path) -> str:
    try:
        return str(path.resolve().relative_to(repo_root.resolve())).replace("\\", "/")
    except ValueError:
        return str(path.resolve()).replace("\\", "/")


def iter_sources(src_root: Path) -> list[Path]:
    files: list[Path] = []
    for path in src_root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_EXTS:
            continue
        parts = {part.lower() for part in path.relative_to(src_root).parts}
        if parts & EXCLUDED_PARTS:
            continue
        files.append(path)
    return sorted(files)


def strip_strings_and_comments(line: str) -> str:
    without_strings = STRING_RE.sub(lambda match: " " * (match.end() - match.start()), line)
    return LINE_COMMENT_RE.sub("", without_strings)


def short_name(name: str) -> str:
    return name.split("::")[-1]


def is_definition_or_declaration(line: str, name: str) -> bool:
    definition = FUNC_DEF_RE.match(line)
    if definition and short_name(definition.group("name")) == name:
        return True
    declaration = FUNC_DECL_RE.match(line)
    return bool(declaration and short_name(declaration.group("name")) == name)


def is_member_call(prefix: str) -> bool:
    trimmed = prefix.rstrip()
    return trimmed.endswith("->") or trimmed.endswith(".")


def external_reason(name: str, prefix: str) -> str | None:
    if name in EXTERNAL_NAMES:
        return "known external C/runtime call"
    if name in EXTERNAL_WINDOWS_NAMES:
        return "known Windows/system API call"
    for external_name_prefix in EXTERNAL_NAME_PREFIXES:
        if name.startswith(external_name_prefix):
            return f"external name prefix `{external_name_prefix}`"
    compact = prefix.rstrip()
    for external_prefix in EXTERNAL_PREFIXES:
        if compact.endswith(external_prefix.rstrip(":")) or external_prefix in compact[-80:]:
            return f"external prefix `{external_prefix}`"
    return None


def string_binding_reason(name: str) -> str | None:
    if name.startswith("DIAG_"):
        return "diagnostics macro/string-bound interface"
    return None


def manual_contract_reason(prefix: str) -> str | None:
    if prefix.rstrip().endswith("TestSeam::"):
        return "test seam alternate implementation contract"
    return None


def nearest_scope(scopes: list[tuple[int, dict[str, Any]]], line: int, file_node: dict[str, Any]) -> dict[str, Any]:
    current = file_node
    for idx, (scope_line, scope_node) in enumerate(scopes):
        next_line = scopes[idx + 1][0] if idx + 1 < len(scopes) else 10**9
        if scope_line <= line < next_line:
            current = scope_node
            break
    return current


def build_audit_graph(src_root: Path = SRC_ROOT, repo_root: Path = ROOT) -> dict[str, Any]:
    files = iter_sources(src_root)
    nodes: list[dict[str, Any]] = []
    node_by_id: dict[str, dict[str, Any]] = {}
    symbols_by_name: dict[str, list[dict[str, Any]]] = defaultdict(list)
    scopes_by_file: dict[Path, list[tuple[int, dict[str, Any]]]] = defaultdict(list)
    file_nodes: dict[Path, dict[str, Any]] = {}

    def add_node(path: Path, label: str, line: int, kind: str) -> dict[str, Any]:
        clean = re.sub(r"\W+", "_", f"{path.stem}_{label}_{line}_{kind}").strip("_").lower()
        node_id = clean
        suffix = 2
        while node_id in node_by_id:
            node_id = f"{clean}_{suffix}"
            suffix += 1
        node = {
            "id": node_id,
            "label": label,
            "kind": kind,
            "source_file": rel(path, repo_root),
            "source_location": f"L{line}",
        }
        node_by_id[node_id] = node
        nodes.append(node)
        return node

    for path in files:
        file_node = add_node(path, path.name, 1, "file")
        file_nodes[path] = file_node
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for lineno, line in enumerate(lines, start=1):
            definition = FUNC_DEF_RE.match(line)
            declaration = FUNC_DECL_RE.match(line)
            match = definition or declaration
            if not match:
                continue
            full_name = match.group("name")
            name = short_name(full_name)
            if name in KEYWORDS:
                continue
            kind = "function_definition" if definition else "function_declaration"
            node = add_node(path, f"{name}()", lineno, kind)
            symbols_by_name[name].append(node)
            if full_name != name:
                symbols_by_name[full_name].append(node)
            if definition:
                scopes_by_file[path].append((lineno, node))

    call_sites: list[dict[str, Any]] = []
    for path in files:
        file_node = file_nodes[path]
        scopes = sorted(scopes_by_file[path], key=lambda item: item[0])
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for lineno, original_line in enumerate(lines, start=1):
            line = strip_strings_and_comments(original_line)
            for match in CALL_RE.finditer(line):
                name = match.group("name")
                if name in KEYWORDS or is_definition_or_declaration(line, name):
                    continue
                prefix = line[:match.start()]
                external = external_reason(name, prefix)
                targets = symbols_by_name.get(name, [])
                member_call = is_member_call(prefix)
                string_binding = string_binding_reason(name)
                manual_contract = manual_contract_reason(prefix)
                if string_binding:
                    classification = "STRING_BOUND"
                    reason = string_binding
                elif manual_contract:
                    classification = "MANUAL_CONTRACT"
                    reason = manual_contract
                elif external:
                    classification = "EXTERNAL"
                    reason = external
                elif member_call:
                    classification = "POSSIBLE_DYNAMIC"
                    reason = "member call requires type/dispatch resolution"
                elif targets:
                    classification = "RESOLVED_STATIC"
                    reason = "matched known function declaration/definition"
                else:
                    classification = "UNKNOWN_BLOCKING"
                    reason = "no static symbol, external rule, or dynamic dispatch marker matched"

                scope = nearest_scope(scopes, lineno, file_node)
                call_sites.append({
                    "name": name,
                    "classification": classification,
                    "reason": reason,
                    "source": scope["id"],
                    "source_label": scope["label"],
                    "source_file": rel(path, repo_root),
                    "source_location": f"L{lineno}",
                    "line_text": original_line.strip(),
                    "target_candidates": [target["id"] for target in targets[:12]],
                    "target_candidate_labels": [target["label"] for target in targets[:12]],
                })

    counts = Counter(call["classification"] for call in call_sites)
    return {
        "schema": "auro-audit-call-graph-v1",
        "source": "tools/graphify_audit_graph.py",
        "scope": f"{rel(src_root, repo_root)} C/C++ source audit",
        "generated_at": datetime.now().replace(microsecond=0).isoformat(),
        "classifications": {
            "RESOLVED_STATIC": "Matched a known function declaration or definition.",
            "POSSIBLE_DYNAMIC": "Member dispatch or callback-like call that must be treated as candidate-based.",
            "EXTERNAL": "Known runtime, system, library, or namespace-prefixed external call.",
            "STRING_BOUND": "Macro or string-bound interface that should be audited by textual contract rules.",
            "MANUAL_CONTRACT": "Reserved for future behavior/contract bindings.",
            "UNKNOWN_BLOCKING": "Unclassified call-like token. Must be resolved, marked external, or covered by a contract before trusting 100% coverage.",
        },
        "counts": {
            "files": len(files),
            "nodes": len(nodes),
            "call_sites": len(call_sites),
            "resolved_static": counts["RESOLVED_STATIC"],
            "possible_dynamic": counts["POSSIBLE_DYNAMIC"],
            "external": counts["EXTERNAL"],
            "string_bound": counts["STRING_BOUND"],
            "manual_contract": counts["MANUAL_CONTRACT"],
            "unknown_blocking": counts["UNKNOWN_BLOCKING"],
        },
        "nodes": nodes,
        "call_sites": call_sites,
    }


def write_markdown_list(path: Path, title: str, calls: Iterable[dict[str, Any]]) -> None:
    lines = [f"# {title}", ""]
    call_list = list(calls)
    if not call_list:
        lines.append("No entries.")
    for call in call_list:
        lines.append(
            f"- `{call['name']}` at `{call['source_file']}:{call['source_location']}` "
            f"from `{call['source_label']}` - {call['classification']}: {call['reason']}"
        )
        if call.get("line_text"):
            lines.append(f"  - line: `{call['line_text'][:220]}`")
        candidates = call.get("target_candidate_labels") or []
        if candidates:
            lines.append(f"  - candidates: {', '.join(f'`{candidate}`' for candidate in candidates[:8])}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_report(graph: dict[str, Any], path: Path) -> None:
    counts = graph["counts"]
    unknown_names = Counter(
        call["name"] for call in graph["call_sites"] if call["classification"] == "UNKNOWN_BLOCKING"
    )
    dynamic_names = Counter(
        call["name"] for call in graph["call_sites"] if call["classification"] == "POSSIBLE_DYNAMIC"
    )
    lines = [
        "# Audit Call Graph",
        "",
        f"- Generated at: {graph['generated_at']}",
        f"- Scope: {graph['scope']}",
        f"- Files: {counts['files']}",
        f"- Nodes: {counts['nodes']}",
        f"- Call sites: {counts['call_sites']}",
        f"- `RESOLVED_STATIC`: {counts['resolved_static']}",
        f"- `POSSIBLE_DYNAMIC`: {counts['possible_dynamic']}",
        f"- `EXTERNAL`: {counts['external']}",
        f"- `STRING_BOUND`: {counts['string_bound']}",
        f"- `MANUAL_CONTRACT`: {counts['manual_contract']}",
        f"- `UNKNOWN_BLOCKING`: {counts['unknown_blocking']}",
        "",
        "## Rule",
        "",
        "`UNKNOWN_BLOCKING` entries are not silent misses. They must be resolved by better static extraction, an external rule, a dynamic candidate map, or a future behavior contract before the graph can be treated as complete.",
        "",
        "## Outputs",
        "",
        "- `audit-call-graph.json`",
        "- `AUDIT_CALL_GRAPH.md`",
        "- `UNKNOWN_CALLS.md`",
        "- `DYNAMIC_CALLS.md`",
        "",
        "## Top Unknown Blocking Names",
        "",
        *[
            f"- `{name}`: {count}"
            for name, count in unknown_names.most_common(30)
        ],
        "",
        "## Top Possible Dynamic Names",
        "",
        *[
            f"- `{name}`: {count}"
            for name, count in dynamic_names.most_common(30)
        ],
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def write_outputs(graph: dict[str, Any], out_dir: Path = MAIN_OUT_DIR) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "audit-call-graph.json").write_text(
        json.dumps(graph, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    write_report(graph, out_dir / "AUDIT_CALL_GRAPH.md")
    write_markdown_list(
        out_dir / "UNKNOWN_CALLS.md",
        "Unknown Blocking Calls",
        (call for call in graph["call_sites"] if call["classification"] == "UNKNOWN_BLOCKING"),
    )
    write_markdown_list(
        out_dir / "DYNAMIC_CALLS.md",
        "Possible Dynamic Calls",
        (call for call in graph["call_sites"] if call["classification"] == "POSSIBLE_DYNAMIC"),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cmd", choices=("rebuild", "check"), help="Build or check the audit graph")
    parser.add_argument("--src-root", type=Path, default=SRC_ROOT, help="Source root to scan")
    parser.add_argument("--out-dir", type=Path, default=MAIN_OUT_DIR, help="Output directory")
    parser.add_argument(
        "--fail-on-unknown",
        action="store_true",
        help="Return non-zero when UNKNOWN_BLOCKING entries exist",
    )
    args = parser.parse_args()

    graph_path = args.out_dir / "audit-call-graph.json"
    if args.cmd == "check":
        if not graph_path.exists():
            print(f"Audit graph missing: {graph_path}")
            return 2
        graph = json.loads(graph_path.read_text(encoding="utf-8"))
    else:
        graph = build_audit_graph(src_root=args.src_root, repo_root=ROOT)
        write_outputs(graph, args.out_dir)
        print(f"Wrote {args.out_dir / 'audit-call-graph.json'}")
        print(f"Wrote {args.out_dir / 'AUDIT_CALL_GRAPH.md'}")
        print(f"Wrote {args.out_dir / 'UNKNOWN_CALLS.md'}")
        print(f"Wrote {args.out_dir / 'DYNAMIC_CALLS.md'}")

    unknown = int(graph["counts"]["unknown_blocking"])
    print(f"Audit graph call sites: {graph['counts']['call_sites']}; UNKNOWN_BLOCKING: {unknown}")
    if args.fail_on_unknown and unknown:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
