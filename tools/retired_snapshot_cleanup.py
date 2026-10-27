"""Archive reviewed obsolete source exports, then remove only those exact exports.

The archive preserves unique historical evidence outside the working repository.
No Git repository, worktree, arbitrary directory, or link target is removed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import zipfile

SNAPSHOTS = (
    "stage5-clean-index-20260711-0219",
    "stage5-clean-index-v2-20260711-0243",
    "stage5-clean-index-v3-20260711-035028",
    "stage5-clean-index-v4-20260711-051119",
    "stage5-clean-index-v4-final-20260711-054234",
)


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def linked(path: Path) -> bool:
    return path.is_symlink() or path.is_junction()


def checked_target(root: Path, name: str) -> Path:
    if name not in SNAPSHOTS:
        raise ValueError("snapshot is outside the exact reviewed allowlist")
    root = root.resolve(strict=True)
    if not (root / ".git").exists():
        raise ValueError("root is not a repository")
    parent = root / ".local"
    target = parent / name
    if linked(parent) or linked(target) or target.resolve().parent != parent.resolve():
        raise ValueError("snapshot path is a link or escapes the reviewed directory")
    if (target / ".git").exists():
        raise ValueError("refusing to remove a repository or worktree")
    return target


def inventory(root: Path, name: str) -> dict:
    target = checked_target(root, name)
    if not target.is_dir():
        raise ValueError("snapshot directory changed or is missing")
    files, links = {}, {}
    for base, directories, names in os.walk(target, followlinks=False):
        if ".git" in directories or ".git" in names:
            raise ValueError("snapshot contains a nested repository")
        for directory in directories[:]:
            path = Path(base) / directory
            if linked(path):
                if path != target / "build_dbg":
                    raise ValueError("unreviewed directory link in snapshot")
                links[path.relative_to(target).as_posix()] = os.readlink(path)
                directories.remove(directory)
        for name in names:
            path = Path(base) / name
            if linked(path):
                raise ValueError("unreviewed file link in snapshot")
            files[path.relative_to(target).as_posix()] = {"sha256": digest(path), "size": path.stat().st_size}
    return {"name": target.name, "files": dict(sorted(files.items())), "links": links}


def make_plan(root: Path) -> dict:
    root = root.resolve(strict=True)
    return {"repository": str(root), "snapshots": [
        inventory(root, name) for name in SNAPSHOTS if (root / ".local" / name).exists()
    ]}


def apply_plan(root: Path, plan: dict, archive: Path) -> dict:
    root = root.resolve(strict=True)
    if plan["repository"] != str(root):
        raise ValueError("reviewed repository changed")
    if archive.resolve().is_relative_to(root):
        raise ValueError("archive must be outside the repository")
    for item in plan["snapshots"]:
        if inventory(root, item["name"]) != item:
            raise ValueError("snapshot content changed after the dry-run")
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=4) as output:
        output.writestr("cleanup-manifest.json", json.dumps(plan, ensure_ascii=False, indent=2))
        for item in plan["snapshots"]:
            target = checked_target(root, item["name"])
            for relative in item["files"]:
                output.write(target / relative, item["name"] + "/" + relative)
    with zipfile.ZipFile(archive) as saved:
        for item in plan["snapshots"]:
            for relative, expected in item["files"].items():
                content = saved.read(item["name"] + "/" + relative)
                if len(content) != expected["size"] or hashlib.sha256(content).hexdigest() != expected["sha256"]:
                    raise ValueError("archive verification failed; no source deleted")
    # Recheck every source before deleting any export. A concurrent edit aborts.
    for item in plan["snapshots"]:
        if inventory(root, item["name"]) != item:
            raise ValueError("snapshot content changed during archival; no source deleted")
    removed = []
    for item in plan["snapshots"]:
        target = checked_target(root, item["name"])
        for relative in item["files"]:
            (target / relative).unlink()
        for relative in item["links"]:
            link = target / relative
            if link.is_junction():
                link.rmdir()
            else:
                link.unlink()
        for base, _, _ in os.walk(target, topdown=False, followlinks=False):
            Path(base).rmdir()
        removed.append(str(target))
    return {"archive": str(archive.resolve()), "archiveSha256": digest(archive), "removed": removed}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if args.dry_run:
        result = make_plan(args.root)
        args.plan.parent.mkdir(parents=True, exist_ok=True)
        args.plan.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({"dryRun": True, "snapshots": [item["name"] for item in result["snapshots"]]}))
    else:
        if args.archive is None or args.receipt is None:
            parser.error("apply requires --archive and --receipt")
        result = apply_plan(args.root, json.loads(args.plan.read_text(encoding="utf-8")), args.archive)
        args.receipt.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
