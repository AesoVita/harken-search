"""Contract for MetadataWriter and PAL atomic replacement semantics."""

from pathlib import Path


def main() -> int:
    source = (Path(__file__).resolve().parents[1] / "src" / "MetadataWriter.cpp").read_text(
        encoding="utf-8"
    )
    platform_source = (
        Path(__file__).resolve().parents[1] / "src" / "platform" / "filesystem_win32.cpp"
    ).read_text(encoding="utf-8")
    start = source.index("bool MetadataWriter::atomicSwap")
    body = source[start : source.index("// General Field Writeback", start + 20)]
    platform_start = platform_source.index("bool AtomicFileReplace")
    platform_body = platform_source[platform_start:]
    failures = []
    if '#include "platform/filesystem.h"' not in source:
        failures.append("MetadataWriter does not depend on the PAL file boundary")
    if "Platform::FileSystem::AtomicFileReplace(originalPath, tempPath)" not in body:
        failures.append("MetadataWriter temp finalization does not delegate to PAL")
    if any(operation in source for operation in ("DeleteFileW(", "MoveFileW(", "MoveFileExW(")):
        failures.append("MetadataWriter still owns native replacement operations")
    if source.count("atomicSwap(") < 8:
        failures.append("metadata writers do not share the atomic swap helper")
    if "DeleteFileW(" in platform_body or "MoveFileW(" in platform_body:
        failures.append("PAL fallback can destroy the target before replacement succeeds")
    for failure in failures:
        print(f"RED: {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
