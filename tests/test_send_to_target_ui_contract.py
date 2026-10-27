from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN_CPP = ROOT / "src" / "main.cpp"
APP_STATE = ROOT / "src" / "AppState.h"


def require(text: str, needle: str, reason: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {needle!r}: {reason}")


def require_order(text: str, first: str, second: str, reason: str) -> None:
    first_index = text.find(first)
    second_index = text.find(second)
    if first_index < 0 or second_index < 0 or first_index >= second_index:
        raise AssertionError(f"expected {first!r} before {second!r}: {reason}")


def block_after(text: str, marker: str) -> str:
    start = text.index("{", text.index(marker))
    depth = 0
    for end in range(start, len(text)):
        depth += (text[end] == "{") - (text[end] == "}")
        if depth == 0:
            return text[start:end + 1]
    raise AssertionError(f"unclosed block: {marker}")


def check_same_named_send_targets_have_separate_menu_ids(main_cpp: str) -> None:
    browser = block_after(main_cpp, "if (ImGui::BeginMenu(L(Ctx_SendToDAW)))")
    require_order(browser, "if (!dt.enabled) continue;", "ImGui::PushID(di);",
                  "disabled targets must not leave an ID scope open")
    require_order(browser, "ImGui::PushID(di);", "ImGui::MenuItem(dt.name.c_str())",
                  "same-named browser targets need their configuration index as ID")
    require_order(browser, "SendToDAWForPaths(state, di, browserSendPaths);", "ImGui::PopID();",
                  "click routing and ID scope must refer to the same target")
    assert browser.count("ImGui::PushID(") == browser.count("ImGui::PopID();") == 1

    targets = block_after(main_cpp, "for (const auto& target : sendModel.targets)")
    require_order(targets, "if (!target.visible) continue;", "ImGui::PushID(target.key.c_str());",
                  "hidden targets must not leave an ID scope open")
    require_order(targets, "ImGui::PushID(target.key.c_str());", "ImGui::BeginMenu(target.label.c_str())",
                  "result-list target IDs must use model identity instead of display name")
    closed_menu = block_after(targets, "if (!ImGui::BeginMenu(target.label.c_str()))")
    require_order(closed_menu, "ImGui::PopID();", "continue;",
                  "a closed submenu must release its parent target ID")
    tail = targets[targets.rindex("ImGui::EndMenu();"):]
    require(tail, "ImGui::PopID();", "the open submenu path must restore its parent ID stack")
    # The newer QA menu also scopes each mode within its target. The unopened
    # target branch has one extra PopID because it returns before normal close.
    assert targets.count("ImGui::PushID(target.key.c_str());") == 1
    assert targets.count("ImGui::PopID();") == targets.count("ImGui::PushID(") + 1
    require(targets, "ExecuteSendMode(target, mode);", "same-named target clicks must route by target identity")


def main() -> int:
    main_cpp = MAIN_CPP.read_text(encoding="utf-8", errors="ignore")
    app_state = APP_STATE.read_text(encoding="utf-8", errors="ignore")
    check_same_named_send_targets_have_separate_menu_ids(main_cpp)

    require(app_state, "std::vector<HarkenSearch::UI::SendTargetHistoryItem> sendTargetHistory;",
            "session-local lightweight transfer history must live in AppState")

    require(main_cpp, '#include "SendToTargetSurfaceModel.h"',
            "result-list UI must use the richer target/mode surface model")
    require(main_cpp, '#include "CubaseAdapter.h"',
            "Cubase direct insert must have a dedicated adapter seam")
    if '#include "EditorSendSurfaceModel.h"' in main_cpp:
        raise AssertionError("main.cpp must not keep using the older route-only EditorSendSurfaceModel include")

    block_start = main_cpp.find('BeginPopupContextItem("RowContextMenu")')
    if block_start < 0:
        raise AssertionError("missing result-list row context menu block")
    block_end = main_cpp.find("if (ImGui::MenuItem(L(Ctx_RebuildWaveform)))", block_start)
    if block_end < 0:
        raise AssertionError("cannot locate end of Send to Target result-list block")
    block = main_cpp[block_start:block_end]

    require(block, "BuildCachedRowContextSendModel",
            "expensive target/preflight model construction must be isolated behind the row-context cache builder")
    require(block, "rowContextAppearing || !s_rowContextCacheValid",
            "right-click menu must snapshot target/preflight state on popup open instead of rebuilding every frame")
    require(block, "const auto& sendModel = s_rowContextSendModel",
            "menu rendering must reuse the cached target surface model while the popup remains open")
    require(block, "BuildSendToTargetSurface(sendCtx)",
            "UI must render from the unified surface model")
    require(block, "ImGui::BeginMenu(sendToEditorLabel)",
            "unified handoff surface must remain reachable from Send to Editor")
    require(block, "sendCtx.dawTargets.push_back",
            "target selector must include DAW targets")
    require(block, "IsSupportedDawDisplayTarget(dt)",
            "DAW target selector must filter old/unsupported configured hosts out of the visible list")
    require(block, "hostModeLabelOverride",
            "UI surface should be able to present truthful per-target host mode labels")
    require(block, "Public Adapter Not Ready",
            "Nuendo route must not imply direct insert before the public adapter passes preflight")
    require(block, "Cubase launch-only send is intentionally blocked",
            "Cubase route should expose truthful unavailable copy instead of misleading launch text")
    require(main_cpp, "HarkenSearch::CubaseAdapter::EvaluatePreflight(dt)",
            "Nuendo must use the Aris-owned public adapter preflight")
    require(block, "const bool publicAdapterReady",
            "Nuendo host handoff must have one explicit release-safe readiness gate")
    require(block, "dawCtx.hostModeEnabledOverride = publicAdapterReady",
            "Nuendo send must remain disabled until the public adapter is genuinely ready")
    require(block, "Use Export Fallback",
            "Nuendo must expose a truthful recovery route while its public adapter is unavailable")
    for forbidden in (
        "SendFilesToNuendo" + "ExperimentalTarget",
        "HasNuendo" + "ExperimentalExecutionSurface",
        "TrySendNuendoSkiDirectSpotJob",
    ):
        if forbidden in main_cpp:
            raise AssertionError(f"private Nuendo execution token must be removed: {forbidden}")
    require(block, "dawCtx.label = \"DAW Targets\"",
            "unconfigured DAW state must stay visible as a configure-first target")
    require(block, "sendCtx.exposeWwiseTransfer = false",
            "DAW target selector must not mix Wwise into the currently-supported DAW list")
    require(block, "sendCtx.exportFallbackVisible = false",
            "DAW target selector must not mix File Export into the currently-supported DAW list")
    defaults_start = main_cpp.find('add("REAPER"')
    defaults_end = main_cpp.find("state.selectedDAWIndex = 0;", defaults_start)
    if defaults_start < 0 or defaults_end < 0:
        raise AssertionError("cannot locate default DAW target block")
    defaults_block = main_cpp[defaults_start:defaults_end]
    for supported in ('"REAPER"', '"Pro Tools"', '"Cubase"', '"Nuendo"'):
        require(defaults_block, supported, f"default supported DAW list must include {supported}")
    for unsupported in ('"Logic Pro"', '"Ableton Live"', '"Fender Studio Pro"', '"FL Studio"'):
        if unsupported in defaults_block:
            raise AssertionError(f"default DAW list must not include unsupported host {unsupported}")
    presets_start = main_cpp.find("static const DawPreset kDawPresets[]")
    presets_end = main_cpp.find("constexpr int kPresetCount", presets_start)
    if presets_start < 0 or presets_end < 0:
        raise AssertionError("cannot locate settings DAW preset block")
    presets_block = main_cpp[presets_start:presets_end]
    for supported in ('"REAPER"', '"Pro Tools"', '"Cubase"', '"Nuendo"'):
        require(presets_block, supported, f"settings preset list must include {supported}")
    for unsupported in ('"Logic Pro"', '"Ableton Live"', '"Fender Studio Pro"', '"FL Studio"', '"Studio One"', '"DaVinci Resolve"', '"Audition"'):
        if unsupported in presets_block:
            raise AssertionError(f"settings preset list must not show unsupported host {unsupported}")

    require(block, "LocalizeSendModeLabel",
            "mode selector labels must be centralized in the UI block")
    for mode in (
        "HostHandoff",
        "OpenTransferPanel",
        "ExportFallback",
        "ExportFiles",
        "ConfigureTargets",
    ):
        require(block, mode, f"transfer mode selector must expose {mode}")

    require(block, "LocalizePreflight",
            "preflight summary must be explicitly rendered")
    require(block, "ImGui::TextWrapped(\"%s\", preflightLabel.c_str())",
            "preflight copy must be visible, not just stored in the model")
    require(block, "LocalizeCapability",
            "capability summary must be explicitly rendered")
    require(block, "ImGui::TextDisabled(\"%s\", capabilityLabel.c_str())",
            "capability truth must be visible to the user")

    require(block, "SendToDAW(state, target.dawIndex)",
            "existing DAW backend must remain reachable")
    require(block, "OpenWwiseTransferPanelForRecords(selectedRecords)",
            "existing Wwise backend must remain reachable")
    require(block, "OpenTransferDialogForRecords(state, selectedRecords)",
            "export fallback backend must remain reachable")
    require(block, "state.showDawTargetSettings = true",
            "configure mode must route to the existing DAW settings source of truth")

    require(block, "PushSendTargetHistory",
            "reachable actions must append lightweight transfer history")
    require(block, "state.sendTargetHistory.empty()",
            "recent history menu must be visible after at least one action")
    require(block, "Recent Delivery",
            "recent history surface must have a user-visible label")
    require(block, "item.resultDetail",
            "recent history should expose structured detail when available")
    require(block, 'ImGui::TextDisabled("%s", item.resultDetail.c_str())',
            "recent history should render a secondary detail line for host context")
    if "Started host handoff" in block:
        raise AssertionError("Host handoff history must reflect actual send result, not a generic started placeholder")

    require_order(block, "rowContextAppearing || !s_rowContextCacheValid", "const auto& sendModel = s_rowContextSendModel",
                  "the popup-open cache refresh must happen before menu rendering reads the model")
    require_order(block, "BuildSendToTargetSurface(sendCtx)", "ImGui::BeginMenu(sendToEditorLabel)",
                  "model should be built before rendering the menu")
    require_order(block, "ImGui::TextDisabled(\"%s\", capabilityLabel.c_str())",
                  "ImGui::TextWrapped(\"%s\", preflightLabel.c_str())",
                  "capability should be shown before preflight detail")
    if ("HarkenSearch::CubaseAdapter::DispatchToCubaseAdapter(target, files, outStatus)" not in main_cpp and
            "DispatchToCubaseAdapterDetailed(" not in main_cpp):
        raise AssertionError(
            "Cubase must dispatch through a dedicated adapter branch instead of generic launch-only handling")

    print("PASS: Send to Target UI source contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
