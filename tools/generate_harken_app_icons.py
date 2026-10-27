"""Generate or verify Harken Search icons from the preserved, user-supplied PNGs."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/icons/source/harken-app-icons"
SIZES = (16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 128, 192, 256)


def png(size: int) -> Image.Image:
    source_size = next(n for n in (16, 20, 24, 30, 32, 36, 40, 48, 60, 64,
                                  72, 80, 96, 128, 192, 256, 512, 1024)
                       if n >= size)
    with Image.open(SOURCE / "Search" / f"Search transparent {source_size}.png") as image:
        result = image.convert("RGBA")
    return result if size == source_size else result.resize((size, size), Image.Resampling.LANCZOS)


def outputs(website: Path | None) -> dict[Path, bytes]:
    icon = io.BytesIO()
    png(256).save(icon, format="ICO", sizes=[(n, n) for n in SIZES],
                  append_images=[png(n) for n in SIZES if n != 256])
    result = {ROOT / "HarkenSearch.ico": icon.getvalue()}
    for name, size in (("auro.png", 20), ("auro@2x.png", 40),
                       ("harken.png", 20), ("harken@2x.png", 40)):
        buffer = io.BytesIO()
        png(size).save(buffer, format="PNG")
        result[ROOT / "assets/icons/brand" / name] = buffer.getvalue()
    if website is not None:
        result[website / "public/favicon.ico"] = icon.getvalue()
        buffer = io.BytesIO()
        png(180).save(buffer, format="PNG")
        result[website / "public/apple-touch-icon.png"] = buffer.getvalue()
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check without changing any files")
    parser.add_argument("--website-root", type=Path, help="HarkenSearch/frontend directory")
    args = parser.parse_args()
    manifest = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
    for item in manifest["files"]:
        path = SOURCE / item["path"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise SystemExit(f"Source checksum mismatch: {path}")
        with Image.open(path) as image:
            image.verify()
    generated = outputs(args.website_root)
    for path, expected in generated.items():
        if args.check:
            if not path.is_file() or path.read_bytes() != expected:
                raise SystemExit(f"Generated icon mismatch: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(expected)
    with Image.open(ROOT / "HarkenSearch.ico") as icon:
        if icon.ico.sizes() != {(n, n) for n in SIZES}:
            raise SystemExit("Missing ICO resolutions")
        for size in SIZES:
            if icon.ico.getimage((size, size)).convert("RGBA").tobytes() != png(size).tobytes():
                raise SystemExit(f"ICO pixels differ from source: {size}")
    print(f"PASS: {len(manifest['files'])} source files; {len(generated)} outputs; {len(SIZES)} ICO resolutions")


if __name__ == "__main__":
    main()
