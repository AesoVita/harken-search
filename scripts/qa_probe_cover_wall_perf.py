from __future__ import annotations

import argparse
import ctypes
import json
import os
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
HARNESS_DIR = REPO_ROOT / ".openclaw" / "harness"
if str(HARNESS_DIR) not in sys.path:
    sys.path.insert(0, str(HARNESS_DIR))

from gui_smoke import USER32, enumerate_windows_for_pid, submit_basic_login, wait_for_window  # noqa: E402
from harness_core import merge_ini_settings_text  # noqa: E402
from qa_driver import QaDriver, QaDriverError  # noqa: E402
from real_machine_smoke import classify_pointer_frame_spikes  # noqa: E402
from run_local_harness import clear_crash_sentinel, clear_dev_mode_flag, write_dev_mode_flag  # noqa: E402
from screenshot import capture_window, get_client_rect_screen  # noqa: E402


MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_WHEEL = 0x0800
SW_RESTORE = 9
VK_ESCAPE = 0x1B
VK_MENU = 0x12
KEYEVENTF_KEYUP = 0x0002
SWP_NOMOVE = 0x0002
SWP_NOSIZE = 0x0001
SWP_SHOWWINDOW = 0x0040
HWND_TOPMOST = -1
HWND_NOTOPMOST = -2
WHEEL_DELTA = 120


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


def cleanup_existing_auro() -> None:
    subprocess.run(
        ["taskkill", "/F", "/IM", "HarkenSearch.exe"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    time.sleep(0.5)


def find_visible_window_for_pid(pid: int) -> dict[str, object]:
    result = wait_for_window(pid, 5.0)
    if not bool(result.get("passed")):
        candidates = enumerate_windows_for_pid(pid)
        visible_candidates = [entry for entry in candidates if bool(entry.get("visible"))]
        fallback_window = visible_candidates[0] if visible_candidates else (candidates[0] if candidates else None)
        if fallback_window is None:
            raise RuntimeError(str(result.get("message", f"window not found for pid {pid}")))
        window = dict(fallback_window)
        hwnd = int(window.get("hwnd", 0) or 0)
        if hwnd <= 0:
            raise RuntimeError(f"window handle missing for pid {pid}")
        return {
            "passed": True,
            "pid": pid,
            "window": window,
            "candidates": candidates,
            "fallbackReason": str(result.get("message", "")),
        }
    window = dict(result.get("window", {}))
    hwnd = int(window.get("hwnd", 0) or 0)
    if hwnd <= 0:
        raise RuntimeError(f"window handle missing for pid {pid}")
    return {
        "passed": True,
        "pid": pid,
        "window": window,
        "candidates": list(result.get("candidates", [])),
    }


def resolve_settings_path(exe_dir: Path, *, use_dev_mode: bool) -> Path:
    if use_dev_mode:
        return exe_dir / "settings_dev.ini"
    appdata = os.environ.get("APPDATA", "").strip()
    if appdata:
        return Path(appdata) / "AuroAudio" / "settings.ini"
    return exe_dir / "settings.ini"


def ensure_probe_settings(settings_path: Path, db_path: Path) -> None:
    settings_path.parent.mkdir(parents=True, exist_ok=True)
    existing_text = (
        settings_path.read_text(encoding="utf-8", errors="ignore")
        if settings_path.exists()
        else "# Aris settings (cover wall perf probe)\n"
    )
    merged = merge_ini_settings_text(
        existing_text,
        {
            "diagnosticMode": "1",
            "showDiagnosticOverlay": "0",
            "currentDbPath": str(db_path),
            "recentDbCount": "1",
            "recentDb0": str(db_path),
        },
    )
    settings_path.write_text(merged, encoding="utf-8")


def _send_vk(vk: int) -> None:
    USER32.keybd_event(vk, 0, 0, 0)
    time.sleep(0.03)
    USER32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)
    time.sleep(0.05)


def set_foreground(hwnd: int) -> None:
    USER32.ShowWindow(hwnd, SW_RESTORE)
    USER32.keybd_event(VK_MENU, 0, 0, 0)
    USER32.keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0)
    USER32.SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)
    USER32.SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)
    USER32.BringWindowToTop(hwnd)
    USER32.SetForegroundWindow(hwnd)
    USER32.SetActiveWindow(hwnd)
    USER32.SetFocus(hwnd)
    time.sleep(0.4)


def wait_ready_with_login_retry(
    driver: QaDriver,
    *,
    hwnd: int,
    auth_mode: str,
    login_account: str,
    login_password: str,
    total_timeout: float,
) -> tuple[dict[str, object], dict[str, object] | None]:
    """Mirror qa_suite startup hardening for probes that may boot through login UI."""
    login_modes = {"basic_login", "basic_login_dev", "harness_multi"}
    attempt_timeouts = [20.0, 15.0, 15.0]
    last_error: Exception | None = None
    login_report: dict[str, object] | None = None

    deadline = time.time() + max(total_timeout, 1.0)
    for attempt_index, attempt_timeout in enumerate(attempt_timeouts):
        remaining = deadline - time.time()
        if remaining <= 0:
            break
        try:
            ready = driver.wait_ready(timeout=min(attempt_timeout, remaining))
            return ready, login_report
        except QaDriverError as exc:
            last_error = exc
            if auth_mode in login_modes and hwnd > 0:
                set_foreground(hwnd)
                login_report = submit_basic_login(hwnd, login_account, login_password)
                login_report["attempt"] = attempt_index + 1
                time.sleep(1.0)

    remaining = deadline - time.time()
    if remaining > 0:
        try:
            ready = driver.wait_ready(timeout=remaining)
            return ready, login_report
        except QaDriverError as exc:
            last_error = exc

    if last_error is not None:
        raise last_error
    raise QaDriverError("App did not become ready before deadline")


def click_point(x: int, y: int) -> None:
    USER32.SetCursorPos(int(x), int(y))
    time.sleep(0.08)
    USER32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(0.03)
    USER32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.1)


def wheel_at(x: int, y: int, delta: int) -> None:
    USER32.SetCursorPos(int(x), int(y))
    time.sleep(0.05)
    USER32.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, int(delta), 0)
    time.sleep(0.08)


def drag_path(start_x: int, start_y: int, end_x: int, end_y: int, steps: int = 14) -> None:
    USER32.SetCursorPos(int(start_x), int(start_y))
    time.sleep(0.08)
    USER32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    try:
        for index in range(1, steps + 1):
            t = float(index) / float(steps)
            next_x = int(start_x + (end_x - start_x) * t)
            next_y = int(start_y + (end_y - start_y) * t)
            USER32.SetCursorPos(next_x, next_y)
            time.sleep(0.04)
    finally:
        USER32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.12)


def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    entries: list[dict] = []
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        raw = raw.strip()
        if not raw:
            continue
        try:
            entries.append(json.loads(raw))
        except json.JSONDecodeError:
            continue
    return entries


def parent_folder_from_file_path(file_path: str) -> str:
    try:
        return str(Path(file_path).parent)
    except Exception:
        return ""


def summarize_entries(entries: list[dict], start_ms: int, end_ms: int) -> dict[str, object]:
    window_entries = [
        entry
        for entry in entries
        if start_ms <= int(entry.get("timestampMs", 0)) <= end_ms
    ]
    frame_spike_groups = classify_pointer_frame_spikes(window_entries)
    trusted_spikes = [float(item.get("value", 0.0) or 0.0) for item in frame_spike_groups["trusted"]]
    suspicious_spikes = frame_spike_groups["suspicious"]
    raw_frame_spikes = trusted_spikes + [float(item.get("value", 0.0) or 0.0) for item in suspicious_spikes]
    cover_summaries = [
        {
            "timestampMs": int(entry.get("timestampMs", 0)),
            "detail": str(entry.get("detail", "")),
        }
        for entry in window_entries
        if str(entry.get("category", "")) == "CoverWall"
    ]
    relevant_perf = [
        {
            "category": str(entry.get("category", "")),
            "detail": str(entry.get("detail", "")),
            "durationMs": round(float(entry.get("durationMs", 0.0) or 0.0), 3),
            "timestampMs": int(entry.get("timestampMs", 0)),
        }
        for entry in window_entries
        if int(entry.get("type", -1)) == 1
        and (
            str(entry.get("category", "")).startswith("RenderUI.Artwork")
            or str(entry.get("category", "")) in {"CoverWall", "Database.searchFiles", "Database.getCoverFacets"}
        )
    ]
    relevant_perf.sort(key=lambda item: float(item.get("durationMs", 0.0)), reverse=True)
    return {
        "entryCount": len(window_entries),
        "frameSpikeCount": len(trusted_spikes),
        "frameSpikeMaxMs": max(trusted_spikes) if trusted_spikes else 0.0,
        "frameSpikesMs": trusted_spikes,
        "rawFrameSpikeCount": len(raw_frame_spikes),
        "rawFrameSpikeMaxMs": max(raw_frame_spikes) if raw_frame_spikes else 0.0,
        "rawFrameSpikesMs": raw_frame_spikes,
        "suspiciousFrameSpikes": suspicious_spikes,
        "coverWallEventsTail": cover_summaries[-12:],
        "topPerf": relevant_perf[:12],
    }


def cover_wall_ready_from_state(state: dict) -> bool:
    cover_wall = dict(state.get("cover_wall", {}))
    return bool(cover_wall.get("loaded", False)) or int(cover_wall.get("items", 0) or 0) > 0


def cover_wall_visibly_ready_from_state(state: dict) -> bool:
    cover_wall = dict(state.get("cover_wall", {}))
    visible_tiles = int(cover_wall.get("visible_tiles", 0) or 0)
    visible_ready = int(cover_wall.get("visible_ready", 0) or 0)
    visible_live = int(cover_wall.get("visible_live_textures", 0) or 0)
    if visible_tiles <= 0:
        return False
    return visible_ready > 0 or visible_live > 0


def cover_wall_black_grid_free_from_state(state: dict) -> bool:
    cover_wall = dict(state.get("cover_wall", {}))
    visible_tiles = int(cover_wall.get("visible_tiles", 0) or 0)
    visible_placeholders = int(cover_wall.get("visible_placeholders", 0) or 0)
    return visible_tiles > 0 and visible_placeholders == 0


def cover_wall_priority_mode_from_state(state: dict) -> bool:
    return bool(state.get("cover_wall_priority_mode", False))


def cover_wall_debt_from_state(state: dict) -> dict[str, int]:
    cover_wall = dict(state.get("cover_wall", {}))
    fields = [
        "pending_cover_work",
        "hidden_pending_items",
        "pending_cover_keys",
        "pending_cover_queue",
        "pending_cover_uploads",
        "pending_cover_upload_keys",
        "failed_cover_keys",
    ]
    return {field: int(cover_wall.get(field, 0) or 0) for field in fields}


def compare_cover_wall_debt(before_state: dict, after_state: dict) -> dict[str, object]:
    before = cover_wall_debt_from_state(before_state)
    after = cover_wall_debt_from_state(after_state)
    return {
        "before": before,
        "after": after,
        "delta": {field: int(before.get(field, 0)) - int(after.get(field, 0)) for field in before},
    }


def count_state_settled_from_state(state: dict) -> bool:
    if bool(state.get("pending_count_follow_up", False)):
        return False
    result_count = int(state.get("result_count", 0) or 0)
    total_count = int(state.get("total_count", 0) or 0)
    if result_count <= 0:
        return total_count >= 0
    return total_count >= result_count


def wait_cover_wall_ready(driver: QaDriver, timeout: float) -> dict:
    deadline = time.time() + timeout
    last_state: dict = {}
    while time.time() < deadline:
        last_state = driver.get_state()
        cover_wall = dict(last_state.get("cover_wall", {}))
        if bool(cover_wall.get("loaded", False)) or int(cover_wall.get("items", 0) or 0) > 0:
            return last_state
        time.sleep(0.5)
    return last_state


def wait_cover_wall_visibly_ready(driver: QaDriver, timeout: float) -> dict:
    deadline = time.time() + timeout
    last_state: dict = {}
    while time.time() < deadline:
        last_state = driver.get_state()
        if cover_wall_visibly_ready_from_state(last_state):
            return last_state
        time.sleep(0.2)
    return last_state


def wait_cover_wall_black_grid_free(driver: QaDriver, timeout: float) -> dict:
    start = time.time()
    deadline = time.time() + timeout
    last_state: dict = {}
    while time.time() < deadline:
        last_state = driver.get_state()
        if cover_wall_black_grid_free_from_state(last_state):
            last_state["black_grid_free_wait_ms"] = int((time.time() - start) * 1000)
            last_state["black_grid_free_wait_ok"] = True
            return last_state
        time.sleep(0.25)
    last_state["black_grid_free_wait_ms"] = int((time.time() - start) * 1000)
    last_state["black_grid_free_wait_ok"] = False
    return last_state


def wait_cover_wall_probe_stable(
    driver: QaDriver,
    timeout: float,
    *,
    require_visible: bool,
    require_count_settled: bool,
) -> dict:
    deadline = time.time() + timeout
    last_state: dict = {}
    while time.time() < deadline:
        last_state = driver.get_state()
        if not cover_wall_ready_from_state(last_state):
            time.sleep(0.2)
            continue
        if require_visible and not cover_wall_visibly_ready_from_state(last_state):
            time.sleep(0.2)
            continue
        if require_count_settled and not count_state_settled_from_state(last_state):
            time.sleep(0.2)
            continue
        return last_state
    return last_state


def wait_for_results(driver: QaDriver, minimum_count: int, timeout: float) -> dict:
    deadline = time.time() + timeout
    last_state: dict = {}
    while time.time() < deadline:
        last_state = driver.get_state()
        if int(last_state.get("result_count", 0) or 0) >= minimum_count:
            return last_state
        time.sleep(0.3)
    return last_state


def filter_state_matches_scenario(
    state: dict,
    scenario: str,
    *,
    search_query: str,
) -> bool:
    if scenario == "keyword_search":
        return str(state.get("search_text", "")).strip().lower() == search_query.strip().lower()
    if scenario == "path_filter":
        folder_filter = dict(state.get("folder_filter", {}))
        return bool(folder_filter.get("active", False)) and bool(str(folder_filter.get("path", "")).strip())
    return True


def wait_filter_state_applied(
    driver: QaDriver,
    scenario: str,
    *,
    search_query: str,
    timeout: float,
) -> dict:
    deadline = time.time() + timeout
    last_state: dict = {}
    while time.time() < deadline:
        last_state = driver.get_state()
        if filter_state_matches_scenario(last_state, scenario, search_query=search_query):
            return last_state
        time.sleep(0.3)
    return last_state


def click_artwork_tab(hwnd: int) -> tuple[int, int]:
    left, top, right, bottom = get_client_rect_screen(hwnd)
    width = right - left
    height = bottom - top
    tab_x = left + int(width * 0.77)
    tab_y = top + int(height * 0.11)
    click_point(tab_x, tab_y)
    return tab_x, tab_y


def try_capture_window(hwnd: int, output_path: Path) -> dict[str, object]:
    try:
        return capture_window(hwnd, output_path, client_only=True)
    except ModuleNotFoundError as exc:
        return {
            "success": False,
            "path": str(output_path),
            "skipped": True,
            "reason": f"missing optional dependency: {exc}",
        }
    except Exception as exc:
        return {
            "success": False,
            "path": str(output_path),
            "skipped": True,
            "reason": str(exc),
        }


def probe_cover_wall(
    hwnd: int,
    diagnostics_path: Path,
    *,
    driver: QaDriver,
    mode: str,
) -> dict[str, object]:
    left, top, right, bottom = get_client_rect_screen(hwnd)
    width = right - left
    height = bottom - top
    cover_center_x = left + int(width * 0.78)
    cover_center_y = top + int(height * 0.48)
    scrollbar_x = left + int(width * 0.975)
    scrollbar_top_y = top + int(height * 0.36)
    scrollbar_bottom_y = top + int(height * 0.80)

    before_ms = int(time.time() * 1000)
    phase_windows: dict[str, dict[str, int]] = {}
    phase_states: dict[str, dict[str, object]] = {}

    if mode in {"wheel_only", "combined"}:
        wheel_start_ms = int(time.time() * 1000)
        for _ in range(5):
            wheel_at(cover_center_x, cover_center_y, -WHEEL_DELTA)
        for _ in range(3):
            wheel_at(cover_center_x, cover_center_y, WHEEL_DELTA)
        phase_windows["wheel"] = {"start": wheel_start_ms, "end": int(time.time() * 1000)}
        phase_states["afterWheel"] = driver.get_state()

    if mode in {"drag_only", "combined"}:
        drag_start_ms = int(time.time() * 1000)
        drag_path(scrollbar_x, scrollbar_top_y, scrollbar_x, scrollbar_bottom_y, steps=18)
        drag_path(scrollbar_x, scrollbar_bottom_y, scrollbar_x, scrollbar_top_y, steps=18)
        phase_windows["drag"] = {"start": drag_start_ms, "end": int(time.time() * 1000)}
        phase_states["afterDrag"] = driver.get_state()

    if mode == "deep_hold":
        deep_start_ms = int(time.time() * 1000)
        drag_path(scrollbar_x, scrollbar_top_y, scrollbar_x, scrollbar_bottom_y, steps=24)
        time.sleep(1.0)
        phase_windows["deepHold"] = {"start": deep_start_ms, "end": int(time.time() * 1000)}
        phase_states["afterDeepHold"] = driver.get_state()

    time.sleep(1.0)

    after_ms = int(time.time() * 1000)
    entries = load_jsonl(diagnostics_path)
    phase_diagnostics = {
        name: summarize_entries(entries, int(window["start"]), int(window["end"]))
        for name, window in phase_windows.items()
    }
    return {
        "mode": mode,
        "probeWindowMs": {"start": before_ms, "end": after_ms},
        "coverArea": {
            "centerX": cover_center_x,
            "centerY": cover_center_y,
            "scrollbarX": scrollbar_x,
            "scrollbarTopY": scrollbar_top_y,
            "scrollbarBottomY": scrollbar_bottom_y,
        },
        "phaseWindowsMs": phase_windows,
        "phaseStates": phase_states,
        "phaseDiagnostics": phase_diagnostics,
        "diagnostics": summarize_entries(entries, before_ms, after_ms),
    }


def apply_filter_scenario(driver: QaDriver,
                          scenario: str,
                          *,
                          search_query: str,
                          folder_filter_path: str,
                          folder_filter_row: int,
                          filter_ready_timeout: float) -> dict[str, object]:
    report: dict[str, object] = {"scenario": scenario}
    filter_start_ms = int(time.time() * 1000)

    if scenario == "keyword_search":
        report["searchApply"] = driver.search(search_query)
    elif scenario == "path_filter":
        resolved_folder = folder_filter_path
        if not resolved_folder and folder_filter_row >= 0:
            wait_for_results(driver, 1, timeout=15.0)
            row_info = driver.get_row(folder_filter_row)
            if not bool(row_info.get("ok", False)):
                raise RuntimeError(f"Could not resolve folder-filter row {folder_filter_row}: {row_info}")
            report["folderFilterSourceRow"] = row_info
            item = dict(row_info.get("item", {}))
            resolved_folder = parent_folder_from_file_path(str(item.get("path", "")))
        if not resolved_folder:
            raise RuntimeError("Path-filter probe requires --folder-filter-path or a resolvable --folder-filter-row")
        report["folderFilterResolvedPath"] = resolved_folder
        report["folderFilterSet"] = driver.set_folder_filter(resolved_folder, timeout=10.0)
    else:
        report["noop"] = True

    time.sleep(0.5)
    report["filterWaitReady"] = driver.wait_search_idle(timeout=filter_ready_timeout)
    report["stateAfterFilterIdle"] = driver.get_state()
    report["filterProbeReady"] = driver.wait_probe_ready(timeout=filter_ready_timeout)
    report["filterStateApplied"] = wait_filter_state_applied(
        driver,
        scenario,
        search_query=search_query,
        timeout=filter_ready_timeout,
    )
    report["stateAfterFilterCoverReady"] = wait_cover_wall_probe_stable(
        driver,
        timeout=min(max(filter_ready_timeout, 1.0), 8.0),
        require_visible=True,
        require_count_settled=True,
    )
    report["stateAfterFilter"] = driver.get_state()
    report["filterWindowMs"] = {
        "start": filter_start_ms,
        "end": int(time.time() * 1000),
    }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Probe cover-wall drag/scroll performance with diagnostics.")
    parser.add_argument("--exe-dir", default=str(REPO_ROOT / "build_dbg" / "Debug"))
    parser.add_argument("--db-path", required=True, help="Database path for the large-library probe.")
    parser.add_argument(
        "--auth-mode",
        choices=["harness_multi", "dev_bypass", "basic_login", "basic_login_dev"],
        default="harness_multi",
    )
    parser.add_argument("--login-account", default="qa")
    parser.add_argument("--login-password", default="qa")
    parser.add_argument("--wait-window-timeout", type=float, default=30.0)
    parser.add_argument("--wait-ready-timeout", type=float, default=120.0)
    parser.add_argument("--cover-settle-seconds", type=float, default=8.0)
    parser.add_argument("--post-probe-visible-timeout", type=float, default=3.0)
    parser.add_argument("--attach-pid", type=int, default=0, help="Attach to an already running Aris process instead of launching a fresh one.")
    parser.add_argument("--attach-hwnd", type=int, default=0, help="Attach to an already discovered Aris window handle when pid-based enumeration is unreliable.")
    parser.add_argument("--force-artwork-tab-click", action="store_true")
    parser.add_argument("--select-row", type=int, default=-1)
    parser.add_argument("--search-query", default="", help="Optional keyword query to apply before probing.")
    parser.add_argument("--folder-filter-path", default="", help="Optional folder path filter to apply before probing.")
    parser.add_argument("--folder-filter-row", type=int, default=-1, help="Resolve folder filter from the parent folder of a result row.")
    parser.add_argument("--filter-ready-timeout", type=float, default=20.0)
    parser.add_argument(
        "--probe-mode",
        choices=["combined", "wheel_only", "drag_only", "deep_hold"],
        default="combined",
        help="Choose whether the interaction probe uses both wheel+drag, wheel only, or scrollbar drag only.",
    )
    parser.add_argument("--frame-spike-fail-ms", type=float, default=250.0)
    parser.add_argument(
        "--require-cover-debt-progress",
        action="store_true",
        help="Fail when pending cover work does not decrease between cover settle and final state.",
    )
    parser.add_argument(
        "--cover-debt-progress-min",
        type=int,
        default=1,
        help="Minimum pending_cover_work decrease required when --require-cover-debt-progress is set.",
    )
    parser.add_argument(
        "--hidden-pending-progress-min",
        type=int,
        default=0,
        help="Optional minimum hidden_pending_items decrease required when hidden pending debt exists.",
    )
    parser.add_argument("--report-json", default=str(
        REPO_ROOT / ".openclaw" / "reports" / "qa_probe_cover_wall_perf" / time.strftime("%Y%m%d_%H%M%S") / "report.json"
    ))
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
            write_dev_mode_flag(exe_dir, "qa_probe_cover_wall_perf")
        else:
            if use_dev_mode:
                write_dev_mode_flag(exe_dir, "qa_probe_cover_wall_perf_login")
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
        "pid": attach_pid if attach_pid > 0 else int(proc.pid if proc else 0),
        "attachPid": attach_pid,
        "attachHwnd": attach_hwnd,
        "reportJson": str(report_path),
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
        hwnd = int(dict(window_result.get("window", {})).get("hwnd", 0))
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
        if login_report is not None:
            report["login"] = login_report
        report["readyState"] = ready_state
        report["probeReadyState"] = driver.wait_probe_ready(timeout=args.wait_ready_timeout)
        try:
            driver.focus_window(timeout=5.0)
        except QaDriverError:
            pass
        if int(args.select_row) >= 0:
            try:
                report["selectRow"] = driver.select_row(int(args.select_row), timeout=5.0)
            except QaDriverError as exc:
                report["selectRowError"] = str(exc)
        else:
            report["selectRow"] = {"skipped": True}

        report["initialState"] = driver.get_state()
        scenario = "global"
        if str(args.folder_filter_path) or int(args.folder_filter_row) >= 0:
            scenario = "path_filter"
        elif str(args.search_query):
            scenario = "keyword_search"
        report["scenario"] = scenario

        if scenario != "global":
            filter_phase_start_ms = int(time.time() * 1000)
            report["filterScenario"] = apply_filter_scenario(
                driver,
                scenario,
                search_query=str(args.search_query),
                folder_filter_path=str(args.folder_filter_path),
                folder_filter_row=int(args.folder_filter_row),
                filter_ready_timeout=float(args.filter_ready_timeout),
            )
        else:
            filter_phase_start_ms = 0

        report_dir = report_path.parent
        report_dir.mkdir(parents=True, exist_ok=True)
        report["initialScreenshot"] = try_capture_window(hwnd, report_dir / "window_before_cover_probe.png")

        initial_cover_state = dict(report.get("initialState", {}))
        filter_cover_state = dict(dict(report.get("filterScenario", {})).get("stateAfterFilter", {}))
        probe_entry_state = filter_cover_state or initial_cover_state
        cover_wall_priority_active = cover_wall_priority_mode_from_state(probe_entry_state)
        auto_click_artwork_tab = not cover_wall_priority_active and (
            not cover_wall_ready_from_state(probe_entry_state)
            or not cover_wall_visibly_ready_from_state(probe_entry_state)
        )
        if args.force_artwork_tab_click or auto_click_artwork_tab:
            report["artworkTabClick"] = {"point": click_artwork_tab(hwnd)}
            time.sleep(0.8)
            try:
                report["postArtworkProbeReady"] = driver.wait_probe_ready(timeout=args.wait_ready_timeout)
            except QaDriverError as exc:
                report["postArtworkProbeReadyError"] = str(exc)
        else:
            report["artworkTabClick"] = {"skipped": True}
        time.sleep(float(args.cover_settle_seconds))
        report["coverWallStateAfterTab"] = wait_cover_wall_probe_stable(
            driver,
            timeout=max(float(args.post_probe_visible_timeout), 1.0),
            require_visible=True,
            require_count_settled=False,
        )
        probe_start_ms = int(time.time() * 1000)
        startup_end_ms = filter_phase_start_ms if filter_phase_start_ms > 0 else probe_start_ms
        report["startupDiagnostics"] = summarize_entries(
            load_jsonl(diagnostics_path),
            launch_start_ms,
            startup_end_ms,
        )
        if "filterScenario" in report:
            filter_window = dict(report["filterScenario"]).get("filterWindowMs", {})
            report["filterDiagnostics"] = summarize_entries(
                load_jsonl(diagnostics_path),
                int(filter_window.get("start", probe_start_ms)),
                int(filter_window.get("end", probe_start_ms)),
            )
        report["coverProbe"] = probe_cover_wall(
            hwnd,
            diagnostics_path,
            driver=driver,
            mode=str(args.probe_mode),
        )
        report["postProbeVisibleWaitState"] = wait_cover_wall_black_grid_free(
            driver,
            timeout=float(args.post_probe_visible_timeout),
        )
        report["finalState"] = (
            report["postProbeVisibleWaitState"]
            if dict(report.get("postProbeVisibleWaitState", {}))
            else driver.get_state()
        )
        if bool(args.require_cover_debt_progress):
            report["coverDebtSettleSeconds"] = max(float(args.post_probe_visible_timeout), 0.0)
        report["coverDebtProgress"] = compare_cover_wall_debt(
            dict(dict(report.get("coverProbe", {})).get("phaseStates", {}).get("afterDeepHold", {}))
            or dict(report.get("coverWallStateAfterTab", {})),
            dict(report.get("finalState", {})),
        )
        report["finalScreenshot"] = try_capture_window(hwnd, report_dir / "window_after_cover_probe.png")

        startup_diagnostics = dict(report.get("startupDiagnostics", {}))
        diagnostics = dict(report.get("coverProbe", {})).get("diagnostics", {})
        startup_frame_spike_max_ms = float(startup_diagnostics.get("frameSpikeMaxMs", 0.0) or 0.0)
        max_spike_ms = float(dict(diagnostics).get("frameSpikeMaxMs", 0.0) or 0.0)
        cover_wall_ready = cover_wall_ready_from_state(dict(report.get("finalState", {})))
        cover_wall_visibly_ready = cover_wall_visibly_ready_from_state(dict(report.get("finalState", {})))
        assessment_failures: list[str] = []
        if startup_frame_spike_max_ms > float(args.frame_spike_fail_ms):
            assessment_failures.append(
                f"startup frame spike max {startup_frame_spike_max_ms:.3f}ms exceeded limit {float(args.frame_spike_fail_ms):.3f}ms"
            )
        if max_spike_ms > float(args.frame_spike_fail_ms):
            assessment_failures.append(
                f"cover probe frame spike max {max_spike_ms:.3f}ms exceeded limit {float(args.frame_spike_fail_ms):.3f}ms"
            )
        if not cover_wall_ready:
            assessment_failures.append("cover wall never reached a loaded/items-ready state during the probe")
        if cover_wall_ready and not cover_wall_visibly_ready:
            assessment_failures.append("cover wall reached loaded/items-ready state but never produced a visibly ready first-screen tile")
        final_cover_wall = dict(dict(report.get("finalState", {})).get("cover_wall", {}))
        final_visible_tiles = int(final_cover_wall.get("visible_tiles", 0) or 0)
        final_visible_placeholders = int(final_cover_wall.get("visible_placeholders", 0) or 0)
        if final_visible_tiles > 0 and final_visible_placeholders > 0:
            assessment_failures.append(
                f"cover wall still has visible placeholder black cells: {final_visible_placeholders}/{final_visible_tiles}"
            )
        cover_debt_progress = dict(report.get("coverDebtProgress", {}))
        cover_debt_before = dict(cover_debt_progress.get("before", {}))
        cover_debt_delta = dict(cover_debt_progress.get("delta", {}))
        pending_before = int(cover_debt_before.get("pending_cover_work", 0) or 0)
        pending_delta = int(cover_debt_delta.get("pending_cover_work", 0) or 0)
        hidden_before = int(cover_debt_before.get("hidden_pending_items", 0) or 0)
        hidden_delta = int(cover_debt_delta.get("hidden_pending_items", 0) or 0)
        if bool(args.require_cover_debt_progress) and pending_before > 0:
            min_delta = int(args.cover_debt_progress_min)
            if pending_delta < min_delta:
                assessment_failures.append(
                    f"cover pending debt did not decrease enough: delta={pending_delta}, required>={min_delta}, before={pending_before}"
                )
        hidden_min_delta = int(args.hidden_pending_progress_min)
        if hidden_min_delta > 0 and hidden_before > 0 and hidden_delta < hidden_min_delta:
            assessment_failures.append(
                f"hidden pending cover debt did not decrease enough: delta={hidden_delta}, required>={hidden_min_delta}, before={hidden_before}"
            )
        report["assessment"] = {
            "passed": not assessment_failures,
            "frameSpikeFailMs": float(args.frame_spike_fail_ms),
            "startupFrameSpikeMaxMs": startup_frame_spike_max_ms,
            "frameSpikeMaxMs": max_spike_ms,
            "coverWallReady": cover_wall_ready,
            "coverWallVisiblyReady": cover_wall_visibly_ready,
            "coverDebtProgressRequired": bool(args.require_cover_debt_progress),
            "coverDebtPendingDelta": pending_delta,
            "coverDebtHiddenPendingDelta": hidden_delta,
            "failures": assessment_failures,
        }
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        return 0 if bool(dict(report["assessment"]).get("passed")) else 2
    except Exception as exc:
        report["ok"] = False
        report["error"] = str(exc)
        try:
            report_dir = report_path.parent
            report_dir.mkdir(parents=True, exist_ok=True)
            window = dict(report.get("window", {})).get("window", {})
            hwnd = int(dict(window).get("hwnd", 0) or 0)
            if hwnd > 0:
                report["failureScreenshot"] = try_capture_window(hwnd, report_dir / "window_failure.png")
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
    finally:
        try:
            _send_vk(VK_ESCAPE)
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
