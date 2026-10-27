#!/usr/bin/env python3
"""Generate a deterministic CycloneDX SBOM from Aris's reviewed component ledger."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "THIRD_PARTY_COMPONENTS.json"


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read component manifest {path}: {error}") from error
    if not isinstance(payload, dict) or not isinstance(payload.get("components"), list):
        raise ValueError("component manifest must contain a components array")
    return payload


def component_bom_ref(component: dict[str, Any]) -> str:
    return f"aris:third-party:{component['name'].casefold().replace(' ', '-')}@{component['version']}"


def generate_sbom(manifest: dict[str, Any]) -> dict[str, Any]:
    components = []
    for component in sorted(manifest["components"], key=lambda item: (item["name"].casefold(), item["version"])):
        properties = [
            {"name": "aris:distributed", "value": str(component["distributed"]).lower()},
            {"name": "aris:load-mode", "value": component["load_mode"]},
            {"name": "aris:risk-classification", "value": component["risk_classification"]},
            {"name": "aris:policy-category", "value": component["policy_category"]},
            {"name": "aris:manual-approval-required", "value": str(component["manual_approval_required"]).lower()},
            {"name": "aris:approval-reference", "value": component["approval_reference"]},
            {"name": "aris:license-files", "value": ",".join(component["license_files"])},
            {"name": "aris:notice-files", "value": ",".join(component["notice_files"])},
            {"name": "aris:distribution-paths", "value": ",".join(component["distribution_paths"])},
        ]
        components.append(
            {
                "type": "library",
                "bom-ref": component_bom_ref(component),
                "name": component["name"],
                "version": component["version"],
                "licenses": [{"license": {"id": component["license_expression"]}}],
                "purl": component["source"],
                "description": component["purpose"],
                "externalReferences": [{"type": "website", "url": component["source"]}],
                "properties": properties,
            }
        )
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": "urn:uuid:aris-third-party-components-v1",
        "version": 1,
        "metadata": {
            "component": {
                "type": "application",
                "name": "Aris",
                "version": "source-tree",
                "bom-ref": "aris:application",
            },
            "properties": [{"name": "aris:manifest-schema", "value": str(manifest["schema_version"])}],
        },
        "components": components,
    }


def canonical_json(payload: Any) -> str:
    return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(canonical_json(payload), encoding="utf-8")


def tracked_files(repo_root: Path) -> set[str]:
    completed = subprocess.run(
        ["git", "-C", str(repo_root), "ls-files", "-z"],
        check=False,
        capture_output=True,
        text=False,
    )
    if completed.returncode:
        raise ValueError("git ls-files failed; source scans require a Git worktree")
    return {entry.decode("utf-8") for entry in completed.stdout.split(b"\0") if entry}


def compare_sboms(base: dict[str, Any], current: dict[str, Any]) -> dict[str, Any]:
    def index(payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
        return {component["name"]: component for component in payload.get("components", [])}

    old, new = index(base), index(current)
    added = sorted(set(new) - set(old), key=str.casefold)
    removed = sorted(set(old) - set(new), key=str.casefold)
    changed = []
    for name in sorted(set(new) & set(old), key=str.casefold):
        old_component, new_component = old[name], new[name]
        delta: dict[str, Any] = {"name": name}
        if old_component.get("version") != new_component.get("version"):
            delta["version"] = {"before": old_component.get("version"), "after": new_component.get("version")}
        old_license = old_component.get("licenses")
        new_license = new_component.get("licenses")
        if old_license != new_license:
            delta["licenses"] = {"before": old_license, "after": new_license}
        if len(delta) > 1:
            changed.append(delta)
    return {"added": added, "removed": removed, "changed": changed}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate deterministic Aris CycloneDX SBOM data.")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare-base", type=Path, help="Compare generated SBOM with a baseline SBOM.")
    parser.add_argument("--empty-base", action="store_true", help="Treat a missing first-introduced baseline as zero components.")
    parser.add_argument("--diff-output", type=Path, help="Write deterministic SBOM delta JSON.")
    args = parser.parse_args(argv)
    try:
        sbom = generate_sbom(load_manifest(args.manifest))
        write_json(args.output, sbom)
        if args.compare_base or args.empty_base:
            if not args.diff_output:
                parser.error("--compare-base/--empty-base requires --diff-output")
            if args.empty_base:
                base = generate_sbom({"schema_version": 1, "components": []})
            else:
                base = json.loads(args.compare_base.read_text(encoding="utf-8-sig"))
            diff = compare_sboms(base, sbom)
            write_json(args.diff_output, diff)
            if diff["added"] or diff["removed"] or diff["changed"]:
                print("SBOM DIFF FAILED: third-party components changed", file=sys.stderr)
                return 1
    except (ValueError, OSError, json.JSONDecodeError) as error:
        print(f"SBOM GENERATION FAILED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
