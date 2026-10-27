from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverWallVisibleMirrorAudit.h"
SOURCE = ROOT / "src/CoverWallVisibleMirrorAudit.cpp"


def test_visible_mirror_audit_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "AuditResult",
        "CullStaleVisibleTextureMirrors",
        "IsCoverTextureHandleCurrent",
        "texCache.erase",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_visible_mirror_audit():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverWallVisibleMirrorAudit.h"' in main
    assert "CullStaleVisibleTextureMirrors" in main
