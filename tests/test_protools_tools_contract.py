from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_protools_probe_script_keeps_live_and_fallback_truth_separate():
    script = (ROOT / "tools/probe_protools_host_send.ps1").read_text(encoding="utf-8")
    assert "PTSL.dll" in script
    assert "PtAAF.dll" in script
    assert "AAFCOAPI.dll" in script
    assert "DigiAppWndClass" in script
    assert "DigiMDIWndClass" in script
    assert "AvidEditorMainWndClass" in script
    assert "PACEEdenExperienceProxy" in script
    assert "livePtslPossible" in script
    assert "aafFallbackPossible" in script
    assert "not proof of timeline insertion" in script
    assert "AAF fallback must not be reported as live spot" in script


if __name__ == "__main__":
    test_protools_probe_script_keeps_live_and_fallback_truth_separate()
    print("TestProToolsToolsContract passed")
