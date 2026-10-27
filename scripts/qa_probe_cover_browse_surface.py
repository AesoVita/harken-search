from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
HARNESS_DIR = REPO_ROOT / ".openclaw" / "harness"
SCRIPT_DIR = Path(__file__).resolve().parent
if str(HARNESS_DIR) not in sys.path:
    sys.path.insert(0, str(HARNESS_DIR))
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from gui_smoke import wait_for_window  # noqa: E402
from qa_driver import QaDriver, QaDriverError  # noqa: E402
from qa_probe_cover_wall_perf import (  # noqa: E402
    cleanup_existing_auro,
    click_artwork_tab,
    ensure_probe_settings,
    find_visible_window_for_pid,
    load_jsonl,
    resolve_settings_path,
    set_foreground,
    try_capture_window,
    wait_cover_wall_probe_stable,
    wait_for_results,
    wait_ready_with_login_retry,
)
from run_local_harness import clear_crash_sentinel, clear_dev_mode_flag, write_dev_mode_flag  # noqa: E402


PRESET_THUMB_SIZES = {
    "compact": 80.0,
    "browse": 132.0,
    "inspect": 208.0,
}


def path_key(path: str) -> str:
    return str(path).replace("/", "\\").rstrip("\\").lower()


def read_row(driver: QaDriver, row: int) -> dict[str, object]:
    result = driver.get_row(row)
    if not bool(result.get("ok", False)):
        raise QaDriverError(f"get_row({row}) failed: {result}")
    item = result.get("item")
    if not isinstance(item, dict):
        raise QaDriverError(f"get_row({row}) returned no item payload: {result}")
    return item


def extract_browse_surface(state: dict[str, object]) -> dict[str, object]:
    return dict(state.get("browse_surface", {}))


def extract_folder_filter(state: dict[str, object]) -> dict[str, object]:
    return dict(state.get("folder_filter", {}))


def extract_cover_wall(state: dict[str, object]) -> dict[str, object]:
    return dict(state.get("cover_wall", {}))


def ensure_integrated_mode(driver: QaDriver, *, enabled: bool, timeout: float) -> dict[str, object]:
    command_result = driver.set_cover_wall_integrated_mode(enabled, timeout=timeout)
    state_after = driver.get_state()
    cover_wall = extract_cover_wall(state_after)
    browse_surface = extract_browse_surface(state_after)
    return {
        "commandResult": command_result,
        "stateAfter": state_after,
        "coverWallIntegratedMode": bool(cover_wall.get("integrated_mode", False)),
        "browseSurfaceIntegratedMode": bool(browse_surface.get("integrated_mode", False)),
    }


def collect_relevant_db_events(
    diagnostics_entries: list[dict[str, object]],
    *,
    start_ms: int,
    end_ms: int,
) -> list[dict[str, object]]:
    relevant_categories = {
        "Database.searchFiles",
        "Database.getCoverFacets",
        "Database.getGlobalCoverWallEntriesPage",
        "Database.getFilePathsWithCoverKey",
    }
    relevant: list[dict[str, object]] = []
    for entry in diagnostics_entries:
        timestamp_ms = int(entry.get("timestampMs", 0) or 0)
        if timestamp_ms < start_ms or timestamp_ms > end_ms:
            continue
        category = str(entry.get("category", ""))
        if category not in relevant_categories:
            continue
        relevant.append(
            {
                "timestampMs": timestamp_ms,
                "category": category,
                "detail": str(entry.get("detail", "")),
                "durationMs": float(entry.get("durationMs", 0.0) or 0.0),
            }
        )
    return relevant


def ensure_cover_surface_ready(
    driver: QaDriver,
    *,
    hwnd: int,
    force_artwork_tab_click: bool,
    visible_timeout: float,
) -> dict[str, object]:
    initial_state = driver.get_state()
    cover_wall = extract_cover_wall(initial_state)
    should_click = force_artwork_tab_click or (
        not bool(cover_wall.get("loaded", False))
        and int(cover_wall.get("items", 0) or 0) <= 0
    )
    if should_click:
        click_artwork_tab(hwnd)
        time.sleep(0.8)
    return wait_cover_wall_probe_stable(
        driver,
        timeout=max(visible_timeout, 1.0),
        require_visible=True,
        require_count_settled=False,
    )


def resolve_tree_path(
    driver: QaDriver,
    *,
    explicit_path: str,
    preferred_rows: list[int],
) -> tuple[str, dict[str, object]]:
    if explicit_path.strip():
        return explicit_path.strip(), {"source": "arg", "row": -1}

    seen_rows: set[int] = set()
    for row in preferred_rows:
        if row < 0 or row in seen_rows:
            continue
        seen_rows.add(row)
        row_state = read_row(driver, row)
        folder_path = str(row_state.get("folder_path", "")).strip()
        if folder_path:
            return folder_path, {"source": "row", "row": row, "rowState": row_state}

    raise RuntimeError("Could not resolve a non-empty folder_path for tree-scope proof")


def run_preview_parity_proof(driver: QaDriver, *, timeout: float) -> dict[str, object]:
    failures: list[str] = []
    results_state = wait_for_results(driver, minimum_count=2, timeout=timeout)
    if int(results_state.get("result_count", 0) or 0) < 2:
        failures.append("preview parity proof requires at least two visible results")
        return {
            "proofId": "PROOF-COVER-PREVIEW-PARITY",
            "passed": False,
            "failures": failures,
            "stateBefore": results_state,
        }

    first_row = read_row(driver, 0)
    second_row = read_row(driver, 1)
    first_path = str(first_row.get("path", "")).strip()
    second_path = str(second_row.get("path", "")).strip()
    if not first_path or not second_path:
        failures.append("preview parity proof requires non-empty row paths for row 0 and row 1")
        return {
            "proofId": "PROOF-COVER-PREVIEW-PARITY",
            "passed": False,
            "failures": failures,
            "stateBefore": results_state,
            "row0": first_row,
            "row1": second_row,
        }

    driver.load_path(first_path)
    time.sleep(0.5)
    driver.select_row(1, timeout=5.0)
    time.sleep(0.2)
    state_after = driver.get_state()
    browse_surface = extract_browse_surface(state_after)

    selected_path = str(browse_surface.get("selected_path", "")).strip()
    effective_preview_path = str(browse_surface.get("effective_preview_path", "")).strip()
    current_file_path = str(state_after.get("current_file", "")).strip()
    if selected_path != second_path:
        failures.append(
            f"selected browse path drifted from row 1: selected_path={selected_path!r}, row1={second_path!r}"
        )
    if effective_preview_path != second_path:
        failures.append(
            f"effective preview path drifted from row 1: preview={effective_preview_path!r}, row1={second_path!r}"
        )
    if not bool(browse_surface.get("preview_uses_selected_record", False)) and current_file_path != second_path:
        failures.append(
            "preview parity proof expected either selected-record preview fallback or an auto-loaded current file that matches the selected row"
        )
    if int(browse_surface.get("selected_row", -1) or -1) != 1:
        failures.append("selected_row should remain 1 after the preview parity proof selection step")

    return {
        "proofId": "PROOF-COVER-PREVIEW-PARITY",
        "passed": not failures,
        "failures": failures,
        "stateBefore": results_state,
        "stateAfter": state_after,
        "row0": first_row,
        "row1": second_row,
    }


def run_tree_scope_proof(
    driver: QaDriver,
    *,
    tree_path: str,
    search_query: str,
    timeout: float,
) -> dict[str, object]:
    failures: list[str] = []
    if search_query.strip():
        driver.search_and_wait(search_query.strip(), timeout=timeout)
        time.sleep(0.2)

    driver.set_folder_filter(tree_path, timeout=timeout)
    driver.wait_search_idle(timeout=timeout)
    state_after = wait_cover_wall_probe_stable(
        driver,
        timeout=timeout,
        require_visible=True,
        require_count_settled=True,
    )
    folder_filter = extract_folder_filter(state_after)
    browse_surface = extract_browse_surface(state_after)
    cover_wall = extract_cover_wall(state_after)

    if not bool(folder_filter.get("active", False)):
        failures.append("folder_filter.active did not stay true after applying the tree scope")
    filtered_path = str(folder_filter.get("path", "")).strip()
    if path_key(filtered_path) != path_key(tree_path):
        failures.append(
            f"folder filter path drifted: actual={filtered_path!r}, expected={tree_path!r}"
        )
    bottom_info = str(browse_surface.get("bottom_info", ""))
    if path_key(tree_path) not in path_key(bottom_info):
        failures.append("browse_surface.bottom_info does not describe the active tree path")
    if search_query.strip():
        active_query = str(state_after.get("search_text", "")).strip()
        if active_query.lower() != search_query.strip().lower():
            failures.append(
                f"tree scope lost the keyword scope: actual={active_query!r}, expected={search_query!r}"
            )
    if int(state_after.get("result_count", 0) or 0) <= 0 and int(cover_wall.get("items", 0) or 0) <= 0:
        failures.append("tree scope produced neither visible results nor cover-wall items")

    return {
        "proofId": "PROOF-COVER-TREE-SCOPE",
        "passed": not failures,
        "failures": failures,
        "treePath": tree_path,
        "stateAfter": state_after,
    }


def run_slider_stability_proof(
    driver: QaDriver,
    *,
    diagnostics_path: Path,
    requested_size: float,
    tile_preset: str,
    timeout: float,
) -> dict[str, object]:
    failures: list[str] = []
    before_state = driver.get_state()
    before_browse_surface = extract_browse_surface(before_state)

    slider_start_ms = int(time.time() * 1000)
    driver.set_cover_wall_thumb_size(requested_size, timeout=5.0)
    time.sleep(0.2)
    after_size_state = driver.get_state()
    driver.set_cover_wall_tile_preset(tile_preset, timeout=5.0)
    time.sleep(0.2)
    driver.wait_search_idle(timeout=timeout)
    final_state = driver.get_state()
    slider_end_ms = int(time.time() * 1000)

    after_size_browse_surface = extract_browse_surface(after_size_state)
    final_browse_surface = extract_browse_surface(final_state)
    diagnostics_entries = load_jsonl(diagnostics_path)
    db_events = collect_relevant_db_events(
        diagnostics_entries,
        start_ms=slider_start_ms,
        end_ms=slider_end_ms,
    )

    if str(before_state.get("search_text", "")) != str(final_state.get("search_text", "")):
        failures.append("slider stability proof changed the active keyword scope")
    if path_key(str(extract_folder_filter(before_state).get("path", ""))) != path_key(
        str(extract_folder_filter(final_state).get("path", ""))
    ):
        failures.append("slider stability proof changed the active folder tree scope")
    if str(before_browse_surface.get("top_info", "")) != str(final_browse_surface.get("top_info", "")):
        failures.append("top_info changed during size/preset mutation even though browse scope should be unchanged")
    if str(before_browse_surface.get("bottom_info", "")) != str(final_browse_surface.get("bottom_info", "")):
        failures.append("bottom_info changed during size/preset mutation even though browse scope should be unchanged")

    expected_size = max(64.0, min(300.0, float(requested_size)))
    actual_after_size = float(after_size_browse_surface.get("thumb_size", 0.0) or 0.0)
    if abs(actual_after_size - expected_size) > 0.01:
        failures.append(
            f"thumb slider did not clamp to the requested size: actual={actual_after_size}, expected={expected_size}"
        )

    expected_preset_size = PRESET_THUMB_SIZES[tile_preset]
    actual_final_size = float(final_browse_surface.get("thumb_size", 0.0) or 0.0)
    if abs(actual_final_size - expected_preset_size) > 0.01:
        failures.append(
            f"tile preset did not set the expected thumb size: actual={actual_final_size}, expected={expected_preset_size}"
        )

    if bool(final_state.get("async_search_in_progress", False)):
        failures.append("slider stability proof left async_search_in_progress=true")
    if int(final_state.get("pending_search_request_id", 0) or 0) != 0:
        failures.append("slider stability proof created a pending_search_request_id")
    if int(final_state.get("pending_facet_request_id", 0) or 0) != 0:
        failures.append("slider stability proof created a pending_facet_request_id")
    if db_events:
        failures.append(f"slider stability proof triggered unexpected DB work: {db_events}")

    return {
        "proofId": "PROOF-COVER-SLIDER-STABILITY",
        "passed": not failures,
        "failures": failures,
        "stateBefore": before_state,
        "stateAfterSize": after_size_state,
        "stateAfterPreset": final_state,
        "dbEvents": db_events,
        "sliderWindowMs": {
            "start": slider_start_ms,
            "end": slider_end_ms,
        },
    }


def run_cover_filter_scope_proof(
    driver: QaDriver,
    *,
    timeout: float,
) -> dict[str, object]:
    failures: list[str] = []
    state_before = wait_for_results(driver, minimum_count=1, timeout=timeout)
    row0 = read_row(driver, 0)
    cover_key = str(row0.get("cover_key", "")).strip()
    file_path = str(row0.get("path", "")).strip()
    folder_path = str(row0.get("folder_path", "")).strip()
    label = str(row0.get("name", "")).strip()

    if not cover_key:
        failures.append("cover-filter proof requires row 0 to expose a non-empty cover_key")
        return {
            "proofId": "PROOF-COVER-FILTER",
            "passed": False,
            "failures": failures,
            "stateBefore": state_before,
            "row0": row0,
        }

    driver.set_specific_cover_filter(
        cover_key,
        file_path=file_path or None,
        folder_path=folder_path or None,
        label=label or None,
        timeout=timeout,
    )
    driver.wait_search_idle(timeout=timeout)
    state_after = wait_cover_wall_probe_stable(
        driver,
        timeout=timeout,
        require_visible=True,
        require_count_settled=True,
    )

    cover_filter = dict(state_after.get("cover_filter", {}))
    browse_surface = extract_browse_surface(state_after)
    cover_wall = extract_cover_wall(state_after)
    if not bool(cover_filter.get("active", False)):
        failures.append("cover-filter proof expected cover_filter.active=true after setting a specific cover filter")
    if str(cover_filter.get("path", "")).strip() != cover_key:
        failures.append("cover-filter proof observed a different active cover key than the selected row")
    if str(browse_surface.get("pinned_cover_key", "")).strip() != cover_key:
        failures.append("browse_surface.pinned_cover_key drifted from the applied specific cover key")
    if str(browse_surface.get("cover_filter_label", "")).strip() in {"", "(none)"}:
        failures.append("browse_surface.cover_filter_label should describe the pinned cover after applying a specific cover filter")
    if bool(extract_folder_filter(state_after).get("active", False)):
        failures.append("specific cover filter should not leave folder_filter.active=true")
    if int(state_after.get("result_count", 0) or 0) <= 0:
        failures.append("specific cover filter returned zero visible results")
    if int(cover_wall.get("items", 0) or 0) <= 0:
        failures.append("specific cover filter did not keep any cover-wall items visible")

    return {
        "proofId": "PROOF-COVER-FILTER",
        "passed": not failures,
        "failures": failures,
        "stateBefore": state_before,
        "stateAfter": state_after,
        "row0": row0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Targeted QA probe for replacement cover browse-surface tree scope, slider stability, and preview parity."
    )
    parser.add_argument("--exe-dir", default=str(REPO_ROOT / "build_dbg" / "Debug"))
    parser.add_argument("--db-path", required=True)
    parser.add_argument(
        "--auth-mode",
        choices=["harness_multi", "dev_bypass", "basic_login", "basic_login_dev"],
        default="harness_multi",
    )
    parser.add_argument("--login-account", default="qa")
    parser.add_argument("--login-password", default="qa")
    parser.add_argument("--wait-window-timeout", type=float, default=30.0)
    parser.add_argument("--wait-ready-timeout", type=float, default=120.0)
    parser.add_argument("--filter-ready-timeout", type=float, default=20.0)
    parser.add_argument("--attach-pid", type=int, default=0)
    parser.add_argument("--attach-hwnd", type=int, default=0)
    parser.add_argument("--force-artwork-tab-click", action="store_true")
    parser.add_argument("--search-query", default="")
    parser.add_argument("--tree-path", default="")
    parser.add_argument("--tree-row", type=int, default=1)
    parser.add_argument("--thumb-size", type=float, default=96.0)
    parser.add_argument(
        "--tile-preset",
        choices=sorted(PRESET_THUMB_SIZES.keys()),
        default="inspect",
    )
    parser.add_argument(
        "--report-json",
        default=str(
            REPO_ROOT
            / ".openclaw"
            / "reports"
            / "qa_probe_cover_browse_surface"
            / time.strftime("%Y%m%d_%H%M%S")
            / "report.json"
        ),
    )
    args = parser.parse_args()

    exe_dir = Path(args.exe_dir).resolve()
    db_path = Path(args.db_path).resolve()
    report_path = Path(args.report_json).resolve()
    diagnostics_path = exe_dir / "aura_diagnostics_live.jsonl"
    use_dev_mode = args.auth_mode in {"dev_bypass", "basic_login_dev"}
    settings_path = resolve_settings_path(exe_dir, use_dev_mode=use_dev_mode)

    if not (exe_dir / "HarkenSearch.exe").exists():
        raise SystemExit(f"Missing HarkenSearch.exe in {exe_dir}")
    if not db_path.exists():
        raise SystemExit(f"Missing db-path: {db_path}")

    attach_pid = int(args.attach_pid)
    attach_hwnd = int(args.attach_hwnd)
    if attach_pid <= 0 and attach_hwnd <= 0:
        cleanup_existing_auro()
        clear_crash_sentinel(exe_dir)
        ensure_probe_settings(settings_path, db_path)

    env = os.environ.copy()
    env["AURA_STARTUP_DB"] = str(db_path)
    env["AURA_RUNTIME_ROOT"] = str(exe_dir)
    if attach_pid <= 0 and attach_hwnd <= 0:
        if args.auth_mode == "harness_multi":
            clear_dev_mode_flag(exe_dir)
            env["AURA_HARNESS_MULTI_INSTANCE"] = "1"
            env["AURA_HARNESS_LANE_KIND"] = "functional"
        elif args.auth_mode == "dev_bypass":
            write_dev_mode_flag(exe_dir, "qa_probe_cover_browse_surface")
        elif use_dev_mode:
            write_dev_mode_flag(exe_dir, "qa_probe_cover_browse_surface_login")
        else:
            clear_dev_mode_flag(exe_dir)

    try:
        diagnostics_path.unlink()
    except OSError:
        pass

    proc: subprocess.Popen[str] | None = None
    if attach_pid > 0 or attach_hwnd > 0:
        launch_start_ms = int(time.time() * 1000)
    else:
        cleanup_existing_auro()
        proc = subprocess.Popen(
            [str(exe_dir / "HarkenSearch.exe")],
            cwd=str(exe_dir),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        launch_start_ms = int(time.time() * 1000)

    report: dict[str, object] = {
        "exeDir": str(exe_dir),
        "dbPath": str(db_path),
        "authMode": args.auth_mode,
        "settingsPath": str(settings_path),
        "reportJson": str(report_path),
        "proofIds": [
            "PROOF-COVER-TREE-SCOPE",
            "PROOF-COVER-SLIDER-STABILITY",
            "PROOF-COVER-PREVIEW-PARITY",
            "PROOF-COVER-FILTER",
        ],
        "pid": attach_pid if attach_pid > 0 else int(proc.pid if proc else 0),
        "attachPid": attach_pid,
        "attachHwnd": attach_hwnd,
        "launchStartMs": launch_start_ms,
    }

    try:
        if attach_hwnd > 0:
            window_result = {
                "passed": True,
                "pid": attach_pid,
                "window": {
                    "hwnd": attach_hwnd,
                    "title": "attached_hwnd",
                    "visible": True,
                },
                "candidates": [],
                "fallbackReason": "attach_hwnd",
            }
        elif attach_pid > 0:
            window_result = find_visible_window_for_pid(attach_pid)
        else:
            window_result = wait_for_window(int(proc.pid if proc else 0), args.wait_window_timeout)

        report["window"] = window_result
        if not bool(window_result.get("passed")):
            raise RuntimeError(str(window_result.get("message", "window not found")))

        hwnd = int(dict(window_result.get("window", {})).get("hwnd", 0) or 0)
        if hwnd <= 0:
            raise RuntimeError("window handle missing")

        set_foreground(hwnd)
        driver = QaDriver(exe_dir)
        ready_state, login_report = wait_ready_with_login_retry(
            driver,
            hwnd=hwnd,
            auth_mode=str(args.auth_mode),
            login_account=str(args.login_account),
            login_password=str(args.login_password),
            total_timeout=float(args.wait_ready_timeout),
        )
        report["readyState"] = ready_state
        if login_report is not None:
            report["login"] = login_report

        report["probeReadyState"] = driver.wait_probe_ready(timeout=args.wait_ready_timeout)
        try:
            driver.focus_window(timeout=5.0)
        except QaDriverError:
            pass

        report_dir = report_path.parent
        report_dir.mkdir(parents=True, exist_ok=True)
        report["initialState"] = ensure_cover_surface_ready(
            driver,
            hwnd=hwnd,
            force_artwork_tab_click=bool(args.force_artwork_tab_click),
            visible_timeout=float(args.filter_ready_timeout),
        )
        report["initialScreenshot"] = try_capture_window(hwnd, report_dir / "window_before_browse_surface_probe.png")

        preview_parity_proof = run_preview_parity_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )
        report["previewParityProof"] = preview_parity_proof

        tree_path, tree_resolution = resolve_tree_path(
            driver,
            explicit_path=str(args.tree_path),
            preferred_rows=[int(args.tree_row), 1, 0],
        )
        report["treePathResolution"] = tree_resolution
        tree_scope_proof = run_tree_scope_proof(
            driver,
            tree_path=tree_path,
            search_query=str(args.search_query),
            timeout=float(args.filter_ready_timeout),
        )
        report["treeScopeProof"] = tree_scope_proof

        slider_stability_proof = run_slider_stability_proof(
            driver,
            diagnostics_path=diagnostics_path,
            requested_size=float(args.thumb_size),
            tile_preset=str(args.tile_preset),
            timeout=float(args.filter_ready_timeout),
        )
        report["sliderStabilityProof"] = slider_stability_proof

        cover_filter_scope_proof = run_cover_filter_scope_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )
        report["coverFilterScopeProof"] = cover_filter_scope_proof

        report["finalState"] = driver.get_state()
        report["finalScreenshot"] = try_capture_window(hwnd, report_dir / "window_after_browse_surface_probe.png")

        all_failures: list[str] = []
        for key in ("previewParityProof", "treeScopeProof", "coverFilterScopeProof", "sliderStabilityProof"):
            proof = dict(report.get(key, {}))
            if not bool(proof.get("passed", False)):
                for failure in proof.get("failures", []):
                    all_failures.append(f"{key}: {failure}")

        report["assessment"] = {
            "passed": not all_failures,
            "proofs": {
                "previewParityProof": bool(dict(report.get("previewParityProof", {})).get("passed", False)),
                "treeScopeProof": bool(dict(report.get("treeScopeProof", {})).get("passed", False)),
                "coverFilterScopeProof": bool(dict(report.get("coverFilterScopeProof", {})).get("passed", False)),
                "sliderStabilityProof": bool(dict(report.get("sliderStabilityProof", {})).get("passed", False)),
            },
            "failures": all_failures,
        }
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        return 0 if bool(dict(report["assessment"]).get("passed")) else 2
    except Exception as exc:
        report["ok"] = False
        report["error"] = str(exc)
        try:
            window = dict(report.get("window", {})).get("window", {})
            hwnd = int(dict(window).get("hwnd", 0) or 0)
            if hwnd > 0:
                report["failureScreenshot"] = try_capture_window(
                    hwnd,
                    report_path.parent / "window_failure.png",
                )
        except Exception as screenshot_exc:
            report["failureScreenshotError"] = str(screenshot_exc)
        try:
            if diagnostics_path.exists():
                report["diagnosticsTail"] = load_jsonl(diagnostics_path)[-40:]
        except Exception as diag_exc:
            report["diagnosticsTailError"] = str(diag_exc)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        raise


if __name__ == "__main__":
    raise SystemExit(main())
