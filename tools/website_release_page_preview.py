#!/usr/bin/env python3
"""Serve the local Aris release page on loopback for visual QA."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import signal
import sys
import threading
from urllib.parse import unquote, urlsplit
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PAGE_ROOT = ROOT / "website" / "release"
MANUAL_ROOT = ROOT / "docs" / "manual"
RELEASE_MODULE = ROOT / "website" / "src" / "release" / "project.mjs"
LOOPBACK_HOST = "127.0.0.1"


class ReleasePageHandler(BaseHTTPRequestHandler):
    server_version = "ArisReleasePagePreview/1"

    def _send(self, status: int, content_type: str, payload: bytes, *, include_body: bool = True) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if include_body:
            self.wfile.write(payload)

    def _dispatch(self, *, include_body: bool) -> None:
        raw_route = urlsplit(self.path).path
        if any(token in raw_route.lower() for token in ("%2e", "%2f", "%5c")) or ".." in Path(raw_route).parts:
            self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
            return
        route = unquote(raw_route)
        if route == "/healthz":
            self._send(200, "application/json; charset=utf-8", b'{"ok":true}', include_body=include_body)
            return
        if route == "/catalog.json":
            self._send(200, "application/json; charset=utf-8", self.server.catalog_bytes, include_body=include_body)
            return
        if route == "/catalog-r2.json":
            if self.server.catalog_r2_bytes is None:
                self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
                return
            self._send(200, "application/json; charset=utf-8", self.server.catalog_r2_bytes, include_body=include_body)
            return
        if route == "/catalog-invalid.json":
            self._send(200, "application/json; charset=utf-8", b"{\"invalid\":true}", include_body=include_body)
            return
        if route == "/project.mjs":
            if not RELEASE_MODULE.is_file():
                self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
                return
            self._send(200, "text/javascript; charset=utf-8", RELEASE_MODULE.read_bytes(), include_body=include_body)
            return
        if route.startswith("/manual-assets/"):
            relative = route.removeprefix("/manual-assets/")
            if not relative or relative.startswith(("/", "\\")) or ".." in Path(relative).parts or ":" in relative or "//" in relative:
                self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
                return
            target = (MANUAL_ROOT / "assets" / relative).resolve()
            try:
                target.relative_to(MANUAL_ROOT.resolve())
            except ValueError:
                self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
                return
            if not target.is_file() or target.suffix.lower() not in {".png", ".svg"}:
                self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
                return
            content_type = "image/svg+xml" if target.suffix.lower() == ".svg" else "image/png"
            self._send(200, content_type, target.read_bytes(), include_body=include_body)
            return
        if route in {"/", "/index.html"}:
            route = "/index.html"
        target = (PAGE_ROOT / route.lstrip("/")).resolve()
        try:
            target.relative_to(PAGE_ROOT.resolve())
        except ValueError:
            self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
            return
        if not target.is_file():
            self._send(404, "text/plain; charset=utf-8", b"not found", include_body=include_body)
            return
        content_types = {".html": "text/html", ".css": "text/css", ".mjs": "text/javascript"}
        self._send(200, f"{content_types.get(target.suffix, 'application/octet-stream')}; charset=utf-8", target.read_bytes(), include_body=include_body)

    def do_GET(self) -> None:  # noqa: N802
        self._dispatch(include_body=True)

    def do_HEAD(self) -> None:  # noqa: N802
        self._dispatch(include_body=False)

    def log_message(self, _format: str, *_args: Any) -> None:
        return


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--catalog", required=True, type=Path)
    result.add_argument("--port", type=int, default=0)
    return result


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, OSError, ValueError):
            pass
    args = parser().parse_args()
    try:
        if args.port < 0 or args.port > 65535:
            raise ValueError("port must be between 0 and 65535")
        catalog_bytes = args.catalog.read_bytes()
        json.loads(catalog_bytes.decode("utf-8"))
        if not (MANUAL_ROOT / "assets" / "aris-public-reference.svg").is_file():
            raise FileNotFoundError("missing repository-local manual asset")
        if not RELEASE_MODULE.is_file():
            raise FileNotFoundError("missing repository-local release module")
        server = ThreadingHTTPServer((LOOPBACK_HOST, args.port), ReleasePageHandler)
        server.catalog_bytes = catalog_bytes
        catalog_r2 = ROOT / "tests" / "website" / "fixtures" / "release_catalog_r2.json"
        server.catalog_r2_bytes = catalog_r2.read_bytes() if catalog_r2.is_file() else None
    except (OSError, UnicodeDecodeError, ValueError, json.JSONDecodeError) as error:
        print(f"release page preview error: {error}", file=sys.stderr, flush=True)
        return 2

    def stop(_signum: int, _frame: Any) -> None:
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGINT, stop)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, stop)
    port = int(server.server_address[1])
    print(json.dumps({
        "event": "ready",
        "pid": os.getpid(),
        "host": LOOPBACK_HOST,
        "port": port,
        "origin": f"http://{LOOPBACK_HOST}:{port}",
    }, separators=(",", ":")), flush=True)
    try:
        server.serve_forever(poll_interval=0.1)
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
