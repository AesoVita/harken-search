#!/usr/bin/env python3
"""Assemble a self-contained Aris release website in an empty directory."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]
PAGE_ROOT = ROOT / "website" / "release"
PROJECT_MODULE = ROOT / "website" / "src" / "release" / "project.mjs"
PUBLIC_ASSET = ROOT / "docs" / "manual" / "assets" / "aris-public-reference.svg"
PAGE_FILES = ("index.html", "styles.css", "app.mjs")


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _strict_catalog(path: Path) -> bytes:
    payload = path.read_bytes()
    if payload.startswith(b"\xef\xbb\xbf"):
        raise ValueError("catalog must not contain a UTF-8 BOM")
    value = json.loads(payload.decode("utf-8"))
    if not isinstance(value, dict) or value.get("schema_version") != 1 or value.get("product") != "aris":
        raise ValueError("catalog does not match the Aris release schema")
    return payload


def assemble(catalog: Path, output: Path) -> dict[str, str]:
    catalog_bytes = _strict_catalog(catalog)
    required = [*(PAGE_ROOT / name for name in PAGE_FILES), PROJECT_MODULE, PUBLIC_ASSET]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"release website source is incomplete: {', '.join(missing)}")

    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("output directory must be empty")
    asset_output = output / "manual-assets"
    asset_output.mkdir()
    for name in PAGE_FILES:
        shutil.copyfile(PAGE_ROOT / name, output / name)
    shutil.copyfile(PROJECT_MODULE, output / "project.mjs")
    (output / "catalog.json").write_bytes(catalog_bytes)
    shutil.copyfile(PUBLIC_ASSET, asset_output / PUBLIC_ASSET.name)

    if (output / "project.mjs").read_bytes() != PROJECT_MODULE.read_bytes():
        raise RuntimeError("assembled project.mjs drifted from the repository source")
    return {
        "catalog_sha256": _sha256(output / "catalog.json"),
        "project_mjs_sha256": _sha256(output / "project.mjs"),
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--catalog", required=True, type=Path)
    result.add_argument("--output", required=True, type=Path)
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        receipt = assemble(args.catalog.resolve(), args.output.resolve())
    except (OSError, UnicodeDecodeError, ValueError, json.JSONDecodeError) as error:
        print(f"release website assembly error: {error}", file=sys.stderr)
        return 2
    print(json.dumps(receipt, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
