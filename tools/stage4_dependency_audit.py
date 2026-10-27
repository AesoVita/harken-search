"""Deterministic dependency-direction audit for the declared Stage 4 scope."""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
from pathlib import Path
from typing import Any


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inl", ".mm"}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def _relative(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def _source_files(root: Path) -> list[Path]:
    source_root = root / "src"
    if not source_root.is_dir():
        return []
    return sorted(
        (
            path
            for path in source_root.rglob("*")
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
        ),
        key=lambda path: _relative(root, path),
    )


def _matches(relative_path: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatchcase(relative_path, pattern) for pattern in patterns)


def _resolve_include(root: Path, source: Path, include: str) -> str | None:
    normalized = Path(include.replace("\\", "/"))
    candidates = (source.parent / normalized, root / "src" / normalized, root / normalized)
    for candidate in candidates:
        if not candidate.is_file():
            continue
        try:
            return _relative(root, candidate.resolve())
        except ValueError:
            continue
    return None


def _canonical_cycle(nodes: list[str]) -> tuple[str, ...]:
    body = nodes[:-1]
    rotations = [tuple(body[index:] + body[:index]) for index in range(len(body))]
    canonical = min(rotations)
    return canonical + (canonical[0],)


def _find_cycles(adjacency: dict[str, set[str]]) -> list[list[str]]:
    active: list[str] = []
    active_index: dict[str, int] = {}
    visited: set[str] = set()
    cycles: set[tuple[str, ...]] = set()

    def visit(node: str) -> None:
        visited.add(node)
        active_index[node] = len(active)
        active.append(node)
        for target in sorted(adjacency.get(node, set())):
            if target in active_index:
                start = active_index[target]
                cycles.add(_canonical_cycle(active[start:] + [target]))
            elif target not in visited:
                visit(target)
        active.pop()
        active_index.pop(node)

    for module in sorted(adjacency):
        if module not in visited:
            visit(module)
    return [list(cycle) for cycle in sorted(cycles)]


def audit_repository(root: Path, rules: dict[str, Any]) -> dict[str, Any]:
    root = root.resolve()
    module_rules = rules.get("modules", {})
    if not isinstance(module_rules, dict):
        raise ValueError("rules.modules must be an object")

    source_files = _source_files(root)
    relative_files = {_relative(root, path): path for path in source_files}
    scope_patterns = rules.get("scope")
    if scope_patterns is None:
        scope_patterns = [
            pattern
            for module in module_rules.values()
            for pattern in module.get("files", [])
        ]

    scoped_files = sorted(
        relative_path
        for relative_path in relative_files
        if _matches(relative_path, scope_patterns)
    )
    memberships: dict[str, str] = {}
    membership_errors: list[dict[str, Any]] = []
    for relative_path in scoped_files:
        matched_modules = sorted(
            name
            for name, module in module_rules.items()
            if _matches(relative_path, module.get("files", []))
        )
        if len(matched_modules) != 1:
            membership_errors.append(
                {"file": relative_path, "matched_modules": matched_modules}
            )
            continue
        memberships[relative_path] = matched_modules[0]

    edges: list[dict[str, str]] = []
    for source_relative in sorted(memberships):
        source_path = relative_files[source_relative]
        source_module = memberships[source_relative]
        for line in source_path.read_text(encoding="utf-8", errors="replace").splitlines():
            match = INCLUDE_RE.match(line)
            if not match:
                continue
            include = match.group(1)
            target_relative = _resolve_include(root, source_path, include)
            if target_relative is None or target_relative not in memberships:
                continue
            target_module = memberships[target_relative]
            if source_module == target_module:
                continue
            edges.append(
                {
                    "source": source_module,
                    "target": target_module,
                    "source_file": source_relative,
                    "target_file": target_relative,
                    "include": include.replace("\\", "/"),
                }
            )

    edge_key = lambda edge: (
        edge["source"],
        edge["target"],
        edge["source_file"],
        edge["target_file"],
        edge["include"],
    )
    edges = sorted(edges, key=edge_key)
    forbidden_edges = []
    for edge in edges:
        allowed = module_rules[edge["source"]].get("may_depend_on", [])
        if edge["target"] not in allowed:
            forbidden_edges.append(edge)

    adjacency = {name: set() for name in module_rules}
    for edge in edges:
        adjacency[edge["source"]].add(edge["target"])

    return {
        "scope_files": scoped_files,
        "memberships": dict(sorted(memberships.items())),
        "edges": edges,
        "forbidden_edges": forbidden_edges,
        "cycles": _find_cycles(adjacency),
        "membership_errors": membership_errors,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--rules", type=Path, required=True)
    parser.add_argument("--json-out", type=Path)
    args = parser.parse_args()

    rules = json.loads(args.rules.read_text(encoding="utf-8"))
    result = audit_repository(args.root, rules)
    summary = {
        "scope_files": len(result["scope_files"]),
        "edges": len(result["edges"]),
        "forbidden_edges": len(result["forbidden_edges"]),
        "cycles": len(result["cycles"]),
        "membership_errors": len(result["membership_errors"]),
    }
    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(
            json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    has_failure = any(
        result[key]
        for key in ("forbidden_edges", "cycles", "membership_errors")
    )
    return 1 if has_failure else 0


if __name__ == "__main__":
    raise SystemExit(main())
