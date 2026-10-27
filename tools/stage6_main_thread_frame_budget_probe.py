from __future__ import annotations
import argparse, ctypes, json, math, os, shutil, subprocess, sys, time, wave
from array import array
from pathlib import Path
from ctypes import wintypes
from statistics import median
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".openclaw" / "harness"))
from qa_driver import QaDriver, QaDriverError  # noqa: E402
WHOLE_TIMEOUT = 120.0
CAPTURE_WAIT = 20.0
CLEANUP_RESERVE = 20.0
DEFAULT_EXE = (ROOT.parents[1] if ROOT.parent.name == ".worktrees" else ROOT) / ".local/stage6-entry-build-20260714/Debug/Aris.exe"
class ProbeFailure(RuntimeError):
    def __init__(self, reason, stage="driver"): super().__init__(reason); self.reason, self.stage = reason, stage
class EnvironmentGate(ProbeFailure):
    def __init__(self, reason, stage="preflight"): super().__init__(reason, stage)
def fixture_metadata():
    return {"rows": {"files": 1000, "format": "pcm_s16le", "sample_rate": 8000,
                     "channels": 1, "frames_per_file": 800},
            "playback": {"format": "pcm_s16le", "sample_rate": 48000,
                         "channels": 2, "frames": 480000, "seconds": 10}}
def _wav(path, channels, rate, samples):
    with wave.open(str(path), "wb") as out:
        out.setnchannels(channels); out.setsampwidth(2); out.setframerate(rate)
        out.writeframes(array("h", samples).tobytes())
def create_fixtures(root):
    rows = root / "rows"; rows.mkdir(parents=True)
    for i in range(1000):
        _wav(rows / f"frame_{i:04d}.wav", 1, 8000,
             (((j * 37 + i * 101) % 2000) - 1000 for j in range(800)))
    _wav(root / "playback.wav", 2, 48000,
         (v for n in range(480000) for v in (((n % 480) - 240) * 50, (240 - (n % 480)) * 50)))
    return fixture_metadata()
def launch_spec(exe, runtime):
    env = os.environ.copy(); env.update(AURA_RUNTIME_ROOT=str(runtime),
        AURA_DISABLE_SINGLE_INSTANCE="1", AURA_HARNESS_MULTI_INSTANCE="1",
        AURA_HARNESS_LANE_KIND="functional")
    return [str(exe)], Path(exe).parent, env
def parse_one(raw):
    try:
        decoder = json.JSONDecoder(parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
        value, end = decoder.raw_decode(raw.strip())
    except (json.JSONDecodeError, ValueError) as exc: raise ValueError("invalid JSON") from exc
    if raw.strip()[end:].strip() or not isinstance(value, dict): raise ValueError("not one JSON object")
    return value
def _number(value): return type(value) in (int, float) and math.isfinite(value)
def _close(a, b): return _number(a) and math.isclose(a, b, rel_tol=1e-7, abs_tol=1e-6)
def _workload_ready(state, ui=None):
    total, count = state.get("total_count"), state.get("result_count")
    ready = type(total) is int and total == 1000 and type(count) is int and count >= 120 and state.get("async_search_in_progress") is False
    if ui is None: return ready
    rows = ui.get("list", {}).get("visibleRows"); return ready and type(rows) is int and rows > 0 and count - rows >= 119
def _list_ready(value): return _workload_ready(value)
def _playback_ready(state, expected):
    current = state.get("current_file")
    if current is None: current = state.get("current_path")
    return (state.get("audio_loaded") is True and type(state.get("play_state")) is str
        and state["play_state"] in {"stopped", "paused"}
        and _number(state.get("audio_duration")) and state["audio_duration"] >= 9.9
        and state.get("waveform_detail_pending") is False and state.get("bg_cache_running") is False
        and isinstance(current, str) and
        os.path.normcase(os.path.abspath(current)) == os.path.normcase(os.path.abspath(expected)))
def _control_failure(payload, cmd):
    keys = {"schema", "cmd", "status", "reason", "stage"}
    if (not isinstance(payload, dict) or set(payload) != keys or type(payload.get("schema")) is not int
        or payload["schema"] != 1 or payload.get("cmd") != cmd
        or payload.get("status") not in {"environment_gate", "failed"}
        or any(type(payload.get(k)) is not str or not payload[k] for k in ("reason", "stage"))):
        raise ValueError("control failure schema")
    return failure(payload["status"], payload["reason"], payload["stage"])
def _prepare_failure(payload):
    if not isinstance(payload, dict) or "counts" not in payload:
        mapped = _control_failure(payload, "prepare_frame_budget_cold_mini")
        valid_stage = (payload["status"] == "failed" and payload["stage"] == "preflight") or (
            payload["status"] == "environment_gate" and payload["stage"] in {"preflight", "isolation"})
        if not valid_stage:
            raise ValueError("prepare control failure stage")
        return mapped
    keys = {"schema", "cmd", "status", "reason", "stage", "counts"}
    numeric = ("file_count", "expected_files", "mini_missing_before", "purged_rows",
        "mini_missing_after", "detail_missing_before", "detail_missing_after",
        "waveform_worker_pending", "backfill_in_flight", "backfill_mini_queued",
        "durable_pending", "waveform_memory_entries", "waveform_cache_entries", "ui_pending",
        "pending_set", "failed_count", "miss_count", "visible_count", "meter_pending_count")
    booleans = ("db_switch_in_progress", "pending_db_path_present", "db_identity_ok")
    counts = payload.get("counts")
    if (set(payload) != keys or type(payload.get("schema")) is not int or payload["schema"] != 1
        or payload.get("cmd") != "prepare_frame_budget_cold_mini" or payload.get("status") != "failed"
        or payload.get("stage") != "prepare" or type(payload.get("reason")) is not str or not payload["reason"]
        or not isinstance(counts, dict) or set(counts) != set(numeric) | set(booleans)
        or counts.get("expected_files") != 1000
        or any(type(counts[k]) is not int or counts[k] < (-1 if k == "purged_rows" else 0) for k in numeric)
        or any(type(counts[k]) is not bool for k in booleans)):
        raise ValueError("counted prepare failure schema")
    return failure("failed", payload["reason"], payload["stage"])
def validate_prepare_payload(payload, expected_runtime, expected_db):
    if not isinstance(payload, dict) or payload.get("status") != "prepared":
        return _prepare_failure(payload)
    keys = {"schema", "cmd", "status", "runtime_root", "db_path", "db_generation",
        "isolation_verified", "file_count", "purged_rows", "mini_missing_before",
        "mini_missing_after", "detail_missing_before", "detail_missing_after",
        "durable_pending", "backfill_in_flight", "backfill_mini_queued",
        "waveform_worker_pending", "waveform_memory_entries", "ui_pending",
        "idle_backfill_suppressed", "cold_mini_armed", "worker_start_phase"}
    zero = ("durable_pending", "backfill_in_flight", "backfill_mini_queued",
        "waveform_worker_pending", "waveform_memory_entries", "ui_pending")
    if (set(payload) != keys or type(payload.get("schema")) is not int or payload["schema"] != 1
        or payload.get("cmd") != "prepare_frame_budget_cold_mini"
        or type(payload.get("db_generation")) is not int or payload["db_generation"] <= 0
        or type(payload.get("file_count")) is not int or payload["file_count"] != 1000
        or type(payload.get("mini_missing_before")) is not int
        or not 0 <= payload["mini_missing_before"] <= payload["file_count"]
        or type(payload.get("purged_rows")) is not int or payload["purged_rows"] < 0
        or payload["mini_missing_before"] + payload["purged_rows"] != payload["file_count"]
        or type(payload.get("mini_missing_after")) is not int or payload["mini_missing_after"] != payload["file_count"]
        or any(type(payload.get(k)) is not int or payload[k] != 0 for k in zero)
        or any(type(payload.get(k)) is not int or payload[k] < 0 for k in ("detail_missing_before", "detail_missing_after"))
        or payload["detail_missing_before"] != payload["detail_missing_after"]
        or any(payload.get(k) is not True for k in ("isolation_verified", "idle_backfill_suppressed", "cold_mini_armed"))
        or type(payload.get("worker_start_phase")) is not str or payload["worker_start_phase"] != "busy_setup"
        or type(payload.get("runtime_root")) is not str or type(payload.get("db_path")) is not str):
        raise ValueError("prepared schema")
    runtime = os.path.normcase(os.path.abspath(str(expected_runtime)))
    db = os.path.normcase(os.path.abspath(str(expected_db)))
    if (os.path.normcase(os.path.abspath(payload["runtime_root"])) != runtime
        or os.path.normcase(os.path.abspath(payload["db_path"])) != db):
        raise ValueError("prepared path")
    try: descendant = os.path.commonpath((runtime, db)) == runtime and db != runtime
    except ValueError: descendant = False
    if not descendant: raise ValueError("prepared isolation")
    return None
def _environment(value, measured, allow_invalid_dpi=False):
    keys = {"window_visible", "window_focused", "window_iconic", "client_width", "client_height",
            "dpi_scale", "opengl", "vsync", "audio_device"}
    if not isinstance(value, dict) or set(value) != keys: raise ValueError("environment schema")
    if any(type(value[k]) is not bool for k in ("window_visible", "window_focused", "window_iconic", "opengl", "vsync", "audio_device")) or any(type(value[k]) is not int for k in ("client_width", "client_height")): raise ValueError("environment types")
    if measured and value != {"window_visible": True, "window_focused": True, "window_iconic": False,
            "client_width": 1280, "client_height": 720, "dpi_scale": value["dpi_scale"],
            "opengl": True, "vsync": True, "audio_device": True}: raise ValueError("environment invariant")
    if not _number(value["dpi_scale"]) or (value["dpi_scale"] <= 0 and not allow_invalid_dpi): raise ValueError("dpi")
def _environment_failure(value, start_environment):
    keys = {"expected", "observed", "failed_fields", "scenario", "phase",
            "warmup_progress", "sample_count", "capture_frame_index"}
    if not isinstance(value, dict) or set(value) != keys: raise ValueError("environment failure schema")
    expected, observed = value["expected"], value["observed"]
    _environment(expected, True); _environment(observed, False, True)
    if expected != start_environment: raise ValueError("environment failure expected")
    fields = []
    def add(failed, name):
        if failed: fields.append(name)
    add(not observed["window_visible"], "window_visible")
    add(not observed["window_focused"], "window_focused")
    add(observed["window_iconic"], "window_iconic")
    add(observed["client_width"] <= 0, "client_width_invalid")
    add(observed["client_width"] != expected["client_width"], "client_width_changed")
    add(observed["client_height"] <= 0, "client_height_invalid")
    add(observed["client_height"] != expected["client_height"], "client_height_changed")
    add(observed["dpi_scale"] <= 0, "dpi_scale_invalid")
    add(observed["dpi_scale"] != expected["dpi_scale"], "dpi_scale_changed")
    add(not observed["opengl"], "opengl"); add(not observed["vsync"], "vsync")
    add(not observed["audio_device"], "audio_device")
    if value["failed_fields"] != fields or not fields: raise ValueError("environment failed fields")
    if type(value["scenario"]) is not str or value["scenario"] not in {"idle", "busy_scroll_mini_playback"}: raise ValueError("environment scenario")
    if type(value["phase"]) is not str or value["phase"] not in {"awaiting", "warmup", "sampling"}: raise ValueError("environment phase")
    counts = tuple(value[k] for k in ("warmup_progress", "sample_count", "capture_frame_index"))
    if any(type(item) is not int for item in counts) or counts[2] <= 0: raise ValueError("environment progress types")
    warmup, samples, frame = counts
    valid_progress = ((value["phase"] == "awaiting" and warmup == 0 and samples == 0)
        or (value["phase"] == "warmup" and 0 <= warmup < 60 and samples == 0)
        or (value["phase"] == "sampling" and warmup == 60 and 0 <= samples < 240))
    if not valid_progress or frame < warmup + samples + 1: raise ValueError("environment progress")
def _metric(value):
    if not isinstance(value, dict) or set(value) != {"samples", "median_ms", "p95_ms", "max_ms"}: raise ValueError("metric schema")
    samples = value["samples"]
    if not isinstance(samples, list) or len(samples) != 240 or any(not _number(x) or x < 0 for x in samples): raise ValueError("samples")
    ordered = sorted(samples); expected = (median(samples), ordered[math.ceil(.95 * 240) - 1], ordered[-1])
    if not all(_close(value[key], item) for key, item in zip(("median_ms", "p95_ms", "max_ms"), expected)): raise ValueError("summary")
    return expected[1]
def validate_payload(payload):
    if not isinstance(payload, dict): raise ValueError("payload schema")
    common = {"ok", "cmd", "schema", "status", "metric", "environment"}
    status = payload.get("status")
    if payload.get("cmd") != "frame_budget_result" or type(payload.get("schema")) is not int or payload["schema"] != 1 or payload.get("metric") != "main_thread_frame_budget": raise ValueError("identity")
    if status != "measured":
        environment_lost = payload.get("reason") == "environment_lost"
        expected = common | {"reason", "stage"} | ({"environment_failure"} if environment_lost else set())
        if status not in {"environment_gate", "failed", "running"} or set(payload) != expected: raise ValueError("non-measured schema")
        if payload.get("ok") is not False or not all(isinstance(payload.get(k), str) and payload[k] for k in ("reason", "stage")): raise ValueError("non-measured detail")
        _environment(payload["environment"], False)
        if environment_lost:
            if status != "failed" or payload["stage"] != "frame": raise ValueError("environment failure identity")
            _environment_failure(payload["environment_failure"], payload["environment"])
        return
    expected = common | {"budget_ms", "scenarios", "distinctScrollPositions", "playbackFramesObserved",
        "miniRequestsSubmitted", "miniResultsConsumed", "busyIntentionalThrottleFrames",
        "internal_deadline_seconds", "external_timeout_seconds", "value_ms"}
    integers = ("budget_ms", "distinctScrollPositions", "playbackFramesObserved", "miniRequestsSubmitted",
        "miniResultsConsumed", "busyIntentionalThrottleFrames", "internal_deadline_seconds", "external_timeout_seconds")
    if set(payload) != expected or payload["ok"] is not True or any(type(payload[k]) is not int for k in integers): raise ValueError("measured schema")
    _environment(payload["environment"], True)
    scenarios = payload["scenarios"]
    if not isinstance(scenarios, dict) or set(scenarios) != {"idle", "busy_scroll_mini_playback"}: raise ValueError("scenarios")
    p95 = None
    for name, scenario in scenarios.items():
        keys = {"warmup_frames", "sample_count", "cpu_work_ms", "present_block_ms", "frame_interval_ms",
                "render_ui_frames", "gl_render_frames", "present_frames"}
        counts = ("sample_count", "render_ui_frames", "gl_render_frames", "present_frames")
        if not isinstance(scenario, dict) or set(scenario) != keys or any(type(scenario[k]) is not int or scenario[k] != 240 for k in counts) or type(scenario["warmup_frames"]) is not int or scenario["warmup_frames"] != 60: raise ValueError("scenario schema")
        cpu = _metric(scenario["cpu_work_ms"]); _metric(scenario["present_block_ms"]); _metric(scenario["frame_interval_ms"])
        if name == "busy_scroll_mini_playback": p95 = cpu
    if payload["budget_ms"] != 16 or payload["distinctScrollPositions"] < 120 or payload["playbackFramesObserved"] != 240 or payload["miniRequestsSubmitted"] <= 0 or payload["miniResultsConsumed"] <= 0 or payload["busyIntentionalThrottleFrames"] != 0: raise ValueError("busy proof")
    if payload["internal_deadline_seconds"] != 90 or payload["external_timeout_seconds"] != 120 or not _close(payload["value_ms"], p95): raise ValueError("scalar")
def exit_code(payload): return 0 if payload["status"] == "measured" else 2 if payload["status"] == "environment_gate" else 1
def failure(status, reason, stage):
    return {"ok": False, "cmd": "frame_budget_result", "schema": 1, "status": status,
        "metric": "main_thread_frame_budget", "environment": {"window_visible": False,
        "window_focused": False, "window_iconic": False, "client_width": 0, "client_height": 0,
        "dpi_scale": 1.0, "opengl": False, "vsync": False, "audio_device": False},
        "reason": reason, "stage": stage}
def _window(pid, prepare=False):
    user = ctypes.windll.user32; user.GetForegroundWindow.restype = ctypes.c_void_p; found = []
    @ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
    def visit(hwnd, _):
        owner = ctypes.c_ulong(); user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        length = user.GetWindowTextLengthW(hwnd); title = ctypes.create_unicode_buffer(length + 1); user.GetWindowTextW(hwnd, title, length + 1)
        if owner.value == pid and user.IsWindowVisible(hwnd) and "aris" in title.value.lower(): found.append(int(hwnd))
        return True
    user.EnumWindows(visit, 0)
    if not found: return None
    hwnd = ctypes.c_void_p(found[0]); rect = wintypes.RECT()
    if prepare:
        user.ShowWindow(hwnd, 9); style = user.GetWindowLongW(hwnd, -16); ex = user.GetWindowLongW(hwnd, -20)
        outer = wintypes.RECT(0, 0, 1280, 720); dpi = user.GetDpiForWindow(hwnd)
        adjust = getattr(user, "AdjustWindowRectExForDpi", None)
        (adjust(ctypes.byref(outer), style, False, ex, dpi) if adjust else user.AdjustWindowRectEx(ctypes.byref(outer), style, False, ex))
        user.SetWindowPos(hwnd, None, 0, 0, outer.right - outer.left, outer.bottom - outer.top, 0x0002 | 0x0004); user.SetForegroundWindow(hwnd); time.sleep(.3)
    user.GetClientRect(hwnd, ctypes.byref(rect))
    return {"hwnd": found[0], "width": rect.right, "height": rect.bottom,
            "visible": bool(user.IsWindowVisible(hwnd)), "focused": user.GetForegroundWindow() == found[0],
            "iconic": bool(user.IsIconic(hwnd)), "dpi": user.GetDpiForWindow(hwnd) / 96.0}
def _wait_window(process, deadline):
    while time.monotonic() < deadline:
        if process.poll() is not None: raise ProbeFailure("app_exited_before_window", "launch")
        state = _window(process.pid)
        if state: return state
        time.sleep(.2)
    raise EnvironmentGate("window_unavailable")
class InteractiveSession:
    def __init__(self, work, evidence, exe): self.work, self.evidence, self.exe = work, evidence, Path(exe); self.process = self.hwnd = None; self.controls = []
    def _cmd(self, driver, request, deadline, timeout=10):
        remaining = min(timeout, deadline - time.monotonic())
        if remaining <= 0: raise subprocess.TimeoutExpired([str(self.exe)], WHOLE_TIMEOUT)
        try: driver.send(request, timeout=remaining)
        except QaDriverError as exc:
            if time.monotonic() >= deadline: raise subprocess.TimeoutExpired([str(self.exe)], WHOLE_TIMEOUT) from exc
            raise RuntimeError(str(exc)) from exc
        raw = driver.result_path.read_text(encoding="utf-8"); response = parse_one(raw)
        self.controls.append({"request": request, "response": response}); (self.evidence / "control.json").write_text(json.dumps(self.controls, indent=2), encoding="utf-8")
        return response, raw
    def _until(self, driver, request, predicate, deadline):
        while time.monotonic() < deadline:
            try:
                response, _ = self._cmd(driver, request, deadline, 5)
            except RuntimeError as exc:
                if isinstance(exc.__cause__, QaDriverError):
                    time.sleep(.2)
                    continue
                raise
            if predicate(response): return response
            time.sleep(.2)
        raise subprocess.TimeoutExpired([str(self.exe)], WHOLE_TIMEOUT)
    def run(self, whole_deadline):
        deadline = whole_deadline - CLEANUP_RESERVE; fixture = self.work / "fixture"; runtime = self.work / "runtime"
        try:
            runtime.mkdir(parents=True); create_fixtures(fixture); db = runtime / "database/frame-budget.db"; db.parent.mkdir(); db.touch()
            (runtime / "settings_dev.ini").write_text(f"diagnosticMode=1\nshowDiagnosticOverlay=0\nautoplayOnSelect=0\nfpsLimit=60\ncloseButtonAction=2\ncurrentDbPath={db}\nrecentDbCount=1\nrecentDb0={db}\n", encoding="utf-8")
            self.stdout = open(self.evidence / "app.stdout.log", "wb"); self.stderr = open(self.evidence / "app.stderr.log", "wb")
        except OSError as exc: raise EnvironmentGate("temp_policy", "fixture_setup") from exc
        marker = self.exe.parent / "dev_mode.txt"; self.marker, self.marker_before = marker, marker.read_bytes() if marker.exists() else None
        try: marker.write_text("frame-budget-probe\n", encoding="utf-8")
        except OSError as exc: raise ProbeFailure("app_launch_failed", "launch") from exc
        args, cwd, env = launch_spec(self.exe, runtime)
        try: self.process = subprocess.Popen(args, cwd=cwd, env=env, stdout=self.stdout, stderr=self.stderr)
        except OSError as exc: raise ProbeFailure("app_launch_failed", "launch") from exc
        initial_window = _wait_window(self.process, deadline)
        self.hwnd = initial_window.get("hwnd") if isinstance(initial_window, dict) else None
        if not self.hwnd: raise ProbeFailure("window_handle_missing", "launch")
        driver = QaDriver(runtime)
        until = lambda request, predicate: self._until(driver, request, predicate, deadline)
        until({"cmd": "wait_ready"}, lambda x: x.get("ready") is True)
        started, _ = self._cmd(driver, {"cmd": "scan_folder", "path": str(fixture / "rows"), "recursive": True, "update_existing": True, "remove_missing": False, "cache_waveforms": False, "worker_threads": 2}, deadline, 30)
        if not started.get("started"): raise RuntimeError("scan_start_failed")
        scan = until({"cmd": "scan_status"}, lambda x: not x.get("scanning", True))
        if (scan.get("total"), scan.get("added"), scan.get("errors")) != (1000, 1000, 0): raise RuntimeError("scan_invariant")
        self._cmd(driver, {"cmd": "search", "query": "frame_"}, deadline)
        until({"cmd": "get_state"}, _workload_ready)
        until({"cmd": "wait_probe_ready"}, lambda x: x.get("ready") is True)
        prepare_request = {"cmd": "prepare_frame_budget_cold_mini",
            "expected_runtime_root": str(runtime), "expected_db_path": str(db), "expected_files": 1000}
        prepare, raw = self._cmd(driver, prepare_request, deadline)
        (self.evidence / "prepare-result.json").write_text(raw, encoding="utf-8")
        prepare_failure = validate_prepare_payload(prepare, runtime, db)
        if prepare_failure: return prepare_failure
        self._cmd(driver, {"cmd": "load_path", "path": str(fixture / "playback.wav")}, deadline)
        playback = fixture / "playback.wav"
        state = until({"cmd": "get_state"}, lambda x: _playback_ready(x, playback))
        ui, _ = self._cmd(driver, {"cmd": "get_ui_regions"}, deadline)
        if not _workload_ready(state, ui): raise ProbeFailure("list_invariant", "preflight")
        self._cmd(driver, {"cmd": "focus_window"}, deadline); window = _window(self.process.pid, True)
        if not window or (window["width"], window["height"], window["visible"], window["focused"], window["iconic"]) != (1280, 720, True, True, False): raise EnvironmentGate("window_invariant")
        self.hwnd = window["hwnd"]
        start, _ = self._cmd(driver, {"cmd": "start_frame_budget_capture"}, deadline)
        if start.get("status") != "started":
            if start.get("cmd") == "frame_budget_result": validate_payload(start); return start
            return _control_failure(start, "start_frame_budget_capture")
        self.capture_started = True
        if deadline - time.monotonic() <= CAPTURE_WAIT: raise subprocess.TimeoutExpired(args, WHOLE_TIMEOUT)
        time.sleep(CAPTURE_WAIT)
        payload, raw = self._cmd(driver, {"cmd": "frame_budget_result"}, deadline, deadline - time.monotonic())
        (self.evidence / "app-result.json").write_text(raw, encoding="utf-8"); validate_payload(payload); return payload
    def close(self, deadline):
        clean = not self.process or self.process.poll() in (None, 0)
        if self.process and self.process.poll() is None:
            if self.hwnd: ctypes.windll.user32.PostMessageW(ctypes.c_void_p(self.hwnd), 0x0010, 0, 0)
            remaining = max(0.0, deadline - time.monotonic())
            try:
                self.process.wait(timeout=min(10.0, remaining / 2.0)); clean = self.process.returncode == 0
            except subprocess.TimeoutExpired:
                self.process.kill(); clean = False
                try: self.process.wait(timeout=max(0.0, deadline - time.monotonic()))
                except subprocess.TimeoutExpired: pass
        for stream in (getattr(self, "stdout", None), getattr(self, "stderr", None)):
            if stream: stream.close()
        marker = getattr(self, "marker", None)
        if marker:
            if self.marker_before is None: marker.unlink(missing_ok=True)
            else: marker.write_bytes(self.marker_before)
        return {"app_terminated": not self.process or self.process.poll() is not None,
                "clean_app_exit": clean, "marker_restored": not marker or marker.exists() == (self.marker_before is not None),
                "deadline_exceeded": time.monotonic() > deadline}
def run_probe(evidence, exe=DEFAULT_EXE, session_factory=InteractiveSession, clock=time.monotonic):
    started_at = clock(); whole_deadline = started_at + WHOLE_TIMEOUT; evidence, exe = Path(evidence), Path(exe)
    if not exe.is_file(): return 1, failure("failed", "missing_exe", "launch")
    try:
        if evidence.exists() and (not evidence.is_dir() or any(evidence.iterdir())): return 2, failure("environment_gate", "evidence_exists", "preflight")
        evidence.mkdir(parents=True, exist_ok=True); work = evidence / "work"; work.mkdir()
    except OSError: return 2, failure("environment_gate", "temp_policy", "evidence_setup")
    session = session_factory(work, evidence, exe); payload = None
    try: payload = session.run(whole_deadline); code = exit_code(payload)
    except EnvironmentGate as exc: payload, code = failure("environment_gate", exc.reason, exc.stage), 2
    except ProbeFailure as exc: payload, code = failure("failed", exc.reason, exc.stage), 1
    except subprocess.TimeoutExpired: payload, code = failure("failed", "subprocess_timeout", "driver"), 1
    except OSError as exc: payload, code = failure("failed", "io_failure", "driver"), 1; (evidence / "driver.stderr.log").write_text(str(exc), encoding="utf-8")
    except Exception as exc: payload, code = failure("failed", "probe_error", "driver"), 1; (evidence / "driver.stderr.log").write_text(str(exc), encoding="utf-8")
    try: cleanup = session.close(whole_deadline)
    except Exception as exc: cleanup = {"app_terminated": False, "clean_app_exit": False, "marker_restored": False}; (evidence / "driver.stderr.log").write_text(f"cleanup: {exc}", encoding="utf-8")
    cleanup["deadline_exceeded"] = cleanup.get("deadline_exceeded", False) or clock() > whole_deadline; removal_started = clock()
    try: shutil.rmtree(work); cleanup["work_removed"] = True
    except OSError: cleanup["work_removed"] = False
    finished_at = clock(); cleanup["removal_elapsed_seconds"] = max(0.0, finished_at - removal_started); cleanup["deadline_exceeded"] |= finished_at > whole_deadline
    cleanup_failed = (not cleanup.get("app_terminated") or not cleanup.get("clean_app_exit") or
        not cleanup.get("marker_restored") or not cleanup.get("work_removed") or cleanup["deadline_exceeded"])
    if cleanup_failed:
        code = 1
        if payload["status"] == "environment_gate": payload = {**payload, "status": "failed"}
        elif payload["status"] != "failed": payload = failure("failed", "cleanup_failed", "cleanup")
    text = json.dumps(payload, separators=(",", ":"), allow_nan=False); (evidence / "result.json").write_text(text, encoding="utf-8"); (evidence / "driver.stdout.json").write_text(text, encoding="utf-8")
    index = {"schema": 1, "status": payload["status"], "exit_code": code, "rounds": 1,
             "fixture": fixture_metadata(), "whole_timeout_seconds": 120, "elapsed_seconds": max(0.0, finished_at - started_at), "cleanup": cleanup,
             "paths": {name: str((evidence / name).resolve()) for name in ("app.stdout.log", "app.stderr.log", "control.json", "prepare-result.json", "app-result.json", "result.json") if (evidence / name).exists()}}
    (evidence / "run-index.json").write_text(json.dumps(index, indent=2), encoding="utf-8"); return code, payload
def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--evidence-root", required=True); parser.add_argument("--exe", default=str(DEFAULT_EXE)); args = parser.parse_args()
    code, payload = run_probe(args.evidence_root, args.exe); print(json.dumps(payload, separators=(",", ":"), allow_nan=False)); return code
if __name__ == "__main__": raise SystemExit(main())
