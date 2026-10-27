"""Regression checks for the playback and UI paths used by this QA card."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def section(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]


def test_decoded_pcm_and_reverse_playback_use_processing_source() -> None:
    source = (ROOT / "src/AudioEngine.cpp").read_text(encoding="utf-8")
    pcm = section(source, "bool AudioEngine::installDecodedPcmSourceLocked", "void AudioEngine::unload")
    reverse = section(source, "bool AudioEngine::reloadWithReverseState", "void AudioEngine::setInPoint")
    for path in (pcm, reverse):
        assert "m_centerSenseDataSource->init(" in path
        assert "m_centerSenseDataSource->getDataSource()" in path
        assert "&m_magicEQRealtime" in path
    assert "TimeStretch" not in pcm
    assert reverse.count("m_centerSenseDataSource->init(") >= 2


def test_floating_handoff_moves_window_before_dock_target_probe() -> None:
    source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    drag = section(source, "auto driveFloatingDockDrag =", "auto drawFloatingDockTestPanel =")
    assert drag.index("ImGui::SetWindowPos(") < drag.index("detectDockZone(pos, size, moduleId)")
    assert "pos = ImGui::GetWindowPos();" in drag


def test_settings_expansion_remeasures_scrollable_content() -> None:
    source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    assert "ShouldUpdateSettingsBodyContentHeight(" in source
