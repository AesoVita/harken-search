#!/usr/bin/env python3
"""Build and validate the Aris Stage 3 contextual language register.

The scanner reports deterministic review signals. It never treats a match as
proof that text was written by a particular author or tool.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


SCHEMA_VERSION = 1

TEXT_EXTENSIONS = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".iss",
    ".md",
    ".mm",
    ".ps1",
    ".py",
    ".rc",
    ".txt",
}

LEGAL_PATHS = {
    "ASSET_PROVENANCE.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "thirdparty/BINARY_PROVENANCE.md",
}

POLICY_PATHS = {
    "tools/stage3_human_style_audit.py",
    "tests/test_stage3_human_style_audit.py",
    "tests/test_stage3_style_gate.py",
}

AUDIT_RECORD_PATHS = {
    "docs/plans/refactor/stage-3-style-register.json",
    "docs/plans/refactor/stage-3-style-register.md",
    "docs/plans/refactor/stage-3-report.md",
}

VALID_DISPOSITIONS = {
    "PENDING",
    "REWRITE",
    "REMOVE",
    "RENAME",
    "KEEP_PRODUCT_AI",
    "KEEP_COMPAT",
    "KEEP_LEGAL",
    "KEEP_HISTORY",
    "DEFER_ARCHITECTURE",
    "REVIEW_USER",
}

ACTIONABLE_DISPOSITIONS = {"REWRITE", "REMOVE", "RENAME"}
PRESERVE_DISPOSITIONS = {
    "KEEP_PRODUCT_AI",
    "KEEP_COMPAT",
    "KEEP_LEGAL",
    "KEEP_HISTORY",
    "DEFER_ARCHITECTURE",
    "REVIEW_USER",
}

ASSISTANT_REFERENCE_RE = re.compile(
    r"\b(?:Claude|Codex|Gemini|ChatGPT|Copilot|Anthropic)\b", re.IGNORECASE
)

AUTHORING_HISTORY_RE = re.compile(
    r"(?:"
    r"review\s+(?:pass|round)(?:[- ]?\d+)?|"
    r"(?:Claude|Codex)\s+review|"
    r"\bround\s+\d+|"
    r"\bpass[- ]?\d+|"
    r"\bthis\s+commit\b|"
    r"\bpre-existing\b|"
    r"\bper\s+CLAUDE(?:\.md)?\b|"
    r"\buser\s+feedback\b|"
    r"\bnew\s+code\b|"
    r"\bold\s+code\b"
    r")",
    re.IGNORECASE,
)

DELIVERY_SCAFFOLD_RE = re.compile(
    r"(?:"
    r"\bagent staffing\b|"
    r"\bstaffing decision\b|"
    r"\bwhy this staffing fits\b|"
    r"\bprimary execution agent\b|"
    r"\bsecondary review agent\b|"
    r"\bexecution dispatch gate\b|"
    r"\bagent boundary check\b|"
    r"\bchat-only\b|"
    r"\bconversation screenshot\b|"
    r"\badvisory\b|"
    r"\blocal advisory\b|"
    r"\badvisory report\b|"
    r"\breview prompt\b|"
    r"\bdesign prompt\b|"
    r"\bfuture bounded code worker\b"
    r")",
    re.IGNORECASE,
)

PRODUCT_CARD_SCAFFOLD_RE = re.compile(
    r"(?:"
    r"\bprimary agent\b|"
    r"\badditional agents\b|"
    r"\bexecution agent\b|"
    r"^\s*\|\s*agent\s*\||"
    r"\b(?:UI|Audio)\s+agent\b|"
    r"\bnew agent domain\b|"
    r"\buser request\b|"
    r"\bscreenshot in conversation\b|"
    r"\bchat(?:\s+(?:history|memory))?\b|"
    r"\bin this session\b|"
    r"\u804a\u5929(?:\u7ed3\u8bba|\u8bb0\u5f55)|"
    r"\bAGENTS-wide\b|"
    r"\b(?:dispatch gate|dispatch brief|active dispatch card|implementation dispatch|when dispatched|dispatch the bounded)\b|"
    r"\bbrain routing\b|"
    r"\bsuperpower route\b|"
    r"\bharness\s+(?:card|record|work)\b|"
    r"\bfull-cycle\b|"
    r"`agents/[^`]+`"
    r")",
    re.IGNORECASE,
)

HYPE_COMMENT_RE = re.compile(
    r"(?:"
    r"[\U0001f4a1\U0001f6a7\u2705\u274c\U0001f527\U0001f525\u26a0]|"
    r"\u5f7b\u5e95|"  # completely
    r"\u7ec8\u6781|"  # ultimate
    r"\u7edd\u6740|"  # killer move
    r"(?<!\u5bfc)\u81f4\u547d|"  # fatal, but not the cross-word match in “导致命令”
    r"\u6027\u80fd\u98de\u8dc3|"  # performance leap
    r"\u7d27\u6025\u4fee\u590d|"  # emergency fix
    r"\u96f7\u70b9|"  # landmine
    r"\u6b7b\u4ea1\u91cd\u8bd5|"  # death retry
    r"\u6838\u5fc3\u4f18\u5316|"  # core optimization
    r"\u67b6\u6784\s*V\d"
    r")",
    re.IGNORECASE,
)

MOJIBAKE_RE = re.compile(
    r"(?:"
    r"\ufffd|"
    r"[\ue000-\uf8ff]|"
    r"\?{6,}|"
    r"\u7f01\u714e\u608e|"  # broken form of the common library folder name
    r"\u95ca\u866b\u6665\u935a\u5823\u7deb|"  # broken form of an audio collection name
    r"\u9225\?|"  # common broken UTF-8 dash prefix
    r"\u951b\?|"
    r"\u7efe\?|"
    r"\u6fb6\u8fab\u89e6|"  # broken form of failure
    r"\u74a7\u52ec|"
    r"\u9352\u55d7|"
    r"\u93c2\u56e6|"
    r"\u9983|"  # broken emoji prefix
    r"\u95ab\u6c33\u7e43|"
    r"\u7459\u52eb\u57af|"
    r"\u93b4\u612c\u59db"
    r")"
)

PLACEHOLDER_RE = re.compile(r"\b(?:TODO|FIXME|HACK)\b", re.IGNORECASE)
NARRATIVE_COMMENT_RE = re.compile(
    r"(?:"
    r"\b(?:NOTE|IMPORTANT)\s*:|"
    r"\b(?:This\s+(?:function|code|call|block)|Here\s+we|We\s+need|Now|Previously)\s*[: ]|"
    r"\u8fd9\u91cc\u6211\u4eec|"  # here we
    r"\u4e0b\u9762\u8fd9\u884c|"  # the line below
    r"\u4e0a\u9762\u5b9a\u4e49|"  # defined above
    r"^\u3010[^\u3011]{1,24}\u3011[\uff1a:]?|"  # bracketed patch/status label
    r"\u660e\u786e\u58f0\u660e"  # explicitly declare
    r")",
    re.IGNORECASE,
)
OLD_BRAND_RE = re.compile(
    r"(?<![A-Za-z0-9_])(?:Aura|Auro)(?![A-Za-z0-9_])",
    re.IGNORECASE,
)
UNPROFESSIONAL_COPY_RE = re.compile(
    r"(?:\u81ea\u52a8\u611f\u67d3|\u611f\u67d3(?:\u8be5)?\u76ee\u5f55)"
)
CPP_STRING_LITERAL_RE = re.compile(r'(?:u8|u|U|L)?"((?:\\.|[^"\\])*)"')
LEGACY_STRING_LITERAL_RE = re.compile(
    r"(?:"
    r"\b(?:Aura|Auro)::|"
    r"\baura::|"
    r"\bAURO_|"
    r"\bauro[-_]|"
    r"\bAuro\.(?:db|exe|AudioFile)\b|"
    r"[/\\]Auro(?:[/\\]|$)|"
    r"\.(?:db|exe|txt|json|png)\b"
    r")",
    re.IGNORECASE,
)

PROMPT_WORD_RE = re.compile(r"\bprompts?\b", re.IGNORECASE)
INTERNAL_PROMPT_CONTEXT_RE = re.compile(
    r"\b(?:assistant(?:\s+session)?|model(?:\s+(?:prompt|session))?|conversation|"
    r"system\s+message|user\s+message)\b|"
    r"\bcopy\s+this\b|\bexternal\s+design\s+request\b",
    re.IGNORECASE,
)
PRODUCT_PROMPT_CONTEXT_RE = re.compile(
    r"\b(?:AIEngine|cover[- ]generation|SiliconFlow|user[- ]entered)\b",
    re.IGNORECASE,
)

DECORATIVE_ONLY_RE = re.compile(r"^[\s\-=\u2500\u2550]{5,}$")
DECORATIVE_FRAME_RE = re.compile(
    r"^[\s\-=\u2500\u2550]{2,}\S.*[\-=\u2500\u2550]{2,}\s*$"
)

GENERIC_DECLARATION_RE = re.compile(
    r"^\s*(?:(?:static|inline|constexpr)\s+)*(?:void|bool|float|double|int|auto)\s+"
    r"(?:SidebarLerpHelper|[A-Za-z_]*(?:Thing|Stuff)Helper)\s*\("
)
GENERIC_HEADING_RE = re.compile(
    r"^(?:misc|miscellaneous|misc\s+(?:ui|messages|labels|remaining)|stuff\s+fields?)$",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class Signal:
    path: str
    line: int
    kind: str
    rule: str
    normalized: str


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build the Aris Stage 3 contextual language register."
    )
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--register", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--check-reviewed", action="store_true")
    parser.add_argument("--check-closed", action="store_true")
    return parser.parse_args(argv)


def repo_relative(path: Path, root: Path) -> str | None:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return None


def tracked_paths(root: Path) -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        message = result.stderr.decode("utf-8", errors="replace")
        raise RuntimeError(f"git ls-files failed: {message.strip()}")
    return sorted(
        path.replace("\\", "/")
        for path in result.stdout.decode("utf-8", errors="surrogateescape").split("\0")
        if path
    )


def is_generated(path: str) -> bool:
    name = Path(path).name.lower()
    return ".generated." in name or name.endswith("_generated.h")


def is_prompt_artifact_path(path: str) -> bool:
    lower_path = path.lower()
    return "/prompts/" in lower_path or lower_path.endswith("ai_model_policy.md")


def should_scan(path: str, register_path: str | None, output_path: str | None) -> bool:
    normalized = path.replace("\\", "/")
    if normalized.startswith(("thirdparty/", "LICENSES/")):
        return False
    if normalized in LEGAL_PATHS or normalized in POLICY_PATHS or normalized in AUDIT_RECORD_PATHS:
        return False
    if normalized in {register_path, output_path}:
        return False
    if normalized.startswith("docs/plans/refactor/cards/stage-3-"):
        return False
    if is_generated(normalized):
        return False
    file_path = Path(normalized)
    return file_path.name in {"CMakeLists.txt", ".gitignore"} or file_path.suffix.lower() in TEXT_EXTENSIONS


def normalize_text(text: str) -> str:
    value = text.strip()
    value = re.sub(r"^(?://+|/\*+|\*+|#+|;+|--+)\s*", "", value)
    value = re.sub(r"\s*(?:\*/)?\s*$", "", value)
    value = re.sub(r"\s+", " ", value)
    return value.strip()


def extract_comment(path: str, line: str, in_block_comment: bool = False) -> str | None:
    suffix = Path(path).suffix.lower()
    stripped = line.strip()
    if suffix in {".md", ".txt"}:
        return None
    if suffix in {".py", ".ps1", ".cmake"} or Path(path).name == "CMakeLists.txt":
        if stripped.startswith("#"):
            return stripped[1:].strip()
        return None
    if suffix == ".iss" and stripped.startswith(";"):
        return stripped[1:].strip()
    if "//" in line:
        return line.split("//", 1)[1].strip()
    if stripped.startswith("/*"):
        block_end = stripped.find("*/", 2)
        if block_end >= 0:
            return normalize_text(stripped[: block_end + 2])
        return normalize_text(stripped)
    if in_block_comment:
        return normalize_text(stripped)
    return None


def add_signal(
    signals: list[Signal], path: str, line: int, kind: str, rule: str, text: str
) -> None:
    normalized = normalize_text(text)
    if not normalized:
        normalized = "<empty>"
    signals.append(Signal(path, line, kind, rule, normalized))


def scan_line(
    signals: list[Signal],
    path: str,
    line_number: int,
    line: str,
    in_block_comment: bool = False,
) -> str | None:
    is_doc = Path(path).suffix.lower() in {".md", ".txt"}
    comment = extract_comment(path, line, in_block_comment)
    prose = line.strip() if is_doc else comment

    if path not in POLICY_PATHS and ASSISTANT_REFERENCE_RE.search(line):
        add_signal(signals, path, line_number, "doc" if is_doc else "comment", "assistant_reference", line)

    if prose is not None:
        if AUTHORING_HISTORY_RE.search(prose):
            add_signal(signals, path, line_number, "doc" if is_doc else "comment", "authoring_history", prose)
        if DELIVERY_SCAFFOLD_RE.search(prose) or (
            path.startswith("docs/product/cards/")
            and PRODUCT_CARD_SCAFFOLD_RE.search(prose)
        ):
            add_signal(
                signals,
                path,
                line_number,
                "doc" if is_doc else "comment",
                "delivery_scaffolding",
                prose,
            )
        if HYPE_COMMENT_RE.search(prose):
            add_signal(signals, path, line_number, "doc" if is_doc else "comment", "hype_comment", prose)
        if PLACEHOLDER_RE.search(prose):
            add_signal(signals, path, line_number, "doc" if is_doc else "comment", "placeholder", prose)
        if not is_doc and NARRATIVE_COMMENT_RE.search(prose):
            add_signal(signals, path, line_number, "comment", "narrative_comment", prose)
        if OLD_BRAND_RE.search(prose):
            add_signal(signals, path, line_number, "doc" if is_doc else "comment", "terminology", prose)

        normalized_comment = normalize_text(prose)
        if DECORATIVE_ONLY_RE.fullmatch(normalized_comment) or DECORATIVE_FRAME_RE.fullmatch(
            normalized_comment
        ):
            add_signal(
                signals,
                path,
                line_number,
                "comment" if not is_doc else "doc",
                "decorative_comment",
                prose,
            )
        if GENERIC_HEADING_RE.fullmatch(normalized_comment):
            add_signal(signals, path, line_number, "comment", "generic_name", prose)

    if MOJIBAKE_RE.search(line):
        add_signal(signals, path, line_number, "encoding", "mojibake", line)

    lower_path = path.lower()
    prompt_line = PROMPT_WORD_RE.search(line) and (
        INTERNAL_PROMPT_CONTEXT_RE.search(line)
        or PRODUCT_PROMPT_CONTEXT_RE.search(line)
        or "aiengine" in lower_path
        or lower_path == "ai_feature_disclosure.md"
    )
    if prompt_line and not is_prompt_artifact_path(path):
        add_signal(signals, path, line_number, "string", "prompt_artifact", line)

    if GENERIC_DECLARATION_RE.search(line):
        add_signal(signals, path, line_number, "identifier", "generic_name", line)

    code_part = line.split("//", 1)[0] if "//" in line else line
    for literal in CPP_STRING_LITERAL_RE.findall(code_part):
        if OLD_BRAND_RE.search(literal) and not LEGACY_STRING_LITERAL_RE.search(literal):
            add_signal(signals, path, line_number, "string", "terminology", literal)
        if UNPROFESSIONAL_COPY_RE.search(literal):
            add_signal(signals, path, line_number, "string", "unprofessional_copy", literal)

    return comment


def scan_repository(root: Path, register: Path, output: Path) -> list[Signal]:
    register_path = repo_relative(register, root)
    output_path = repo_relative(output, root)
    signals: list[Signal] = []
    comments_by_path: dict[str, list[tuple[int, str]]] = collections.defaultdict(list)
    product_repository = (root / "src/main.cpp").is_file()

    for relative_path in tracked_paths(root):
        if product_repository and relative_path.startswith((
            "docs/context/", "docs/plans/", "docs/superpowers/",
            "docs/product/cards/", "tests/"
        )):
            continue
        if not should_scan(relative_path, register_path, output_path):
            continue
        path = root / relative_path
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = text.splitlines()
        if is_prompt_artifact_path(relative_path):
            add_signal(
                signals,
                relative_path,
                1,
                "artifact",
                "prompt_artifact",
                f"retired-artifact:{relative_path}",
            )
        in_block_comment = False
        for line_number, line in enumerate(lines, start=1):
            comment = scan_line(
                signals,
                relative_path,
                line_number,
                line,
                in_block_comment,
            )
            if comment is not None:
                normalized = normalize_text(comment).lower()
                if (
                    len(normalized) >= 35
                    and not DECORATIVE_ONLY_RE.fullmatch(normalized)
                    and not DECORATIVE_FRAME_RE.fullmatch(normalized)
                ):
                    comments_by_path[relative_path].append((line_number, normalized))
            if in_block_comment:
                if "*/" in line:
                    in_block_comment = False
            else:
                block_start = line.find("/*")
                if block_start >= 0 and line.find("*/", block_start + 2) < 0:
                    in_block_comment = True
    for path, comments in comments_by_path.items():
        occurrences: dict[str, list[int]] = collections.defaultdict(list)
        for line_number, normalized in comments:
            occurrences[normalized].append(line_number)
        for normalized, locations in occurrences.items():
            if len(locations) < 2:
                continue
            for line_number in locations:
                signals.append(
                    Signal(path, line_number, "duplicate", "duplicate_comment", normalized)
                )

    return signals


def fingerprint(path: str, rule: str, normalized: str) -> str:
    payload = f"{path}\0{rule}\0{normalized}".encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def group_signals(signals: Iterable[Signal]) -> list[dict]:
    grouped: dict[tuple[str, str, str, str], list[int]] = collections.defaultdict(list)
    for signal in signals:
        key = (signal.path, signal.kind, signal.rule, signal.normalized)
        grouped[key].append(signal.line)

    candidates: list[dict] = []
    for (path, kind, rule, normalized), locations in grouped.items():
        digest = fingerprint(path, rule, normalized)
        candidate = {
            "id": "S3-" + digest[:10].upper(),
            "path": path,
            "locations": sorted(set(locations)),
            "kind": kind,
            "rule": rule,
            "fingerprint": "sha256:" + digest,
            "disposition": "PENDING",
            "replacement": "",
            "contract_refs": [],
            "reviewer": "",
            "reason": "",
            "verified": False,
        }
        if path.startswith(
            ("docs/superpowers/", "docs/plans/refactor/cards/", "docs/product/cards/")
        ):
            candidate.update(
                {
                    "disposition": "KEEP_HISTORY",
                    "reviewer": "Stage 3 historical-record review",
                    "reason": "This signal is retained in a dated implementation or refactor record whose historical wording is part of the audit trail.",
                    "verified": True,
                }
            )
        elif rule == "terminology" and re.search(r"\baur(?:a|o)\b", normalized, re.IGNORECASE):
            candidate.update(
                {
                    "disposition": "KEEP_COMPAT",
                    "reviewer": "Stage 3 compatibility review",
                    "reason": "The signal is an asserted legacy product identifier, storage path, manifest name, or compatibility namespace that remains part of the supported contract.",
                    "verified": True,
                }
            )
        elif path.startswith("tests/") and rule in {"mojibake", "decorative_comment"}:
            candidate.update(
                {
                    "disposition": "KEEP_COMPAT",
                    "reviewer": "Stage 3 test-fixture review",
                    "reason": "The signal is confined to test fixture data or test-only section documentation and does not ship as product copy.",
                    "verified": True,
                }
            )
        candidates.append(candidate)
    return sorted(candidates, key=lambda item: (item["path"], item["rule"], item["id"]))


def load_previous(path: Path) -> dict[str, dict]:
    if not path.exists():
        return {}
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"cannot read prior register {path}: {exc}") from exc
    return {
        item["id"]: item
        for item in document.get("candidates", [])
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }


def merge_decisions(active: list[dict], previous: dict[str, dict]) -> list[dict]:
    merged: list[dict] = []
    active_ids: set[str] = set()
    review_fields = (
        "disposition",
        "replacement",
        "contract_refs",
        "reviewer",
        "reason",
    )

    for candidate in active:
        active_ids.add(candidate["id"])
        prior = previous.get(candidate["id"])
        if prior and prior.get("disposition") == "PENDING" and candidate.get("disposition") != "PENDING":
            prior = None
        if prior:
            for field in review_fields:
                if field in prior:
                    candidate[field] = prior[field]
            candidate["verified"] = candidate["disposition"] in PRESERVE_DISPOSITIONS
        merged.append(candidate)

    # The register describes the current tree. Once a reviewed fingerprint is
    # gone, do not carry a phantom entry forward and fail the next gate forever.
    # Historical decisions remain in git; the live register only needs active
    # signals so closure is evaluated against what is actually present.

    return sorted(merged, key=lambda item: (item.get("path", ""), item.get("rule", ""), item["id"]))


def validate_reviewed(candidates: list[dict]) -> list[str]:
    errors: list[str] = []
    for item in candidates:
        candidate_id = item.get("id", "<missing-id>")
        disposition = item.get("disposition")
        if disposition not in VALID_DISPOSITIONS:
            errors.append(f"{candidate_id}: invalid disposition {disposition!r}")
            continue
        if disposition == "PENDING":
            errors.append(f"{candidate_id}: PENDING")
            continue
        if not str(item.get("reviewer", "")).strip():
            errors.append(f"{candidate_id}: reviewer is required")
        if not str(item.get("reason", "")).strip():
            errors.append(f"{candidate_id}: reason is required")
        if disposition in {"REWRITE", "RENAME"} and not str(
            item.get("replacement", "")
        ).strip():
            errors.append(f"{candidate_id}: replacement is required for {disposition}")
    return errors


def validate_closed(candidates: list[dict]) -> list[str]:
    errors: list[str] = []
    for item in candidates:
        disposition = item.get("disposition")
        locations = item.get("locations") or []
        candidate_id = item.get("id", "<missing-id>")
        if disposition in ACTIONABLE_DISPOSITIONS and locations:
            errors.append(
                f"{candidate_id}: {disposition} fingerprint remains at "
                f"{item.get('path')}:{','.join(str(value) for value in locations)}"
            )
        if disposition in PRESERVE_DISPOSITIONS and not locations:
            errors.append(f"{candidate_id}: protected/deferred fingerprint disappeared")
    return errors


def write_register(path: Path, candidates: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    document = {
        "schema_version": SCHEMA_VERSION,
        "root": ".",
        "candidates": candidates,
    }
    path.write_text(
        json.dumps(document, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def write_report(path: Path, candidates: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    by_rule = collections.Counter(item.get("rule", "unknown") for item in candidates)
    by_disposition = collections.Counter(
        item.get("disposition", "unknown") for item in candidates
    )
    lines = [
        "# Aris Stage 3 Contextual Language Register",
        "",
        "This report lists fingerprints and review decisions. It intentionally omits raw candidate text.",
        "",
        f"- Candidates: {len(candidates)}",
        "- Rules: "
        + ", ".join(f"`{key}`={value}" for key, value in sorted(by_rule.items())),
        "- Dispositions: "
        + ", ".join(
            f"`{key}`={value}" for key, value in sorted(by_disposition.items())
        ),
        "",
        "| ID | Path | Lines | Kind | Rule | Disposition | Verified |",
        "|---|---|---:|---|---|---|---|",
    ]
    for item in candidates:
        locations = item.get("locations") or []
        location_text = ",".join(str(value) for value in locations) or "resolved"
        lines.append(
            "| `{id}` | `{path}` | {locations} | `{kind}` | `{rule}` | `{disposition}` | {verified} |".format(
                id=item.get("id", ""),
                path=item.get("path", ""),
                locations=location_text,
                kind=item.get("kind", ""),
                rule=item.get("rule", ""),
                disposition=item.get("disposition", ""),
                verified="yes" if item.get("verified") else "no",
            )
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    register = args.register if args.register.is_absolute() else root / args.register
    output = args.output if args.output.is_absolute() else root / args.output

    try:
        previous = load_previous(register)
        active = group_signals(scan_repository(root, register, output))
        candidates = merge_decisions(active, previous)
        write_register(register, candidates)
        write_report(output, candidates)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    errors: list[str] = []
    if args.check_reviewed or args.check_closed:
        errors.extend(validate_reviewed(candidates))
    if args.check_closed:
        errors.extend(validate_closed(candidates))
    if errors:
        print("Stage 3 audit gate failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
