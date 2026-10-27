#!/usr/bin/env python3
"""Build or check Aris's narrow Graphify interface call graph.

This intentionally scans only src/ C++ interface/source files. It is a safe,
fast refresh path for graphify_debug_impact.py after local code edits; it is
not a replacement for the full semantic /graphify rebuild.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = ROOT / "src"
OUT_DIR = ROOT / "graphify-out"
MAIN_OUT_DIR = OUT_DIR / "main"
GRAPH_PATH = MAIN_OUT_DIR / "interface-call-graph.json"
REPORT_PATH = MAIN_OUT_DIR / "INTERFACE_CALL_GRAPH.md"
ARTIFACT_MANAGER = ROOT / "tools" / "graphify_artifact_manager.py"

SOURCE_EXTS = {".h", ".hpp", ".hh", ".cpp", ".cc", ".cxx"}
EXCLUDED_PARTS = {"webui", "generated", "__pycache__"}
KEYWORDS = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
    "static_cast",
    "dynamic_cast",
    "reinterpret_cast",
    "const_cast",
    "catch",
    "new",
    "delete",
    "alignof",
}

FUNC_DEF_RE = re.compile(
    r"""^\s*
    (?:template\s*<[^>]+>\s*)?
    (?:(?:static|inline|constexpr|virtual|extern|friend|explicit)\s+)*
    (?:[\w:<>,~*&\[\]\s]+\s+)?
    (?P<name>(?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*
    \([^;{}()]*\)\s*
    (?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?
    (?:->\s*[\w:<>,~*&\[\]\s]+)?
    \{
    """,
    re.VERBOSE,
)
CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+(?P<name>[A-Za-z_]\w*)\b")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"(?P<path>[^"]+)"')
CALL_RE = re.compile(r"\b(?P<name>[A-Za-z_]\w*)\s*\(")


def rel(path: Path) -> str:
    return str(path.resolve()).replace("\\", "/")


def node_id_for(path: Path, label: str | None = None) -> str:
    base = path.stem.lower()
    if label:
        clean = label
        clean = clean.replace("::", "_")
        clean = clean.replace("~", "destruct_")
        clean = re.sub(r"\W+", "_", clean).strip("_").lower()
        return f"{base}_{clean}" if clean else base
    return re.sub(r"\W+", "_", base).strip("_").lower()


def iter_sources() -> list[Path]:
    files: list[Path] = []
    for path in SRC_ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_EXTS:
            continue
        parts = {part.lower() for part in path.relative_to(SRC_ROOT).parts}
        if parts & EXCLUDED_PARTS:
            continue
        files.append(path)
    return sorted(files)


def graph_mtime() -> float:
    return GRAPH_PATH.stat().st_mtime if GRAPH_PATH.exists() else 0.0


def stale_files(files: Iterable[Path]) -> list[Path]:
    mtime = graph_mtime()
    if mtime <= 0.0:
        return list(files)
    return [path for path in files if path.stat().st_mtime > mtime]


def add_edge(edges: list[dict[str, Any]],
             source: dict[str, Any],
             target: dict[str, Any],
             edge_file: Path,
             line: int,
             relation: str) -> None:
    edges.append({
        "source": source["id"],
        "source_label": source["label"],
        "source_file": source["source_file"],
        "source_location": source["source_location"],
        "target": target["id"],
        "target_label": target["label"],
        "target_file": target["source_file"],
        "target_location": target["source_location"],
        "edge_file": rel(edge_file),
        "edge_location": f"L{line}",
        "relation": relation,
        "confidence": "EXTRACTED",
    })


def build_graph() -> dict[str, Any]:
    files = iter_sources()
    nodes: list[dict[str, Any]] = []
    node_by_id: dict[str, dict[str, Any]] = {}
    file_node_by_path: dict[Path, dict[str, Any]] = {}
    func_nodes_by_name: dict[str, list[dict[str, Any]]] = defaultdict(list)
    file_symbols: dict[Path, list[tuple[int, dict[str, Any]]]] = defaultdict(list)
    file_call_scopes: dict[Path, list[tuple[int, dict[str, Any]]]] = defaultdict(list)

    def add_node(path: Path, label: str, line: int, node_id: str) -> dict[str, Any]:
        original = node_id
        suffix = 2
        while node_id in node_by_id:
            node_id = f"{original}_{suffix}"
            suffix += 1
        node = {
            "id": node_id,
            "label": label,
            "file_type": "code",
            "source_file": rel(path),
            "source_location": f"L{line}",
        }
        nodes.append(node)
        node_by_id[node_id] = node
        return node

    for path in files:
        file_node = add_node(path, path.name, 1, node_id_for(path))
        file_node_by_path[path] = file_node
        try:
            lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            continue
        for lineno, line in enumerate(lines, start=1):
            class_match = CLASS_RE.match(line)
            if class_match:
                name = class_match.group("name")
                node = add_node(path, f"{name}", lineno, node_id_for(path, name))
                func_nodes_by_name[name].append(node)
                file_symbols[path].append((lineno, node))
                continue
            func_match = FUNC_DEF_RE.match(line)
            if not func_match:
                continue
            full_name = func_match.group("name")
            short_name = full_name.split("::")[-1]
            if short_name in KEYWORDS:
                continue
            node = add_node(path, f"{short_name}()", lineno, node_id_for(path, short_name))
            func_nodes_by_name[short_name].append(node)
            if full_name != short_name:
                func_nodes_by_name[full_name].append(node)
            file_symbols[path].append((lineno, node))
            file_call_scopes[path].append((lineno, node))

    edges: list[dict[str, Any]] = []
    include_index: dict[str, list[Path]] = defaultdict(list)
    for path in files:
        include_index[path.name.lower()].append(path)

    for path in files:
        file_node = file_node_by_path[path]
        symbols = sorted(file_symbols[path], key=lambda item: item[0])
        call_scopes = sorted(file_call_scopes[path], key=lambda item: item[0])
        try:
            lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            continue

        for _, node in symbols:
            add_edge(edges, file_node, node, path, int(node["source_location"][1:]), "contains")

        for lineno, line in enumerate(lines, start=1):
            include = INCLUDE_RE.match(line)
            if include:
                include_name = Path(include.group("path")).name.lower()
                for target_path in include_index.get(include_name, [])[:3]:
                    add_edge(edges, file_node, file_node_by_path[target_path], path, lineno, "imports")

            current = file_node
            for idx, (func_line, node) in enumerate(call_scopes):
                next_line = call_scopes[idx + 1][0] if idx + 1 < len(call_scopes) else 10**9
                if func_line <= lineno < next_line:
                    current = node
                    break

            for call in CALL_RE.finditer(line):
                name = call.group("name")
                if name in KEYWORDS:
                    continue
                targets = func_nodes_by_name.get(name)
                if not targets:
                    continue
                for target in targets[:5]:
                    if target["id"] == current["id"] and relation_line_is_definition(line, name):
                        continue
                    add_edge(edges, current, target, path, lineno, "calls")

    callers: dict[str, list[dict[str, Any]]] = defaultdict(list)
    callees: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for edge in edges:
        callers[edge["target"]].append(edge)
        callees[edge["source"]].append(edge)

    cross_file_edges = [
        edge for edge in edges
        if edge["source_file"] != edge["target_file"]
    ]
    top_fan_in = sorted(
        ((node["id"], len(callers.get(node["id"], []))) for node in nodes),
        key=lambda item: item[1],
        reverse=True,
    )[:20]
    top_fan_out = sorted(
        ((node["id"], len(callees.get(node["id"], []))) for node in nodes),
        key=lambda item: item[1],
        reverse=True,
    )[:20]

    now = datetime.now().replace(microsecond=0).isoformat()
    return {
        "schema": "auro-interface-call-graph-v1",
        "source": "tools/graphify_interface_graph.py",
        "scope": "main graph: first-party src/ core; excludes generated files",
        "generated_at": now,
        "counts": {
            "nodes": len(nodes),
            "all_ast_edges": len(edges),
            "call_edges": sum(1 for edge in edges if edge["relation"] == "calls"),
            "cross_file_call_edges": len(cross_file_edges),
        },
        "nodes": nodes,
        "call_edges": edges,
        "cross_file_call_edges": cross_file_edges,
        "top_fan_in": top_fan_in,
        "top_fan_out": top_fan_out,
        "callers": callers,
        "callees": callees,
    }


def relation_line_is_definition(line: str, name: str) -> bool:
    return "{" in line and re.search(rf"\b{re.escape(name)}\s*\(", line) is not None


def write_report(graph: dict[str, Any]) -> None:
    node_by_id = {node["id"]: node for node in graph["nodes"]}
    relation_counts = Counter(edge["relation"] for edge in graph["call_edges"])
    cross_counts = Counter(edge["relation"] for edge in graph["cross_file_call_edges"])

    def fan_line(item: tuple[str, int]) -> str:
        node = node_by_id.get(item[0], {"label": item[0], "source_file": "", "source_location": ""})
        return f"- `{node['label']}` ({node['source_file']}:{node['source_location']}) - {item[1]}"

    lines = [
        "# Interface Call Graph",
        "",
        f"- Generated at: {graph['generated_at']}",
        f"- Scope: {graph['scope']}",
        f"- Nodes: {graph['counts']['nodes']}",
        f"- AST edges: {graph['counts']['all_ast_edges']}",
        f"- Cross-file edges: {graph['counts']['cross_file_call_edges']}",
        "",
        "## Top Fan In",
        *[fan_line(item) for item in graph["top_fan_in"]],
        "",
        "## Top Fan Out",
        *[fan_line(item) for item in graph["top_fan_out"]],
        "",
        "## Relation Counts",
        *[f"- `{name}`: {count}" for name, count in sorted(relation_counts.items())],
        "",
        "## Cross-file Relation Counts",
        *[f"- `{name}`: {count}" for name, count in sorted(cross_counts.items())],
        "",
    ]
    REPORT_PATH.write_text("\n".join(lines), encoding="utf-8")


def check() -> int:
    files = iter_sources()
    stale = stale_files(files)
    if not GRAPH_PATH.exists():
        print(f"Graphify interface graph missing: {GRAPH_PATH}")
        print("Run: python tools/graphify_interface_graph.py rebuild")
        return 2
    if stale:
        print(f"Graphify interface graph stale: {len(stale)} src files newer than {GRAPH_PATH.name}")
        for path in stale[:20]:
            print(f"- {path.relative_to(ROOT)}")
        if len(stale) > 20:
            print(f"- ... {len(stale) - 20} more")
        print("Run: python tools/graphify_interface_graph.py rebuild")
        return 1
    print(f"Graphify interface graph fresh: {GRAPH_PATH}")
    return 0


def rebuild() -> int:
    MAIN_OUT_DIR.mkdir(parents=True, exist_ok=True)
    graph = build_graph()
    GRAPH_PATH.write_text(json.dumps(graph, ensure_ascii=False, indent=2), encoding="utf-8")
    write_report(graph)
    if ARTIFACT_MANAGER.exists():
        subprocess.run([sys.executable, str(ARTIFACT_MANAGER), "enforce"], cwd=str(ROOT), text=True)
    print(f"Wrote {GRAPH_PATH}")
    print(f"Wrote {REPORT_PATH}")
    print(f"Counts: {graph['counts']}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("check", help="Report whether src/ is newer than the interface graph")
    sub.add_parser("rebuild", help="Rebuild the narrow src/ interface graph")
    args = parser.parse_args()
    if args.cmd == "check":
        return check()
    if args.cmd == "rebuild":
        return rebuild()
    raise AssertionError(args.cmd)


if __name__ == "__main__":
    raise SystemExit(main())
