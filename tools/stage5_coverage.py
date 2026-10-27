#!/usr/bin/env python3
"""Collect and summarize the repeatable ARIS Stage 5 native C++ coverage baseline.

Microsoft's collector reports each statically linked test executable as a separate
module.  Because the same production source can be linked into several tests, the
raw Cobertura root percentage double-counts those source lines.  This tool keeps
the raw report and also emits the Stage 5 metric: the union of unique executable
lines under ``src/``, with a line covered when any measured test binary hits it.
"""

from __future__ import annotations

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONSOLE = Path(
    r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE"
    r"\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe"
)


def discover_coverage_console(explicit: Path | None = None) -> Path:
    """Return an installed Microsoft native code-coverage console."""
    requested = explicit or (
        Path(os.environ["ARIS_CODE_COVERAGE_CONSOLE"])
        if os.environ.get("ARIS_CODE_COVERAGE_CONSOLE")
        else None
    )
    if requested:
        requested = requested.expanduser().resolve()
        if requested.is_file():
            return requested
        raise FileNotFoundError(f"code coverage console not found: {requested}")

    candidates = [DEFAULT_CONSOLE]
    for base in (Path(r"C:\Program Files"), Path(r"C:\Program Files (x86)")):
        visual_studio = base / "Microsoft Visual Studio"
        if visual_studio.is_dir():
            candidates.extend(
                visual_studio.glob(
                    "*/*/Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/"
                    "Microsoft.CodeCoverage.Console.exe"
                )
            )
    for candidate in sorted(set(candidates), reverse=True):
        if candidate.is_file():
            return candidate.resolve()
    raise FileNotFoundError(
        "Microsoft.CodeCoverage.Console.exe was not found; set "
        "ARIS_CODE_COVERAGE_CONSOLE to its absolute path"
    )


def build_collect_command(
    *,
    coverage_console: Path,
    ctest: Path,
    build_dir: Path,
    config: str,
    report: Path,
    log: Path,
    jobs: int,
    test_regex: str | None = None,
) -> list[str]:
    """Build a collector command that fails if CTest selects no tests."""
    command = [
        str(coverage_console),
        "collect",
        str(ctest),
        "--test-dir",
        str(build_dir),
        "-C",
        config,
        "--output-on-failure",
        "--no-tests=error",
        "-j",
        str(jobs),
    ]
    if test_regex:
        command.extend(["-R", test_regex])
    command.extend(
        [
            "-o",
            str(report),
            "-f",
            "cobertura",
            "-l",
            str(log),
            "-ll",
            "Info",
            "--nologo",
        ]
    )
    return command


def _production_relative_path(filename: str, repo_root: Path) -> str | None:
    candidate = Path(filename)
    if not candidate.is_absolute():
        candidate = repo_root / candidate
    try:
        relative = candidate.resolve(strict=False).relative_to(repo_root.resolve())
    except ValueError:
        return None
    if not relative.parts or relative.parts[0].casefold() != "src":
        return None
    return relative.as_posix()


def parse_cobertura(report_path: Path, repo_root: Path) -> dict[str, object]:
    """Return unique production-source line coverage from a Cobertura report."""
    tree = ET.parse(report_path)
    hits_by_line: dict[tuple[str, int], int] = {}

    for class_node in tree.findall(".//class"):
        filename = class_node.get("filename", "")
        relative = _production_relative_path(filename, repo_root)
        if relative is None:
            continue
        lines_node = class_node.find("lines")
        if lines_node is None:
            continue
        for line_node in lines_node.findall("line"):
            try:
                number = int(line_node.get("number", ""))
                hits = int(float(line_node.get("hits", "0")))
            except ValueError as exc:
                raise ValueError(
                    f"invalid Cobertura line in {relative}: {ET.tostring(line_node, encoding='unicode')}"
                ) from exc
            key = (relative, number)
            hits_by_line[key] = max(hits_by_line.get(key, 0), hits)

    if not hits_by_line:
        raise ValueError("coverage report contains no production source lines under src/")

    source_files = sorted({path for path, _ in hits_by_line})
    lines_valid = len(hits_by_line)
    lines_covered = sum(1 for hits in hits_by_line.values() if hits > 0)
    line_rate = lines_covered / lines_valid
    per_file: dict[str, dict[str, int | float]] = {}
    for source_file in source_files:
        file_hits = [hits for (path, _), hits in hits_by_line.items() if path == source_file]
        valid = len(file_hits)
        covered = sum(1 for hits in file_hits if hits > 0)
        per_file[source_file] = {
            "lines_valid": valid,
            "lines_covered": covered,
            "line_rate": covered / valid,
        }
    return {
        "metric": "unique production-source executable line union",
        "source_scope": "src/",
        "files_valid": len(source_files),
        "lines_valid": lines_valid,
        "lines_covered": lines_covered,
        "line_rate": line_rate,
        "line_rate_percent": round(line_rate * 100, 4),
        "source_files": source_files,
        "per_file": per_file,
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--build-dir", type=Path, default=Path("build_dbg"))
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--output-dir", type=Path, default=Path(".local/stage-5-coverage"))
    parser.add_argument("--coverage-console", type=Path)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--test-regex", help="Optional focused smoke regex; omit for the baseline")
    parser.add_argument("--parse-only", action="store_true", help="Summarize an existing report")
    parser.add_argument("--minimum-line-rate", type=float, default=0.0)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    root = args.root.resolve()
    build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
    output_dir = args.output_dir if args.output_dir.is_absolute() else root / args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    report = output_dir / "stage5-coverage.cobertura.xml"
    log = output_dir / "stage5-coverage.collector.log"
    summary_path = output_dir / "stage5-coverage.summary.json"
    command: list[str] | None = None
    existing_summary: dict[str, object] = {}
    if args.parse_only and summary_path.is_file():
        existing_summary = json.loads(summary_path.read_text(encoding="utf-8"))

    if not args.parse_only:
        if args.jobs < 1:
            raise ValueError("--jobs must be at least 1")
        coverage_console = discover_coverage_console(args.coverage_console)
        ctest_name = shutil.which("ctest") or shutil.which("ctest.exe")
        if not ctest_name:
            raise FileNotFoundError("ctest was not found on PATH")
        for stale in (report, log, summary_path):
            stale.unlink(missing_ok=True)
        command = build_collect_command(
            coverage_console=coverage_console,
            ctest=Path(ctest_name),
            build_dir=build_dir,
            config=args.config,
            report=report,
            log=log,
            jobs=args.jobs,
            test_regex=args.test_regex,
        )
        print("Coverage command:", subprocess.list2cmdline(command), flush=True)
        completed = subprocess.run(command, cwd=root, check=False)
        if completed.returncode != 0:
            print(f"coverage collection failed with exit code {completed.returncode}", file=sys.stderr)
            return completed.returncode

    if not report.is_file():
        raise FileNotFoundError(f"coverage report not found: {report}")
    summary = parse_cobertura(report, root)
    summarized_at = datetime.now().astimezone().isoformat(timespec="seconds")
    collected_at = (
        summarized_at
        if command is not None
        else str(existing_summary.get("collected_at") or existing_summary.get("generated_at") or summarized_at)
    )
    summary.update(
        {
            "generated_at": collected_at,
            "collected_at": collected_at,
            "summarized_at": summarized_at,
            "repo_root": str(root),
            "build_dir": str(build_dir),
            "config": args.config,
            "ctest_scope": args.test_regex
            or str(existing_summary.get("ctest_scope") or "all registered tests"),
            "raw_report": str(report),
            "collector_log": str(log),
            "command": command if command is not None else existing_summary.get("command"),
        }
    )
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(
        "Stage 5 production-source line coverage: "
        f"{summary['line_rate_percent']:.4f}% "
        f"({summary['lines_covered']}/{summary['lines_valid']} lines, "
        f"{summary['files_valid']} files)"
    )
    print(f"Summary: {summary_path}")
    if float(summary["line_rate"]) < args.minimum_line_rate:
        print(
            f"coverage {summary['line_rate']:.6f} is below required {args.minimum_line_rate:.6f}",
            file=sys.stderr,
        )
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
