#!/usr/bin/env python3
"""Run Stage 6 focused performance probes and preserve raw evidence outside Git."""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import Any
import stage6_main_thread_frame_budget_probe as frame_probe


PROBES = (
    "TestStage6AudioDeviceProbe",
    "TestStage6FileWatcherProbe",
    "TestStage6PlaybackLoadSeekProbe",
    "TestStage6ScanTerminalThroughputProbe",
    "TestStage6MainThreadFrameBudgetProbe",
)
MIN_ROUNDS = 5
INDEX_NAME = "run-index.json"
SCAN_TIMEOUT_SECONDS = 60
FRAME_CLEANUP_MARGIN_SECONDS = 30.0
FRAME_SUPERVISOR_TIMEOUT_SECONDS = frame_probe.WHOLE_TIMEOUT + FRAME_CLEANUP_MARGIN_SECONDS
FRAME_NAME = PROBES[-1]
FRAME_DRIVER = Path(__file__).with_name("stage6_main_thread_frame_budget_probe.py").resolve()
FRAME_MAX_ATTEMPTS = 3
FRAME_RETRYABLE_REASONS = {"authenticity_incomplete", "environment_lost"}


def _probe_executable(build_dir: Path, name: str) -> Path:
    suffix = ".exe" if sys.platform == "win32" else ""
    configured = build_dir / "Debug" / f"{name}{suffix}"
    return configured if configured.exists() else build_dir / f"{name}{suffix}"


def _finite_nonnegative(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value >= 0


def _exact_int_counts(values: Any, expected: dict[str, int]) -> bool:
    return isinstance(values, dict) and all(type(values.get(key)) is int and values[key] == value
                                            for key, value in expected.items())


def _validate_stats(stats: Any) -> str | None:
    if not isinstance(stats, dict):
        return "playback statistics must be an object"
    values = [stats.get(key) for key in ("median_ms", "p95_ms", "max_ms")]
    if not all(_finite_nonnegative(value) for value in values):
        return "playback statistics must be finite and non-negative"
    if not values[0] <= values[1] <= values[2]:
        return "playback statistics must be ordered median <= p95 <= max"
    return None


def _validate_playback_output(payload: Any) -> str | None:
    if not isinstance(payload, dict) or payload.get("schema") != 1 or payload.get("status") != "measured":
        return "playback JSON schema/status is invalid"
    fixture = payload.get("fixture")
    expected_fixture = {
        "format": "pcm_s16_wav", "sample_rate": 48000, "channels": 2,
        "duration_seconds": 10.0, "total_frames": 480000,
    }
    if fixture != expected_fixture:
        return "playback fixture metadata is invalid"
    load = payload.get("load_to_ready")
    seek = payload.get("seek_call")
    if not isinstance(load, dict) or load.get("count") != 20 or load.get("poll_interval_ms") != 1:
        return "playback load metadata is invalid"
    if not isinstance(seek, dict) or seek.get("count") != 100:
        return "playback seek metadata is invalid"
    load_error = _validate_stats(load)
    if load_error:
        return load_error
    seek_error = _validate_stats(seek)
    if seek_error:
        return seek_error
    if not _finite_nonnegative(seek.get("max_position_error")):
        return "playback position error must be finite and non-negative"
    value = payload.get("value_ms")
    if not _finite_nonnegative(value) or not math.isclose(value, max(load["p95_ms"], seek["p95_ms"]), rel_tol=1e-9, abs_tol=1e-9):
        return "playback value_ms is inconsistent with component p95"
    return None


def _validate_scan_output(payload: Any) -> str | None:
    if not isinstance(payload, dict) or payload.get("schema") != 1 or payload.get("status") != "measured":
        return "scan JSON schema/status is invalid"
    fixture = payload.get("fixture")
    exact_fixture = {"files": 100, "sample_rate": 8000, "channels": 1, "frames_per_file": 800}
    if (not _exact_int_counts(fixture, exact_fixture) or set(fixture) != set(exact_fixture) | {"format"} or
            fixture.get("format") != "pcm_s16_wav"):
        return "scan fixture metadata is invalid"
    if not _exact_int_counts(payload.get("workers"), {"scan_threads": 2, "backfill_threads": 2}):
        return "scan worker metadata is invalid"
    scan = payload.get("scan")
    exact_scan = {"total": 100, "added": 100, "updated": 0, "skipped": 0, "ingested": 100, "errors": 0}
    if (not _exact_int_counts(scan, exact_scan) or
            not _finite_nonnegative(scan.get("ingest_duration_ms"))):
        return "scan ingest metrics are invalid"
    terminal = payload.get("terminal")
    exact_terminal = {"total": 100, "pending_files": 0, "mini_terminal": 100, "cover_terminal": 100,
                      "metadata_terminal": 100, "mini_unresolved": 0, "cover_unresolved": 0,
                      "metadata_unresolved": 0, "non_success_terminal": 100, "resolved_cover_files": 100}
    if (not _exact_int_counts(terminal, exact_terminal) or "failure_terminal" in terminal):
        return "scan terminal metrics are invalid"
    elapsed, rate = terminal.get("elapsed_ms"), terminal.get("files_per_second")
    value = payload.get("value_files_per_second")
    if (not _finite_nonnegative(elapsed) or elapsed <= 0 or not _finite_nonnegative(rate) or rate <= 0 or
            not _finite_nonnegative(value) or value <= 0 or
            not math.isclose(value, rate, rel_tol=1e-9, abs_tol=1e-9) or
            not math.isclose(rate, 100000.0 / elapsed, rel_tol=1e-9, abs_tol=1e-9)):
        return "scan throughput value is invalid"
    return None


def _validate_frame_evidence(root: Path, payload: Any, exit_code: int = 0) -> str | None:
    index_path, result_path = root / "run-index.json", root / "result.json"
    if not root.is_dir() or not index_path.is_file() or not result_path.is_file():
        return "frame evidence is missing"
    try:
        frame_index = json.loads(index_path.read_text(encoding="utf-8"))
        recorded = frame_probe.parse_one(result_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        return f"frame evidence is invalid: {exc}"
    paths = frame_index.get("paths") if isinstance(frame_index, dict) else None
    if (not isinstance(frame_index, dict) or frame_index.get("schema") != 1 or
            frame_index.get("status") != payload.get("status") or
            frame_index.get("exit_code") != exit_code or not isinstance(paths, dict) or not paths):
        return "frame evidence index is invalid"
    resolved = root.resolve()
    for value in paths.values():
        path = Path(value).resolve() if isinstance(value, str) else None
        if path is None or not path.is_relative_to(resolved) or not path.is_file():
            return "frame evidence path escapes its round root"
    if recorded != payload:
        return "frame evidence result differs from stdout"
    return None


def _validate_output(name: str, payload: Any) -> str | None:
    if not isinstance(payload, dict) or payload.get("status") != "measured":
        return "probe JSON status must be measured"
    if name == "TestStage6PlaybackLoadSeekProbe":
        return _validate_playback_output(payload)
    if name == "TestStage6ScanTerminalThroughputProbe":
        return _validate_scan_output(payload)
    if name == FRAME_NAME:
        try:
            frame_probe.validate_payload(payload)
        except (KeyError, TypeError, ValueError) as exc:
            return f"frame-budget JSON is invalid: {exc}"
        if not _finite_nonnegative(payload.get("value_ms")) or payload["value_ms"] >= 16:
            return "frame-budget value_ms must be below 16 ms"
        return None
    if name == "TestStage6AudioDeviceProbe":
        execution = payload.get("callback_execution")
        inter_arrival = payload.get("inter_arrival")
        if (payload.get("callback_count") != 250 or not isinstance(payload.get("period_ms"), (int, float)) or
                not isinstance(execution, dict) or not all(key in execution for key in ("median_ms", "p95_ms", "max_ms")) or
                not isinstance(inter_arrival, dict) or "p95_jitter_ms" not in inter_arrival):
            return "audio probe JSON missing required metrics"
    else:
        delivery = payload.get("delivery")
        if (payload.get("received_count") != 150 or not isinstance(delivery, dict) or
                not all(key in delivery for key in ("median_ms", "p95_ms", "max_ms")) or
                "event_order" not in payload or "shutdown_elapsed_ms" not in payload):
            return "watcher probe JSON missing required metrics"
    return None


def _retryable_frame_failure(payload: Any, exit_code: int) -> bool:
    if (exit_code != 1 or not isinstance(payload, dict) or
            payload.get("status") != "failed" or
            payload.get("reason") not in FRAME_RETRYABLE_REASONS):
        return False
    try:
        frame_probe.validate_payload(payload)
    except (KeyError, TypeError, ValueError):
        return False
    return True


def _write_index(evidence_dir: Path, index: dict[str, Any]) -> None:
    evidence_dir.mkdir(parents=True, exist_ok=True)
    (evidence_dir / INDEX_NAME).write_text(
        json.dumps(index, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def run_probes(build_dir: Path, rounds: int, evidence_dir: Path,
               frame_exe: Path) -> tuple[int, dict[str, Any]]:
    if rounds < MIN_ROUNDS:
        raise ValueError(f"rounds must be at least {MIN_ROUNDS}")
    build_dir = build_dir.resolve()
    evidence_dir = evidence_dir.resolve()
    frame_exe = frame_exe.resolve()
    if evidence_dir.exists():
        raise ValueError(f"evidence directory must be fresh: {evidence_dir}")
    if not FRAME_DRIVER.is_file():
        raise ValueError(f"missing frame driver: {FRAME_DRIVER}")
    if not frame_exe.is_file():
        raise ValueError(f"missing diagnostics executable: {frame_exe}")
    index: dict[str, Any] = {
        "schema": 1,
        "kind": "stage6-performance-run-index",
        "status": "running",
        "build_dir": str(build_dir),
        "rounds_requested": rounds,
        "probes": {},
        "existing_probe_audit": {
            "TestBackgroundIoPolicy": "unsupported: exit 0 but assertion-only output; no machine-readable business metric",
            "TestBackfillRecoveryWorker": "unsupported: correctness gate passes after source-contract repair, but output has no machine-readable performance metric",
            "TestScanWaveformCacheContract": "unsupported: exit 0 but output has no machine-readable performance metric",
            "TestCoverClickPerf": "unsupported: exit 0 with human-readable timing only; no stable JSON metric",
        },
    }
    evidence_dir.mkdir(parents=True, exist_ok=True)
    for name in PROBES:
        is_frame = name == FRAME_NAME
        executable = Path(sys.executable) if is_frame else _probe_executable(build_dir, name)
        probe = {"executable": str(executable), "rounds": []}
        if is_frame:
            probe["driver"] = str(FRAME_DRIVER)
            probe["diagnostics_executable"] = str(frame_exe)
            probe["retry_attempts"] = []
        index["probes"][name] = probe
        if not executable.exists():
            index.update(status="failed", error=f"missing executable: {executable}")
            _write_index(evidence_dir, index)
            return 1, index
        probe_dir = evidence_dir / name
        probe_dir.mkdir(parents=True, exist_ok=True)
        for round_number in range(1, rounds + 1):
            attempt = 1
            while True:
                retry_suffix = "" if attempt == 1 else f"-retry-{attempt - 1:02d}"
                frame_root = probe_dir / f"round-{round_number:02d}{retry_suffix}-evidence"
                if is_frame and frame_root.exists():
                    index.update(status="failed", error=f"{name} round {round_number}: evidence root is not fresh: {frame_root}")
                    _write_index(evidence_dir, index)
                    return 1, index
                command = ([str(executable), str(FRAME_DRIVER), "--evidence-root", str(frame_root),
                            "--exe", str(frame_exe)] if is_frame else [str(executable)])
                stdout_path = probe_dir / f"round-{round_number:02d}{retry_suffix}-stdout.txt"
                stderr_path = probe_dir / f"round-{round_number:02d}{retry_suffix}-stderr.txt"
                run_options: dict[str, Any] = {"text": True, "capture_output": True}
                if name == "TestStage6ScanTerminalThroughputProbe":
                    run_options["timeout"] = SCAN_TIMEOUT_SECONDS
                elif is_frame:
                    run_options["timeout"] = FRAME_SUPERVISOR_TIMEOUT_SECONDS
                try:
                    result = subprocess.run(command, **run_options)
                except subprocess.TimeoutExpired as exc:
                    stdout = exc.stdout.decode("utf-8", errors="replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
                    stderr = exc.stderr.decode("utf-8", errors="replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
                    stdout_path.write_text(stdout, encoding="utf-8")
                    stderr_path.write_text(stderr, encoding="utf-8")
                    timeout = run_options.get("timeout")
                    record = {"round": round_number, "attempt": attempt, "command": command, "exit_code": None,
                              "status": "timeout", "stdout": str(stdout_path), "stderr": str(stderr_path),
                              "error": f"timed out after {timeout} seconds"}
                    if is_frame:
                        record["evidence"] = str(frame_root)
                    probe["rounds"].append(record)
                    index.update(status="failed", error=f"{name} round {round_number}: {record['error']}")
                    _write_index(evidence_dir, index)
                    return 1, index
                stdout_path.write_text(result.stdout, encoding="utf-8")
                stderr_path.write_text(result.stderr, encoding="utf-8")
                record: dict[str, Any] = {
                    "round": round_number,
                    "attempt": attempt,
                    "command": command,
                    "exit_code": result.returncode,
                    "stdout": str(stdout_path),
                    "stderr": str(stderr_path),
                }
                if is_frame:
                    record["evidence"] = str(frame_root)
                try:
                    payload = frame_probe.parse_one(result.stdout) if is_frame else json.loads(result.stdout.strip())
                except (json.JSONDecodeError, ValueError) as exc:
                    payload = None
                    record["error"] = f"invalid JSON: {exc}"
                record["status"] = payload.get("status") if isinstance(payload, dict) else "invalid_json"
                evidence_error = (_validate_frame_evidence(frame_root, payload, result.returncode)
                                  if is_frame and isinstance(payload, dict) else None)
                if evidence_error:
                    record["error"] = evidence_error
                    probe["rounds"].append(record)
                    index.update(status="failed", error=f"{name} round {round_number}: {record['error']}")
                    _write_index(evidence_dir, index)
                    return 1, index
                if result.returncode == 2 and record["status"] == "environment_gate":
                    if is_frame:
                        try:
                            frame_probe.validate_payload(payload)
                        except (KeyError, TypeError, ValueError) as exc:
                            record["error"] = f"invalid environment gate: {exc}"
                            probe["rounds"].append(record)
                            index.update(status="failed", error=f"{name} round {round_number}: {record['error']}")
                            _write_index(evidence_dir, index)
                            return 1, index
                    record["payload"] = payload
                    probe["rounds"].append(record)
                    index.update(status="environment_gate", error=payload.get("reason", "environment gate"))
                    _write_index(evidence_dir, index)
                    return 2, index
                if result.returncode != 0:
                    if is_frame and attempt < FRAME_MAX_ATTEMPTS and _retryable_frame_failure(payload, result.returncode):
                        record["reason"] = payload["reason"]
                        record["payload"] = payload
                        probe["retry_attempts"].append(record)
                        attempt += 1
                        continue
                    probe["rounds"].append(record)
                    index.update(status="failed", error=f"{name} round {round_number} exited {result.returncode}")
                    _write_index(evidence_dir, index)
                    return 1, index
                error = _validate_output(name, payload)
                if error:
                    record["error"] = error
                    probe["rounds"].append(record)
                    index.update(status="failed", error=f"{name} round {round_number}: {error}")
                    _write_index(evidence_dir, index)
                    return 1, index
                record["payload"] = payload
                probe["rounds"].append(record)
                break
    index["status"] = "complete"
    _write_index(evidence_dir, index)
    return 0, index


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--rounds", type=int, default=MIN_ROUNDS)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--frame-exe", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        code, index = run_probes(args.build_dir, args.rounds, args.evidence_dir, args.frame_exe)
    except ValueError as exc:
        parser.error(str(exc))
    print(json.dumps({"status": index["status"], "index": str(args.evidence_dir.resolve() / INDEX_NAME)}))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
