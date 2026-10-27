import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import stage6_performance_runner as runner  # noqa: E402

PLAYBACK = {
    "schema": 1, "status": "measured",
    "fixture": {"format": "pcm_s16_wav", "sample_rate": 48000, "channels": 2,
                "duration_seconds": 10.0, "total_frames": 480000},
    "load_to_ready": {"count": 20, "median_ms": 1.0, "p95_ms": 2.0,
                      "max_ms": 3.0, "poll_interval_ms": 1},
    "seek_call": {"count": 100, "median_ms": 0.01, "p95_ms": 0.02,
                  "max_ms": 0.03, "max_position_error": 0.000002},
    "value_ms": 2.0,
}
SCAN = {
    "schema": 1, "status": "measured",
    "fixture": {"format": "pcm_s16_wav", "files": 100, "sample_rate": 8000,
                "channels": 1, "frames_per_file": 800},
    "workers": {"scan_threads": 2, "backfill_threads": 2},
    "scan": {"total": 100, "added": 100, "updated": 0, "skipped": 0,
             "ingested": 100, "errors": 0, "ingest_duration_ms": 200},
    "terminal": {"total": 100, "pending_files": 0, "mini_terminal": 100,
                 "cover_terminal": 100, "metadata_terminal": 100,
                 "mini_unresolved": 0, "cover_unresolved": 0,
                 "metadata_unresolved": 0, "non_success_terminal": 100,
                 "resolved_cover_files": 100, "elapsed_ms": 700,
                 "files_per_second": 142.8571429},
    "value_files_per_second": 142.8571429,
}
def frame_measured():
    samples = [2.0] * 240
    metric = {"samples": samples, "median_ms": 2.0, "p95_ms": 2.0, "max_ms": 2.0}
    scenario = {"warmup_frames": 60, "sample_count": 240,
        "cpu_work_ms": copy.deepcopy(metric), "present_block_ms": copy.deepcopy(metric),
        "frame_interval_ms": copy.deepcopy(metric), "render_ui_frames": 240,
        "gl_render_frames": 240, "present_frames": 240}
    return {"ok": True, "cmd": "frame_budget_result", "schema": 1, "status": "measured",
        "metric": "main_thread_frame_budget", "environment": {"window_visible": True,
        "window_focused": True, "window_iconic": False, "client_width": 1280,
        "client_height": 720, "dpi_scale": 1.0, "opengl": True, "vsync": True,
        "audio_device": True}, "budget_ms": 16, "scenarios": {"idle": copy.deepcopy(scenario),
        "busy_scroll_mini_playback": copy.deepcopy(scenario)}, "distinctScrollPositions": 120,
        "playbackFramesObserved": 240, "miniRequestsSubmitted": 1, "miniResultsConsumed": 1,
        "busyIntentionalThrottleFrames": 0, "internal_deadline_seconds": 90,
        "external_timeout_seconds": 120, "value_ms": 2.0}
def frame_gate():
    return {"ok": False, "cmd": "frame_budget_result", "schema": 1,
        "status": "environment_gate", "metric": "main_thread_frame_budget",
        "environment": {"window_visible": False, "window_focused": False,
        "window_iconic": False, "client_width": 0, "client_height": 0,
        "dpi_scale": 1.0, "opengl": False, "vsync": False, "audio_device": False},
        "reason": "device", "stage": "preflight"}


def frame_retryable_failure(reason="authenticity_incomplete"):
    payload = frame_gate()
    payload.update(
        status="failed",
        reason=reason,
        stage="result" if reason == "authenticity_incomplete" else "frame",
        environment=frame_measured()["environment"],
    )
    return payload


class Result:
    def __init__(self, code=0, stdout="", stderr=""):
        self.returncode, self.stdout, self.stderr = code, stdout, stderr


def measured(name):
    if name == "TestStage6AudioDeviceProbe":
        return '{"status":"measured","period_ms":10,"callback_execution":{"median_ms":0.1,"p95_ms":0.2,"max_ms":0.3},"inter_arrival":{"p95_jitter_ms":0.4},"callback_count":250}'
    if name == "TestStage6PlaybackLoadSeekProbe":
        return json.dumps(PLAYBACK)
    if name == "TestStage6ScanTerminalThroughputProbe":
        return json.dumps(SCAN)
    if name == "TestStage6MainThreadFrameBudgetProbe":
        return json.dumps(frame_measured())
    return '{"status":"measured","delivery":{"median_ms":1,"p95_ms":2,"max_ms":3},"received_count":150,"event_order":[],"shutdown_elapsed_ms":0.5}'


class PerformanceRunnerTests(unittest.TestCase):
    def run_fixture(self, side_effect, rounds=5, create_exes=True,
                    create_frame_evidence=True, stale_frame_root=False):
        with tempfile.TemporaryDirectory() as temp_dir:
            root, calls = Path(temp_dir), []
            build, evidence = root / "build", root / "evidence"
            (build / "Debug").mkdir(parents=True)
            if create_exes:
                for name in runner.PROBES:
                    if name != "TestStage6MainThreadFrameBudgetProbe":
                        (build / "Debug" / f"{name}.exe").write_text("")
            frame_exe = build / "Debug" / "Aris.exe"; frame_exe.write_text("")

            def execute(command, **kwargs):
                calls.append((command, kwargs))
                result = side_effect(command, **kwargs)
                if len(calls) == 20 and stale_frame_root:
                    stale = evidence / "TestStage6MainThreadFrameBudgetProbe/round-01-evidence"
                    stale.mkdir(parents=True); (stale / "stale.txt").write_text("stale")
                if len(command) > 1 and Path(command[1]).name == "stage6_main_thread_frame_budget_probe.py" and create_frame_evidence:
                    frame_root = Path(command[command.index("--evidence-root") + 1])
                    frame_root.mkdir(parents=True)
                    (frame_root / "result.json").write_text(result.stdout)
                    try: frame_status = json.loads(result.stdout)["status"]
                    except (json.JSONDecodeError, KeyError, TypeError): frame_status = "invalid"
                    (frame_root / "run-index.json").write_text(json.dumps({"schema": 1,
                        "status": frame_status, "exit_code": result.returncode,
                        "paths": {"result.json": str(frame_root / "result.json")}}))
                return result

            with mock.patch("stage6_performance_runner.subprocess.run", side_effect=execute):
                code, index = runner.run_probes(build, rounds, evidence, frame_exe)
            files = {p.relative_to(evidence).as_posix(): p.read_text(encoding="utf-8")
                     for p in evidence.rglob("*") if p.is_file()}
            return code, index, files, calls

    def test_runs_five_probes_in_order_with_exact_commands_and_raw_streams(self):
        expected_order = (
            "TestStage6AudioDeviceProbe", "TestStage6FileWatcherProbe",
            "TestStage6PlaybackLoadSeekProbe", "TestStage6ScanTerminalThroughputProbe",
            "TestStage6MainThreadFrameBudgetProbe")
        expected = set(expected_order)
        self.assertEqual(expected_order, runner.PROBES)
        code, index, files, calls = self.run_fixture(
            lambda command, **_: Result(stdout=measured(
                "TestStage6MainThreadFrameBudgetProbe" if len(command) > 1 else Path(command[0]).stem), stderr="diag"))
        self.assertEqual((0, "complete", 25), (code, index["status"], len(calls)))
        self.assertEqual(expected, set(index["probes"]))
        self.assertIn("passes after source-contract repair", index["existing_probe_audit"]["TestBackfillRecoveryWorker"])
        legacy = [[str(Path(index["probes"][name]["executable"]))]
                  for name in runner.PROBES[:-1] for _ in range(5)]
        frame = index["probes"][runner.PROBES[-1]]
        self.assertEqual((sys.executable, str(runner.FRAME_DRIVER)),
                         (frame["executable"], frame["driver"]))
        frame_commands = [[sys.executable, str(runner.FRAME_DRIVER), "--evidence-root",
            str(Path(frame["rounds"][i]["evidence"])), "--exe", frame["diagnostics_executable"]]
            for i in range(5)]
        self.assertEqual(legacy + frame_commands, [call[0] for call in calls])
        self.assertEqual([60] * 5, [call[1].get("timeout") for call in calls if "ScanTerminal" in call[0][0]])
        self.assertEqual(runner.frame_probe.WHOLE_TIMEOUT + 30.0,
                         runner.FRAME_SUPERVISOR_TIMEOUT_SECONDS)
        self.assertGreater(runner.FRAME_SUPERVISOR_TIMEOUT_SECONDS,
                           runner.frame_probe.WHOLE_TIMEOUT)
        self.assertEqual([runner.FRAME_SUPERVISOR_TIMEOUT_SECONDS] * 5,
                         [call[1].get("timeout") for call in calls if len(call[0]) > 1])
        self.assertEqual(5, len({round_["evidence"] for round_ in frame["rounds"]}))
        self.assertEqual(frame_measured(), frame["rounds"][0]["payload"])
        for name, probe in index["probes"].items():
            self.assertEqual(5, len(probe["rounds"]))
            for round_ in probe["rounds"]:
                expected_command = (frame_commands[round_["round"] - 1]
                                    if name == runner.FRAME_NAME else [probe["executable"]])
                self.assertEqual(expected_command, round_["command"])
                self.assertIn(f"{name}/round-{round_['round']:02d}-stdout.txt", files)
                self.assertIn(f"{name}/round-{round_['round']:02d}-stderr.txt", files)

    def test_accepts_exact_playback_payload(self):
        self.assertIsNone(runner._validate_output("TestStage6PlaybackLoadSeekProbe", PLAYBACK))

    def test_rejects_malformed_or_inconsistent_playback_payloads(self):
        mutations = []
        for path, value in (("schema", 2), ("fixture.sample_rate", 44100),
                            ("load_to_ready.count", 19), ("seek_call.count", 99),
                            ("load_to_ready.p95_ms", float("nan")),
                            ("seek_call.max_position_error", -1),
                            ("load_to_ready.max_ms", 1.5), ("value_ms", 9.0)):
            payload = copy.deepcopy(PLAYBACK)
            target = payload
            parts = path.split(".")
            for part in parts[:-1]:
                target = target[part]
            target[parts[-1]] = value
            mutations.append(payload)
        for payload in mutations:
            with self.subTest(payload=payload):
                self.assertIsNotNone(runner._validate_output("TestStage6PlaybackLoadSeekProbe", payload))

    def test_accepts_exact_scan_payload_and_rejects_mutations(self):
        self.assertIsNone(runner._validate_output("TestStage6ScanTerminalThroughputProbe", SCAN))
        for path, value in (("schema", 2), ("fixture.files", 99), ("fixture.files", 100.0),
                            ("fixture.channels", True), ("workers.scan_threads", 3),
                            ("scan.added", 99), ("scan.total", 100.0), ("scan.ingest_duration_ms", float("nan")),
                            ("terminal.pending_files", 1), ("terminal.non_success_terminal", 0),
                            ("terminal.resolved_cover_files", 99), ("terminal.elapsed_ms", 0),
                            ("value_files_per_second", 99)):
            payload = copy.deepcopy(SCAN)
            target = payload
            parts = path.split(".")
            for part in parts[:-1]:
                target = target[part]
            target[parts[-1]] = value
            with self.subTest(path=path):
                self.assertIsNotNone(runner._validate_output("TestStage6ScanTerminalThroughputProbe", payload))
        payload = copy.deepcopy(SCAN)
        payload["terminal"]["files_per_second"] = payload["value_files_per_second"] = 99.0
        self.assertIsNotNone(runner._validate_output("TestStage6ScanTerminalThroughputProbe", payload))

    def test_accepts_exact_frame_payload_and_rejects_schema_or_budget_mutations(self):
        payload = frame_measured()
        self.assertIsNone(runner._validate_output("TestStage6MainThreadFrameBudgetProbe", payload))
        for path, value in (("schema", 2), ("status", "environment_gate"),
                            ("metric", "other"), ("value_ms", 16.0),
                            ("scenarios.busy_scroll_mini_playback.sample_count", 239),
                            ("scenarios.busy_scroll_mini_playback.cpu_work_ms.p95_ms", 3.0)):
            item = copy.deepcopy(payload); target = item; parts = path.split(".")
            for part in parts[:-1]: target = target[part]
            target[parts[-1]] = value
            with self.subTest(path=path):
                self.assertIsNotNone(runner._validate_output(
                    "TestStage6MainThreadFrameBudgetProbe", item))

    def test_frame_failures_are_fail_fast_without_retry(self):
        cases = ((Result(0, "{}\n{}"), True, 1, "failed"),
                 (Result(1, '{"status":"failed"}'), True, 1, "failed"),
                 (Result(2, json.dumps(frame_gate())), True, 2, "environment_gate"),
                 (Result(2, json.dumps(frame_gate())), False, 1, "failed"),
                 (Result(2, '{"status":"environment_gate","reason":"device"}'), True, 1, "failed"),
                 (Result(0, json.dumps({**frame_measured(), "value_ms": 16.0})), True, 1, "failed"),
                 (Result(0, json.dumps(frame_measured())), False, 1, "failed"))
        for frame_result, evidence, expected_code, expected_status in cases:
            def execute(command, **_kwargs):
                return frame_result if len(command) > 1 else Result(stdout=measured(Path(command[0]).stem))
            code, index, _, calls = self.run_fixture(execute, create_frame_evidence=evidence)
            with self.subTest(frame_result=frame_result.stdout, evidence=evidence):
                self.assertEqual((expected_code, expected_status, 21),
                                 (code, index["status"], len(calls)))
                self.assertEqual(1, sum(len(call[0]) > 1 for call in calls))

    def test_retries_only_retryable_frame_authenticity_failures_and_preserves_attempt(self):
        failed = Result(1, json.dumps(frame_retryable_failure()))
        measured_result = Result(0, json.dumps(frame_measured()))
        frame_calls = 0

        def execute(command, **_kwargs):
            nonlocal frame_calls
            if len(command) == 1:
                return Result(stdout=measured(Path(command[0]).stem))
            frame_calls += 1
            return failed if frame_calls == 1 else measured_result

        code, index, files, calls = self.run_fixture(execute)
        frame = index["probes"][runner.FRAME_NAME]
        self.assertEqual((0, "complete", 26), (code, index["status"], len(calls)))
        self.assertEqual(5, len(frame["rounds"]))
        self.assertEqual(1, len(frame["retry_attempts"]))
        self.assertEqual("authenticity_incomplete", frame["retry_attempts"][0]["reason"])
        self.assertEqual("measured", frame["rounds"][0]["status"])
        self.assertIn(
            f"{runner.FRAME_NAME}/round-01-retry-01-stdout.txt", files
        )

    def test_valid_child_cleanup_failure_is_preserved_before_supervisor_timeout(self):
        child = frame_gate(); child.update(status="failed", reason="cleanup_failed", stage="cleanup")
        raw = json.dumps(child)
        def execute(command, **_kwargs):
            return Result(1, raw) if len(command) > 1 else Result(stdout=measured(Path(command[0]).stem))
        code, index, files, calls = self.run_fixture(execute)
        record = index["probes"][runner.FRAME_NAME]["rounds"][0]
        self.assertEqual((1, "failed", 1, "failed", 21),
                         (code, index["status"], record["exit_code"], record["status"], len(calls)))
        self.assertEqual(raw, files[f"{runner.FRAME_NAME}/round-01-stdout.txt"])
        self.assertGreater(calls[-1][1]["timeout"], runner.frame_probe.WHOLE_TIMEOUT)

    def test_frame_timeout_and_stale_root_fail_before_retry(self):
        def timeout(command, **_kwargs):
            if len(command) > 1: raise subprocess.TimeoutExpired(
                command, _kwargs["timeout"], output="partial", stderr=b"timeout")
            return Result(stdout=measured(Path(command[0]).stem))
        code, index, files, calls = self.run_fixture(timeout, create_frame_evidence=False)
        frame = index["probes"]["TestStage6MainThreadFrameBudgetProbe"]["rounds"][0]
        self.assertEqual((1, "failed", 21, "timeout", None),
                         (code, index["status"], len(calls), frame["status"], frame["exit_code"]))
        self.assertEqual("partial", files["TestStage6MainThreadFrameBudgetProbe/round-01-stdout.txt"])
        self.assertEqual("timeout", files["TestStage6MainThreadFrameBudgetProbe/round-01-stderr.txt"])
        self.assertIn(str(runner.FRAME_SUPERVISOR_TIMEOUT_SECONDS), frame["error"])
        code, index, _, calls = self.run_fixture(
            lambda command, **_: Result(stdout=measured(Path(command[0]).stem)),
            stale_frame_root=True)
        self.assertEqual((1, "failed", 20), (code, index["status"], len(calls)))
        self.assertIn("not fresh", index["error"])

    def test_frame_evidence_must_be_present_and_contained(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); frame_root = root / "frame"; frame_root.mkdir()
            payload = frame_measured(); (frame_root / "result.json").write_text(json.dumps(payload))
            outside = root / "outside.json"; outside.write_text("{}")
            (frame_root / "run-index.json").write_text(json.dumps({"schema": 1,
                "status": "measured", "exit_code": 0, "paths": {"outside": str(outside)}}))
            self.assertIsNotNone(runner._validate_frame_evidence(frame_root, payload))

    def test_requires_fresh_output_root_and_diagnostics_executable(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); build = root / "build"; build.mkdir(); frame_exe = root / "Aris.exe"
            evidence = root / "evidence"; evidence.mkdir(); (evidence / "old.txt").write_text("old")
            with self.assertRaises(ValueError): runner.run_probes(build, 5, evidence, frame_exe)
            for child in evidence.iterdir(): child.unlink()
            evidence.rmdir()
            with self.assertRaises(ValueError): runner.run_probes(build, 5, evidence, frame_exe)

    def test_scan_timeout_is_raw_recorded_and_fails_fast(self):
        def execute(command, **_kwargs):
            if "ScanTerminal" in command[0]:
                raise subprocess.TimeoutExpired(command, 60, output=b"partial", stderr="timed out")
            return Result(stdout=measured(Path(command[0]).stem))
        code, index, files, calls = self.run_fixture(execute)
        record = index["probes"]["TestStage6ScanTerminalThroughputProbe"]["rounds"][0]
        self.assertEqual((1, "failed", 16, "timeout", None),
                         (code, index["status"], len(calls), record["status"], record["exit_code"]))
        self.assertEqual("failed", json.loads(files["run-index.json"])["status"])
        self.assertIn("timed out after 60 seconds", record["error"])
        self.assertEqual("partial", files["TestStage6ScanTerminalThroughputProbe/round-01-stdout.txt"])
        self.assertEqual("timed out", files["TestStage6ScanTerminalThroughputProbe/round-01-stderr.txt"])

    def test_fail_fast_environment_failure_multi_json_and_bad_playback(self):
        cases = [
            (Result(2, '{"status":"environment_gate","reason":"no device"}'), 2, "environment_gate"),
            (Result(1, '{"status":"failed"}'), 1, "failed"),
            (Result(0, '{}\n{}'), 1, "failed"),
            (Result(0, json.dumps({**PLAYBACK, "value_ms": 9})), 1, "failed"),
        ]
        for result, expected_code, status in cases:
            code, index, _, calls = self.run_fixture(lambda *_args, result=result, **_: result)
            self.assertEqual((expected_code, status, 1), (code, index["status"], len(calls)))

    def test_rejects_plain_invalid_json(self):
        code, index, _, calls = self.run_fixture(
            lambda *_args, **_kwargs: Result(stdout="not-json"))
        self.assertEqual((1, "failed", 1), (code, index["status"], len(calls)))

    def test_rejects_missing_audio_metrics(self):
        payload = {"status": "measured", "callback_count": 250}
        code, index, _, calls = self.run_fixture(
            lambda *_args, **_kwargs: Result(stdout=json.dumps(payload)))
        self.assertEqual((1, "failed", 1), (code, index["status"], len(calls)))

    def test_rejects_missing_watcher_metrics(self):
        payload = {"status": "measured", "delivery": {"median_ms": 1}}

        def execute(command, **_kwargs):
            name = Path(command[0]).stem
            return Result(stdout=measured(name) if name != "TestStage6FileWatcherProbe"
                           else json.dumps(payload))

        code, index, _, calls = self.run_fixture(execute)
        self.assertEqual((1, "failed", 6), (code, index["status"], len(calls)))


if __name__ == "__main__":
    unittest.main()
