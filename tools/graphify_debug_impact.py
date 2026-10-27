#!/usr/bin/env python3
"""Query Aris's Graphify interface call graph for debugging impact analysis."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any
import re


ROOT = Path(__file__).resolve().parents[1]
GRAPH_PATH = ROOT / "graphify-out" / "main" / "interface-call-graph.json"
REFRESH_SCRIPT = ROOT / "tools" / "graphify_interface_graph.py"
SOURCE_EXTS = {".h", ".hpp", ".hh", ".cpp", ".cc", ".cxx"}
EXCLUDED_PARTS = {"webui", "generated", "__pycache__"}
KEYWORDS = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
    "catch",
    "new",
    "delete",
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


def iter_sources() -> list[Path]:
    src_root = ROOT / "src"
    files: list[Path] = []
    for path in src_root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_EXTS:
            continue
        parts = {part.lower() for part in path.relative_to(src_root).parts}
        if parts & EXCLUDED_PARTS:
            continue
        files.append(path)
    return sorted(files)


def stale_sources(path: Path) -> list[Path]:
    if not path.exists():
        return iter_sources()
    graph_mtime = path.stat().st_mtime
    return [src for src in iter_sources() if src.stat().st_mtime > graph_mtime]


def ensure_fresh_graph(path: Path, rebuild_if_stale: bool, allow_stale: bool) -> None:
    stale = stale_sources(path)
    if not stale:
        return
    if rebuild_if_stale:
        if not REFRESH_SCRIPT.exists():
            raise SystemExit(f"Cannot refresh Graphify interface graph; missing {REFRESH_SCRIPT}")
        result = subprocess.run(
            [sys.executable, str(REFRESH_SCRIPT), "rebuild"],
            cwd=str(ROOT),
            text=True,
        )
        if result.returncode != 0:
            raise SystemExit(result.returncode)
        return
    if allow_stale:
        print(
            f"WARNING: Graphify interface graph stale ({len(stale)} newer src files); "
            "continuing because --allow-stale was set."
        )
        return
    print(f"Graphify interface graph stale: {len(stale)} src files are newer than {path.name}")
    for src in stale[:20]:
        print(f"- {src.relative_to(ROOT)}")
    if len(stale) > 20:
        print(f"- ... {len(stale) - 20} more")
    print("Run: python tools/graphify_interface_graph.py rebuild")
    print("Or rerun this command with --rebuild-if-stale.")
    raise SystemExit(2)


def load_graph(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise SystemExit(
            f"Graph not found: {path}\n"
            "Run the Graphify interface build first, or mark Graphify unavailable/stale."
        )
    return json.loads(path.read_text(encoding="utf-8"))


def score_node(node: dict[str, Any], terms: list[str]) -> int:
    node_id = str(node.get("id", "")).lower()
    label = str(node.get("label", "")).lower()
    source = str(node.get("source_file", "")).lower()
    haystack = f"{node_id} {label} {source} {node.get('source_location', '')}".lower()
    score = sum(1 for term in terms if term in haystack)
    joined = " ".join(terms)
    compact = joined.replace(" ", "")
    if node_id == compact or label == joined:
        score += 100
    if node_id.endswith(f"_{compact}") or label.startswith(joined):
        score += 40
    if source.endswith(f"{compact}.h") or source.endswith(f"{compact}.cpp"):
        score += 20
    return score


def choose_nodes(graph: dict[str, Any], query: str, limit: int) -> list[dict[str, Any]]:
    terms = [part.lower() for part in query.replace("::", " ").replace(".", " ").split() if part]
    if not terms:
        terms = [query.lower()]
    scored = [(score_node(node, terms), node) for node in graph.get("nodes", [])]
    scored = [(score, node) for score, node in scored if score > 0]
    scored.sort(key=lambda item: (-item[0], item[1].get("source_file", ""), item[1].get("label", "")))
    return [node for _, node in scored[:limit]]


def nearest_function(lines: list[str], line_index: int) -> tuple[str, int] | None:
    for idx in range(line_index, -1, -1):
        match = FUNC_DEF_RE.match(lines[idx])
        if match:
            name = match.group("name").split("::")[-1]
            if name not in KEYWORDS:
                return name, idx + 1
    return None


def find_source_mentions(query: str, limit: int) -> list[dict[str, Any]]:
    if not query:
        return []
    mentions: list[dict[str, Any]] = []
    lowered = query.lower()
    for path in iter_sources():
        try:
            lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
        except OSError:
            continue
        for idx, line in enumerate(lines):
            if lowered not in line.lower():
                continue
            scope = nearest_function(lines, idx)
            mentions.append({
                "path": path,
                "line": idx + 1,
                "text": line.strip(),
                "scope": scope,
            })
            if len(mentions) >= limit:
                return mentions
    return mentions


def fmt_node(node: dict[str, Any]) -> str:
    loc = node.get("source_location") or ""
    suffix = f":{loc}" if loc else ""
    return f"{node.get('label', node.get('id'))} [{node.get('id')}] {node.get('source_file', '')}{suffix}"


def fmt_edge(edge: dict[str, Any], incoming: bool) -> str:
    if incoming:
        loc = edge.get("edge_location") or edge.get("source_location") or ""
        suffix = f":{loc}" if loc else ""
        return (
            f"{edge.get('source_label')} [{edge.get('source')}] "
            f"from {edge.get('source_file')}{suffix}"
        )
    loc = edge.get("target_location") or edge.get("edge_location") or ""
    suffix = f":{loc}" if loc else ""
    target_file = edge.get("target_file") or edge.get("edge_file") or ""
    return f"{edge.get('target_label')} [{edge.get('target')}] at {target_file}{suffix}"


def module_hint(path: str) -> str:
    lowered = path.lower()
    if "database" in lowered:
        return "Database tests and SQL/FTS diagnostics"
    if "asyncworker" in lowered:
        return "generation-token, cancellation, and worker handoff diagnostics"
    if "filesystemwatcher" in lowered:
        return "scan/watch diagnostics and incremental update tests"
    if "waveform" in lowered:
        return "waveform decode/cache tests and render diagnostics"
    if "audioengine" in lowered:
        return "playback/load/seek diagnostics and audio tests"
    if "cover" in lowered:
        return "cover cache, texture upload, and memory budget diagnostics"
    if "main.cpp" in lowered or "moduleui" in lowered or "uirenderer" in lowered:
        return "UI smoke, diagnostics assertions, and screenshot checks when visual"
    return "focused unit/integration coverage for the affected file"


def print_impact(graph: dict[str, Any], query: str, limit: int) -> int:
    nodes = choose_nodes(graph, query, limit)
    if not nodes:
        print(f"No graph nodes matched: {query}")
        print("Graph schema note: the main interface graph indexes files, classes/structs, functions, includes, and calls; it does not index local variables as target nodes.")
        mentions = find_source_mentions(query, limit=10)
        if mentions:
            print()
            print("Source mentions for fallback:")
            suggested_scopes: set[str] = set()
            for item in mentions:
                rel_path = item["path"].relative_to(ROOT)
                scope = item["scope"]
                if scope:
                    scope_name, scope_line = scope
                    suggested_scopes.add(scope_name)
                    scope_text = f"nearest function `{scope_name}` at L{scope_line}"
                else:
                    scope_text = "nearest function unresolved"
                print(f"- {rel_path}:L{item['line']} ({scope_text})")
                print(f"  {item['text'][:180]}")
            if suggested_scopes:
                print()
                print("Suggested Graphify fallback target(s):")
                for scope in sorted(suggested_scopes):
                    print(f"- python tools/graphify_debug_impact.py {scope} --rebuild-if-stale")
        else:
            print("No source mentions found either.")
        print()
        print(f"Run rg fallback: rg -n \"{query}\" src tools docs")
        print("Record Graphify as unavailable for this symbol, then continue with nearest function + rg/source scan evidence.")
        return 1

    callers = graph.get("callers", {})
    callees = graph.get("callees", {})
    cross = graph.get("cross_file_call_edges", [])

    print(f"Graphify Debug Impact: {query}")
    print(f"Graph: {GRAPH_PATH}")
    print(f"Counts: {graph.get('counts', {})}")
    print()

    affected_files: set[str] = set()
    verification_hints: set[str] = set()

    for node in nodes:
        node_id = node.get("id", "")
        print(f"## Target: {fmt_node(node)}")
        if node.get("source_file"):
            affected_files.add(str(node["source_file"]).replace("\\", "/"))
            verification_hints.add(module_hint(str(node["source_file"])))

        incoming = callers.get(node_id, [])
        outgoing = callees.get(node_id, [])
        cross_related = [
            edge
            for edge in cross
            if edge.get("source") == node_id or edge.get("target") == node_id
        ]

        print(f"Direct callers: {len(incoming)}")
        for edge in incoming[:20]:
            print(f"- {fmt_edge(edge, incoming=True)}")
            if edge.get("source_file"):
                affected_files.add(str(edge["source_file"]).replace("\\", "/"))
                verification_hints.add(module_hint(str(edge["source_file"])))

        print(f"Direct callees: {len(outgoing)}")
        for edge in outgoing[:20]:
            print(f"- {fmt_edge(edge, incoming=False)}")
            if edge.get("target_file"):
                affected_files.add(str(edge["target_file"]).replace("\\", "/"))
                verification_hints.add(module_hint(str(edge["target_file"])))

        if cross_related:
            print(f"Cross-file call edges: {len(cross_related)}")
            for edge in cross_related[:20]:
                print(
                    f"- {edge.get('source_label')} ({edge.get('source_file')})"
                    f" -> {edge.get('target_label')} ({edge.get('target_file')})"
                )
        print()

    print("## Blast Radius Checklist")
    for path in sorted(affected_files):
        print(f"- {path}")
    if not affected_files:
        print("- No affected files resolved from graph; inspect source manually.")
    print()

    print("## Verification Targets")
    for hint in sorted(verification_hints):
        print(f"- {hint}")
    if not verification_hints:
        print("- Add focused coverage for the changed interface and its callers.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("symbol", help="Function/class/interface/file symbol to inspect")
    parser.add_argument("--limit", type=int, default=5, help="Maximum matching target nodes")
    parser.add_argument("--allow-stale", action="store_true", help="Warn but continue if src/ is newer than the graph")
    parser.add_argument("--rebuild-if-stale", action="store_true", help="Refresh the narrow src/ interface graph before querying")
    args = parser.parse_args()

    ensure_fresh_graph(GRAPH_PATH, args.rebuild_if_stale, args.allow_stale)
    graph = load_graph(GRAPH_PATH)
    return print_impact(graph, args.symbol, args.limit)


if __name__ == "__main__":
    raise SystemExit(main())
