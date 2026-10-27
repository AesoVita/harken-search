#!/usr/bin/env python3
"""Auto-refresh Aris's narrow Graphify interface graph after src/ edits."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN_ROOT = ROOT / "src"
MAIN_REFRESH_SCRIPT = ROOT / "tools" / "graphify_interface_graph.py"
AUDIT_REFRESH_SCRIPT = ROOT / "tools" / "graphify_audit_graph.py"
EXTENDED_REFRESH_SCRIPT = ROOT / "tools" / "graphify_extended_graph.py"
MAIN_SOURCE_EXTS = {".h", ".hpp", ".hh", ".cpp", ".cc", ".cxx"}
EXTENDED_SOURCE_EXTS = {
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
MAIN_EXCLUDED_PARTS = {"webui", "generated", "__pycache__"}
EXTENDED_EXCLUDED_PARTS = {"node_modules", "__pycache__", "dist", "build", ".git", ".next", ".astro", "coverage"}
EXTENDED_ROOTS = [
    ROOT / "audiox-workstation",
    ROOT / "python_backend",
    ROOT / "web-ui-demo",
    ROOT / "website",
]

def iter_main_sources() -> list[Path]:
    files: list[Path] = []
    for path in MAIN_ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in MAIN_SOURCE_EXTS:
            continue
        parts = {part.lower() for part in path.relative_to(MAIN_ROOT).parts}
        if parts & MAIN_EXCLUDED_PARTS:
            continue
        files.append(path)
    return sorted(files)


def iter_extended_sources() -> list[Path]:
    files: list[Path] = []
    for base in EXTENDED_ROOTS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in EXTENDED_SOURCE_EXTS:
                continue
            parts = {part.lower() for part in path.relative_to(ROOT).parts}
            if parts & EXTENDED_EXCLUDED_PARTS:
                continue
            files.append(path)
    return sorted(files)


def snapshot(scope: str) -> dict[str, tuple[int, int]]:
    state: dict[str, tuple[int, int]] = {}
    files = iter_main_sources() if scope == "main" else iter_extended_sources()
    for path in files:
        try:
            stat = path.stat()
        except OSError:
            continue
        state[str(path.relative_to(ROOT)).replace("\\", "/")] = (stat.st_mtime_ns, stat.st_size)
    return state


def changed_files(before: dict[str, tuple[int, int]], after: dict[str, tuple[int, int]]) -> list[str]:
    keys = set(before) | set(after)
    return sorted(path for path in keys if before.get(path) != after.get(path))


def rebuild(scope: str) -> int:
    refresh_script = MAIN_REFRESH_SCRIPT if scope == "main" else EXTENDED_REFRESH_SCRIPT
    result = subprocess.run(
        [sys.executable, str(refresh_script), "rebuild"],
        cwd=str(ROOT),
        text=True,
    )
    if result.returncode != 0 or scope != "main":
        return result.returncode

    audit_result = subprocess.run(
        [sys.executable, str(AUDIT_REFRESH_SCRIPT), "rebuild"],
        cwd=str(ROOT),
        text=True,
    )
    return audit_result.returncode


def watch(scope: str, poll_seconds: float, debounce_seconds: float) -> int:
    refresh_script = MAIN_REFRESH_SCRIPT if scope == "main" else EXTENDED_REFRESH_SCRIPT
    if not refresh_script.exists():
        print(f"Graphify refresh script missing: {refresh_script}", file=sys.stderr)
        return 2

    current = snapshot(scope)
    watch_label = str(MAIN_ROOT) if scope == "main" else ", ".join(str(path) for path in EXTENDED_ROOTS if path.exists())
    print(f"Watching {watch_label} for Graphify {scope} refreshes. Press Ctrl+C to stop.")
    try:
        while True:
            time.sleep(poll_seconds)
            latest = snapshot(scope)
            changed = changed_files(current, latest)
            if not changed:
                continue

            print(f"Detected {len(changed)} file change(s); waiting {debounce_seconds:.1f}s...")
            time.sleep(debounce_seconds)
            latest = snapshot(scope)
            changed = changed_files(current, latest)
            if not changed:
                current = latest
                continue

            print(f"Refreshing Graphify {scope} graph:")
            for path in changed[:20]:
                print(f"- {path}")
            if len(changed) > 20:
                print(f"- ... {len(changed) - 20} more")

            code = rebuild(scope)
            if code != 0:
                print(f"Graphify refresh failed with exit code {code}. Fix before continuing.", file=sys.stderr)
                current = latest
                continue

            print("Graphify refresh complete.")
            current = latest
    except KeyboardInterrupt:
        print("Stopped Graphify auto-refresh.")
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--once", action="store_true", help="Refresh once and exit")
    parser.add_argument("--scope", choices=("main", "extended"), default="main", help="Graph scope to refresh")
    parser.add_argument("--poll-seconds", type=float, default=1.0, help="Filesystem polling interval")
    parser.add_argument("--debounce-seconds", type=float, default=2.0, help="Wait for edit bursts to settle")
    args = parser.parse_args()

    if args.once:
        return rebuild(args.scope)
    return watch(args.scope, args.poll_seconds, args.debounce_seconds)


if __name__ == "__main__":
    raise SystemExit(main())
