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

from qa_driver import QaDriver  # noqa: E402
from qa_driver import QaDriverError  # noqa: E402
from qa_probe_cover_browse_surface import (  # noqa: E402
    ensure_cover_surface_ready,
    ensure_integrated_mode,
    extract_browse_surface,
    extract_cover_wall,
    read_row,
    resolve_tree_path,
    run_cover_filter_scope_proof,
    run_preview_parity_proof,
    run_tree_scope_proof,
)
from qa_probe_cover_replacement_integration import (  # noqa: E402
    clear_browse_filters,
    ensure_integration_probe_settings,
    require_idle_cover_surface,
    run_keyword_integrated_proof,
    run_nofilter_integrated_proof,
    run_rollback_proof,
    run_startup_integrated_proof,
)
from qa_probe_cover_wall_perf import (  # noqa: E402
    cleanup_existing_auro,
    find_visible_window_for_pid,
    resolve_settings_path,
    set_foreground,
    try_capture_window,
    wait_for_window,
    wait_ready_with_login_retry,
)
from run_local_harness import (  # noqa: E402
    clear_crash_sentinel,
    clone_database_package,
    clear_dev_mode_flag,
    write_dev_mode_flag,
)


def clone_proof(proof: dict[str, object], proof_id: str) -> dict[str, object]:
    cloned = dict(proof)
    cloned["proofId"] = proof_id
    return cloned


def append_failure(failures: list[str], proof_key: str, message: str) -> None:
    failures.append(f"{proof_key}: {message}")


def choose_rebuild_proof_source_db(
    source_db_path: Path,
    explicit_rebuild_db_path: Path | None,
) -> Path:
    if explicit_rebuild_db_path is not None:
        return explicit_rebuild_db_path
    preferred_repo_fixture = (
        REPO_ROOT
        / ".openclaw"
        / "fixtures"
        / "manual_delete_repro"
        / "auro_delete_repro.db"
    )
    if preferred_repo_fixture.exists():
        return preferred_repo_fixture
    return source_db_path


def clone_rebuild_proof_fixture(
    rebuild_source_db_path: Path,
    target_db_path: Path,
) -> list[Path]:
    target_db_path.parent.mkdir(parents=True, exist_ok=True)
    return clone_database_package(rebuild_source_db_path, target_db_path)


def require_expected_source(
    state: dict[str, object],
    *,
    expected_source: str,
    proof_key: str,
    failures: list[str],
) -> None:
    cover_wall = extract_cover_wall(state)
    actual_source = str(cover_wall.get("source_name", "")).strip().lower()
    if actual_source != expected_source:
        append_failure(
            failures,
            proof_key,
            f"expected cover_wall.source_name={expected_source!r}, got {actual_source!r}",
        )


def reject_legacy_search_fallback(
    state: dict[str, object],
    *,
    proof_key: str,
    failures: list[str],
) -> None:
    cover_wall = extract_cover_wall(state)
    actual_source = str(cover_wall.get("source_name", "")).strip().lower()
    if actual_source == "search_fallback":
        append_failure(
            failures,
            proof_key,
            "cover_wall.source_name regressed to legacy search_fallback",
        )


def run_retirement_nofilter_prime_repeat(
    driver: QaDriver,
    *,
    timeout: float,
) -> dict[str, object]:
    proof_failures: list[str] = []

    prime_state = clear_browse_filters(driver, timeout=timeout)
    prime = clone_proof(
        run_nofilter_integrated_proof(prime_state),
        "PROOF-COVER-RETIRE-NOFILTER-PRIME-REPEAT",
    )
    if not bool(prime.get("passed", False)):
        for failure in prime.get("failures", []):
            append_failure(proof_failures, "prime", str(failure))
    require_expected_source(prime_state, expected_source="global", proof_key="prime", failures=proof_failures)
    reject_legacy_search_fallback(prime_state, proof_key="prime", failures=proof_failures)

    repeat_state = clear_browse_filters(driver, timeout=timeout)
    repeat = clone_proof(
        run_nofilter_integrated_proof(repeat_state),
        "PROOF-COVER-RETIRE-NOFILTER-PRIME-REPEAT",
    )
    if not bool(repeat.get("passed", False)):
        for failure in repeat.get("failures", []):
            append_failure(proof_failures, "repeat", str(failure))
    require_expected_source(repeat_state, expected_source="global", proof_key="repeat", failures=proof_failures)
    reject_legacy_search_fallback(repeat_state, proof_key="repeat", failures=proof_failures)

    if int(prime_state.get("total_count", 0) or 0) != int(repeat_state.get("total_count", 0) or 0):
        append_failure(proof_failures, "repeat", "total_count changed between no-filter prime and repeat")
    if int(prime_state.get("result_count", 0) or 0) != int(repeat_state.get("result_count", 0) or 0):
        append_failure(proof_failures, "repeat", "result_count changed between no-filter prime and repeat")

    return {
        "proofId": "PROOF-COVER-RETIRE-NOFILTER-PRIME-REPEAT",
        "passed": not proof_failures,
        "failures": proof_failures,
        "prime": prime,
        "repeat": repeat,
        "primeState": prime_state,
        "repeatState": repeat_state,
    }


def run_retirement_keyword_prime_repeat(
    driver: QaDriver,
    *,
    search_query: str,
    timeout: float,
) -> dict[str, object]:
    proof_failures: list[str] = []

    clear_browse_filters(driver, timeout=timeout)
    prime = clone_proof(
        run_keyword_integrated_proof(driver, search_query=search_query, timeout=timeout),
        "PROOF-COVER-RETIRE-KEYWORD-PRIME-REPEAT",
    )
    prime_state = dict(prime.get("state", {}))
    if not bool(prime.get("passed", False)):
        for failure in prime.get("failures", []):
            append_failure(proof_failures, "prime", str(failure))
    require_expected_source(prime_state, expected_source="facet", proof_key="prime", failures=proof_failures)
    reject_legacy_search_fallback(prime_state, proof_key="prime", failures=proof_failures)

    clear_browse_filters(driver, timeout=timeout)
    repeat = clone_proof(
        run_keyword_integrated_proof(driver, search_query=search_query, timeout=timeout),
        "PROOF-COVER-RETIRE-KEYWORD-PRIME-REPEAT",
    )
    repeat_state = dict(repeat.get("state", {}))
    if not bool(repeat.get("passed", False)):
        for failure in repeat.get("failures", []):
            append_failure(proof_failures, "repeat", str(failure))
    require_expected_source(repeat_state, expected_source="facet", proof_key="repeat", failures=proof_failures)
    reject_legacy_search_fallback(repeat_state, proof_key="repeat", failures=proof_failures)

    if int(prime_state.get("total_count", 0) or 0) != int(repeat_state.get("total_count", 0) or 0):
        append_failure(proof_failures, "repeat", "total_count changed between keyword prime and repeat")

    return {
        "proofId": "PROOF-COVER-RETIRE-KEYWORD-PRIME-REPEAT",
        "passed": not proof_failures,
        "failures": proof_failures,
        "prime": prime,
        "repeat": repeat,
        "primeState": prime_state,
        "repeatState": repeat_state,
    }


def run_retirement_path_prime_repeat(
    driver: QaDriver,
    *,
    tree_path: str,
    timeout: float,
) -> dict[str, object]:
    proof_failures: list[str] = []

    clear_browse_filters(driver, timeout=timeout)
    prime = clone_proof(
        run_tree_scope_proof(driver, tree_path=tree_path, search_query="", timeout=timeout),
        "PROOF-COVER-RETIRE-PATH-PRIME-REPEAT",
    )
    prime_state = dict(prime.get("stateAfter", {}))
    if not bool(prime.get("passed", False)):
        for failure in prime.get("failures", []):
            append_failure(proof_failures, "prime", str(failure))
    require_expected_source(prime_state, expected_source="facet", proof_key="prime", failures=proof_failures)
    reject_legacy_search_fallback(prime_state, proof_key="prime", failures=proof_failures)

    clear_browse_filters(driver, timeout=timeout)
    repeat = clone_proof(
        run_tree_scope_proof(driver, tree_path=tree_path, search_query="", timeout=timeout),
        "PROOF-COVER-RETIRE-PATH-PRIME-REPEAT",
    )
    repeat_state = dict(repeat.get("stateAfter", {}))
    if not bool(repeat.get("passed", False)):
        for failure in repeat.get("failures", []):
            append_failure(proof_failures, "repeat", str(failure))
    require_expected_source(repeat_state, expected_source="facet", proof_key="repeat", failures=proof_failures)
    reject_legacy_search_fallback(repeat_state, proof_key="repeat", failures=proof_failures)

    if int(prime_state.get("total_count", 0) or 0) != int(repeat_state.get("total_count", 0) or 0):
        append_failure(proof_failures, "repeat", "total_count changed between path prime and repeat")

    return {
        "proofId": "PROOF-COVER-RETIRE-PATH-PRIME-REPEAT",
        "passed": not proof_failures,
        "failures": proof_failures,
        "treePath": tree_path,
        "prime": prime,
        "repeat": repeat,
        "primeState": prime_state,
        "repeatState": repeat_state,
    }


def run_retirement_cover_filter_proof(
    driver: QaDriver,
    *,
    timeout: float,
) -> dict[str, object]:
    clear_browse_filters(driver, timeout=timeout)
    proof = clone_proof(
        run_cover_filter_scope_proof(driver, timeout=timeout),
        "PROOF-COVER-RETIRE-FILTER",
    )
    failures = [str(item) for item in proof.get("failures", [])]
    state_after = dict(proof.get("stateAfter", {}))
    reject_legacy_search_fallback(state_after, proof_key="stateAfter", failures=failures)
    if bool(state_after):
        source_name = str(extract_cover_wall(state_after).get("source_name", "")).strip().lower()
        if source_name not in {"facet", "selection"}:
            append_failure(
                failures,
                "stateAfter",
                f"expected cover filter wall to stay on facet/selection surface, got {source_name!r}",
            )
    proof["failures"] = failures
    proof["passed"] = not failures
    return proof


def run_retirement_preview_parity_proof(
    driver: QaDriver,
    *,
    timeout: float,
) -> dict[str, object]:
    clear_browse_filters(driver, timeout=timeout)
    proof = clone_proof(
        run_preview_parity_proof(driver, timeout=timeout),
        "PROOF-COVER-RETIRE-PREVIEW-PARITY",
    )
    failures = [str(item) for item in proof.get("failures", [])]
    state_after = dict(proof.get("stateAfter", {}))
    reject_legacy_search_fallback(state_after, proof_key="stateAfter", failures=failures)
    proof["failures"] = failures
    proof["passed"] = not failures
    return proof


def run_retirement_rollback_proof(
    driver: QaDriver,
    *,
    timeout: float,
) -> dict[str, object]:
    proof = clone_proof(
        run_rollback_proof(driver, timeout=timeout),
        "PROOF-COVER-RETIRE-ROLLBACK",
    )
    failures = [str(item) for item in proof.get("failures", [])]
    state_after_toggle_on = dict(proof.get("stateAfterToggleOn", {}))
    if state_after_toggle_on:
        require_expected_source(
            state_after_toggle_on,
            expected_source="global",
            proof_key="stateAfterToggleOn",
            failures=failures,
        )
        reject_legacy_search_fallback(
            state_after_toggle_on,
            proof_key="stateAfterToggleOn",
            failures=failures,
        )
    proof["failures"] = failures
    proof["passed"] = not failures
    return proof


def run_cover_rebuild_proof(
    driver: QaDriver,
    *,
    hwnd: int,
    source_db_path: Path,
    rebuild_proof_db_path: Path | None,
    report_dir: Path,
    timeout: float,
) -> dict[str, object]:
    failures: list[str] = []
    baseline_state = clear_browse_filters(driver, timeout=timeout)
    baseline_db_path = str(baseline_state.get("db_path", ""))
    rebuild_source_db = choose_rebuild_proof_source_db(source_db_path, rebuild_proof_db_path)
    if not rebuild_source_db.exists():
        raise FileNotFoundError(f"Missing rebuild proof db: {rebuild_source_db}")
    rebuild_fixture_dir = report_dir / "rebuild_fixture"
    rebuild_fixture_db = rebuild_fixture_dir / "cover_rebuild_retirement.db"
    rebuild_fixture_dir.mkdir(parents=True, exist_ok=True)
    copied_files = clone_rebuild_proof_fixture(rebuild_source_db, rebuild_fixture_db)

    switch_error = ""
    switch_result = driver.switch_db(str(rebuild_fixture_db))
    try:
        switched_state = driver.wait_db_switch_complete(
            str(rebuild_fixture_db),
            timeout=min(timeout, 180.0),
            poll_interval=0.5,
        )
    except QaDriverError as exc:
        switch_error = str(exc)
        failures.append(switch_error)
        switched_state = driver.get_state()
    if str(switched_state.get("db_path", "")) != str(rebuild_fixture_db):
        failures.append("cover rebuild proof did not switch onto the requested rebuild fixture db")
        return {
            "proofId": "PROOF-COVER-RETIRE-REBUILD",
            "passed": False,
            "failures": failures,
            "baselineState": baseline_state,
            "rebuildSourceDbPath": str(rebuild_source_db),
            "boundedRebuildDbPath": str(rebuild_fixture_db),
            "copiedFiles": [str(path) for path in copied_files],
            "switchResult": switch_result,
            "switchError": switch_error,
            "stateAfterSwitch": switched_state,
        }
    switched_surface_state = ensure_cover_surface_ready(
        driver,
        hwnd=hwnd,
        force_artwork_tab_click=True,
        visible_timeout=timeout,
    )
    clear_browse_filters(driver, timeout=timeout)

    start_result = driver.start_cover_rebuild(timeout=min(timeout, 10.0))
    if not bool(start_result.get("ok", False)):
        failures.append(f"start_cover_rebuild failed: {start_result}")

    wait_error = ""
    try:
        completed_state = driver.wait_cover_rebuild_complete(timeout=timeout, poll_interval=0.5)
    except QaDriverError as exc:
        wait_error = str(exc)
        failures.append(wait_error)
        completed_state = driver.get_state()
    settled_surface_state = ensure_cover_surface_ready(
        driver,
        hwnd=hwnd,
        force_artwork_tab_click=True,
        visible_timeout=timeout,
    )
    settled_state = require_idle_cover_surface(driver, timeout=timeout)
    cover_rebuild = dict(completed_state.get("cover_rebuild", {}))
    settled_cover_rebuild = dict(settled_state.get("cover_rebuild", {}))

    if not bool(start_result.get("active", False)):
        failures.append("start_cover_rebuild did not report an active rebuild")
    if bool(cover_rebuild.get("active", False)):
        failures.append("cover rebuild remained active after wait_cover_rebuild_complete")
    if int(cover_rebuild.get("progress", 0) or 0) <= 0:
        failures.append("cover rebuild completed without reporting positive progress")
    if int(cover_rebuild.get("total", 0) or 0) <= 0:
        failures.append("cover rebuild completed without a positive total")
    if int(cover_rebuild.get("progress", 0) or 0) > int(cover_rebuild.get("total", 0) or 0):
        failures.append("cover rebuild progress exceeded total")
    if bool(settled_cover_rebuild.get("active", False)):
        failures.append("cover rebuild was still active after the cover wall settled")
    if str(settled_state.get("db_path", "")) != str(rebuild_fixture_db):
        failures.append("cover rebuild did not stay on the bounded rebuild fixture db")
    if bool(settled_state.get("db_switch_in_progress", False)):
        failures.append("cover rebuild left db_switch_in_progress=true")
    if bool(dict(settled_state.get("scan_status", {})).get("scanning", False)):
        failures.append("cover rebuild should not start a library scan")

    settled_cover_wall = extract_cover_wall(settled_state)
    if int(settled_cover_wall.get("display_count", 0) or 0) <= 0:
        failures.append("cover rebuild did not restore a visible cover wall")
    require_expected_source(settled_state, expected_source="global", proof_key="settledState", failures=failures)
    reject_legacy_search_fallback(settled_state, proof_key="settledState", failures=failures)

    restore_switch_error = ""
    restore_switch_result = driver.switch_db(baseline_db_path)
    try:
        restored_switch_state = driver.wait_db_switch_complete(
            baseline_db_path,
            timeout=min(timeout, 180.0),
            poll_interval=0.5,
        )
    except QaDriverError as exc:
        restore_switch_error = str(exc)
        failures.append(restore_switch_error)
        restored_switch_state = driver.get_state()
    restored_surface_state = ensure_cover_surface_ready(
        driver,
        hwnd=hwnd,
        force_artwork_tab_click=True,
        visible_timeout=timeout,
    )
    restored_state = clear_browse_filters(driver, timeout=timeout)
    if str(restored_state.get("db_path", "")) != baseline_db_path:
        failures.append("cover rebuild proof did not restore the original db after bounded rebuild verification")

    return {
        "proofId": "PROOF-COVER-RETIRE-REBUILD",
        "passed": not failures,
        "failures": failures,
        "baselineState": baseline_state,
        "rebuildSourceDbPath": str(rebuild_source_db),
        "boundedRebuildDbPath": str(rebuild_fixture_db),
        "copiedFiles": [str(path) for path in copied_files],
        "switchResult": switch_result,
        "switchError": switch_error,
        "stateAfterSwitch": switched_state,
        "surfaceStateAfterSwitch": switched_surface_state,
        "startResult": start_result,
        "waitError": wait_error,
        "completedState": completed_state,
        "settledSurfaceState": settled_surface_state,
        "settledState": settled_state,
        "restoreSwitchResult": restore_switch_result,
        "restoreSwitchError": restore_switch_error,
        "restoredSwitchState": restored_switch_state,
        "restoredSurfaceState": restored_surface_state,
        "restoredState": restored_state,
    }


def run_cover_rebuild_proof_from_report(report_path: Path) -> dict[str, object]:
    failures: list[str] = []
    if not report_path.exists():
        failures.append(f"external rebuild proof report is missing: {report_path}")
        return {
            "proofId": "PROOF-COVER-RETIRE-REBUILD",
            "passed": False,
            "failures": failures,
            "externalReportPath": str(report_path),
        }

    report = json.loads(report_path.read_text(encoding="utf-8"))
    assessment = dict(report.get("assessment", {}))
    cover_wall = dict(assessment.get("coverWall", {}))
    cover_rebuild = dict(assessment.get("coverRebuild", {}))
    db_counts = dict(report.get("dbCountsAfterRebuild", {}))
    settled_state = dict(report.get("settledState", {}))

    if not bool(assessment.get("passed", False)):
        for failure in assessment.get("failures", []):
            failures.append(str(failure))
        if not failures:
            failures.append("external rebuild proof assessment did not pass")
    if int(db_counts.get("scanRootFiles", 0) or 0) <= 0:
        failures.append("external rebuild proof has no imported scan-root rows")
    if int(db_counts.get("uniqueCoverGroups", 0) or 0) <= 0:
        failures.append("external rebuild proof has no unique cover groups")
    if bool(cover_rebuild.get("active", False)):
        failures.append("external rebuild proof still reports cover_rebuild.active=true")
    if int(cover_rebuild.get("progress", 0) or 0) <= 0:
        failures.append("external rebuild proof has no positive rebuild progress")
    if int(cover_rebuild.get("total", 0) or 0) <= 0:
        failures.append("external rebuild proof has no positive rebuild total")
    if int(cover_rebuild.get("progress", 0) or 0) != int(cover_rebuild.get("total", 0) or 0):
        failures.append("external rebuild proof progress did not reach total")
    if int(cover_wall.get("display_count", 0) or 0) <= 0:
        failures.append("external rebuild proof did not restore a visible wall")
    if str(cover_wall.get("source_name", "")).strip().lower() != "global":
        failures.append(f"external rebuild proof expected global wall source, got {cover_wall.get('source_name')!r}")
    if int(cover_wall.get("fallback_items", 0) or 0) != 0:
        failures.append("external rebuild proof still has fallback_items")

    return {
        "proofId": "PROOF-COVER-RETIRE-REBUILD",
        "passed": not failures,
        "failures": failures,
        "externalReportPath": str(report_path),
        "externalAssessment": assessment,
        "dbCountsAfterRebuild": db_counts,
        "completedState": report.get("rebuildComplete", {}),
        "settledState": settled_state,
    }


def run_legacy_fallback_audit(
    named_states: dict[str, dict[str, object]],
) -> dict[str, object]:
    failures: list[str] = []
    states: dict[str, dict[str, object]] = {}
    for name, state in named_states.items():
        if not state:
            failures.append(f"{name} state missing from retirement audit")
            continue
        cover_wall = extract_cover_wall(state)
        source_name = str(cover_wall.get("source_name", "")).strip().lower()
        states[name] = {
            "source_name": source_name,
            "display_count": int(cover_wall.get("display_count", 0) or 0),
            "result_count": int(state.get("result_count", 0) or 0),
            "total_count": int(state.get("total_count", 0) or 0),
        }
        if source_name == "search_fallback":
            failures.append(f"{name} still uses legacy search_fallback")

    return {
        "proofId": "PROOF-COVER-RETIRE-LEGACY-FALLBACK-AUDIT",
        "passed": not failures,
        "failures": failures,
        "states": states,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Retirement QA probe for CoverWall V2 Slice 5 fallback narrowing, rollback, and rebuild proofs."
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
    parser.add_argument("--rebuild-timeout", type=float, default=300.0)
    parser.add_argument("--attach-pid", type=int, default=0)
    parser.add_argument("--attach-hwnd", type=int, default=0)
    parser.add_argument("--force-artwork-tab-click", action="store_true")
    parser.add_argument("--search-query", default="wind")
    parser.add_argument("--tree-path", default="")
    parser.add_argument("--tree-row", type=int, default=1)
    parser.add_argument("--rebuild-proof-db", default="")
    parser.add_argument(
        "--rebuild-proof-report",
        default=str(REPO_ROOT / ".openclaw" / "reports" / "qa_probe_cover_rebuild_8082" / "harness_multi" / "report.json"),
    )
    parser.add_argument(
        "--report-json",
        default=str(
            REPO_ROOT
            / ".openclaw"
            / "reports"
            / "qa_probe_cover_replacement_retirement"
            / time.strftime("%Y%m%d_%H%M%S")
            / "report.json"
        ),
    )
    args = parser.parse_args()

    exe_dir = Path(args.exe_dir).resolve()
    db_path = Path(args.db_path).resolve()
    rebuild_proof_db_path = Path(args.rebuild_proof_db).resolve() if str(args.rebuild_proof_db).strip() else None
    rebuild_proof_report_path = (
        Path(args.rebuild_proof_report).resolve()
        if str(args.rebuild_proof_report).strip()
        else None
    )
    report_path = Path(args.report_json).resolve()
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
            write_dev_mode_flag(exe_dir, "qa_probe_cover_replacement_retirement")
        elif use_dev_mode:
            write_dev_mode_flag(exe_dir, "qa_probe_cover_replacement_retirement_login")
        else:
            clear_dev_mode_flag(exe_dir)

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
            "PROOF-COVER-RETIRE-NOFILTER-PRIME-REPEAT",
            "PROOF-COVER-RETIRE-KEYWORD-PRIME-REPEAT",
            "PROOF-COVER-RETIRE-PATH-PRIME-REPEAT",
            "PROOF-COVER-RETIRE-FILTER",
            "PROOF-COVER-RETIRE-PREVIEW-PARITY",
            "PROOF-COVER-RETIRE-ROLLBACK",
            "PROOF-COVER-RETIRE-REBUILD",
            "PROOF-COVER-RETIRE-LEGACY-FALLBACK-AUDIT",
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
            report_dir / "window_before_cover_replacement_retirement_probe.png",
        )

        report["startupBaselineIntegratedProof"] = run_startup_integrated_proof(initial_surface_state)
        report["noFilterPrimeRepeatProof"] = run_retirement_nofilter_prime_repeat(
            driver,
            timeout=float(args.filter_ready_timeout),
        )
        report["keywordPrimeRepeatProof"] = run_retirement_keyword_prime_repeat(
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
        report["pathPrimeRepeatProof"] = run_retirement_path_prime_repeat(
            driver,
            tree_path=tree_path,
            timeout=float(args.filter_ready_timeout),
        )

        report["coverFilterRetirementProof"] = run_retirement_cover_filter_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )
        report["previewParityRetirementProof"] = run_retirement_preview_parity_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )
        report["rollbackRetirementProof"] = run_retirement_rollback_proof(
            driver,
            timeout=float(args.filter_ready_timeout),
        )
        if rebuild_proof_report_path is not None and rebuild_proof_report_path.exists():
            report["coverRebuildProof"] = run_cover_rebuild_proof_from_report(rebuild_proof_report_path)
        else:
            report["coverRebuildProof"] = run_cover_rebuild_proof(
                driver,
                hwnd=hwnd,
                source_db_path=db_path,
                rebuild_proof_db_path=rebuild_proof_db_path,
                report_dir=report_dir,
                timeout=float(args.rebuild_timeout),
            )

        fallback_states = {
            "noFilterPrime": dict(report["noFilterPrimeRepeatProof"].get("primeState", {})),
            "noFilterRepeat": dict(report["noFilterPrimeRepeatProof"].get("repeatState", {})),
            "keywordPrime": dict(report["keywordPrimeRepeatProof"].get("primeState", {})),
            "keywordRepeat": dict(report["keywordPrimeRepeatProof"].get("repeatState", {})),
            "pathPrime": dict(report["pathPrimeRepeatProof"].get("primeState", {})),
            "pathRepeat": dict(report["pathPrimeRepeatProof"].get("repeatState", {})),
            "coverFilter": dict(report["coverFilterRetirementProof"].get("stateAfter", {})),
            "previewParity": dict(report["previewParityRetirementProof"].get("stateAfter", {})),
            "rollbackRestored": dict(report["rollbackRetirementProof"].get("stateAfterToggleOn", {})),
            "rebuildSettled": dict(report["coverRebuildProof"].get("settledState", {})),
        }
        report["legacyFallbackAudit"] = run_legacy_fallback_audit(fallback_states)

        report["finalState"] = driver.get_state()
        report["finalScreenshot"] = try_capture_window(
            hwnd,
            report_dir / "window_after_cover_replacement_retirement_probe.png",
        )

        proof_keys = [
            "noFilterPrimeRepeatProof",
            "keywordPrimeRepeatProof",
            "pathPrimeRepeatProof",
            "coverFilterRetirementProof",
            "previewParityRetirementProof",
            "rollbackRetirementProof",
            "coverRebuildProof",
            "legacyFallbackAudit",
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
