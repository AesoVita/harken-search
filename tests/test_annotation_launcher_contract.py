from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_annotation_launcher_is_loopback_only_and_non_destructive():
    script = (ROOT / "tools" / "start_aris_annotation.ps1").read_text(encoding="utf-8")

    assert "$env:ARIS_ANNOTATION_BRIDGE = '1'" in script
    assert "http://127.0.0.1:$WebPort/?desktop-annotation=1&bridge=$BridgePort" in script
    assert "aris_annotation_server.py" in script
    assert "-WindowStyle Hidden" in script
    assert "Invoke-WebRequest" in script
    assert "$DryRun" in script
    assert "Stop-Process" not in script
    assert "taskkill" not in script.lower()
    assert "Remove-Item" not in script
