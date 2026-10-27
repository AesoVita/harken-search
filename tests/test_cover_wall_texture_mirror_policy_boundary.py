from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src/main.cpp"
HEADER = ROOT / "src/CoverWallTextureMirrorPolicy.h"
SOURCE = ROOT / "src/CoverWallTextureMirrorPolicy.cpp"


def test_texture_mirror_policy_has_independent_owner():
    assert HEADER.exists()
    assert SOURCE.exists()
    source = SOURCE.read_text(encoding="utf-8")
    for contract in (
        "MarkTextureMirrorsStaleFromNonCoverTexture",
        "CullTextureMirrorsMatchingNonCoverTexture",
        "HasCurrentTextureMirror",
        "nonCoverTextureEpoch",
        "texCache.erase",
    ):
        assert contract in source
    assert '"main.cpp"' not in source
    assert '"WindowsMain.h"' not in source
    assert '"platform/' not in source


def test_main_consumes_texture_mirror_policy():
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "CoverWallTextureMirrorPolicy.h"' in main
    assert "static void MarkCoverWallTextureMirrorsStaleFromNonCoverTexture(" not in main
    assert "static void CullCoverWallTextureMirrorsMatchingNonCoverTexture(" not in main
    assert "static bool CoverWallHasCurrentTextureMirror(" not in main
