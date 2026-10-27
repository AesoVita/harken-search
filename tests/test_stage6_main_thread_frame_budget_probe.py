import importlib.util
import json
import math
import subprocess
import tempfile
import unittest
import wave
from copy import deepcopy
from pathlib import Path
from unittest import mock
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/stage6_main_thread_frame_budget_probe.py"
spec = importlib.util.spec_from_file_location("frame_probe", SOURCE)
probe = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(probe)
def metric(values):
    ordered = sorted(values)
    return {"samples": values, "median_ms": (ordered[119] + ordered[120]) / 2,
            "p95_ms": ordered[math.ceil(.95 * len(ordered)) - 1], "max_ms": ordered[-1]}
def measured():
    values = [float(i) / 100 for i in range(240)]
    scenario = {"warmup_frames": 60, "sample_count": 240,
                "cpu_work_ms": metric(values), "present_block_ms": metric(values),
                "frame_interval_ms": metric(values), "render_ui_frames": 240,
                "gl_render_frames": 240, "present_frames": 240}
    return {"ok": True, "cmd": "frame_budget_result", "schema": 1, "status": "measured",
            "metric": "main_thread_frame_budget", "environment": {"window_visible": True,
            "window_focused": True, "window_iconic": False, "client_width": 1280,
            "client_height": 720, "dpi_scale": 1.0, "opengl": True, "vsync": True,
            "audio_device": True}, "budget_ms": 16, "scenarios": {"idle": deepcopy(scenario),
            "busy_scroll_mini_playback": deepcopy(scenario)}, "distinctScrollPositions": 120,
            "playbackFramesObserved": 240, "miniRequestsSubmitted": 1,
            "miniResultsConsumed": 1, "busyIntentionalThrottleFrames": 0,
            "internal_deadline_seconds": 90, "external_timeout_seconds": 120,
            "value_ms": scenario["cpu_work_ms"]["p95_ms"]}
def prepared(runtime, db, mini_before=0, purged=1000):
    return {"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "prepared",
            "runtime_root": str(runtime), "db_path": str(db), "db_generation": 1,
            "isolation_verified": True, "file_count": 1000, "purged_rows": purged,
            "mini_missing_before": mini_before, "mini_missing_after": 1000,
            "detail_missing_before": 1000, "detail_missing_after": 1000,
            "durable_pending": 0, "backfill_in_flight": 0, "backfill_mini_queued": 0,
            "waveform_worker_pending": 0, "waveform_memory_entries": 0, "ui_pending": 0,
            "idle_backfill_suppressed": True, "cold_mini_armed": True,
            "worker_start_phase": "busy_setup"}
def counted_prepare_failure():
    counts = {key: 0 for key in ("file_count", "expected_files", "mini_missing_before",
        "purged_rows", "mini_missing_after", "detail_missing_before", "detail_missing_after",
        "waveform_worker_pending", "backfill_in_flight", "backfill_mini_queued",
        "durable_pending", "waveform_memory_entries", "waveform_cache_entries", "ui_pending",
        "pending_set", "failed_count", "miss_count", "visible_count", "meter_pending_count")}
    counts.update({"file_count": 0, "expected_files": 1000, "purged_rows": -1,
                   "db_switch_in_progress": True,
                   "pending_db_path_present": True, "db_identity_ok": False})
    return {"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "failed",
            "reason": "cold_mini_db_identity_lost", "stage": "prepare", "counts": counts}
def environment_loss(observed=None, failed_fields=None):
    expected = deepcopy(measured()["environment"])
    if observed is None:
        observed = {**expected, "window_focused": False}
    if failed_fields is None:
        failed_fields = ["window_focused"]
    return {"ok": False, "cmd": "frame_budget_result", "schema": 1, "status": "failed",
            "metric": "main_thread_frame_budget", "environment": expected,
            "reason": "environment_lost", "stage": "frame", "environment_failure": {
                "expected": deepcopy(expected), "observed": observed,
                "failed_fields": failed_fields, "scenario": "idle", "phase": "warmup",
                "warmup_progress": 12, "sample_count": 0, "capture_frame_index": 13}}
class FrameBudgetProbeTest(unittest.TestCase):
    def test_launch_and_deterministic_fixtures(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); exe = root / "Debug" / "Aris.exe"; exe.parent.mkdir(); exe.touch()
            args, cwd, env = probe.launch_spec(exe, root / "runtime")
            self.assertEqual(args, [str(exe)])
            self.assertEqual(cwd, exe.parent)
            self.assertEqual(env["AURA_RUNTIME_ROOT"], str(root / "runtime"))
            self.assertEqual(env["AURA_DISABLE_SINGLE_INSTANCE"], "1")
            self.assertTrue(probe._list_ready({"total_count": 1000, "result_count": 500, "async_search_in_progress": False})); self.assertFalse(probe._list_ready({"total_count": 999, "result_count": 500, "async_search_in_progress": False}))
            meta = probe.create_fixtures(root / "fixture")
            self.assertEqual(meta, probe.fixture_metadata())
            self.assertEqual(len(list((root / "fixture" / "rows").glob("*.wav"))), 1000)
            with wave.open(str(root / "fixture" / "playback.wav"), "rb") as wav:
                self.assertEqual((wav.getnchannels(), wav.getframerate(), wav.getnframes()), (2, 48000, 480000))
    def test_playback_ready_accepts_only_loaded_matching_nonplaying_state(self):
        expected = Path("fixture/playback.wav").resolve()
        ready = {"audio_loaded": True, "play_state": "stopped", "audio_duration": 10.0,
                 "waveform_detail_pending": False, "bg_cache_running": False,
                 "current_file": str(expected)}
        self.assertTrue(probe._playback_ready(ready, expected))
        self.assertTrue(probe._playback_ready({**ready, "play_state": "paused", "current_file": None,
                                              "current_path": str(expected)}, expected))
        bad = [{**ready, **change} for change in (
            {"play_state": "playing"}, {"play_state": "unknown"}, {"play_state": ["paused"]},
            {"current_file": str(expected.with_name("other.wav"))}, {"current_file": None},
            {"audio_loaded": False}, {"audio_duration": 9.8}, {"audio_duration": float("nan")},
            {"waveform_detail_pending": True}, {"bg_cache_running": True})]
        for state in bad:
            with self.subTest(state=state): self.assertFalse(probe._playback_ready(state, expected))
    def test_strict_measured_validation_and_rejections(self):
        payload = measured(); probe.validate_payload(payload)
        bad = []
        p = deepcopy(payload); p["scenarios"]["idle"]["cpu_work_ms"]["samples"].pop(); bad.append(p)
        p = deepcopy(payload); p["scenarios"]["idle"]["cpu_work_ms"]["p95_ms"] += 1; bad.append(p)
        p = deepcopy(payload); p["scenarios"]["busy_scroll_mini_playback"]["present_frames"] = 239; bad.append(p)
        p = deepcopy(payload); p["value_ms"] += 1; bad.append(p)
        p = deepcopy(payload); p["environment"]["vsync"] = False; bad.append(p)
        p = deepcopy(payload); p["environment"]["client_width"] = 1280.0; bad.append(p)
        for item in bad:
            with self.subTest(item=item):
                with self.assertRaises(ValueError): probe.validate_payload(item)
        with self.assertRaises(ValueError): probe.parse_one('{} {}')
        with self.assertRaises(ValueError): probe.parse_one('{"x":NaN}')
        integer_fields = ("budget_ms", "distinctScrollPositions", "playbackFramesObserved",
            "miniRequestsSubmitted", "miniResultsConsumed", "busyIntentionalThrottleFrames",
            "internal_deadline_seconds", "external_timeout_seconds")
        for key in integer_fields:
            for lookalike in (float(payload[key]), True):
                item = deepcopy(payload); item[key] = lookalike
                with self.subTest(key=key, lookalike=lookalike), self.assertRaises(ValueError): probe.validate_payload(item)
    def test_strict_prepare_validation_and_rejections(self):
        self.assertTrue(hasattr(probe, "validate_prepare_payload"), "missing prepare validator")
        with tempfile.TemporaryDirectory() as td:
            runtime = Path(td) / "runtime"; db = runtime / "database/frame-budget.db"
            payload = prepared(runtime, db)
            for before, purged in ((0, 1000), (750, 250), (1000, 0)):
                self.assertIsNone(probe.validate_prepare_payload(
                    prepared(runtime, db, before, purged), runtime, db))
            bad = []
            for key, value in (("schema", True), ("db_generation", 0), ("db_generation", 1.0),
                               ("file_count", 1000.0), ("purged_rows", True),
                               ("mini_missing_before", 1), ("mini_missing_after", 999),
                               ("detail_missing_after", 999), ("durable_pending", 1),
                               ("backfill_in_flight", 1), ("backfill_mini_queued", 1),
                               ("waveform_worker_pending", 1), ("waveform_memory_entries", 1),
                               ("ui_pending", 1), ("isolation_verified", False),
                               ("idle_backfill_suppressed", False), ("cold_mini_armed", False),
                               ("worker_start_phase", "idle")):
                item = deepcopy(payload); item[key] = value; bad.append(item)
            for key, value in (("mini_missing_before", -1), ("mini_missing_before", 1001),
                               ("mini_missing_before", 750.0), ("mini_missing_before", True),
                               ("purged_rows", -1), ("purged_rows", 250.0),
                               ("mini_missing_after", True)):
                item = prepared(runtime, db, 750, 250); item[key] = value; bad.append(item)
            item = deepcopy(payload); item["runtime_root"] = str(runtime / "wrong"); bad.append(item)
            item = deepcopy(payload); item["db_path"] = str(db.with_name("wrong.db")); bad.append(item)
            item = deepcopy(payload); item["extra"] = 1; bad.append(item)
            item = deepcopy(payload); item.pop("purged_rows"); bad.append(item)
            for item in bad:
                with self.subTest(item=item), self.assertRaises(ValueError):
                    probe.validate_prepare_payload(item, runtime, db)
            outside = runtime.parent / "outside.db"
            with self.assertRaises(ValueError):
                probe.validate_prepare_payload(prepared(runtime, outside), runtime, outside)
    def test_prepare_nonmeasured_mapping_and_schema(self):
        with tempfile.TemporaryDirectory() as td:
            runtime = Path(td) / "runtime"; db = runtime / "database/frame-budget.db"
            for status, stage, code in (("environment_gate", "isolation", 2),
                                        ("failed", "preflight", 1)):
                raw = {"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": status,
                       "reason": "host_policy", "stage": stage}
                mapped = probe.validate_prepare_payload(raw, runtime, db)
                self.assertEqual((mapped["status"], mapped["reason"], probe.exit_code(mapped)),
                                 (status, "host_policy", code))
            counted = counted_prepare_failure()
            mapped = probe.validate_prepare_payload(counted, runtime, db)
            self.assertEqual((mapped["status"], mapped["reason"], mapped["stage"], probe.exit_code(mapped)),
                             ("failed", "cold_mini_db_identity_lost", "prepare", 1))
            self.assertNotIn("counts", mapped)
            bad_counted = []
            numeric = tuple(key for key, value in counted["counts"].items() if type(value) is int)
            booleans = tuple(key for key, value in counted["counts"].items() if type(value) is bool)
            for key in numeric:
                for value in (float(counted["counts"][key]), True):
                    item = deepcopy(counted); item["counts"][key] = value; bad_counted.append(item)
            for key in booleans:
                item = deepcopy(counted); item["counts"][key] = 1; bad_counted.append(item)
            for key, value in (("waveform_worker_pending", -1), ("db_identity_ok", "false")):
                item = deepcopy(counted); item["counts"][key] = value; bad_counted.append(item)
            item = deepcopy(counted); item["counts"]["expected_files"] = 999; bad_counted.append(item)
            item = deepcopy(counted); item["counts"].pop("ui_pending"); bad_counted.append(item)
            item = deepcopy(counted); item["counts"]["extra"] = 0; bad_counted.append(item)
            for key in ("scenarios", "value_ms", "samples"):
                item = deepcopy(counted); item[key] = {}; bad_counted.append(item)
            item = deepcopy(counted); item["status"] = "environment_gate"; bad_counted.append(item)
            item = {"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "environment_gate",
                    "reason": "host_policy", "stage": "prepare"}; bad_counted.append(item)
            for raw in bad_counted:
                with self.subTest(raw=raw), self.assertRaises(ValueError):
                    probe.validate_prepare_payload(raw, runtime, db)
            for raw in ({"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "running",
                         "reason": "x", "stage": "y"},
                        {"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "failed",
                         "reason": "", "stage": "preflight"}):
                with self.assertRaises(ValueError): probe.validate_prepare_payload(raw, runtime, db)
    def test_prepare_failure_stage_tightening(self):
        with tempfile.TemporaryDirectory() as td:
            runtime = Path(td) / "runtime"; db = runtime / "database/frame-budget.db"
            failed = {"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "failed",
                      "reason": "fixed_failure", "stage": "prepare"}
            with self.assertRaises(ValueError): probe.validate_prepare_payload(failed, runtime, db)
    def test_counted_failure_file_count_is_nonnegative(self):
        with tempfile.TemporaryDirectory() as td:
            runtime = Path(td) / "runtime"; db = runtime / "database/frame-budget.db"
            counted = counted_prepare_failure(); counted["counts"]["file_count"] = -1
            with self.assertRaises(ValueError): probe.validate_prepare_payload(counted, runtime, db)
    def test_nonmeasured_exit_and_omission_contract(self):
        base = {"ok": False, "cmd": "frame_budget_result", "schema": 1,
                "metric": "main_thread_frame_budget", "environment": measured()["environment"],
                "reason": "host_policy", "stage": "preflight"}
        for status, code in (("environment_gate", 2), ("failed", 1), ("running", 1)):
            item = {**base, "status": status}
            probe.validate_payload(item)
            self.assertEqual(probe.exit_code(item), code)
            self.assertNotIn("scenarios", item); self.assertNotIn("value_ms", item)
        self.assertEqual(probe.exit_code(measured()), 0)
    def test_environment_loss_failure_schema(self):
        payload = environment_loss()
        probe.validate_payload(payload)
        sampling = deepcopy(payload); sampling["environment_failure"].update(
            phase="sampling", warmup_progress=60, sample_count=1, capture_frame_index=62)
        probe.validate_payload(sampling)
        awaiting = deepcopy(payload); awaiting["environment_failure"].update(
            scenario="busy_scroll_mini_playback", phase="awaiting", warmup_progress=0,
            sample_count=0, capture_frame_index=301)
        probe.validate_payload(awaiting)
        old = deepcopy(payload); old.pop("environment_failure")
        with self.assertRaises(ValueError): probe.validate_payload(old)
        expected = payload["environment_failure"]["expected"]
        observed = {**expected, "window_visible": False, "window_focused": False,
                    "client_width": 0, "client_height": 719, "dpi_scale": 0.0,
                    "opengl": False, "vsync": False, "audio_device": False}
        fields = ["window_visible", "window_focused", "client_width_invalid",
                  "client_width_changed", "client_height_changed", "dpi_scale_invalid",
                  "dpi_scale_changed", "opengl", "vsync", "audio_device"]
        probe.validate_payload(environment_loss(observed, fields))
        bad = []
        item = deepcopy(payload); item["environment_failure"]["extra"] = 1; bad.append(item)
        item = deepcopy(payload); item["environment_failure"].pop("observed"); bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["failed_fields"] = ["unknown"]; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["failed_fields"] *= 2; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["failed_fields"] = ["vsync", "window_focused"]; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["failed_fields"] = ["vsync"]; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["expected"]["vsync"] = False; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["observed"]["client_width"] = 1280.0; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["observed"]["dpi_scale"] = float("nan"); bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["scenario"] = "unknown"; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["phase"] = "capture"; bad.append(item)
        for key in ("warmup_progress", "sample_count", "capture_frame_index"):
            for value in (True, float(payload["environment_failure"][key])):
                item = deepcopy(payload); item["environment_failure"][key] = value; bad.append(item)
        item = deepcopy(payload); item["environment_failure"]["warmup_progress"] = 60; bad.append(item)
        for key in ("value_ms", "scenarios", "budget_ms"):
            item = deepcopy(payload); item[key] = {} if key == "scenarios" else 1.0; bad.append(item)
        item = deepcopy(payload); item["schema"] = True; bad.append(item)
        item = deepcopy(payload); item["status"] = "environment_gate"; bad.append(item)
        item = deepcopy(payload); item["stage"] = "preflight"; bad.append(item)
        for item in bad:
            with self.subTest(item=item), self.assertRaises(ValueError):
                probe.validate_payload(item)
    def test_timeout_is_caught_and_cleanup_evidence_retained(self):
        made = []
        class FakeSession:
            def __init__(self, work, evidence, exe):
                self.work = work; self.evidence = evidence; made.append(self)
                (evidence / "app.stdout.log").write_text("out", encoding="utf-8")
                (evidence / "app.stderr.log").write_text("err", encoding="utf-8")
            def run(self, deadline): raise subprocess.TimeoutExpired(["Aris.exe"], 120)
            def close(self, deadline): self.closed = True; self.close_deadline = deadline; return {"app_terminated": True, "clean_app_exit": True, "marker_restored": True}
        with tempfile.TemporaryDirectory() as td:
            base = Path(td); exe = base / "Aris.exe"; exe.touch(); evidence = base / "evidence"
            code, payload = probe.run_probe(evidence, exe, session_factory=FakeSession)
            self.assertEqual((code, payload["status"], payload["reason"]), (1, "failed", "subprocess_timeout"))
            self.assertTrue(made[0].closed); self.assertFalse((evidence / "work").exists())
            self.assertTrue(all((evidence / name).is_file() for name in ("run-index.json", "result.json", "app.stdout.log", "app.stderr.log")))
            index = json.loads((evidence / "run-index.json").read_text())
            self.assertTrue(all(Path(path).is_relative_to(evidence) for path in index["paths"].values()))
            self.assertNotIn("app-result.json", index["paths"])
            self.assertNotIn("prepare-result.json", index["paths"])
            self.assertEqual((evidence / "app.stdout.log").read_text(), "out")
            class PreparedSession(FakeSession):
                def run(self, deadline):
                    (self.evidence / "prepare-result.json").write_text("{}", encoding="utf-8")
                    return probe.failure("failed", "cold_mini_not_armed", "preflight")
            evidence2 = base / "prepared-evidence"
            probe.run_probe(evidence2, exe, session_factory=PreparedSession)
            self.assertIn("prepare-result.json", json.loads((evidence2 / "run-index.json").read_text())["paths"])
    def test_paginated_workload_predicate(self):
        state = {"total_count": 1000, "result_count": 500, "async_search_in_progress": False}
        self.assertTrue(probe._workload_ready(state, {"list": {"visibleRows": 20}}))
        for changed in ({**state, "total_count": 1000.0}, {**state, "result_count": 119}, state):
            ui = {"list": {"visibleRows": 20 if changed is not state else 382}}
            self.assertFalse(probe._workload_ready(changed, ui))
    def test_fake_full_flow_reaches_start_with_paginated_page(self):
        commands = []
        class Process:
            pid = 7
            def poll(self): return None
        def command(_self, _driver, request, _deadline, timeout=10):
            commands.append(request["cmd"]); cmd = request["cmd"]
            runtime = _self.work / "runtime"; db = runtime / "database/frame-budget.db"
            responses = {"wait_ready": {"ready": True}, "scan_folder": {"started": True},
                "scan_status": {"scanning": False, "total": 1000, "added": 1000, "errors": 0},
                "wait_probe_ready": {"ready": True}, "get_ui_regions": {"list": {"visibleRows": 20}},
                "prepare_frame_budget_cold_mini": prepared(runtime, db, 750, 250), "focus_window": {"ok": True},
                "start_frame_budget_capture": {"schema": 1, "cmd": "start_frame_budget_capture",
                    "status": "environment_gate", "reason": "device", "stage": "preflight"}}
            state = {"total_count": 1000, "result_count": 500, "async_search_in_progress": False,
                "audio_loaded": True, "play_state": "paused", "audio_duration": 10.0,
                "waveform_detail_pending": False, "bg_cache_running": False,
                "current_file": str(_self.work / "fixture/playback.wav")}
            response = state if cmd == "get_state" else responses.get(cmd, {"ok": True})
            _self.controls.append({"request": request, "response": response})
            (_self.evidence / "control.json").write_text(json.dumps(_self.controls), encoding="utf-8")
            return response, json.dumps(response)
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); work = root / "work"; evidence = root / "evidence"; work.mkdir(); evidence.mkdir(); exe = root / "Aris.exe"; exe.touch()
            with mock.patch.object(probe, "create_fixtures", lambda path: path.mkdir(parents=True)), mock.patch.object(probe.subprocess, "Popen", return_value=Process()), mock.patch.object(probe, "_wait_window", return_value={"hwnd": 1}), mock.patch.object(probe, "_window", return_value={"hwnd": 1, "width": 1280, "height": 720, "visible": True, "focused": True, "iconic": False}), mock.patch.object(probe.InteractiveSession, "_cmd", command), mock.patch("builtins.open", mock.mock_open()):
                result = probe.InteractiveSession(work, evidence, exe).run(probe.time.monotonic() + probe.CLEANUP_RESERVE + 1)
            self.assertEqual(result["status"], "environment_gate")
            sequence = ("search", "get_state", "wait_probe_ready", "prepare_frame_budget_cold_mini",
                        "load_path", "get_state", "get_ui_regions", "focus_window", "start_frame_budget_capture")
            self.assertEqual([cmd for cmd in commands if cmd in sequence], list(sequence))
            request = next(item["request"] for item in json.loads((evidence / "control.json").read_text())
                           if item["request"]["cmd"] == "prepare_frame_budget_cold_mini")
            self.assertEqual(request, {"cmd": "prepare_frame_budget_cold_mini",
                "expected_runtime_root": str(work / "runtime"),
                "expected_db_path": str(work / "runtime/database/frame-budget.db"), "expected_files": 1000})
            self.assertEqual(json.loads((evidence / "prepare-result.json").read_text()),
                             prepared(work / "runtime", work / "runtime/database/frame-budget.db", 750, 250))
    def test_prepare_failure_stops_before_load_focus_or_capture(self):
        commands = []; prepare_response = None
        class Process:
            pid = 7
            def poll(self): return None
        def command(_self, _driver, request, _deadline, timeout=10):
            cmd = request["cmd"]; commands.append(cmd)
            state = {"total_count": 1000, "result_count": 500, "async_search_in_progress": False}
            responses = {"wait_ready": {"ready": True}, "scan_folder": {"started": True},
                "scan_status": {"scanning": False, "total": 1000, "added": 1000, "errors": 0},
                "wait_probe_ready": {"ready": True},
                "prepare_frame_budget_cold_mini": prepare_response}
            response = state if cmd == "get_state" else responses.get(cmd, {"ok": True})
            return response, json.dumps(response)
        failures = ({"schema": 1, "cmd": "prepare_frame_budget_cold_mini", "status": "failed",
                     "reason": "cold_mini_db_switch_pending", "stage": "preflight"},
                    counted_prepare_failure())
        for prepare_response in failures:
            with self.subTest(prepare=prepare_response), tempfile.TemporaryDirectory() as td:
                commands.clear(); root = Path(td); work = root / "work"; evidence = root / "evidence"
                work.mkdir(); evidence.mkdir(); exe = root / "Aris.exe"; exe.touch()
                with mock.patch.object(probe, "create_fixtures", lambda path: path.mkdir(parents=True)), \
                     mock.patch.object(probe.subprocess, "Popen", return_value=Process()), \
                     mock.patch.object(probe, "_wait_window", return_value={"hwnd": 1}), \
                     mock.patch.object(probe.InteractiveSession, "_cmd", command), \
                     mock.patch("builtins.open", mock.mock_open()):
                    result = probe.InteractiveSession(work, evidence, exe).run(
                        probe.time.monotonic() + probe.CLEANUP_RESERVE + 1)
                self.assertEqual((result["status"], result["reason"], result["stage"], probe.exit_code(result)),
                                 ("failed", prepare_response["reason"], prepare_response["stage"], 1))
                self.assertNotIn("load_path", commands); self.assertNotIn("focus_window", commands)
                self.assertNotIn("start_frame_budget_capture", commands)
                self.assertEqual(json.loads((evidence / "prepare-result.json").read_text()), prepare_response)
                self.assertFalse((evidence / "app-result.json").exists())
    def test_initial_window_handle_is_retained_before_commands(self):
        class Process:
            pid = 7
            returncode = None
            def poll(self): return self.returncode
            def wait(self, timeout): self.returncode = 0; return 0
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); work = root / "work"; evidence = root / "evidence"; work.mkdir(); evidence.mkdir(); exe = root / "Aris.exe"; exe.touch()
            session = probe.InteractiveSession(work, evidence, exe)
            with mock.patch.object(probe, "create_fixtures", lambda path: path.mkdir(parents=True)), mock.patch.object(probe.subprocess, "Popen", return_value=Process()), mock.patch.object(probe, "_wait_window", return_value={"hwnd": 77}), mock.patch.object(probe, "QaDriver", return_value=object()), mock.patch.object(probe.InteractiveSession, "_cmd", side_effect=probe.ProbeFailure("later", "preflight")), mock.patch("builtins.open", mock.mock_open()):
                with self.assertRaises(probe.ProbeFailure): session.run(probe.time.monotonic() + probe.CLEANUP_RESERVE + 1)
            self.assertEqual(session.hwnd, 77)
            with mock.patch.object(probe.ctypes.windll.user32, "PostMessageW") as post:
                cleanup = session.close(probe.time.monotonic() + 1)
            post.assert_called_once(); self.assertTrue(cleanup["clean_app_exit"])

    def test_polling_retries_transient_qa_timeout_within_shared_deadline(self):
        session = probe.InteractiveSession(Path("work"), Path("evidence"), Path("Aris.exe"))
        calls = 0

        def command(_driver, _request, _deadline, timeout=5):
            nonlocal calls
            calls += 1
            if calls == 1:
                try:
                    raise probe.QaDriverError("cold control channel")
                except probe.QaDriverError as exc:
                    raise RuntimeError(str(exc)) from exc
            return {"ready": True}, "{}"

        with mock.patch.object(session, "_cmd", side_effect=command), mock.patch.object(probe.time, "sleep"):
            response = session._until(
                object(),
                {"cmd": "wait_ready"},
                lambda value: value.get("ready") is True,
                probe.time.monotonic() + 1,
            )

        self.assertEqual({"ready": True}, response)
        self.assertEqual(2, calls)

    def test_failure_classification_precedence_and_shared_deadline(self):
        class Session:
            outcome = subprocess.TimeoutExpired(["Aris.exe"], 120); cleanup = {"app_terminated": True, "clean_app_exit": True, "marker_restored": True}
            def __init__(self, work, evidence, exe): self.work = work; Session.last = self
            def run(self, deadline): self.run_deadline = deadline; raise self.outcome
            def close(self, deadline): self.close_deadline = deadline; return self.cleanup.copy()
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); missing = root / "missing.exe"
            code, payload = probe.run_probe(root / "missing", missing, session_factory=Session)
            self.assertEqual((code, payload["reason"], payload["stage"]), (1, "missing_exe", "launch"))
            exe = root / "Aris.exe"; exe.touch(); Session.outcome = OSError("launch")
            blocked = root / "blocked-evidence"; blocked.write_text("not a directory")
            code, payload = probe.run_probe(blocked, exe, session_factory=Session)
            self.assertEqual((code, payload["status"], payload["reason"]), (2, "environment_gate", "evidence_exists"))
            code, payload = probe.run_probe(root / "launch", exe, session_factory=Session)
            self.assertEqual((code, payload["reason"]), (1, "io_failure"))
            Session.outcome = probe.EnvironmentGate("temp_policy"); code, payload = probe.run_probe(root / "temp", exe, session_factory=Session)
            self.assertEqual((code, payload["status"], payload["reason"]), (2, "environment_gate", "temp_policy"))
            Session.outcome = subprocess.TimeoutExpired(["Aris.exe"], 120); Session.cleanup["clean_app_exit"] = False
            ticks = iter((10.0, 10.1, 10.2, 10.3)); code, payload = probe.run_probe(root / "precedence", exe, session_factory=Session, clock=lambda: next(ticks))
            self.assertEqual((code, payload["reason"], payload["stage"]), (1, "subprocess_timeout", "driver"))
            self.assertEqual(Session.last.run_deadline, Session.last.close_deadline)
            Session.outcome = probe.EnvironmentGate("focus_unavailable"); code, payload = probe.run_probe(root / "gate_cleanup", exe, session_factory=Session)
            self.assertEqual((code, payload["status"], payload["reason"]), (1, "failed", "focus_unavailable"))
    def test_wait_window_classifies_early_exit(self):
        process = mock.Mock(); process.poll.return_value = 7
        with mock.patch.object(probe, "_window", return_value=None), self.assertRaises(probe.ProbeFailure) as caught:
            probe._wait_window(process, probe.time.monotonic() + 1)
        self.assertEqual((caught.exception.reason, caught.exception.stage), ("app_exited_before_window", "launch"))
    def test_close_rejects_nonzero_graceful_exit(self):
        class Process:
            returncode = None
            def poll(self): return self.returncode
            def wait(self, timeout): self.returncode = 7; return 7
        session = probe.InteractiveSession(Path("work"), Path("evidence"), Path("Aris.exe"))
        session.process = Process()
        result = session.close(probe.time.monotonic() + 1)
        self.assertTrue(result["app_terminated"])
        self.assertFalse(result["clean_app_exit"])
if __name__ == "__main__":
    unittest.main()
