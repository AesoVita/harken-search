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

from harness_core import merge_ini_settings_text  # noqa: E402
from qa_driver import QaDriver, QaDriverError  # noqa: E402
from qa_probe_cover_browse_surface import (  # noqa: E402
    ensure_cover_surface_ready,
    ensure_integrated_mode,
    extract_browse_surface,
    extract_cover_wall,
    read_row,
    resolve_tree_path,
    run_cover_filter_scope_proof,
    run_preview_parity_proof,
    run_slider_stability_proof,
    run_tree_scope_proof,
)
from qa_probe_cover_wall_perf import (  # noqa: E402
    cleanup_existing_auro,
    find_visible_window_for_pid,
    resolve_settings_path,
    set_foreground,
    try_capture_window,
    wait_cover_wall_probe_stable,
    wait_for_window,
    wait_ready_with_login_retry,
)
from run_local_harness import clear_crash_sentinel, clear_dev_mode_flag, write_dev_mode_flag  # noqa: E402


def ensure_integration_probe_settings(settings_path: Path, db_path: Path) -> None:
    settings_path.parent.mkdir(parents=True, exist_ok=True)
    existing_text = (
        settings_path.read_text(encoding="utf-8", errors="ignore")
        if settings_path.exists()
        else "# Aris settings (cover replacement integration probe)\n"
    )
    merged = merge_ini_settings_text(
        existing_text,
        {
            "diagnosticMode": "1",
            "showDiagnosticOverlay": "0",
            "currentDbPath": str(db_path),
            "recentDbCount": "1",
            "recentDb0": str(db_path),
            "coverWallIntegratedMode": "1",
        },
    )
    settings_path.write_text(merged, encoding="utf-8")


def require_idle_cover_surface(driver: QaDriver, *, timeout: float) -> dict[str, object]:
    driver.wait_search_idle(timeout=timeout)
    return wait_cover_wall_probe_stable(
        driver,
        timeout=timeout,
        require_visible=True,
        require_count_settled=True,
    )


def run_startup_integrated_proof(state: dict[str, object]) -> dict[str, object]:
    failures: list[str] = []
    startup = dict(state.get("startup_warmup", {}))
    cover_wall = extract_cover_wall(state)
    browse_surface = extract_browse_surface(state)

    if not bool(state.get("main_ui_ready", False)):
        failures.append("main_ui_ready should be true for clean-launch integrated startup proof")
    if str(startup.get("phase", "")).strip().lower() != "ready":
        failures.append(f"startup_warmup.phase expected 'ready', got {startup.get('phase')!r}")
    if not bool(startup.get("cover_wall_loaded", False)):
        failures.append("startup_warmup.cover_wall_loaded should be true after integrated startup settles")
    if not bool(cover_wall.get("integrated_mode", False)):
        failures.append("cover_wall.integrated_mode should be true during integrated startup proof")
    if not bool(browse_surface.get("integrated_mode", False)):
        failures.append("browse_surface.integrated_mode should be true during integrated startup proof")
    if int(cover_wall.get("display_count", 0) or 0) <= 0:
        failures.append("cover_wall.display_count should be positive after integrated startup settles")
    if int(cover_wall.get("visible_ready", 0) or 0) <= 0:
        failures.append("cover_wall.visible_ready should be positive after integrated startup settles")

    return {
        "proofId": "PROOF-COVER-STARTUP-INTEGRATED",
        "passed": not failures,
        "failures": failures,
        "state": state,
    }


def run_nofilter_integrated_proof(state: dict[str, object]) -> dict[str, object]:
    failures: list[str] = []
    cover_wall = extract_cover_wall(state)
    browse_surface = extract_browse_surface(state)
    folder_filter = dict(state.get("folder_filter", {}))
    cover_filter = dict(state.get("cover_filter", {}))

    if str(state.get("search_text", "")).strip():
        failures.append("no-filter proof expects empty search_text")
    if bool(folder_filter.get("active", False)):
        failures.append("no-filter proof expects folder_filter.active=false")
    if bool(cover_filter.get("active", False)):
        failures.append("no-filter proof expects cover_filter.active=false")
    if str(cover_wall.get("source_name", "")).strip().lower() != "global":
        failures.append(f"no-filter proof expected global cover-wall source, got {cover_wall.get('source_name')!r}")
    if int(state.get("result_count", 0) or 0) <= 0:
        failures.append("no-filter proof expects positive result_count")
    if int(cover_wall.get("display_count", 0) or 0) <= 0:
        failures.append("no-filter proof expects positive cover_wall.display_count")
    if "Keyword: (none)" not in str(browse_surface.get("bottom_info", "")):
        failures.append("no-filter proof expects browse_surface.bottom_info to show Keyword: (none)")

    return {
        "proofId": "PROOF-COVER-NOFILTER-INTEGRATED",
        "passed": not failures,
        "failures": failures,
        "state": state,
    }


def run_keyword_integrated_proof(
    driver: QaDriver,
    *,
    search_query: str,
    timeout: float,
) -> dict[str, object]:
    failures: list[str] = []
    driver.search_and_wait(search_query, timeout=timeout)
    state = require_idle_cover_surface(driver, timeout=timeout)
    cover_wall = extract_cover_wall(state)
    browse_surface = extract_browse_surface(state)

    if str(state.get("search_text", "")).strip().lower() != search_query.strip().lower():
        failures.append(
            f"keyword proof expected search_text={search_query!r}, got {state.get('search_text')!r}"
        )
    if int(state.get("result_count", 0) or 0) <= 0:
        failures.append("keyword proof expects positive result_count")
    if int(state.get("total_count", 0) or 0) <= 0:
        failures.append("keyword proof expects positive total_count")
    if int(cover_wall.get("display_count", 0) or 0) <= 0:
        failures.append("keyword proof expects positive cover_wall.display_count")
    if not bool(cover_wall.get("integrated_mode", False)):
        failures.append("keyword proof expects cover_wall.integrated_mode=true")
    if search_query.strip() not in str(browse_surface.get("bottom_info", "")):
        failures.append("keyword proof expects browse_surface.bottom_info to mention the active keyword")

    return {
        "proofId": "PROOF-COVER-KEYWORD-INTEGRATED",
        "passed": not failures,
        "failures": failures,
        "state": state,
    }


def clear_browse_filters(driver: QaDriver, *, timeout: float) -> dict[str, object]:
    driver.clear_cover_filter(timeout=timeout)
    driver.clear_folder_filter(timeout=timeout)
    driver.search_and_wait("", timeout=timeout)
    return require_idle_cover_surface(driver, timeout=timeout)


def run_rollback_proof(
    driver: QaDriver,
    *,
    timeout: float,
) -> dict[str, object]:
    failures: list[str] = []
    baseline = clear_browse_filters(driver, timeout=timeout)
    baseline_cover_wall = extract_cover_wall(baseline)
    baseline_browse = extract_browse_surface(baseline)

    toggle_off = ensure_integrated_mode(driver, enabled=False, timeout=timeout)
    state_off = require_idle_cover_surface(driver, timeout=timeout)
    cover_wall_off = extract_cover_wall(state_off)
    browse_off = extract_browse_surface(state_off)

    row0 = read_row(driver, 0)
    cover_key = str(row0.get("cover_key", "")).strip()
    if not cover_key:
        failures.append("rollback proof needs a non-empty row0 cover_key before testing mode-off specific-cover rejection")
        specific_cover_off = {"ok": False, "error": "row0 cover_key missing"}
    else:
        specific_cover_off = driver.set_specific_cover_filter(
            cover_key,
            file_path=str(row0.get("path", "")).strip() or None,
            folder_path=str(row0.get("folder_path", "")).strip() or None,
            label=str(row0.get("name", "")).strip() or None,
            timeout=timeout,
        )

    toggle_on = ensure_integrated_mode(driver, enabled=True, timeout=timeout)
    restored_state = require_idle_cover_surface(driver, timeout=timeout)
    cover_wall_on = extract_cover_wall(restored_state)
    browse_on = extract_browse_surface(restored_state)

    if bool(toggle_off.get("coverWallIntegratedMode", False)):
        failures.append("rollback proof expected coverWallIntegratedMode=false immediately after toggling off")
    if bool(cover_wall_off.get("integrated_mode", False)):
        failures.append("rollback proof expected cover_wall.integrated_mode=false after toggling off")
    if bool(browse_off.get("integrated_mode", False)):
        failures.append("rollback proof expected browse_surface.integrated_mode=false after toggling off")
    if bool(dict(state_off.get("cover_filter", {})).get("active", False)):
        failures.append("rollback proof expected cover_filter.active=false after toggling integrated mode off")
    if str(browse_off.get("pinned_cover_key", "")).strip():
        failures.append("rollback proof expected pinned_cover_key to clear after toggling integrated mode off")
    if bool(specific_cover_off.get("ok", True)):
        failures.append("rollback proof expected set_specific_cover_filter to fail while integrated mode is disabled")
    if "disabled" not in str(specific_cover_off.get("error", "")).lower():
        failures.append("rollback proof expected the disabled-mode error to mention that integrated mode is disabled")
    if str(state_off.get("db_path", "")) != str(baseline.get("db_path", "")):
        failures.append("rollback proof expected db_path to stay unchanged while toggling integrated mode")
    if bool(state_off.get("db_switch_in_progress", False)):
        failures.append("rollback proof expected db_switch_in_progress=false after toggling integrated mode")
    if bool(dict(state_off.get("scan_status", {})).get("scanning", False)):
        failures.append("rollback proof expected scan_status.scanning=false after toggling integrated mode")

    if not bool(toggle_on.get("coverWallIntegratedMode", False)):
        failures.append("rollback proof expected coverWallIntegratedMode=true after toggling back on")
    if not bool(cover_wall_on.get("integrated_mode", False)):
        failures.append("rollback proof expected cover_wall.integrated_mode=true after toggling back on")
    if not bool(browse_on.get("integrated_mode", False)):
        failures.append("rollback proof expected browse_surface.integrated_mode=true after toggling back on")
    if str(restored_state.get("db_path", "")) != str(baseline.get("db_path", "")):
        failures.append("rollback proof expected db_path to remain unchanged after re-enabling integrated mode")
    if int(restored_state.get("total_count", 0) or 0) != int(baseline.get("total_count", 0) or 0):
        failures.append("rollback proof expected total_count to remain stable across mode off/on")
    if int(restored_state.get("result_count", 0) or 0) != int(baseline.get("result_count", 0) or 0):
        failures.append("rollback proof expected result_count to remain stable across mode off/on")
    if int(cover_wall_on.get("display_count", 0) or 0) <= 0:
        failures.append("rollback proof expected cover_wall.display_count to stay positive after re-enabling integrated mode")
    if str(baseline_browse.get("effective_preview_path", "")) != str(restored_state.get("effective_preview_path", "")):
        # Empty-empty is fine; the important part is stability, not forcing a preview load.
        failures.append("rollback proof expected effective preview path to stay stable across mode off/on")

    return {
        "proofId": "PROOF-COVER-ROLLBACK-MODE-OFF",
        "passed": not failures,
        "failures": failures,
        "baselineState": baseline,
        "toggleOff": toggle_off,
        "stateAfterToggleOff": state_off,
        "specificCoverWhileOff": specific_cover_off,
        "toggleOn": toggle_on,
        "stateAfterToggleOn": restored_state,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Integrated QA probe for CoverWall V2 Slice 4 startup, no-filter, keyword/path/cover, and rollback proofs."
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
    parser.add_argument("--search-query", default="wind")
    parser.add_argument("--tree-path", default="")
    parser.add_argument("--tree-row", type=int, default=1)
    parser.add_argument("--thumb-size", type=float, default=96.0)
    parser.add_argument(
        "--tile-preset",
        choices=["compact", "browse", "inspect"],
        default="inspect",
    )
    parser.add_argument(
        "--report-json",
        default=str(
            REPO_ROOT
            / ".openclaw"
            / "reports"
            / "qa_probe_cover_replacement_integration"
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
        ensure_integration_probe_settings(settings_path, db_path)

    env = os.environ.copy()
    env["AURA_STARTUP_DB"] = str(db_path)
    env["AURA_RUNTIME_ROOT"] = str(exe_dir)
    if attach_pid <= 0 and attach_hwnd <= 0:
        if args.auth_mode == "harness_multi":
            clear_dev_mode_flag(exe_dir)
            env["AURA_HARNESS_MULTI_INSTANCE"] = "1"
            env["AURA_HARNESS_LANE_KIND"] = "functional"
        elif args.auth_mode == "dev_bypass":
            write_dev_mode_flag(exe_dir, "qa_probe_cover_replacement_integration")
        elif use_dev_mode:
            write_dev_mode_flag(exe_dir, "qa_probe_cover_replacement_integration_login")
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
            "PROOF-COVER-STARTUP-INTEGRATED",
            "PROOF-COVER-NOFILTER-INTEGRATED",
            "PROOF-COVER-KEYWORD-INTEGRATED",
            "PROOF-COVER-PATH-INTEGRATED",
            "PROOF-COVER-FILTER",
            "PROOF-COVER-PREVIEW-PARITY",
            "PROOF-COVER-SLIDER-STABILITY",
            "PROOF-COVER-ROLLBACK-MODE-OFF",
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
        report["startupStateBeforeArtwork"] = driver.get_state()
        report["integratedModeEnabled"] = ensure_integrated_mode(
            driver,
            enabled=True,
            timeout=float(args.filter_ready_timeout),
        )

        report_dir = report_path.parent
        report_dir.mkdir(parents=True, exist_ok=True)
        initial_surface_state = ensure_cover_surface_ready(
            driver,
            hwnd=hwnd,
            force_artwork_tab_click=bool(args.force_artwork_tab_click),
            visible_timeout=float(args.filter_ready_timeout),
        )
        report["initialSurfaceState"] = initial_surface_state
        report["initialScreenshot"] = try_capture_window(
            hwnd,
            report_dir / "window_before_cover_replacement_integration_probe.png",
        )

        report["startupIntegratedProof"] = run_startup_integrated_proof(initial_surface_state)
        report["noFilterIntegratedProof"] = run_nofilter_integrated_proof(initial_surface_state)

        report["keywordIntegratedProof"] = run_keyword_integrated_proof(
            driver,
            search_query=str(args.search_query),
            timeout=float(args.filter_ready_timeout),
        )

        clear_browse_filters(driver, timeout=float(args.filter_ready_timeout))
        tree_path, tree_resolution = resolve_tree_path(
            driver,
            explicit_path=str(args.tree_path),
            preferred_rows=[int(args.tree_row), 1, 0],
        )
        report["treePathResolution"] = tree_resolution
        report["pathIntegratedProof"] = run_tree_scope_proof(
            driver,
            tree_path=tree_path,
            search_query="",
            timeout=float(args.filter_ready_timeout),
        )

        clear_browse_filters(driver, timeout=float(args.filter_ready_timeout))
        report["previewParityProof"] = run_preview_parity_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )

        clear_browse_filters(driver, timeout=float(args.filter_ready_timeout))
        report["sliderStabilityProof"] = run_slider_stability_proof(
            driver,
            diagnostics_path=diagnostics_path,
            requested_size=float(args.thumb_size),
            tile_preset=str(args.tile_preset),
            timeout=float(args.filter_ready_timeout),
        )

        clear_browse_filters(driver, timeout=float(args.filter_ready_timeout))
        report["coverFilterIntegratedProof"] = run_cover_filter_scope_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )

        report["rollbackProof"] = run_rollback_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )

        report["finalState"] = driver.get_state()
        report["finalScreenshot"] = try_capture_window(
            hwnd,
            report_dir / "window_after_cover_replacement_integration_probe.png",
        )

        proof_keys = [
            "startupIntegratedProof",
            "noFilterIntegratedProof",
            "keywordIntegratedProof",
            "pathIntegratedProof",
            "previewParityProof",
            "sliderStabilityProof",
            "coverFilterIntegratedProof",
            "rollbackProof",
        ]
        failures: list[str] = []
        proof_status: dict[str, bool] = {}
        for key in proof_keys:
            proof = dict(report.get(key, {}))
            passed = bool(proof.get("passed", False))
            proof_status[key] = passed
            if not passed:
                for failure in proof.get("failures", []):
                    failures.append(f"{key}: {failure}")

        report["assessment"] = {
            "passed": not failures,
            "proofs": proof_status,
            "failures": failures,
        }
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        return 0 if not failures else 2
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
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        raise


if __name__ == "__main__":
    raise SystemExit(main())
