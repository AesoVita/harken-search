#!/usr/bin/env python3
"""Build or serve an Aris release catalog for local website QA only."""

from __future__ import annotations

import argparse
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import signal
import sys
import threading
from typing import Any

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from build_release_catalog import (  # noqa: E402
    build_catalog_bytes,
    canonical_json_bytes,
    load_source,
    write_catalog,
)


LOOPBACK_HOST = "127.0.0.1"


class PreviewHandler(BaseHTTPRequestHandler):
    server_version = "ArisReleaseCatalogPreview/1"

    def _send_bytes(self, status: int, content_type: str, payload: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self) -> None:  # noqa: N802
        route = self.path.split("?", 1)[0]
        if route == "/healthz":
            self._send_bytes(200, "application/json; charset=utf-8", b'{"ok":true}')
            return
        if route == "/catalog.json":
            payload = getattr(self.server, "catalog_bytes", b"")
            self._send_bytes(200, "application/json; charset=utf-8", payload)
            return
        self._send_bytes(404, "text/plain; charset=utf-8", b"not found")

    def log_message(self, _format: str, *_args: Any) -> None:
        return


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--source", type=Path, help="UTF-8 source receipt JSON")
    source.add_argument("--catalog", type=Path, help="UTF-8 catalog JSON to preview or serve")
    parser.add_argument("--output", type=Path, help="catalog output path when building from --source")
    parser.add_argument("--preview", action="store_true", help="write deterministic catalog JSON to stdout")
    parser.add_argument("--serve", action="store_true", help="serve catalog on loopback until stopped")
    parser.add_argument("--port", type=int, default=0, help="loopback TCP port; 0 selects an ephemeral port")
    return parser


def _catalog_bytes(args: argparse.Namespace) -> bytes:
    if args.port < 0 or args.port > 65535:
        raise ValueError("port must be between 0 and 65535")
    if args.output is not None and args.catalog is not None:
        raise ValueError("--output is only valid with --source")
    if not args.preview and not args.serve and args.output is None:
        raise ValueError("one of --preview, --serve, or --output is required")

    if args.source is not None:
        payload = build_catalog_bytes(
            load_source(args.source), source_dir=args.source.parent.resolve()
        )
        if args.output is not None:
            write_catalog(args.source, args.output)
        return payload

    if args.output is not None:
        raise ValueError("--output is only valid with --source")
    return canonical_json_bytes(load_source(args.catalog))


def _serve(payload: bytes, port: int) -> None:
    server = ThreadingHTTPServer((LOOPBACK_HOST, port), PreviewHandler)
    server.catalog_bytes = payload
    actual_port = int(server.server_address[1])

    def stop(_signum: int, _frame: Any) -> None:
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGINT, stop)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, stop)
    print(json.dumps({
        "event": "ready",
        "pid": os.getpid(),
        "host": LOOPBACK_HOST,
        "port": actual_port,
        "health_url": f"http://{LOOPBACK_HOST}:{actual_port}/healthz",
        "catalog_url": f"http://{LOOPBACK_HOST}:{actual_port}/catalog.json",
    }, ensure_ascii=False, separators=(",", ":")), flush=True)
    try:
        server.serve_forever(poll_interval=0.1)
    finally:
        server.server_close()


def main() -> int:
    args = _parser().parse_args()
    if args.preview and args.serve:
        print("preview error: --preview and --serve cannot be combined", file=sys.stderr, flush=True)
        return 2
    try:
        payload = _catalog_bytes(args)
        if args.preview:
            sys.stdout.buffer.write(payload)
            sys.stdout.buffer.flush()
        elif args.serve:
            _serve(payload, args.port)
        elif args.output is not None:
            print(args.output)
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"preview error: {error}", file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
