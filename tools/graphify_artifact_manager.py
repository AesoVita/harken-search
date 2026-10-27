#!/usr/bin/env python3
"""Manage split Graphify artifacts for Aris.

Main graph:
- graphify-out/main/GRAPH_REPORT.md
- graphify-out/main/graph.json
- graphify-out/main/INTERFACE_CALL_GRAPH.md
- graphify-out/main/interface-call-graph.json
- graphify-out/main/AUDIT_CALL_GRAPH.md
- graphify-out/main/audit-call-graph.json
- graphify-out/main/UNKNOWN_CALLS.md
- graphify-out/main/DYNAMIC_CALLS.md

Extended graph:
- graphify-out/extended/GRAPH_REPORT.md
- graphify-out/extended/graph.json

This script prepares the split layout, migrates legacy top-level artifacts,
and writes a small index so agents do not treat graphify-out/ as a mixed bag.
"""

from __future__ import annotations

import argparse
import shutil
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "graphify-out"
MAIN_DIR = OUT_DIR / "main"
EXTENDED_DIR = OUT_DIR / "extended"
QUARANTINE_DIR = OUT_DIR / "quarantine"
INDEX_PATH = OUT_DIR / "README.md"


def write_index() -> None:
    text = """# Graphify Artifacts

This directory is split into two scopes and they must not be mixed in the same report.

## Main graph

- Scope: `src/` core product code only
- Paths:
  - `graphify-out/main/GRAPH_REPORT.md`
  - `graphify-out/main/graph.json`
  - `graphify-out/main/INTERFACE_CALL_GRAPH.md`
  - `graphify-out/main/interface-call-graph.json`
  - `graphify-out/main/AUDIT_CALL_GRAPH.md`
  - `graphify-out/main/audit-call-graph.json`
  - `graphify-out/main/UNKNOWN_CALLS.md`
  - `graphify-out/main/DYNAMIC_CALLS.md`
- Default for impact analysis, task dispatch, and code review
- Audit refresh:
  - `python tools/graphify_audit_graph.py rebuild`
  - Add `--fail-on-unknown` when a task requires zero unresolved call sites.
- Automatic refresh:
  - `python tools/graphify_auto_refresh.py`
  - `python tools/graphify_auto_refresh.py --once`

## Extended graph

- Scope: optional adjacent workspaces such as `audiox-workstation/`, `python_backend/`, `web-ui-demo/`, `website/`
- Paths:
  - `graphify-out/extended/GRAPH_REPORT.md`
  - `graphify-out/extended/graph.json`
- Use only when the task explicitly targets those side systems
- Automatic refresh:
  - `python tools/graphify_auto_refresh.py --scope extended`
  - `python tools/graphify_auto_refresh.py --scope extended --once`

## Rules

- Do not write a mixed-scope `graphify-out/GRAPH_REPORT.md`.
- Do not write a mixed-scope `graphify-out/graph.json`.
- If a graph is stale or unavailable, record that explicitly and fall back to `rg` + source reading.
- If top-level mixed artifacts keep reappearing, audit old background hooks with `powershell -ExecutionPolicy Bypass -File tools/graphify_legacy_hook_guard.ps1`.
"""
    INDEX_PATH.write_text(text, encoding="utf-8")


def prepare_layout() -> None:
    MAIN_DIR.mkdir(parents=True, exist_ok=True)
    EXTENDED_DIR.mkdir(parents=True, exist_ok=True)
    QUARANTINE_DIR.mkdir(parents=True, exist_ok=True)
    write_index()


def move_if_present(src: Path, dst: Path) -> None:
    if not src.exists():
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        backup = dst.with_name(f"{dst.stem}.bak{dst.suffix}")
        shutil.move(str(dst), str(backup))
    shutil.move(str(src), str(dst))


def migrate_legacy() -> None:
    prepare_layout()
    move_if_present(OUT_DIR / "GRAPH_REPORT.md", EXTENDED_DIR / "GRAPH_REPORT.md")
    move_if_present(OUT_DIR / "graph.json", EXTENDED_DIR / "graph.json")
    move_if_present(OUT_DIR / "INTERFACE_CALL_GRAPH.md", MAIN_DIR / "INTERFACE_CALL_GRAPH.md")
    move_if_present(OUT_DIR / "interface-call-graph.json", MAIN_DIR / "interface-call-graph.json")


def enforce() -> None:
    prepare_layout()
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    for name in ("GRAPH_REPORT.md", "graph.json"):
        src = OUT_DIR / name
        if not src.exists():
            continue
        dst = QUARANTINE_DIR / f"{src.stem}-{timestamp}{src.suffix}"
        shutil.move(str(src), str(dst))


def status() -> int:
    prepare_layout()
    print("Graphify artifact layout")
    print(f"- main: {MAIN_DIR}")
    print(f"  - GRAPH_REPORT.md: {'yes' if (MAIN_DIR / 'GRAPH_REPORT.md').exists() else 'no'}")
    print(f"  - graph.json: {'yes' if (MAIN_DIR / 'graph.json').exists() else 'no'}")
    print(f"  - INTERFACE_CALL_GRAPH.md: {'yes' if (MAIN_DIR / 'INTERFACE_CALL_GRAPH.md').exists() else 'no'}")
    print(f"  - interface-call-graph.json: {'yes' if (MAIN_DIR / 'interface-call-graph.json').exists() else 'no'}")
    print(f"  - AUDIT_CALL_GRAPH.md: {'yes' if (MAIN_DIR / 'AUDIT_CALL_GRAPH.md').exists() else 'no'}")
    print(f"  - audit-call-graph.json: {'yes' if (MAIN_DIR / 'audit-call-graph.json').exists() else 'no'}")
    print(f"  - UNKNOWN_CALLS.md: {'yes' if (MAIN_DIR / 'UNKNOWN_CALLS.md').exists() else 'no'}")
    print(f"  - DYNAMIC_CALLS.md: {'yes' if (MAIN_DIR / 'DYNAMIC_CALLS.md').exists() else 'no'}")
    print(f"- extended: {EXTENDED_DIR}")
    print(f"  - GRAPH_REPORT.md: {'yes' if (EXTENDED_DIR / 'GRAPH_REPORT.md').exists() else 'no'}")
    print(f"  - graph.json: {'yes' if (EXTENDED_DIR / 'graph.json').exists() else 'no'}")
    quarantine_count = len(list(QUARANTINE_DIR.glob("*"))) if QUARANTINE_DIR.exists() else 0
    print(f"- quarantine: {QUARANTINE_DIR} ({quarantine_count} file(s))")
    print(f"- legacy top-level GRAPH_REPORT.md: {'yes' if (OUT_DIR / 'GRAPH_REPORT.md').exists() else 'no'}")
    print(f"- legacy top-level graph.json: {'yes' if (OUT_DIR / 'graph.json').exists() else 'no'}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("prepare", help="Create split artifact directories and index")
    sub.add_parser("migrate", help="Move legacy top-level artifacts into main/ or extended/")
    sub.add_parser("enforce", help="Quarantine stray top-level mixed-scope artifacts")
    sub.add_parser("status", help="Print split artifact status")
    args = parser.parse_args()

    if args.cmd == "prepare":
        prepare_layout()
        print(f"Prepared Graphify artifact layout under {OUT_DIR}")
        return 0
    if args.cmd == "migrate":
        migrate_legacy()
        print(f"Migrated legacy Graphify artifacts under {OUT_DIR}")
        return 0
    if args.cmd == "enforce":
        enforce()
        print(f"Enforced split Graphify layout under {OUT_DIR}")
        return 0
    if args.cmd == "status":
        return status()
    raise AssertionError(args.cmd)


if __name__ == "__main__":
    raise SystemExit(main())
