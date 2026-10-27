import json
import inspect
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

import pytest

import tools.aris_annotation_server as annotation_server
from tools.aris_annotation_server import AnnotationState, create_server


def request(url: str, *, method: str = "GET", origin: str | None = None):
    headers = {"Origin": origin} if origin else {}
    return urllib.request.urlopen(urllib.request.Request(url, method=method, headers=headers), timeout=2)


@pytest.fixture()
def running_server(tmp_path: Path):
    zones_path = tmp_path / "zones.json"
    zones_path.write_text(json.dumps({
        "schemaVersion": 1,
        "frameNumber": 7,
        "displayWidth": 1200,
        "displayHeight": 800,
        "zones": [{"stableId": "toolbar--0", "key": "toolbar"}],
    }), encoding="utf-8")
    state = AnnotationState(zones_path=zones_path, window_title="Aris")
    state.set_frame_for_test(b"jpeg-data", width=1200, height=800, sequence=3)
    server = create_server("127.0.0.1", 0, state)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{server.server_port}"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


def test_loopback_endpoints_expose_synchronized_frame_and_zones(running_server: str):
    with request(f"{running_server}/health") as response:
        health = json.load(response)
        assert response.headers["Cache-Control"] == "no-store"
        assert health["ok"] is True

    with request(f"{running_server}/api/session") as response:
        session = json.load(response)
        assert session["frame"]["width"] == 1200
        assert session["frame"]["sequence"] == 3
        assert session["synchronized"] is True

    with request(f"{running_server}/api/zones") as response:
        zones = json.load(response)
        assert zones["zones"][0]["stableId"] == "toolbar--0"
        assert zones["synchronized"] is True

    with request(f"{running_server}/frame.jpg") as response:
        assert response.headers["Content-Type"] == "image/jpeg"
        assert response.read() == b"jpeg-data"


def test_service_rejects_unknown_paths_writes_and_non_local_origins(running_server: str):
    with pytest.raises(urllib.error.HTTPError) as missing:
        request(f"{running_server}/../../secret")
    assert missing.value.code == 404

    for method in ("POST", "PUT", "PATCH", "DELETE"):
        with pytest.raises(urllib.error.HTTPError) as write:
            request(f"{running_server}/api/session", method=method)
        assert write.value.code == 405

    with pytest.raises(urllib.error.HTTPError) as origin:
        request(f"{running_server}/api/session", origin="https://example.com")
    assert origin.value.code == 403


def test_service_rejects_non_loopback_binding(tmp_path: Path):
    state = AnnotationState(zones_path=tmp_path / "zones.json", window_title="Aris")
    with pytest.raises(ValueError, match="127.0.0.1"):
        create_server("0.0.0.0", 0, state)


@pytest.mark.parametrize("origin", [
    "http://localhost:1",
    "http://localhost:65535",
    "http://127.0.0.1:8765",
])
def test_service_preserves_valid_local_cors_origins(running_server: str, origin: str):
    with request(f"{running_server}/health", origin=origin) as response:
        assert response.headers["Access-Control-Allow-Origin"] == origin
        assert response.headers["Vary"] == "Origin"


@pytest.mark.parametrize("origin", [
    "http://localhost:8765@remote.example",
    "http://127.0.0.1:8765/path",
    "http://localhost:8765?query=value",
    "http://localhost:8765#fragment",
    "http://localhost:0",
    "http://localhost:65536",
    "http://localhost:not-a-port",
    "http://localhost:8765\r\nX-Injected: yes",
    "http://localhost:8765\nX-Injected: yes",
    "http://localhost:8765\x00",
])
def test_local_origin_rejects_non_origins_and_header_injection(origin: str):
    assert not annotation_server._local_origin(origin)


def test_service_rejects_folded_origin_without_reflecting_it(running_server: str):
    with pytest.raises(urllib.error.HTTPError) as rejected:
        request(f"{running_server}/health", origin="http://localhost:8765\r\n X-Injected: yes")
    assert rejected.value.code == 403
    assert "Access-Control-Allow-Origin" not in rejected.value.headers
    assert "X-Injected" not in rejected.value.headers


def test_service_never_exposes_arbitrary_files(tmp_path: Path):
    secret = tmp_path / "secret.txt"
    secret.write_text("must-not-leak", encoding="utf-8")
    state = AnnotationState(zones_path=tmp_path / "missing-zones.json", window_title="Aris")
    server = create_server("127.0.0.1", 0, state)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base = f"http://127.0.0.1:{server.server_port}"
        for path in ("/secret.txt", "/../secret.txt", "/%2e%2e/secret.txt"):
            with pytest.raises(urllib.error.HTTPError) as response:
                request(base + path)
            assert response.value.code == 404
            assert b"must-not-leak" not in response.value.read()
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


def test_frame_endpoint_handles_100_requests_and_capture_recovers(tmp_path: Path, monkeypatch):
    zones_path = tmp_path / "zones.json"
    zones_path.write_text(json.dumps({
        "displayWidth": 320,
        "displayHeight": 180,
        "zones": [],
    }), encoding="utf-8")
    state = AnnotationState(zones_path=zones_path, window_title="Aris")
    attempts = 0

    def capture(_window_title: str):
        nonlocal attempts
        attempts += 1
        if attempts == 1:
            raise ModuleNotFoundError("temporary capture dependency failure")
        if attempts == 2:
            raise RuntimeError("temporary capture failure")
        if attempts == 3:
            raise ValueError("temporary capture value failure")
        return b"recovered-jpeg", 320, 180

    monkeypatch.setattr(annotation_server, "capture_aris_client", capture)
    state.start_capture(100.0)
    deadline = time.monotonic() + 2.0
    while not state.frame_bytes() and time.monotonic() < deadline:
        time.sleep(0.01)
    state.stop_capture()

    assert state.frame_bytes() == b"recovered-jpeg"
    assert state.session_document()["captureFailures"] == 3

    server = create_server("127.0.0.1", 0, state)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        url = f"http://127.0.0.1:{server.server_port}/frame.jpg"
        for _ in range(100):
            with request(url) as response:
                assert response.read() == b"recovered-jpeg"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


def test_capture_path_does_not_depend_on_ignored_harness_files():
    source = inspect.getsource(annotation_server.capture_aris_client)

    assert ".openclaw" not in source
    assert "from screenshot import" not in source


def test_dimension_mismatch_hides_stale_zone_overlay(tmp_path: Path):
    zones_path = tmp_path / "zones.json"
    zones_path.write_text(json.dumps({
        "displayWidth": 900,
        "displayHeight": 600,
        "zones": [{"stableId": "stale--0"}],
    }), encoding="utf-8")
    state = AnnotationState(zones_path=zones_path, window_title="Aris")
    state.set_frame_for_test(b"jpeg-data", width=1200, height=800, sequence=1)

    session = state.session_document()
    zones = state.zones_document()

    assert session["synchronized"] is False
    assert zones["synchronized"] is False
    assert zones["zones"] == []
