#!/usr/bin/env python3
"""Build or check Aris's extended Graphify artifact set.

This intentionally scans only allowlisted side-system directories and excludes
heavy/generated/vendor paths. It produces file-level relationship artifacts for
adjacent systems such as `audiox-workstation/`, `python_backend/`,
`web-ui-demo/`, and `website/`.
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
OUT_DIR = ROOT / "graphify-out" / "extended"
GRAPH_PATH = OUT_DIR / "graph.json"
REPORT_PATH = OUT_DIR / "GRAPH_REPORT.md"
ARTIFACT_MANAGER = ROOT / "tools" / "graphify_artifact_manager.py"

INCLUDE_DIRS = [
    "audiox-workstation",
    "python_backend",
    "web-ui-demo",
    "website",
]
SOURCE_EXTS = {
    ".py",
    ".ts",
    ".tsx",
    ".js",
    ".jsx",
    ".mjs",
    ".cjs",
    ".json",
    ".md",
    ".cpp",
    ".cc",
    ".cxx",
    ".h",
    ".hpp",
}
EXCLUDED_PARTS = {
    "node_modules",
    "__pycache__",
    "dist",
    "build",
    ".git",
    ".next",
    ".astro",
    "coverage",
    "graphify-out",
}

IMPORT_PATTERNS = [
    re.compile(r'^\s*import\s+.*?\s+from\s+[\'"](?P<path>[^\'"]+)[\'"]', re.MULTILINE),
    re.compile(r'^\s*import\s+[\'"](?P<path>[^\'"]+)[\'"]', re.MULTILINE),
    re.compile(r'^\s*export\s+.*?\s+from\s+[\'"](?P<path>[^\'"]+)[\'"]', re.MULTILINE),
    re.compile(r'^\s*const\s+.*?=\s*require\([\'"](?P<path>[^\'"]+)[\'"]\)', re.MULTILINE),
    re.compile(r'^\s*from\s+(?P<path>[A-Za-z0-9_./-]+)\s+import\b', re.MULTILINE),
    re.compile(r'^\s*import\s+(?P<path>[A-Za-z0-9_./-]+)\b', re.MULTILINE),
    re.compile(r'^\s*#\s*include\s+"(?P<path>[^"]+)"', re.MULTILINE),
]


def rel(path: Path) -> str:
    return str(path.relative_to(ROOT)).replace("\\", "/")


def node_id_for(path: Path) -> str:
    return re.sub(r"[^a-z0-9_]+", "_", rel(path).lower()).strip("_")


def iter_sources() -> list[Path]:
    files: list[Path] = []
    for include_dir in INCLUDE_DIRS:
        base = ROOT / include_dir
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SOURCE_EXTS:
                continue
            parts = {part.lower() for part in path.relative_to(ROOT).parts}
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


def resolve_import(source: Path, raw: str, path_by_rel: dict[str, Path]) -> Path | None:
    raw = raw.strip()
    if not raw or raw.startswith(("http://", "https://")):
        return None

    candidates: list[Path] = []
    if raw.startswith("."):
        base = (source.parent / raw).resolve()
        candidates.append(base)
        for suffix in SOURCE_EXTS:
            candidates.append(base.with_suffix(suffix))
        for suffix in SOURCE_EXTS:
            candidates.append(base / f"index{suffix}")
    else:
        normalized = raw.replace(".", "/")
        for path in path_by_rel.values():
            rel_path = rel(path)
            if rel_path.endswith(raw) or rel_path.endswith(f"{raw}.py") or rel_path.endswith(f"{normalized}.py"):
                candidates.append(path)

    for candidate in candidates:
        try:
            candidate = candidate.resolve()
        except OSError:
            continue
        if candidate in path_by_rel.values():
            return candidate
    return None


def build_graph() -> dict[str, Any]:
    files = iter_sources()
    path_by_rel = {rel(path): path for path in files}
    nodes: list[dict[str, Any]] = []
    node_by_path: dict[Path, dict[str, Any]] = {}
    edges: list[dict[str, Any]] = []

    for path in files:
        node = {
            "id": node_id_for(path),
            "label": path.name,
            "kind": "file",
            "source_file": rel(path),
            "source_location": "L1",
        }
        nodes.append(node)
        node_by_path[path] = node

    for path in files:
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for pattern in IMPORT_PATTERNS:
            for match in pattern.finditer(text):
                target_path = resolve_import(path, match.group("path"), path_by_rel)
                if not target_path or target_path == path:
                    continue
                edges.append({
                    "source": node_by_path[path]["id"],
                    "source_label": node_by_path[path]["label"],
                    "source_file": rel(path),
                    "target": node_by_path[target_path]["id"],
                    "target_label": node_by_path[target_path]["label"],
                    "target_file": rel(target_path),
                    "relation": "imports",
                    "confidence": "EXTRACTED",
                })

    callers: dict[str, list[dict[str, Any]]] = defaultdict(list)
    callees: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for edge in edges:
        callers[edge["target"]].append(edge)
        callees[edge["source"]].append(edge)

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
        "schema": "auro-extended-graph-v1",
        "source": "tools/graphify_extended_graph.py",
        "scope": "extended graph: side-system allowlist only",
        "include_dirs": INCLUDE_DIRS,
        "generated_at": now,
        "counts": {
            "nodes": len(nodes),
            "edges": len(edges),
        },
        "nodes": nodes,
        "edges": edges,
        "callers": callers,
        "callees": callees,
        "top_fan_in": top_fan_in,
        "top_fan_out": top_fan_out,
    }


def write_report(graph: dict[str, Any]) -> None:
    node_by_id = {node["id"]: node for node in graph["nodes"]}
    relation_counts = Counter(edge["relation"] for edge in graph["edges"])

    def fan_line(item: tuple[str, int]) -> str:
        node = node_by_id[item[0]]
        return f"- `{node['source_file']}` - {item[1]}"

    lines = [
        "# Extended Graph Report",
        "",
        f"- Generated at: {graph['generated_at']}",
        f"- Scope: {graph['scope']}",
        f"- Include dirs: {', '.join(graph['include_dirs'])}",
        f"- Nodes: {graph['counts']['nodes']}",
        f"- Edges: {graph['counts']['edges']}",
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
        "## Guardrail",
        "- This report is for side systems only. Do not copy or merge it into `graphify-out/main/`.",
    ]
    REPORT_PATH.write_text("\n".join(lines), encoding="utf-8")


def check() -> int:
    files = iter_sources()
    stale = stale_files(files)
    if not GRAPH_PATH.exists():
        print(f"Extended graph missing: {GRAPH_PATH}")
        print("Run: python tools/graphify_extended_graph.py rebuild")
        return 2
    if stale:
        print(f"Extended graph stale: {len(stale)} files newer than {GRAPH_PATH.name}")
        for path in stale[:20]:
            print(f"- {path.relative_to(ROOT)}")
        if len(stale) > 20:
            print(f"- ... {len(stale) - 20} more")
        print("Run: python tools/graphify_extended_graph.py rebuild")
        return 1
    print(f"Extended graph fresh: {GRAPH_PATH}")
    return 0


def rebuild() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
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
    sub.add_parser("check", help="Report whether extended sources are newer than the graph")
    sub.add_parser("rebuild", help="Rebuild the extended graph")
    args = parser.parse_args()
    if args.cmd == "check":
        return check()
    if args.cmd == "rebuild":
        return rebuild()
    raise AssertionError(args.cmd)


if __name__ == "__main__":
    raise SystemExit(main())
