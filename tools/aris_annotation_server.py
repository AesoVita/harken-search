"""Loopback-only frame and zone service for the real Aris desktop window."""

from __future__ import annotations

import argparse
import io
import json
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ZONE_PATH = Path.home() / "AppData" / "Local" / "Temp" / "ArisAnnotationBridge" / "zones.json"


class AnnotationState:
    def __init__(self, *, zones_path: Path, window_title: str) -> None:
        self.zones_path = Path(zones_path)
        self.window_title = window_title
        self._lock = threading.Lock()
        self._frame_bytes = b""
        self._frame_width = 0
        self._frame_height = 0
        self._frame_sequence = 0
        self._frame_updated_at = 0.0
        self._capture_failures = 0
        self._capture_stop = threading.Event()
        self._capture_thread: threading.Thread | None = None

    def set_frame_for_test(self, data: bytes, *, width: int, height: int, sequence: int) -> None:
        with self._lock:
            self._frame_bytes = bytes(data)
            self._frame_width = int(width)
            self._frame_height = int(height)
            self._frame_sequence = int(sequence)
            self._frame_updated_at = time.monotonic()

    def _read_zones(self) -> dict[str, Any]:
        try:
            document = json.loads(self.zones_path.read_text(encoding="utf-8"))
            if not isinstance(document, dict) or not isinstance(document.get("zones", []), list):
                raise ValueError("zone document has invalid shape")
            return document
        except (OSError, ValueError, json.JSONDecodeError) as error:
            return {"zones": [], "error": str(error), "displayWidth": 0, "displayHeight": 0}

    def _frame_document(self) -> dict[str, Any]:
        with self._lock:
            age = None if not self._frame_updated_at else max(0.0, time.monotonic() - self._frame_updated_at)
            return {
                "width": self._frame_width,
                "height": self._frame_height,
                "sequence": self._frame_sequence,
                "ageSeconds": age,
                "available": bool(self._frame_bytes),
            }

    @staticmethod
    def _is_synchronized(frame: dict[str, Any], zones: dict[str, Any]) -> bool:
        return bool(
            frame["available"]
            and int(zones.get("displayWidth", 0)) == frame["width"]
            and int(zones.get("displayHeight", 0)) == frame["height"]
        )

    def session_document(self) -> dict[str, Any]:
        frame = self._frame_document()
        zones = self._read_zones()
        synchronized = self._is_synchronized(frame, zones)
        with self._lock:
            failures = self._capture_failures
        return {
            "windowTitle": self.window_title,
            "frame": frame,
            "zoneFrameNumber": zones.get("frameNumber"),
            "zoneExportAvailable": "error" not in zones,
            "captureFailures": failures,
            "synchronized": synchronized,
            "error": zones.get("error", ""),
        }

    def zones_document(self) -> dict[str, Any]:
        zones = self._read_zones()
        synchronized = self._is_synchronized(self._frame_document(), zones)
        result = dict(zones)
        result["synchronized"] = synchronized
        if not synchronized:
            result["zones"] = []
        return result

    def frame_bytes(self) -> bytes:
        with self._lock:
            return self._frame_bytes

    def health_document(self) -> dict[str, Any]:
        session = self.session_document()
        return {
            "ok": session["frame"]["available"] and session["zoneExportAvailable"],
            "synchronized": session["synchronized"],
            "captureFailures": session["captureFailures"],
        }

    def start_capture(self, fps: float) -> None:
        if fps <= 0 or self._capture_thread is not None:
            return
        self._capture_stop.clear()
        self._capture_thread = threading.Thread(target=self._capture_loop, args=(fps,), daemon=True)
        self._capture_thread.start()

    def stop_capture(self) -> None:
        self._capture_stop.set()
        if self._capture_thread is not None:
            self._capture_thread.join(timeout=3)
            self._capture_thread = None

    def _capture_loop(self, fps: float) -> None:
        interval = 1.0 / fps
        while not self._capture_stop.is_set():
            started = time.monotonic()
            try:
                data, width, height = capture_aris_client(self.window_title)
                with self._lock:
                    self._frame_bytes = data
                    self._frame_width = width
                    self._frame_height = height
                    self._frame_sequence += 1
                    self._frame_updated_at = time.monotonic()
            except (ImportError, OSError, RuntimeError, ValueError):
                with self._lock:
                    self._capture_failures += 1
            self._capture_stop.wait(max(0.0, interval - (time.monotonic() - started)))


def _find_visible_window(title_substring: str) -> int | None:
    if sys.platform != "win32":
        raise RuntimeError("Aris desktop annotation capture requires Windows")

    import ctypes
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    matches: list[int] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, _lparam):
        length = user32.GetWindowTextLengthW(hwnd)
        if length <= 0 or not user32.IsWindowVisible(hwnd):
            return True
        buffer = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buffer, length + 1)
        if title_substring.casefold() in buffer.value.casefold():
            matches.append(int(hwnd))
        return True

    user32.EnumWindows(visit, 0)
    return matches[0] if matches else None


def _client_rect_screen(hwnd: int) -> tuple[int, int, int, int]:
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    rect = wintypes.RECT()
    origin = wintypes.POINT(0, 0)
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
        raise OSError(ctypes.get_last_error(), "GetClientRect failed")
    if not user32.ClientToScreen(hwnd, ctypes.byref(origin)):
        raise OSError(ctypes.get_last_error(), "ClientToScreen failed")
    return origin.x, origin.y, origin.x + rect.right, origin.y + rect.bottom


def capture_aris_client(window_title: str) -> tuple[bytes, int, int]:
    from PIL import ImageGrab

    hwnd = _find_visible_window(window_title)
    if not hwnd:
        raise RuntimeError(f"No visible window contains title {window_title!r}")
    left, top, right, bottom = _client_rect_screen(hwnd)
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0:
        raise RuntimeError(f"Invalid Aris client size {width}x{height}")
    image = ImageGrab.grab(window=hwnd)
    if image.size != (width, height):
        image = ImageGrab.grab(bbox=(left, top, right, bottom), all_screens=True)
    if image.size != (width, height):
        raise RuntimeError(
            f"Captured Aris client size {image.width}x{image.height} does not match {width}x{height}")
    encoded = io.BytesIO()
    image.convert("RGB").save(encoded, format="JPEG", quality=86, optimize=False)
    return encoded.getvalue(), width, height


def _canonical_local_origin(origin: str | None) -> str | None:
    if not origin:
        return None
    match = re.fullmatch(r"http://(127\.0\.0\.1|localhost):([1-9][0-9]{0,4})", origin)
    if match is None:
        return None
    port = int(match.group(2))
    if port > 65535:
        return None
    host = "localhost" if match.group(1) == "localhost" else "127.0.0.1"
    return f"http://{host}:{port}"


def _local_origin(origin: str | None) -> bool:
    return not origin or _canonical_local_origin(origin) is not None


def _handler_for(state: AnnotationState):
    class AnnotationHandler(BaseHTTPRequestHandler):
        server_version = "ArisAnnotationBridge/1"

        def log_message(self, _format: str, *_args: object) -> None:
            return

        def _common_headers(self, content_type: str, length: int) -> None:
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(length))
            self.send_header("Cache-Control", "no-store")
            origin = _canonical_local_origin(self.headers.get("Origin"))
            if origin:
                self.send_header("Access-Control-Allow-Origin", origin)
                self.send_header("Vary", "Origin")

        def _send_json(self, status: int, document: dict[str, Any]) -> None:
            body = json.dumps(document, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self._common_headers("application/json; charset=utf-8", len(body))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            if not _local_origin(self.headers.get("Origin")):
                self._send_json(403, {"error": "non-local origin rejected"})
                return
            path = urlsplit(self.path).path
            if path == "/health":
                self._send_json(200, state.health_document())
                return
            if path == "/api/session":
                self._send_json(200, state.session_document())
                return
            if path == "/api/zones":
                self._send_json(200, state.zones_document())
                return
            if path == "/frame.jpg":
                frame = state.frame_bytes()
                if not frame:
                    self._send_json(503, {"error": "Aris frame unavailable"})
                    return
                self.send_response(200)
                self._common_headers("image/jpeg", len(frame))
                self.end_headers()
                self.wfile.write(frame)
                return
            self._send_json(404, {"error": "not found"})

        def _reject_write(self) -> None:
            self._send_json(405, {"error": "read-only service"})

        do_POST = _reject_write
        do_PUT = _reject_write
        do_PATCH = _reject_write
        do_DELETE = _reject_write

    return AnnotationHandler


def create_server(host: str, port: int, state: AnnotationState) -> ThreadingHTTPServer:
    if host != "127.0.0.1":
        raise ValueError("annotation bridge must bind to 127.0.0.1")
    return ThreadingHTTPServer((host, port), _handler_for(state))


def main() -> int:
    parser = argparse.ArgumentParser(description="Serve real Aris frames and ImGui annotation zones")
    parser.add_argument("--host", default="127.0.0.1", choices=["127.0.0.1"])
    parser.add_argument("--port", type=int, default=17891)
    parser.add_argument("--fps", type=float, default=5.0)
    parser.add_argument("--title", default="Aris")
    parser.add_argument("--zones", type=Path, default=DEFAULT_ZONE_PATH)
    args = parser.parse_args()

    state = AnnotationState(zones_path=args.zones, window_title=args.title)
    state.start_capture(args.fps)
    server = create_server(args.host, args.port, state)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        state.stop_capture()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
