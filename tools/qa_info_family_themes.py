"""Capture Info/Similar Sounds in all built-in themes on a verified isolated QA EXE.

The caller starts the package with a synthetic library and Info already floating.
No desktop pointer or keyboard input is sent. Resizing uses the app-owned QA seam.
Requires Pillow; preserves full screenshots, window crops and command receipts.
"""
from __future__ import annotations

import argparse
from collections import Counter
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import time

from PIL import Image, ImageDraw, ImageFont, ImageGrab
from qa_transport_themes import Driver, contrast, window_api

THEMES = [(0, "经典深色"), (1, "极光青"), (2, "深海军蓝"),
          (3, "海玻璃"), (10, "柔雾粉")]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def wait_ready(driver: Driver) -> None:
    deadline = time.monotonic() + 30
    while not driver.command({"cmd": "wait_ready"})["ready"]:
        if time.monotonic() >= deadline:
            raise TimeoutError("The isolated main window did not become ready")
        time.sleep(.1)


def capture(driver, user32, identity, output, theme, scenario):
    dock = driver.command({"cmd": "dock_state"})
    state = driver.command({"cmd": "similar_index", "action": "state"})
    actual = wintypes.DWORD()
    hwnd = identity["hwnd"]
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(actual))
    if actual.value != identity["pid"]:
        raise RuntimeError("QA HWND no longer belongs to the verified process")
    rect, origin = wintypes.RECT(), wintypes.POINT(0, 0)
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)) or not user32.ClientToScreen(hwnd, ctypes.byref(origin)):
        raise OSError("Could not read QA client bounds")
    image = ImageGrab.grab(window=hwnd).convert("RGB")
    if image.size != (rect.right, rect.bottom):
        raise RuntimeError(f"Unexpected capture size: {image.size}")
    stem = f"{theme}-{scenario}"
    full = output / f"{stem}-full.png"
    image.save(full)
    crops = {}
    for module in ("similar-sounds", "info"):
        panel = dock["panels"][module]
        if not panel["floating"]:
            raise RuntimeError(f"{module} must be floating for this comparison")
        x, y = round(panel["x"] - origin.x), round(panel["y"] - origin.y)
        w, h = round(panel["w"]), round(panel["h"])
        bounds = (x, y, x + w, y + h)
        if x < 0 or y < 0 or x + w > image.width or y + h > image.height:
            raise RuntimeError(f"{module} crop lies outside the verified client")
        crop = image.crop(bounds)
        if len(crop.getcolors(w * h) or []) < 20:
            raise RuntimeError(f"{module} appears blank")
        path = output / f"{stem}-{module}.png"
        crop.save(path)
        crops[module] = {"path": str(path), "sha256": digest(path), "bounds": bounds, "size": [w, h]}
    width = crops["similar-sounds"]["size"][0]
    if abs(width - (420 if scenario == "narrow" else 640)) > 2:
        raise RuntimeError(f"Unexpected Similar Sounds width: {width}")
    if scenario != "empty" and state["rows"] != 50:
        raise RuntimeError(f"Expected 50 similarity results: {state}")
    row = {"theme": theme, "scenario": scenario, "dock": dock, "state": state,
           "full": str(full), "full_sha256": digest(full), "crops": crops,
           "client_origin": [origin.x, origin.y]}
    (output / f"{stem}.json").write_text(json.dumps(row, ensure_ascii=False, indent=2), encoding="utf-8")
    return row


def resize_similar(driver, target_width):
    current = driver.command({"cmd": "dock_state"})["panels"]["similar-sounds"]["w"]
    driver.command({"cmd": "dock_pointer", "phase": "hover", "module": "similar-sounds", "part": "resize"})
    point = driver.command({"cmd": "dock_pointer", "phase": "down", "module": "similar-sounds", "part": "resize"})
    time.sleep(.1)
    for phase in ("move", "up"):
        driver.command({"cmd": "dock_pointer", "phase": phase,
                        "x": point["x"] + target_width - current, "y": point["y"]})
        time.sleep(.15)


def comparison(output, results, scenario):
    # Keep screenshot pixels at 1:1; annotations sit outside the real windows.
    height = 292 if scenario != "narrow" else 380
    width = 1320 if scenario != "narrow" else 1100
    canvas = Image.new("RGB", (width, len(THEMES) * (height + 50) + 16), "#edf2f6")
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 20)
    for i, (theme, name) in enumerate(THEMES):
        row = next(r for r in results if r["theme"] == theme and r["scenario"] == scenario)
        y = 12 + i * (height + 50)
        draw.text((20, y), name + " · 相似音效", fill="#23374c", font=font)
        x = 20
        for module in ("similar-sounds", "info"):
            source = Image.open(row["crops"][module]["path"]).convert("RGB")
            if module == "info":
                draw.text((x, y), "Info", fill="#23374c", font=font)
            canvas.paste(source.crop((0, 0, source.width, height)), (x, y + 32))
            x += source.width + 20
    path = output / f"五主题-{scenario}.png"
    canvas.save(path)
    return str(path)


def audit_captures(output, report):
    """Measure readable stroke cores in fixed, 1:1 fixture crops, not AA edges.

    These are targeted screenshot samples, not a claim about the whole app.
    Geometry receipts and the full images remain available for visual review.
    """
    checks = []
    for row in report["results"]:
        narrow = row["scenario"] == "narrow"
        samples = [
            ("similar-sounds", "窗口标题", (8, 4, 68, 20)),
            ("similar-sounds", "当前音效提示", (20, 48, 64, 66)),
            ("similar-sounds", "文件名称或空态文字", (73, 48, 240, 66)),
            ("similar-sounds", "查找按钮（禁用）" if row["scenario"] == "empty" else "查找按钮",
             (162, 77, 240, 99) if narrow else (537, 45, 620, 67)),
            ("info", "Info 空态提示", (20, 76, 165, 92)),
        ]
        for module, label, bounds in samples:
            source_path = Path(row["crops"][module]["path"])
            if digest(source_path) != row["crops"][module]["sha256"]:
                raise RuntimeError(f"Screenshot changed: {source_path}")
            crop = Image.open(source_path).convert("RGB").crop(bounds)
            counts = Counter(dict((color, count) for count, color in crop.getcolors(crop.width * crop.height)))
            background, background_count = counts.most_common(1)[0]
            if background_count / (crop.width * crop.height) < .4:
                raise RuntimeError(f"Expected a flat text surface for {source_path}: {label}")
            ratio, ink = max((contrast([*color, 255], [*background, 255]), color) for color in counts)
            checks.append({"theme": row["theme"], "scenario": row["scenario"], "module": module,
                           "label": label, "bounds": bounds, "foreground_rgb": ink,
                           "background_rgb": background, "ratio": round(ratio, 3),
                           "pass": ratio >= 4.5, "source": str(source_path)})
    audit = {"method": "Real 1:1 screenshot samples: most frequent flat background and highest-contrast glyph core; antialiased edges are not treated as a new text color.",
             "scope": "Window title, current-audio label, filename/empty text, enabled/disabled find button, Info artwork empty message. Other controls need separate coverage.",
             "checks": checks, "failed": [c for c in checks if not c["pass"]],
             "minimum": min(c["ratio"] for c in checks)}
    (output / "contrast-samples.json").write_text(json.dumps(audit, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"contrast_samples": len(checks), "failed": len(audit["failed"]), "minimum": audit["minimum"]}))
    if audit["failed"]:
        raise AssertionError("Some screenshot text samples are below 4.5:1; inspect contrast-samples.json")
    return audit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--audit-only", action="store_true", help="Measure saved verified crops without operating the app")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.audit_only:
        audit_captures(args.output, json.loads((args.output / "report.json").read_text(encoding="utf-8")))
        return
    if not args.runtime or not args.exe or not args.pid:
        parser.error("--runtime, --exe and --pid are required when capturing")
    driver = Driver(args.runtime, args.output)
    user32 = window_api()
    identity = driver.command({"cmd": "magic_eq_identity"}, timeout=45)
    if identity["pid"] != args.pid or Path(identity["exe_path"]).resolve() != args.exe.resolve():
        raise RuntimeError("The runtime does not belong to the requested QA process/package")
    isolation = Path(identity.get("isolation_root", "")).resolve()
    if not identity.get("isolation_root") or not args.runtime.resolve().is_relative_to(isolation):
        raise RuntimeError("A compile-time isolated QA profile is required")
    wait_ready(driver)
    initial_theme = driver.command({"cmd": "transport_audit"})["theme"]
    deadline = time.monotonic() + 20
    while initial_theme not in [theme for theme, _ in THEMES]:
        if time.monotonic() >= deadline:
            raise TimeoutError("No rendered built-in theme was observed")
        time.sleep(.1)
        initial_theme = driver.command({"cmd": "transport_audit"})["theme"]
    results = []
    try:
        driver.command({"cmd": "similar_index", "action": "open", "width": 640,
                        "height": 620, "floating": True, "language": 0})
        for theme, name in THEMES:
            driver.command({"cmd": "set_theme", "theme": theme})
            driver.rendered(theme)
            time.sleep(.4)
            results.append(capture(driver, user32, identity, args.output, theme, "empty"))
            print(f"{name}: empty captured", flush=True)
        driver.command({"cmd": "search", "query": ""})
        wait_ready(driver)
        driver.command({"cmd": "select_row", "row": 0})
        time.sleep(.4)
        driver.command({"cmd": "similar_index", "action": "find"})
        deadline = time.monotonic() + 30
        while driver.command({"cmd": "similar_index", "action": "state"})["rows"] != 50:
            if time.monotonic() >= deadline:
                raise TimeoutError("The fixture did not produce 50 similarity results")
            time.sleep(.1)
        for theme, name in THEMES:
            driver.command({"cmd": "set_theme", "theme": theme})
            driver.rendered(theme)
            time.sleep(.4)
            results.append(capture(driver, user32, identity, args.output, theme, "results"))
            resize_similar(driver, 420)
            time.sleep(.4)
            results.append(capture(driver, user32, identity, args.output, theme, "narrow"))
            resize_similar(driver, 640)
            print(f"{name}: results and narrow captured", flush=True)
        comparisons = [comparison(args.output, results, state) for state in ("empty", "results", "narrow")]
        report = {"identity": identity, "exe_sha256": digest(args.exe), "results": results,
                  "comparisons": comparisons, "scenario_count": len(results),
                  "scope": "Floating Info and Similar Sounds: empty, 50 results, 420px narrow. Screenshots require visual review."}
        (args.output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        audit_captures(args.output, report)
        print(json.dumps({"scenarios": len(results), "comparisons": comparisons}, ensure_ascii=False))
    finally:
        driver.command({"cmd": "set_theme", "theme": initial_theme})


if __name__ == "__main__":
    main()
