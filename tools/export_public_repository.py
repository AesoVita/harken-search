"""Export committed public files, never a private repository's Git history."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tempfile

try:
    from tools import public_repo_manifest
except ModuleNotFoundError:
    import public_repo_manifest


def git(root: Path, *args: str) -> bytes:
    result = subprocess.run(["git", "-C", str(root), *args], capture_output=True)
    if result.returncode:
        raise ValueError(result.stderr.decode("utf-8", errors="replace").strip())
    return result.stdout


def export(root: Path, revision: str, output: Path, report: Path) -> dict:
    root = root.resolve(strict=True)
    try:
        commit = git(root, "rev-parse", "--verify", "--end-of-options", f"{revision}^{{commit}}").decode().strip()
    except ValueError as error:
        raise ValueError(f"Invalid source revision: {error}") from error
    tree = git(root, "rev-parse", f"{commit}^{{tree}}").decode().strip()
    if output.is_symlink() or (hasattr(output, "is_junction") and output.is_junction()):
        raise ValueError("Output must be an empty ordinary directory")
    output = output.resolve()
    if output.is_relative_to(root):
        raise ValueError("Output must be outside the source repository")
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError("Output must be an empty directory")
    report = report.resolve()
    if report.is_relative_to(output) or report.exists():
        raise ValueError("Report must be a new file outside the public output")
    selected = []
    excluded_count = 0
    seen = set()
    for record in git(root, "ls-tree", "-r", "-z", commit).split(b"\0"):
        if not record:
            continue
        metadata, raw_name = record.split(b"\t", 1)
        mode, kind, oid = metadata.decode().split()
        name = raw_name.decode("utf-8")
        if not public_repo_manifest.is_public_path(name):
            excluded_count += 1
            continue
        parts = PurePosixPath(name).parts
        if (not parts or name.startswith("/") or any(p in {".", ".."} for p in parts)
                or "\\" in name or ":" in name or name.casefold() in seen):
            raise ValueError(f"Unsupported public path: {name}")
        if mode not in {"100644", "100755"} or kind != "blob":
            raise ValueError(f"Unsupported public file mode: {name} ({mode})")
        seen.add(name.casefold())
        selected.append((name, oid, mode))
    if not selected:
        raise ValueError("The selected revision has no public files")
    output.mkdir(parents=True, exist_ok=True)
    files = []
    # Reuse one Git process; one request at a time avoids pipe deadlocks and
    # keeps large vendored binaries out of memory. No working-tree reads.
    with tempfile.TemporaryFile() as errors:
        process = subprocess.Popen(
            ["git", "-C", str(root), "cat-file", "--batch"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors,
        )
        try:
            for name, oid, mode in selected:
                process.stdin.write((oid + "\n").encode("ascii"))
                process.stdin.flush()
                header = process.stdout.readline().decode("ascii").strip().split()
                if len(header) != 3 or header[0] != oid or header[1] != "blob":
                    raise ValueError(f"Unable to read committed blob for {name}")
                size = int(header[2])
                destination = output / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                digest = hashlib.sha256()
                remaining = size
                with destination.open("xb") as stream:
                    while remaining:
                        chunk = process.stdout.read(min(remaining, 1024 * 1024))
                        if not chunk:
                            raise ValueError(f"Truncated committed blob for {name}")
                        stream.write(chunk)
                        digest.update(chunk)
                        remaining -= len(chunk)
                if process.stdout.read(1) != b"\n":
                    raise ValueError(f"Invalid Git blob boundary for {name}")
                if mode == "100755":
                    destination.chmod(destination.stat().st_mode | 0o111)
                files.append({"path": name, "size": size, "sha256": digest.hexdigest(), "git_blob": oid})
            process.stdin.close()
            if process.wait() != 0:
                raise ValueError("Git blob export failed")
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait()
            process.stdout.close()
            if not process.stdin.closed:
                process.stdin.close()
    receipt = {
        "schema_version": 1, "source_commit": commit, "source_tree": tree,
        "filter_sha256": hashlib.sha256(Path(public_repo_manifest.__file__).read_bytes()).hexdigest(),
        "excluded_count": excluded_count, "files": files,
    }
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = export(args.root, args.revision, args.output, args.report)
    except (ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps({"source_commit": receipt["source_commit"],
                      "public_file_count": len(receipt["files"]),
                      "excluded_count": receipt["excluded_count"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
