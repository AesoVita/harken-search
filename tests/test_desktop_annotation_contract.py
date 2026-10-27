from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "src" / "webui" / "src"
ANNOTATION = WEB / "components" / "desktop-annotation"


def test_desktop_annotation_route_uses_real_frame_and_stable_dom_zones():
    main = (WEB / "main.tsx").read_text(encoding="utf-8")
    page = (ANNOTATION / "DesktopAnnotationPage.tsx").read_text(encoding="utf-8")
    css = (ANNOTATION / "desktop-annotation.css").read_text(encoding="utf-8")

    assert "desktop-annotation" in main
    assert "DesktopAnnotationPage" in main
    assert "/api/session" in page
    assert "/api/zones" in page
    assert "/frame.jpg?sequence=" in page
    assert "ResizeObserver" in page
    assert "session?.synchronized" in page
    assert 'data-annotation-zone="true"' in page
    assert "id={zone.stableId}" in page
    assert "data-source-anchor={zone.sourceAnchor}" in page
    assert "onKeyDown" not in page
    assert "WebSocket" not in page
    assert 'method: "POST"' not in page
    assert ".desktop-annotation-zone{" in css


def test_annotation_transport_types_preserve_native_identity_and_geometry():
    types = (ANNOTATION / "annotation-types.ts").read_text(encoding="utf-8")

    for field in (
        "stableId", "key", "displayName", "sourceAnchor", "minX", "minY",
        "maxX", "maxY", "displayWidth", "displayHeight", "synchronized",
    ):
        assert field in types
