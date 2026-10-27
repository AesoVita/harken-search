from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_native_ui_regions_collect_for_bridge_without_visual_inspector():
    main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    assert "if (ThemeInspector::IsActive())" not in main
    assert main.count("if (ThemeInspector::IsCollecting())") >= 11
    for key in (
        "menuBarBG",
        "searchButtonBG",
        "toolbarBG",
        "filterBrowserBG",
        "resultsTabBG",
        "resultsBG",
        "metadataPanelBG",
        "waveformBG",
        "transportBarBG",
        "mainWindowBG",
    ):
        assert f'"{key}"' in main


def test_theme_inspector_keeps_visual_overlay_separate_from_collection():
    header = (ROOT / "src" / "ThemeInspector.h").read_text(encoding="utf-8")
    source = (ROOT / "src" / "ThemeInspector.cpp").read_text(encoding="utf-8")

    assert "void RegisterLastItem(" in header
    assert "ImGui::GetItemRectMin()" in source
    assert "ImGui::GetItemRectMax()" in source
    assert "return g_active || g_bridgeCollectionEnabled;" in source
    assert "if (!g_active || g_zones.empty())" in source
    assert "if (!g_active || !g_hasHovered)" in source


def test_transport_and_waveform_controls_publish_granular_annotation_zones():
    main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    for key in (
        "transport.volume",
        "transport.stereo",
        "transport.reverse",
        "transport.pan",
        "transport.centerSense",
        "transport.database",
        "transport.spectrum",
        "waveform.pitch",
        "waveform.pitchMode",
        "waveform.seek",
        "waveform.scroll",
        "waveform.amplitude",
        "waveform.footer.previous",
        "waveform.footer.play",
        "waveform.footer.next",
        "waveform.footer.shuffle",
        "waveform.footer.listeningMode",
        "waveform.footer.loop",
        "waveform.footer.normalize",
        "waveform.footer.sendToDaw",
        "splitter.left",
        "splitter.right",
        "splitter.waveform",
        "sidebar.filter",
    ):
        assert f'"{key}"' in main

    assert '"file.row.browser."' in main
    assert '"file.row.database."' in main
    assert "HashString64Local(entry.fullPath)" in main
    assert "static_cast<std::uint64_t>(record.id)" in main
