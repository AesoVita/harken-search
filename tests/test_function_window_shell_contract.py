from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_four_function_windows_have_embeddable_content_entrypoints():
    module_ui = read("src/ModuleUI.cpp")
    header = read("src/ModuleUI.h")
    main = read("src/main.cpp")
    for name in (
        "RenderUCSBrowserContent",
        "RenderTagBrowserContent",
        "RenderTransferDialogContent",
        "RenderWorkflowDialogContent",
    ):
        assert f"void {name}(AppState& state)" in module_ui
        assert f"void {name}(AppState& state)" in header
        assert name in main
    module_dialogs = module_ui[module_ui.index("void RenderModuleDialogs"):]
    for name in (
        "RenderUCSBrowserContent(state)",
        "RenderTagBrowserContent(state)",
        "RenderTransferDialogContent(state)",
        "RenderWorkflowDialogContent(state)",
    ):
        assert name not in module_dialogs


def test_function_window_states_are_persisted_and_have_dock_ids():
    app_state = read("src/AppState.h")
    main = read("src/main.cpp")
    windows_main = read("src/WindowsMain.cpp")
    layout = read("src/DockLayout.h")
    for field in (
        "panelUCSBrowser",
        "panelTagBrowser",
        "panelWorkflow",
        "panelTransfer",
    ):
        assert field in app_state
        assert f'loadPanel("{field}"' in main
        assert f'savePanel("{field}"' in main
    for module_id in (
        "mod-ucs-browser",
        "mod-tag-browser",
        "mod-workflow",
        "mod-transfer",
    ):
        assert f'"{module_id}"' in main
    assert 'id.find("mod-") == 0' in layout
    assert 'cmd == "function_window"' in windows_main
