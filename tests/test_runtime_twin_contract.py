from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "src/webui/src/components/runtime-twin/runtime-contract.ts"
TWIN = ROOT / "src/webui/src/components/runtime-twin"


def test_runtime_twin_contract_has_reference_geometry():
    text = CONTRACT.read_text(encoding="utf-8")
    assert "width: 2576" in text
    assert "height: 1408" in text
    for region in ("chrome", "sidebar", "workspace", "info", "transport"):
        assert f"{region}:" in text
    assert "src/main.cpp" in text
    assert "build/Release/Aris.exe" in text
    assert "x: 536, y: 96, width: 1490" in text


def test_runtime_twin_preserves_source_backed_runtime_defaults():
    chrome = (TWIN / "RuntimeChrome.tsx").read_text(encoding="utf-8")
    sidebar = (TWIN / "RuntimeSidebar.tsx").read_text(encoding="utf-8")
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    page = (TWIN / "RuntimeTwinPage.tsx").read_text(encoding="utf-8")
    info = (TWIN / "RuntimeInfoPanel.tsx").read_text(encoding="utf-8")
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # WindowsMain title remains; the approved simplification removes the
    # redundant right-side result status.
    assert "Aris - JUSTMIXRFull (0 文件)" in chrome
    assert "result-label" not in chrome
    assert "无结果" not in chrome

    # Source-backed correction pass: toolbar navigation follows the measured
    # main.cpp capsule, and the right-side control is RenderPresetDial rather
    # than a pair of static tabs.
    dial = (TWIN / "RuntimePresetDial.tsx").read_text(encoding="utf-8")
    assert "const INITIAL_PRESETS = ['SFX', 'Music', 'Meta', 'Loud']" in dial
    assert 'className="preset-dial"' in dial
    assert "onWheel={handleWheel}" in dial
    assert "onPointerDown={startDrag}" in dial
    assert "mode-switch" not in chrome
    assert ".runtime-twin .top-modes{left:865px;width:700px}" in css
    assert ".runtime-twin .top-modes button:nth-child(1){width:133px}" in css
    assert ".runtime-twin .top-modes .nav-dice{width:28px;min-width:28px" in css
    assert "No results" not in chrome

    # Current settings.ini: ViewMode hidden; visible order is 5,1,4 | 2,3.
    assert "['spark', 'branch', 'tag', 'funnel', 'list']" in sidebar
    assert "i === 3 ? 'rail-group-break'" in sidebar
    # ImGui's requested 199px child plus its rendered frame extent is 203px in
    # the browser box model at the reference viewport.
    assert ".runtime-twin .related-list{height:203px" in css
    # settings.ini sidebarSize=0 => Compact => 52 * 0.8 = 41.6px.
    assert ".runtime-twin .sidebar-rail{width:42px" in css

    # Browser/FileTableV6 runtime state.
    assert "useState(16)" in workspace
    # RenderUI keeps the full tab shelf + Dummy/spacing + two browser rows
    # before FileTableV6; do not collapse this stack into compact web rows.
    assert "top:113px" in css
    # targetRowHeight=28 plus CellPaddingY=2 on both sides => 32px actual rows.
    assert ".runtime-twin .file-row{height:32px}" in css
    assert "const splitterWidth = 12" in page
    assert "8 + sidebarWidth + splitterWidth" in page
    assert "infoX - workspaceX - splitterWidth" in page
    # FileTableV6: source indexes/widths come from main.cpp plus the active
    # imgui.ini table. Theme.cpp supplies the 8px horizontal cell padding.
    contract = (TWIN / "runtime-contract.ts").read_text(encoding="utf-8")
    for source_index, width in ((17, 30), (0, 394), (1, 306), (2, 88),
                                (3, 50), (5, 77), (8, 61), (9, 60), (16, 1178)):
        assert f"sourceIndex: {source_index}" in contract
        assert f"savedWidth: {width}" in contract
    assert "cellPaddingX: 8" in contract
    assert "grid-template-columns:var(--rt-file-table-columns)" in css
    assert ".runtime-twin .file-table{top:113px;left:0;right:0" in css
    # Approved web override: the generic region frame must not run behind the
    # rounded source tab.  Lock the whole workspace frame off, not only its
    # horizontal top edge (the old left/right edges still cut the tab corners).
    assert ".runtime-twin .runtime-workspace{border:0}" in css
    assert ".runtime-twin .workspace-tab{z-index:2;isolation:isolate}" in css
    assert ".runtime-twin .runtime-workspace:after{content:\"\";position:absolute;left:0;right:0;top:27px;bottom:0;border:1px solid var(--rt-board);border-top:0" in css

    # RenderFileInfoPanelContent defaults Artwork on first open.
    assert "useState<(typeof tabs)[number]>('Artwork')" in info
    assert "height:calc(100% - 82px)" in css
    assert "margin:0;padding:9px 10px 0;border:0;border-radius:0" in css

    # WaveformPanel source: 40px transport shelf and 171px captured wave body.
    assert "useState(true)" in transport
    assert ".runtime-twin .transport-bar" in css and "height:40px" in css
    # 2px shell inset plus ImGui's 12px child-window content padding.
    assert "top:14px" in css
    assert ".runtime-twin .wave-area{left:0;right:0;top:60px" in css
    assert "height:171px" in css
    assert ".runtime-twin .wave-area{left:-8px;right:-8px" in css


def test_runtime_preset_dial_matches_imgui_continuous_motion_contract():
    chrome = (TWIN / "RuntimeChrome.tsx").read_text(encoding="utf-8")
    dial = (TWIN / "RuntimePresetDial.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    assert "<RuntimePresetDial />" in chrome
    assert "const PRESET_ITEM_SPACING = 58" in dial
    assert "const PRESET_WHEEL_STEP = 0.5" in dial
    assert "const PRESET_SNAP_RATE = 10" in dial
    assert "const PRESET_SETTLE_EPSILON = 0.08" in dial
    assert "requestAnimationFrame" in dial
    assert "scrollPosition" in dial
    assert "preset-center-tick top" in dial
    assert "preset-center-tick bottom" in dial
    assert "onContextMenu={openContextMenu}" in dial
    assert "preset-context-menu" in dial
    assert "builtIn: false" in dial
    assert ".runtime-twin .preset-edge-fade.left" in css
    assert ".runtime-twin .preset-edge-fade.right" in css
    assert ".runtime-twin .preset-context-menu" in css
    assert ".runtime-twin .preset-item.active:after" not in css


def test_runtime_chrome_tracks_current_toolbar_flow_metadata():
    chrome = (TWIN / "RuntimeChrome.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # The Web Runtime Twin is a legacy visual surface. Keep its source
    # metadata aligned with the current native flow without treating the twin
    # as the authoritative desktop acceptance surface.
    assert 'data-source="src/main.cpp:42180-42335"' in chrome
    assert "function HistoryChevron" in chrome
    assert 'className="history-divider"' in chrome
    assert 'aria-label="Back" disabled' in chrome
    assert 'aria-label="Continue" disabled' in chrome
    assert 'data-source="src/main.cpp: ResolveToolbarFlowLayout"' in chrome
    assert 'data-source="src/main.cpp: toolbarFlow.itemSlots[0..2]"' in chrome
    assert 'data-source="src/main.cpp: toolbarFlow.itemSlots[3..5]"' in chrome
    assert "function ToolbarGlyph" in chrome
    assert 'className="nav-dice-divider"' in chrome
    assert ".runtime-twin .history-divider{" in css
    assert ".runtime-twin .top-modes .nav-dice-divider{" in css


def test_runtime_preset_has_only_the_source_owned_add_action():
    chrome = (TWIN / "RuntimeChrome.tsx").read_text(encoding="utf-8")
    dial = (TWIN / "RuntimePresetDial.tsx").read_text(encoding="utf-8")

    assert "toolbar-plus" not in chrome
    assert "const virtualItems = [{ name: '+', builtIn: true }, ...presets]" in dial


def test_runtime_workspace_tab_uses_source_add_button():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")

    assert 'className="workspace-tab-add"' in workspace
    assert 'onClick={createWorkspaceTab}' in workspace
    assert 'aria-label="New tab">+</button>' in workspace
    assert "⌁" not in workspace


def test_runtime_transport_preserves_source_module_order_and_geometry():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    ordered_modules = (
        "transport-timecode",
        "transport-volume",
        "transport-stereo",
        "transport-reverse",
        "transport-norm",
        "transport-pan",
        "transport-center-sense",
        "<TransportLoopButton",
        "transport-right",
    )
    transport_bar = transport[transport.index('<div className="transport-bar"'):]
    positions = [transport_bar.index(name) for name in ordered_modules]
    assert positions == sorted(positions)
    assert 'data-source="src/main.cpp:50060-51970"' in transport
    assert ".runtime-twin .transport-timecode{width:195px;height:30px" in css
    assert ".runtime-twin .transport-volume{position:relative;width:260px;height:32px" in css
    assert ".runtime-twin .transport-stereo{width:30px;height:32px" in css
    assert ".runtime-twin .transport-reverse{width:36px;height:32px" in css
    assert ".runtime-twin .transport-norm{width:38px;height:20px" in css
    assert ".runtime-twin .transport-pan,.runtime-twin .transport-center-sense{width:52px;height:32px" in css
    assert ".runtime-twin .transport-loop{position:relative;width:48px;height:24px" in css
    assert ".runtime-twin .transport-bar:before" in css
    assert ".runtime-twin .transport-bar:after" in css
    assert "↶" not in transport
    assert "function TransportStereoGlyph" in transport
    assert "function TransportReverseGlyph" in transport
    assert "function TransportPanGlyph" in transport
    assert "function TransportCenterSenseGlyph" in transport
    assert "function TransportLoopButton" in transport
    assert 'viewBox="0 0 36 32"' in transport
    for source_anchor in ("50968-51010", "51012-51103", "29939-30060",
                          "51146-51270", "29338-29420"):
        assert f'data-source="src/main.cpp:{source_anchor}"' in transport


def test_runtime_transport_matches_imgui_animation_and_db_combo_contract():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:50974-50980 and 51018-51021 animate these controls over time;
    # the web twin must not snap instantly between states.
    assert "const MONO_STEREO_TARGET_OFFSET = 5" in transport
    assert "const MONO_STEREO_TARGET_SCALE = 0.72" in transport
    assert "const MONO_STEREO_DECAY = 12" in transport
    assert "const REVERSE_ANIMATION_SPEED = 5" in transport
    assert "requestAnimationFrame" in transport
    assert "TransportStereoGlyph({ mono, offset, scale }" in transport
    assert "TransportReverseGlyph({ progress }" in transport

    # main.cpp:51784-51833 uses BeginCombo styling, not the browser's native
    # select chrome.
    assert "<select" not in transport
    assert "function TransportDbSwitch" in transport
    assert 'className="transport-db-switch"' in transport
    assert 'className="transport-db-popup"' in transport
    assert 'data-source="src/main.cpp:51784-51833"' in transport
    assert ".runtime-twin .transport-db-switch{" in css
    assert ".runtime-twin .transport-db-switch-arrow{" in css
    assert ".runtime-twin .transport-db-popup{" in css


def test_runtime_transport_preserves_source_stereo_shape_and_loop_alignment():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:50996-51005 draws only the two overlapping circles; there is no
    # extra mono center dot in the transport icon.
    assert "transport-mono-core" not in transport
    assert ".runtime-twin .transport-stereo.active{border-radius:6px;background:rgba(119,189,246,.18);color:var(--rt-accent)}" in css

    # SquareLedButton centers the Loop label across the 48px capsule; do not
    # nudge the text right to compensate for the LED.
    assert ".runtime-twin .transport-loop span{margin-left:0}" in css


def test_runtime_contract_records_user_approved_overrides_above_source_fidelity():
    contract = CONTRACT.read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    assert "ARIS_RUNTIME_APPROVED_WEB_OVERRIDES" in contract
    for protected_override in (
        "removeDuplicateBordersAndGaps",
        "removeToolbarNoResults",
        "fillFileTableAndWaveform",
        "workspaceUFrameBelowTab",
        "removePresetActiveUnderline",
        "preserveAcceptedLayoutAndOptionSpacing",
    ):
        assert protected_override in contract
    for retired_selector in (".no-results{", ".result-label{", ".toolbar-plus{", ".mode-switch{"):
        assert retired_selector not in css


def test_runtime_transport_center_sense_matches_imgui_pointer_contract():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")

    # main.cpp:51164-51174: horizontal pointer drag at 0.006 per pixel,
    # clamped to [-1, 1], with double-click and right-click reset.
    assert "const CENTER_SENSE_DRAG_RATE = 0.006" in transport
    assert "setCenterSense" in transport
    assert "setPointerCapture" in transport
    assert "onPointerMove={handleCenterSensePointerMove}" in transport
    assert "onDoubleClick={resetCenterSense}" in transport
    assert "onContextMenu={resetCenterSenseFromContextMenu}" in transport
    assert "Math.max(-1, Math.min(1" in transport


def test_runtime_transport_volume_and_pan_match_imgui_pointer_contracts():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")

    # main.cpp:50910-50953: direct pointer positioning, Ctrl/right reset to
    # 100%, and five percentage points per wheel step.
    assert "const VOLUME_WHEEL_STEP = 5" in transport
    assert "setVolumePercent" in transport
    assert "handleVolumePointerDown" in transport
    assert "handleVolumePointerMove" in transport
    assert "handleVolumeWheel" in transport
    assert "event.ctrlKey" in transport
    assert "resetVolumeFromContextMenu" in transport
    assert "Math.max(0, Math.min(120" in transport

    # main.cpp:29954-29971: combined horizontal/vertical drag at 0.5 units
    # per pixel for the native -50..50 range, plus double/right reset.
    assert "const PAN_DRAG_RATE = 0.5" in transport
    assert "setPanValue" in transport
    assert "handlePanPointerMove" in transport
    assert "onDoubleClick={resetPan}" in transport
    assert "onContextMenu={resetPanFromContextMenu}" in transport
    assert "disabled={mono}" in transport


def test_runtime_toolbar_navigation_and_dice_match_imgui_behavior():
    chrome = (TWIN / "RuntimeChrome.tsx").read_text(encoding="utf-8")

    # Native desktop behavior is validated through the Aris Desktop Annotation
    # Bridge now. The legacy twin contract only keeps the mode/action split
    # and current flow composition visible; it must not pin an old handler
    # implementation or source line range.
    assert "isModeButton" in chrome
    assert "modes.slice(0, 3)" in chrome
    assert "modes.slice(3)" in chrome

    # main.cpp:42827-42917 rolls faces every 0.05s and settles after 0.3s,
    # with a decaying horizontal shake.
    assert "const DICE_ANIMATION_DURATION = 0.3" in chrome
    assert "const DICE_FACE_INTERVAL = 0.05" in chrome
    assert "const DICE_SHAKE_FREQUENCY = 50" in chrome
    assert "function DiceGlyph" in chrome
    assert "requestAnimationFrame" in chrome
    assert "handleDiceClick" in chrome


def test_runtime_sidebar_matches_imgui_activity_rail_behavior():
    sidebar = (TWIN / "RuntimeSidebar.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # Active settings hide section 0, leaving source order 5,1,4 | 2,3.
    assert "const railSectionIds = [5, 1, 4, 2, 3]" in sidebar
    assert "activeSectionId" in sidebar
    assert "current === sectionId ? -1 : sectionId" in sidebar

    # main.cpp:19438-19501 anchors the module-menu hamburger at the rail bottom.
    assert "function SidebarPanelMenuGlyph" in sidebar
    assert 'className="sidebar-panel-menu-button"' in sidebar
    assert 'data-source="src/main.cpp:19438-19501"' in sidebar
    assert ".runtime-twin .sidebar-panel-menu-button{" in css

    # main.cpp:19266 explicitly forbids a rail background and right divider.
    assert ".runtime-twin .sidebar-rail{width:42px;background:transparent;border-right:0" in css


def test_runtime_browser_path_and_filter_match_imgui_input_behavior():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:45133-45187 renders an editable path input and commits on Enter.
    assert "browserPath" in workspace
    assert 'className="browser-path-input"' in workspace
    assert "handleBrowserPathKeyDown" in workspace
    assert "navigateToParentDirectory" in workspace
    assert ".runtime-twin .browser-path-input{" in css

    # main.cpp:45241-45247 clears the in-view filename filter with Escape.
    assert "handleBrowserFilterKeyDown" in workspace
    assert "event.key === 'Escape'" in workspace


def test_runtime_twin_does_not_embed_acceptance_screenshots_as_ui_surfaces():
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    info = (TWIN / "RuntimeInfoPanel.tsx").read_text(encoding="utf-8")
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")

    # Screenshots are acceptance evidence only. The twin must keep its real
    # DOM/SVG surfaces visible so later accepted changes can be back-ported.
    assert "artwork-reference.png" not in css
    assert "waveform-reference.png" not in css
    assert ".runtime-twin .art-grid .cover{visibility:hidden}" not in css
    assert ".runtime-twin .wave-area .gain-fader" not in css or "visibility:hidden" not in css
    for implementation in (workspace, info, transport):
        assert "runtime-fixtures" not in implementation
        assert "data-fixture-source" not in implementation


def test_runtime_artwork_wall_uses_source_thumb_size_control():
    info = (TWIN / "RuntimeInfoPanel.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:24497-24531 keeps a 22px bottom strip with a quiet thumb-size
    # slider; it changes real grid geometry rather than displaying a fake bar.
    assert "thumbSize" in info
    assert 'type="range"' in info
    assert 'data-source="src/main.cpp:24497-24531"' in info
    assert "--rt-art-thumb" in info
    assert "var(--rt-art-thumb)" in css


def test_runtime_splitters_use_source_geometry_and_real_hit_lanes():
    page = (TWIN / "RuntimeTwinPage.tsx").read_text(encoding="utf-8")
    sidebar = (TWIN / "RuntimeSidebar.tsx").read_text(encoding="utf-8")
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    assert "const LEFT_SIDEBAR_MIN_WIDTH = 160" in page
    assert "const RIGHT_PANEL_MIN_WIDTH = 150" in page
    assert 'className={`runtime-splitter left' in page
    assert 'className={`runtime-splitter right' in page
    assert "<i /><i /><i />" in page
    assert ".runtime-twin .runtime-splitter{position:absolute;width:12px" in css
    assert "splitter-grip" not in sidebar
    assert "splitter-grip" not in workspace


def test_runtime_wave_footer_matches_source_transport_modes():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")

    # main.cpp:53117-53330: Prev / Play / Next followed by Shuffle and
    # Listening Mode. The old infinity glyph and ST/Hz labels do not belong here.
    assert "const FOOTER_MODE_ANIMATION_SPEED = 5" in transport
    assert "function TransportShuffleGlyph" in transport
    assert "footerShuffleProgress" in transport
    assert "listeningMode" in transport
    assert 'TransportGlyph name="infinity"' not in transport
    assert 'className="st"' not in transport
    assert "<span>Hz</span>" not in transport


def test_runtime_waveform_matches_source_seek_and_scroll_contract():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:52575-52722: playback position drives both timecode and playhead,
    # while the full waveform overlay owns seek interaction.
    assert "const FIXTURE_DURATION_SECONDS = 5.2" in transport
    assert "playbackProgress" in transport
    assert "formatTransportTime" in transport
    assert "handleWaveformSeek" in transport
    assert 'className="wave-interaction"' in transport
    assert 'data-source="src/main.cpp:52719-52728"' in transport
    assert ".runtime-twin .wave-interaction{" in css

    # main.cpp:52789-52930: Ctrl+wheel changes zoom and the scroll thumb can pan.
    assert "waveformZoom" in transport
    assert "waveformViewCenter" in transport
    assert "handleWaveformWheel" in transport
    assert "handleWaveScrollPointerDown" in transport
    assert "event.ctrlKey" in transport


def test_runtime_waveform_amplitude_uses_source_overlay_slider():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:53492-53551: 18px floating bottom-left slider, 0.1..8.0,
    # drag/wheel adjustment, Shift fine step, and right-click reset.
    assert "const WAVE_AMPLITUDE_MIN = 0.1" in transport
    assert "const WAVE_AMPLITUDE_MAX = 8" in transport
    assert "waveformAmplitude" in transport
    assert "handleWaveAmplitudePointerMove" in transport
    assert "handleWaveAmplitudeWheel" in transport
    assert "event.shiftKey ? 0.05 : 0.2" in transport
    assert "resetWaveAmplitude" in transport
    assert 'className="wave-amplitude"' in transport
    assert "gain-fader" not in transport
    assert ".runtime-twin .wave-amplitude{position:absolute;left:4px;bottom:14px;width:18px" in css


def test_runtime_send_to_daw_matches_source_split_capsule():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # DAWPanel.cpp:195-390: adaptive [plane/name/status/send][arrow] capsule,
    # upward target popup, 42px target rows, and sending feedback.
    assert "function TransportDawWidget" in transport
    assert "const TRANSPORT_DAW_TARGETS" in transport
    assert "dawSending" in transport
    assert 'className="daw-send-main"' in transport
    assert 'className="daw-send-arrow"' in transport
    assert 'className="daw-send-popup"' in transport
    assert 'data-source="src/DAWPanel.cpp:195-390"' in transport
    assert "Configure DAW..." in transport
    assert ".runtime-twin .daw-send-widget{" in css
    assert '<button type="button">▶ Send to REAPER⌄</button>' not in transport


def test_runtime_sidebar_switches_real_source_sections():
    sidebar = (TWIN / "RuntimeSidebar.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # main.cpp:19759-21243 uses one active section body, not one static folder
    # tree under every rail icon.
    assert "function SidebarQuickPaths" in sidebar
    assert "function SidebarFolderTree" in sidebar
    assert "function SidebarTagCloud" in sidebar
    assert "function SidebarMetadataFilters" in sidebar
    assert "function SidebarPlaylists" in sidebar
    assert "switch (activeSectionId)" in sidebar
    for source_anchor in ("19912-19958", "19960-20361", "20363-20590", "20592-21238", "21240"):
        assert f"src/main.cpp:{source_anchor}" in sidebar
    assert ".runtime-twin .sidebar-section-body{" in css


def test_runtime_file_table_uses_real_horizontal_scroll_contract():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # FileTableV6 owns a scrollable inner width from saved column widths; the
    # bottom bar must move that plane instead of displaying a fixed fake thumb.
    assert "tableContentWidth" in workspace
    assert "tableScrollPx" in workspace
    assert "fileTableRef" in workspace
    assert "ResizeObserver" in workspace
    assert "handleTableScrollPointerDown" in workspace
    assert 'className="file-scroll-plane"' in workspace
    assert 'data-source="src/main.cpp:46155 FileTableV6"' in workspace
    assert ".runtime-twin .file-scroll-plane{" in css


def test_runtime_file_table_uses_native_vertical_scroll_contract():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    # FileTableV6 uses ImGui ScrollY with direct wheel movement and no inertial
    # drift. The twin keeps the header fixed while moving only the row plane.
    assert "tableScrollY" in workspace
    assert "handleTableWheel" in workspace
    assert "handleTableVerticalScrollPointerDown" in workspace
    assert 'className="file-body-plane"' in workspace
    assert 'className="file-v-scroll"' in workspace
    assert 'data-source="src/main.cpp:46155-46166 FileTableV6 ScrollY"' in workspace
    assert ".runtime-twin .file-body-plane{" in css
    assert ".runtime-twin .file-v-scroll{" in css


def test_runtime_info_title_bar_matches_native_edit_and_context_menu_contract():
    info = (TWIN / "RuntimeInfoPanel.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    assert "metadataEditMode" in info
    assert "infoPanelMenuOpen" in info
    assert "handleInfoTitleContextMenu" in info
    assert 'className="info-edit-button"' in info
    assert 'className="info-title-menu"' in info
    assert 'data-source="src/main.cpp:49544-49698"' in info
    assert ".runtime-twin .info-edit-button{" in css
    assert ".runtime-twin .info-title-menu{" in css


def test_runtime_workspace_tabbar_matches_native_new_tab_and_preset_contract():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    assert "workspaceTabs" in workspace
    assert "createWorkspaceTab" in workspace
    assert 'className="workspace-tabbar"' in workspace
    assert 'className="workspace-tab-add"' in workspace
    assert 'className="tab-preset-button"' in workspace
    assert 'data-source="src/main.cpp:44275-44760"' in workspace
    assert ".runtime-twin .workspace-tabbar{" in css
    assert ".runtime-twin .tab-preset-button{" in css


def test_runtime_workspace_tabs_match_native_context_actions_contract():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    for symbol in ("tabContextId", "renamingTabId", "duplicateWorkspaceTab", "closeWorkspaceTab", "handleWorkspaceTabAuxClick"):
        assert symbol in workspace
    assert 'className="workspace-tab-menu"' in workspace
    assert 'className="workspace-tab-colors"' in workspace
    assert 'data-source="src/main.cpp:44372-44427"' in workspace
    assert ".runtime-twin .workspace-tab-menu{" in css
    assert ".runtime-twin .workspace-tab-colors{" in css


def test_runtime_workspace_tabs_match_native_reorderable_contract():
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    assert "draggedTabId" in workspace
    assert "handleTabDragStart" in workspace
    assert "handleTabDrop" in workspace
    assert "draggable" in workspace
    assert 'data-reorder-source="src/main.cpp:44275 ImGuiTabBarFlags_Reorderable"' in workspace
    assert ".runtime-twin .workspace-tab.dragging{" in css


def test_runtime_browser_path_submission_drives_history_navigation_contract():
    page = (TWIN / "RuntimeTwinPage.tsx").read_text(encoding="utf-8")
    chrome = (TWIN / "RuntimeChrome.tsx").read_text(encoding="utf-8")
    workspace = (TWIN / "RuntimeWorkspace.tsx").read_text(encoding="utf-8")

    for symbol in ("browserHistory", "browserHistoryIndex", "navigateBrowserPath", "goBrowserBack", "goBrowserForward"):
        assert symbol in page
    assert "canGoBack" in chrome and "canGoForward" in chrome
    assert "onBack" in chrome and "onForward" in chrome
    assert "committedBrowserPath" in workspace
    assert "onNavigateBrowserPath" in workspace
    assert "commitBrowserPath" in workspace
    assert 'data-source="src/main.cpp:45127-45187"' in workspace


def test_runtime_transport_previous_next_drive_playback_history_contract():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")

    assert "TRANSPORT_HISTORY_ITEMS" in transport
    assert "playbackHistoryIndex" in transport
    assert "navigatePlaybackHistory" in transport
    assert "currentPlaybackItem" in transport
    assert 'data-source="src/main.cpp:53155-53242 NavigatePlaybackHistory"' in transport
    assert 'onClick={() => navigatePlaybackHistory(true)}' in transport
    assert 'onClick={() => navigatePlaybackHistory(false)}' in transport


def test_runtime_daw_popup_actions_match_native_configuration_and_refresh_contract():
    transport = (TWIN / "RuntimeTransport.tsx").read_text(encoding="utf-8")
    css = (TWIN / "runtime-twin.css").read_text(encoding="utf-8")

    for symbol in ("showDawConfiguration", "refreshDawStatus", "dawRefreshFeedback", "dawExecutablePath"):
        assert symbol in transport
    assert 'className="daw-config-panel"' in transport
    assert 'data-source="src/DAWPanel.cpp:356-380"' in transport
    assert ".runtime-twin .daw-config-panel{" in css
    assert ".runtime-twin .daw-refresh-feedback{" in css
