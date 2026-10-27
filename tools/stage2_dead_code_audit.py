#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


SCHEMA_VERSION = 1
DISPOSITIONS = {
    "PENDING",
    "DELETE",
    "KEEP",
    "REVIEW_USER",
    "DEFER_STAGE3",
    "OUT_OF_SCOPE",
}
CANDIDATE_PREFIXES = (
    "src/",
    "reaper_extension/",
    "tools/",
    "scripts/",
    "tests/",
    "assets/",
    "resources/",
    "cmake/",
    "installer/",
)
CANDIDATE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hh",
    ".hpp",
    ".cmake",
    ".css",
    ".html",
    ".js",
    ".jsx",
    ".py",
    ".ps1",
    ".rc",
    ".ico",
    ".png",
    ".jpg",
    ".jpeg",
    ".svg",
    ".ts",
    ".tsx",
    ".ttf",
    ".json",
    ".dat",
    ".bin",
    ".iss",
}
REFERENCE_SUFFIXES = CANDIDATE_SUFFIXES | {
    ".txt",
    ".in",
    ".yml",
    ".yaml",
}
REFERENCE_PREFIXES = (
    ".vscode/",
    "src/",
    "reaper_extension/",
    "tools/",
    "scripts/",
    "tests/",
    "resources/",
    "cmake/",
    "installer/",
)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build the Aris Stage 2 dead-code register.")
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--register", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--coverage", type=Path)
    parser.add_argument("--compiler-log", type=Path)
    parser.add_argument("--linker-log", type=Path)
    parser.add_argument("--check-reviewed", action="store_true")
    parser.add_argument("--check-closed", action="store_true")
    return parser.parse_args(argv)


def existing_tracked_paths(root: Path) -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=True,
        capture_output=True,
    )
    indexed = (
        item.decode("utf-8", errors="surrogateescape").replace("\\", "/")
        for item in result.stdout.split(b"\0")
        if item
    )
    return sorted(path for path in indexed if (root / path).is_file())


def read_reference_text(root: Path, relative_path: str) -> str | None:
    if relative_path not in {"CMakeLists.txt", "resources.rc"} and not relative_path.startswith(
        REFERENCE_PREFIXES
    ):
        return None
    path = root / relative_path
    if path.name == "CMakeLists.txt" or path.suffix.casefold() in REFERENCE_SUFFIXES:
        try:
            return path.read_text(encoding="utf-8", errors="replace").casefold()
        except OSError:
            return None
    return None


def is_candidate_path(relative_path: str) -> bool:
    if not relative_path.startswith(CANDIDATE_PREFIXES):
        return relative_path == "resources.rc"
    path = Path(relative_path)
    return path.name == "CMakeLists.txt" or path.suffix.casefold() in CANDIDATE_SUFFIXES


def path_kind(relative_path: str) -> str:
    path = Path(relative_path)
    suffix = path.suffix.casefold()
    if relative_path.startswith("tests/"):
        return "test"
    if relative_path.startswith(("tools/", "scripts/")):
        return "script"
    if path.name == "CMakeLists.txt" or suffix == ".cmake":
        return "build"
    if suffix in {".c", ".cc", ".cpp", ".js", ".jsx", ".ts", ".tsx"}:
        return "source"
    if suffix in {".h", ".hh", ".hpp"}:
        return "header"
    return "resource"


def reference_aliases(relative_path: str) -> tuple[str, ...]:
    path = Path(relative_path)
    aliases = {
        relative_path.casefold(),
        relative_path.replace("/", "\\").casefold(),
        path.name.casefold(),
    }
    if path.suffix.casefold() in {".js", ".jsx", ".py", ".ts", ".tsx"}:
        aliases.add(path.stem.casefold())
        aliases.add(path.with_suffix("").as_posix().casefold())
    return tuple(sorted(aliases, key=lambda value: (-len(value), value)))


def incoming_references(
    relative_path: str, reference_text: dict[str, str]
) -> list[str]:
    aliases = reference_aliases(relative_path)
    return sorted(
        source
        for source, text in reference_text.items()
        if source != relative_path and any(alias in text for alias in aliases)
    )


def stable_id(relative_path: str, kind: str, symbol: str = "") -> str:
    digest = hashlib.sha256(
        f"{kind}:{relative_path}:{symbol}".encode("utf-8")
    ).hexdigest()[:10]
    return f"S2-{digest.upper()}"


def new_candidate(
    relative_path: str,
    kind: str,
    evidence: list[dict],
    symbol: str = "",
) -> dict:
    if kind == "test":
        evidence = list(evidence) + [
            {
                "type": "E4",
                "source": "test discovery review",
                "detail": "test files are discovered entry points and are not required to have tracked source incoming edges",
            }
        ]
        return {
            "id": stable_id(relative_path, kind, symbol),
            "path": relative_path,
            "symbol": symbol,
            "kind": kind,
            "present": True,
            "disposition": "KEEP",
            "evidence": evidence,
            "contract_refs": [],
            "dynamic_lookup_checked": True,
            "compatibility_checked": True,
            "reviewer": "Stage 2 test-entry review",
            "reason": "Test files are invoked by test discovery or explicit validation commands and must remain even without source-graph incoming edges.",
            "rollback": "git restore --source=refactor/stage-1-done -- " + relative_path,
        }
    return {
        "id": stable_id(relative_path, kind, symbol),
        "path": relative_path,
        "symbol": symbol,
        "kind": kind,
        "present": True,
        "disposition": "PENDING",
        "evidence": evidence,
        "contract_refs": [],
        "dynamic_lookup_checked": False,
        "compatibility_checked": False,
        "reviewer": "",
        "reason": "",
        "rollback": "",
    }


def load_register(path: Path) -> dict[str, dict]:
    if not path.exists():
        return {}
    document = json.loads(path.read_text(encoding="utf-8"))
    return {item["id"]: item for item in document.get("candidates", [])}


def merge_evidence(existing: list[dict], detected: list[dict]) -> list[dict]:
    merged: dict[tuple[str, str, str], dict] = {}
    for item in existing + detected:
        key = (str(item.get("type", "")), str(item.get("source", "")), str(item.get("detail", "")))
        merged[key] = {
            "type": key[0],
            "source": key[1],
            "detail": key[2],
        }
    return [merged[key] for key in sorted(merged)]


def normalize_repo_path(root: Path, value: str, tracked: set[str]) -> str | None:
    raw = value.strip().strip('"').replace("\\", "/")
    path = Path(raw)
    if path.is_absolute():
        try:
            raw = path.resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            return None
    else:
        raw = raw.removeprefix("./")
    return raw if raw in tracked else None


def coverage_findings(root: Path, coverage: Path | None, tracked: set[str]) -> list[dict]:
    if coverage is None:
        return []
    hits_by_file: dict[str, list[int]] = {}
    for class_node in ET.parse(coverage).getroot().findall(".//class"):
        relative_path = normalize_repo_path(root, class_node.get("filename", ""), tracked)
        if relative_path is None or not is_candidate_path(relative_path):
            continue
        hits = [int(line.get("hits", "0")) for line in class_node.findall(".//line")]
        if hits:
            hits_by_file.setdefault(relative_path, []).extend(hits)

    findings: list[dict] = []
    for relative_path, hits in sorted(hits_by_file.items()):
        if any(hit > 0 for hit in hits):
            continue
        findings.append(
            new_candidate(
                relative_path,
                path_kind(relative_path),
                [
                    {
                        "type": "E3",
                        "source": "coverage",
                        "detail": f"0/{len(hits)} executable lines covered",
                    }
                ],
            )
        )
    return findings


COMPILER_DIAGNOSTIC = re.compile(
    r"^(?P<path>.+?)\((?P<line>\d+)(?:,\d+)?\):\s*warning\s+"
    r"(?P<code>C4505|C4189|C4101|C4702):\s*(?P<detail>.+)$",
    re.IGNORECASE,
)
LINKER_DISCARD = re.compile(
    r'^\s*(?:Discarded|Removed|已丢弃|丢弃)\s+(?P<symbol>.+?)'
    r"(?:\s+from\s+(?P<object>[^\s]+\.obj))?\s*$",
    re.IGNORECASE,
)
LINKER_DISCARD_ZH = re.compile(
    r"^\s*已丢弃\s+(?P<symbol>.+)\(从\s+(?P<object>[^()]+\.obj)\s+中\)\s*$"
)


def read_log_text(path: Path) -> str:
    data = path.read_bytes()
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16", errors="replace")
    return data.decode("utf-8-sig", errors="replace")


def compiler_findings(root: Path, log_path: Path | None, tracked: set[str]) -> list[dict]:
    if log_path is None:
        return []
    findings: list[dict] = []
    for raw_line in read_log_text(log_path).splitlines():
        match = COMPILER_DIAGNOSTIC.match(raw_line.strip())
        if not match:
            continue
        relative_path = normalize_repo_path(root, match.group("path"), tracked)
        if relative_path is None or not is_candidate_path(relative_path):
            continue
        detail = re.sub(
            r"\s+\[[^\]]+\.vcxproj\]\s*$", "", match.group("detail").strip(), flags=re.IGNORECASE
        )
        quoted = re.search(
            r"“(?P<double_curly>[^”]+)”|‘(?P<single_curly>[^’]+)’|"
            r'"(?P<double>[^\"]+)"|\'(?P<single>[^\']+)\'',
            detail,
        )
        symbol = (
            next(value for value in quoted.groupdict().values() if value is not None)
            if quoted
            else f"line {match.group('line')}"
        )
        findings.append(
            new_candidate(
                relative_path,
                "symbol",
                [
                    {
                        "type": "E1",
                        "source": f"compiler {match.group('code').upper()}",
                        "detail": f"line {match.group('line')}: {detail}",
                    }
                ],
                symbol=symbol,
            )
        )
    return findings


def linker_findings(
    log_path: Path | None, tracked: set[str], candidates: list[dict]
) -> list[dict]:
    if log_path is None:
        return []
    sources_by_stem: dict[str, list[str]] = {}
    for relative_path in tracked:
        if not is_candidate_path(relative_path) or Path(relative_path).suffix.casefold() not in {
            ".c",
            ".cc",
            ".cpp",
        }:
            continue
        sources_by_stem.setdefault(Path(relative_path).stem.casefold(), []).append(relative_path)

    symbols_by_path: dict[str, list[dict]] = {}
    for candidate in candidates:
        if candidate.get("kind") != "symbol" or not candidate.get("symbol"):
            continue
        if not any(
            evidence.get("source") == "compiler C4505"
            for evidence in candidate.get("evidence", [])
        ):
            continue
        symbols_by_path.setdefault(str(candidate["path"]), []).append(candidate)

    findings: list[dict] = []
    for raw_line in read_log_text(log_path).splitlines():
        match = LINKER_DISCARD_ZH.match(raw_line) or LINKER_DISCARD.match(raw_line)
        if not match or not match.group("object"):
            continue
        object_stem = Path(match.group("object").replace("\\", "/")).stem.casefold()
        matching_sources = sources_by_stem.get(object_stem, [])
        if len(matching_sources) != 1:
            continue
        raw_symbol = match.group("symbol").strip().strip('"')
        for candidate in symbols_by_path.get(matching_sources[0], []):
            name = str(candidate["symbol"]).rsplit("::", 1)[-1].casefold()
            if not name or name not in raw_symbol.casefold():
                continue
            finding = dict(candidate)
            finding["evidence"] = [
                {
                    "type": "E1",
                    "source": "linker OPT:REF",
                    "detail": f"discarded {raw_symbol} from {match.group('object')}",
                }
            ]
            findings.append(finding)
    return findings


def disabled_branch_findings(root: Path, tracked: set[str]) -> list[dict]:
    findings: list[dict] = []
    for relative_path in sorted(tracked):
        if not is_candidate_path(relative_path) or Path(relative_path).suffix.casefold() not in {
            ".c",
            ".cc",
            ".cpp",
            ".h",
            ".hh",
            ".hpp",
        }:
            continue
        text = (root / relative_path).read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), start=1):
            if not re.match(r"^\s*#\s*if\s+0(?:\s|$)", line):
                continue
            findings.append(
                new_candidate(
                    relative_path,
                    "branch",
                    [
                        {
                            "type": "E5",
                            "source": "preprocessor",
                            "detail": f"line {line_number}: #if 0",
                        }
                    ],
                    symbol=f"line {line_number}: #if 0",
                )
            )
    return findings


def add_detected_candidate(
    detected: dict[str, dict], candidate: dict, previous: dict[str, dict]
) -> None:
    candidate_id = candidate["id"]
    current = detected.get(candidate_id) or previous.get(candidate_id)
    if current and current.get("disposition") == "PENDING" and candidate.get("disposition") != "PENDING":
        current = None
    if current:
        current = dict(current)
        current["present"] = True
        current["evidence"] = merge_evidence(
            list(current.get("evidence", [])), candidate["evidence"]
        )
        candidate = current
    detected[candidate_id] = candidate


def recorded_candidate_is_present(root: Path, tracked: set[str], item: dict) -> bool:
    relative_path = str(item.get("path", ""))
    if relative_path not in tracked:
        return False
    kind = str(item.get("kind", ""))
    if kind == "symbol":
        symbol = str(item.get("symbol", "")).rsplit("::", 1)[-1]
        if not symbol:
            return True
        text = (root / relative_path).read_text(encoding="utf-8", errors="replace")
        occurrences = len(re.findall(rf"\b{re.escape(symbol)}\b", text))
        return occurrences > int(item.get("expected_occurrences_after", 0))
    if kind == "branch":
        text = (root / relative_path).read_text(encoding="utf-8", errors="replace")
        return re.search(r"^\s*#\s*if\s+0(?:\s|$)", text, flags=re.MULTILINE) is not None
    return True


def scan(
    root: Path,
    previous: dict[str, dict],
    coverage: Path | None = None,
    compiler_log: Path | None = None,
    linker_log: Path | None = None,
) -> list[dict]:
    paths = existing_tracked_paths(root)
    tracked = set(paths)
    active_webui = (root / "src/webui/package.json").is_file()
    reference_text = {
        relative_path: text
        for relative_path in paths
        if (text := read_reference_text(root, relative_path)) is not None
    }

    detected: dict[str, dict] = {}
    for relative_path in paths:
        if active_webui and relative_path.startswith("src/webui/"):
            continue
        if not is_candidate_path(relative_path):
            continue
        if incoming_references(relative_path, reference_text):
            continue
        kind = path_kind(relative_path)
        candidate = new_candidate(
            relative_path,
            kind,
            [
                {
                    "type": "E2",
                    "source": "reference_graph",
                    "detail": "zero incoming tracked references",
                }
            ],
        )
        add_detected_candidate(detected, candidate, previous)

    extra_findings = coverage_findings(root, coverage, tracked) + compiler_findings(
        root, compiler_log, tracked
    ) + disabled_branch_findings(root, tracked)
    for candidate in extra_findings:
        add_detected_candidate(detected, candidate, previous)
    for candidate in linker_findings(linker_log, tracked, list(detected.values())):
        add_detected_candidate(detected, candidate, previous)

    for candidate_id, item in previous.items():
        if candidate_id in detected:
            continue
        if active_webui and str(item.get("path", "")).startswith("src/webui/"):
            continue
        historical = dict(item)
        historical["present"] = recorded_candidate_is_present(root, tracked, historical)
        detected[candidate_id] = historical

    return sorted(
        detected.values(),
        key=lambda item: (str(item.get("path", "")), str(item.get("symbol", ""))),
    )


def write_register(path: Path, candidates: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    document = {"schema_version": SCHEMA_VERSION, "root": ".", "candidates": candidates}
    path.write_text(
        json.dumps(document, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def markdown_cell(value: object) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def write_report(path: Path, candidates: list[dict]) -> None:
    counts: dict[str, int] = {}
    for item in candidates:
        disposition = str(item.get("disposition", "PENDING"))
        counts[disposition] = counts.get(disposition, 0) + 1
    summary = ", ".join(f"{key}={counts[key]}" for key in sorted(counts)) or "none"
    lines = [
        "# Aris Stage 2 Dead-Code Register",
        "",
        "The register is generated from tracked first-party paths. A candidate is not deletion proof.",
        "",
        f"Summary: {summary}",
        "",
        "| ID | Path | Kind | Present | Disposition | Evidence | Reason |",
        "|---|---|---|---:|---|---|---|",
    ]
    for item in candidates:
        evidence = ", ".join(
            sorted({str(entry.get("type", "")) for entry in item.get("evidence", [])})
        )
        lines.append(
            "| "
            + " | ".join(
                markdown_cell(value)
                for value in (
                    item.get("id", ""),
                    item.get("path", ""),
                    item.get("kind", ""),
                    "yes" if item.get("present", True) else "no",
                    item.get("disposition", "PENDING"),
                    evidence,
                    item.get("reason", ""),
                )
            )
            + " |"
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def review_errors(candidates: list[dict], require_absent: bool) -> list[str]:
    errors: list[str] = []
    for item in candidates:
        identity = f"{item.get('id', '?')} {item.get('path', '?')}"
        disposition = str(item.get("disposition", "PENDING"))
        if disposition not in DISPOSITIONS:
            errors.append(f"{identity}: invalid disposition {disposition}")
            continue
        if disposition == "PENDING":
            errors.append(f"{identity}: PENDING review")
            continue
        if disposition != "OUT_OF_SCOPE":
            if not item.get("reviewer"):
                errors.append(f"{identity}: reviewer is required")
            if not item.get("reason"):
                errors.append(f"{identity}: reason is required")
        if disposition != "DELETE":
            continue
        evidence_types = {
            str(entry.get("type", "")) for entry in item.get("evidence", []) if entry.get("type")
        }
        if len(evidence_types) < 2:
            errors.append(f"{identity}: DELETE requires at least two evidence types")
        if not item.get("dynamic_lookup_checked"):
            errors.append(f"{identity}: dynamic lookup review is required")
        if not item.get("compatibility_checked"):
            errors.append(f"{identity}: compatibility review is required")
        if require_absent and item.get("present", True):
            errors.append(f"{identity}: DELETE target is still present")
    return errors


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    previous = load_register(args.register)
    candidates = scan(
        root,
        previous,
        coverage=args.coverage,
        compiler_log=args.compiler_log,
        linker_log=args.linker_log,
    )
    write_register(args.register, candidates)
    write_report(args.output, candidates)

    if args.check_reviewed or args.check_closed:
        errors = review_errors(candidates, require_absent=args.check_closed)
        if errors:
            print("Stage 2 dead-code register is not closed:", file=sys.stderr)
            for error in errors:
                print(f" - {error}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
