from pathlib import Path


def main() -> int:
    source = (Path(__file__).resolve().parents[1] / "src" / "main.cpp").read_text(
        encoding="utf-8"
    )
    failures = []
    if source.count("if (MetadataWriter::removeAlbumArt(p))") < 2:
        failures.append("cover removal UI paths do not gate success on physical removal")
    if "MetadataWriter::removeAlbumArt(p);\n                succeeded.push_back(p);" in source:
        failures.append("cover removal UI records failed physical removal as succeeded")
    if failures:
        for failure in failures:
            print(f"RED: {failure}")
        return 1
    print("PASS: cover removal UI persists only successful physical removals")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
