#!/usr/bin/env python3
"""Validate a recorded Stage 6 performance-baseline manifest.

This Task 5A tool is deliberately validation-only: it never runs probes or
creates baseline measurements.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
import sys
from decimal import Decimal, ROUND_CEILING, ROUND_FLOOR, localcontext
from pathlib import Path
from typing import Any


REQUIRED_METRICS = {
    "audio_device_callback_budget",
    "playback_load_seek",
    "file_watcher_delivery",
    "scan_terminal_throughput",
    "main_thread_frame_budget",
}
METRIC_SPECS = {
    "audio_device_callback_budget": {"name": "Audio device callback p95", "metric": "callback_execution.p95_ms", "unit": "ms", "direction": "upper"},
    "file_watcher_delivery": {"name": "File watcher delivery p95", "metric": "delivery.p95_ms", "unit": "ms", "direction": "upper"},
    "playback_load_seek": {"name": "Playback load/seek", "metric": "value_ms", "unit": "ms", "direction": "upper"},
    "scan_terminal_throughput": {"name": "Scan terminal throughput", "metric": "value_files_per_second", "unit": "files_per_second", "direction": "lower"},
    "main_thread_frame_budget": {"name": "Main-thread frame budget", "metric": "busy_cpu_work_p95_ms", "unit": "ms", "direction": "upper"},
}
METRIC_DIRECTIONS = {name: spec["direction"] for name, spec in METRIC_SPECS.items()}
RUN_INDEX_PROBES = {
    "audio_device_callback_budget": ("TestStage6AudioDeviceProbe", ("callback_execution", "p95_ms")),
    "file_watcher_delivery": ("TestStage6FileWatcherProbe", ("delivery", "p95_ms")),
    "playback_load_seek": ("TestStage6PlaybackLoadSeekProbe", ("value_ms",)),
    "scan_terminal_throughput": ("TestStage6ScanTerminalThroughputProbe", ("value_files_per_second",)),
    "main_thread_frame_budget": ("TestStage6MainThreadFrameBudgetProbe", ("value_ms",)),
}
MIN_ROUNDS = 5
QUANTUM = Decimal("0.000001")


def _finite(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def derive_bound(values: list[float], direction: str) -> Decimal:
    if direction not in {"upper", "lower"} or len(values) != MIN_ROUNDS or not all(_finite(v) for v in values):
        raise ValueError("five finite values and a valid direction are required")
    with localcontext() as context:
        context.prec = 50
        data = [Decimal(str(value)) for value in values]
        median = sorted(data)[len(data) // 2]
        mean = sum(data) / Decimal(len(data))
        sigma = (sum((value - mean) ** 2 for value in data) / Decimal(len(data))).sqrt()
        raw = median + Decimal(3) * sigma if direction == "upper" else median - Decimal(3) * sigma
        return raw.quantize(QUANTUM, rounding=ROUND_CEILING if direction == "upper" else ROUND_FLOOR)


def _close(left: Any, right: float) -> bool:
    return _finite(left) and math.isclose(left, right, rel_tol=1e-9, abs_tol=1e-12)


def _validate_metric(name: str, metric: Any) -> list[str]:
    prefix = f"metric `{name}`"
    if not isinstance(metric, dict):
        return [f"{prefix} must be an object"]
    if metric.get("status") == "environment_gate":
        gate = metric.get("environment_gate")
        reason = gate.get("reason") if isinstance(gate, dict) else None
        reason = reason.strip() if isinstance(reason, str) else "unspecified"
        return [f"{prefix} environment gate: {reason}"]
    errors: list[str] = []
    spec = METRIC_SPECS[name]
    expected_fields = {"status", "name", "metric", "unit", "rounds", "statistics", "noise_budget"}
    expected_fields |= ({"product_gate", "empirical_baseline_limit"}
                        if name == "main_thread_frame_budget" else {"limit"})
    if set(metric) != expected_fields:
        errors.append(f"{prefix} unexpected fields; expected {sorted(expected_fields)}")
    if metric.get("status") != "measured":
        errors.append(f"{prefix} status must be `measured`")
    for key in ("name", "metric", "unit"):
        if metric.get(key) != spec[key]:
            errors.append(f"{prefix} {key} must be `{spec[key]}`")

    rounds = metric.get("rounds")
    if not isinstance(rounds, list) or len(rounds) != MIN_ROUNDS:
        count = len(rounds) if isinstance(rounds, list) else 0
        errors.append(f"{prefix} requires exactly {MIN_ROUNDS} measured rounds; got {count}")
    if isinstance(rounds, list):
        for index, round_data in enumerate(rounds, 1):
            if not isinstance(round_data, dict):
                errors.append(f"{prefix} round {index} must be an object")
                continue
            if set(round_data) != {"value", "raw_evidence"}:
                errors.append(f"{prefix} round {index} must contain only value/raw_evidence")
            if not _finite(round_data.get("value")):
                errors.append(f"{prefix} round {index} value must be finite")
            evidence = round_data.get("raw_evidence")
            if not isinstance(evidence, str) or not evidence.strip():
                errors.append(f"{prefix} round {index} missing raw evidence")
            elif Path(evidence).is_absolute() or ".." in Path(evidence).parts:
                errors.append(f"{prefix} round {index} raw evidence must be a portable relative path")

    stats = metric.get("statistics")
    if not isinstance(stats, dict):
        errors.append(f"{prefix} missing statistics")
        stats = {}
    elif set(stats) != {"median", "p95", "max"}:
        errors.append(f"{prefix} statistics fields are invalid")
    for key in ("median", "p95", "max"):
        if not _finite(stats.get(key)):
            errors.append(f"{prefix} statistic `{key}` must be finite")
    values = [item.get("value") for item in rounds] if isinstance(rounds, list) and all(isinstance(item, dict) for item in rounds) else []
    if len(values) == MIN_ROUNDS and all(_finite(value) for value in values):
        ordered = sorted(values)
        expected_stats = statistics.median(values), ordered[-1], ordered[-1]
        if not all(_close(stats.get(key), value) for key, value in zip(("median", "p95", "max"), expected_stats)):
            errors.append(f"{prefix} statistics do not match frozen rounds")

    noise = metric.get("noise_budget")
    if not isinstance(noise, dict):
        errors.append(f"{prefix} missing noise budget")
        noise = {}
    elif set(noise) != {"derived_from_rounds", "estimator", "standard_deviation", "coefficient_of_variation"}:
        errors.append(f"{prefix} noise budget fields are invalid")
    derived = noise.get("derived_from_rounds")
    if type(derived) is not int or derived != MIN_ROUNDS:
        errors.append(f"{prefix} noise budget must derive from exactly {MIN_ROUNDS} rounds")
    if noise.get("estimator") != "population":
        errors.append(f"{prefix} noise estimator must be `population`")
    if not _finite(noise.get("coefficient_of_variation")) or noise.get("coefficient_of_variation", -1) < 0:
        errors.append(f"{prefix} noise coefficient must be finite and non-negative")
    if len(values) == MIN_ROUNDS and all(_finite(value) for value in values):
        sigma = statistics.pstdev(values); median = statistics.median(values)
        if not _close(noise.get("standard_deviation"), sigma) or median <= 0 or not _close(noise.get("coefficient_of_variation"), sigma / median):
            errors.append(f"{prefix} population noise values do not match frozen rounds")

    limit_name = "empirical_baseline_limit" if name == "main_thread_frame_budget" else "limit"
    limit = metric.get(limit_name)
    if not isinstance(limit, dict):
        errors.append(f"{prefix} missing {limit_name}")
        limit = {}
    elif set(limit) != {"direction", "value"}:
        errors.append(f"{prefix} limit fields are invalid")
    expected_direction = METRIC_DIRECTIONS[name]
    if limit.get("direction") != expected_direction:
        errors.append(f"{prefix} limit direction must be `{expected_direction}`")
    bound = limit.get("value")
    if not _finite(bound):
        errors.append(f"{prefix} limit value must be finite")
    elif len(values) == MIN_ROUNDS and all(_finite(value) for value in values) and Decimal(str(bound)) != derive_bound(values, expected_direction):
        errors.append(f"{prefix} limit value does not match directed population bound")
    elif expected_direction == "upper" and _finite(stats.get("max")) and stats["max"] > bound:
        label = "frame empirical baseline maximum" if name == "main_thread_frame_budget" else "maximum"
        errors.append(f"{prefix} {label} {stats['max']} exceeds {bound if name == 'main_thread_frame_budget' else f'upper bound {bound}'}")
    elif expected_direction == "lower" and _finite(stats.get("median")) and stats["median"] < bound:
        errors.append(f"{prefix} median {stats['median']} is below lower bound {bound}")
    if name == "main_thread_frame_budget":
        product = metric.get("product_gate")
        if not isinstance(product, dict) or set(product) != {"direction", "value"} or product.get("direction") != "upper" or product.get("value") != 16.0:
            errors.append(f"{prefix} frame product gate must remain 16.0 ms upper")
        elif _finite(stats.get("max")) and stats["max"] > 16.0:
            errors.append(f"{prefix} frame product maximum {stats['max']} exceeds 16.0")
    return errors


def validate_manifest(manifest: Any) -> list[str]:
    if not isinstance(manifest, dict):
        return ["manifest must be an object"]
    errors: list[str] = []
    if set(manifest) != {"schema", "entry_commit", "environment", "metrics"}:
        errors.append("manifest fields are invalid")
    if type(manifest.get("schema")) is not int or manifest["schema"] != 1:
        errors.append("manifest schema must be 1")
    commit = manifest.get("entry_commit")
    if not isinstance(commit, str) or re.fullmatch(r"[0-9a-fA-F]{40}", commit) is None:
        errors.append("missing valid entry commit")
    environment = manifest.get("environment")
    if not isinstance(environment, dict):
        errors.append("missing environment object")
        environment = {}
    elif set(environment) != {"machine", "compiler", "build_configuration"}:
        errors.append("environment fields are invalid")
    for key in ("machine", "compiler", "build_configuration"):
        value = environment.get(key)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"missing environment field `{key}`")
    metrics = manifest.get("metrics")
    if not isinstance(metrics, dict):
        errors.append("missing metrics object")
        metrics = {}
    for name in sorted(set(metrics) - REQUIRED_METRICS):
        errors.append(f"unknown metric `{name}`")
    for name in sorted(REQUIRED_METRICS):
        if name not in metrics:
            errors.append(f"missing required metric `{name}`")
        else:
            errors.extend(_validate_metric(name, metrics[name]))
    return errors


def _payload_value(payload: Any, path: tuple[str, ...]) -> Any:
    value = payload
    for key in path:
        if not isinstance(value, dict):
            return None
        value = value.get(key)
    return value


def validate_run_index(manifest: Any, run_index: Any) -> list[str]:
    """Compare one complete five-round probe run with the frozen manifest limits."""
    if not isinstance(run_index, dict):
        return ["run index must be an object"]
    errors: list[str] = []
    if run_index.get("schema") != 1 or run_index.get("kind") != "stage6-performance-run-index":
        errors.append("run index schema/kind is invalid")
    if run_index.get("status") != "complete":
        errors.append("run index status must be `complete`")
    if run_index.get("rounds_requested") != MIN_ROUNDS:
        errors.append(f"run index must request exactly {MIN_ROUNDS} rounds")
    probes = run_index.get("probes")
    metrics = manifest.get("metrics") if isinstance(manifest, dict) else None
    if not isinstance(probes, dict) or not isinstance(metrics, dict):
        return errors + ["run index probes or manifest metrics are missing"]

    for metric_name, (probe_name, value_path) in RUN_INDEX_PROBES.items():
        probe = probes.get(probe_name)
        rounds = probe.get("rounds") if isinstance(probe, dict) else None
        if not isinstance(rounds, list) or len(rounds) != MIN_ROUNDS:
            errors.append(f"{metric_name} requires exactly {MIN_ROUNDS} current rounds")
            continue
        values: list[float] = []
        for round_number, record in enumerate(rounds, 1):
            payload = record.get("payload") if isinstance(record, dict) else None
            value = _payload_value(payload, value_path)
            if (not isinstance(record, dict) or record.get("exit_code") != 0 or
                    record.get("status") != "measured" or not _finite(value)):
                errors.append(f"{metric_name} round {round_number} is not a valid measured result")
                continue
            values.append(value)
        if len(values) != MIN_ROUNDS:
            continue

        frozen = metrics.get(metric_name)
        limit_name = "empirical_baseline_limit" if metric_name == "main_thread_frame_budget" else "limit"
        limit = frozen.get(limit_name) if isinstance(frozen, dict) else None
        direction = limit.get("direction") if isinstance(limit, dict) else None
        bound = limit.get("value") if isinstance(limit, dict) else None
        if direction not in {"upper", "lower"} or not _finite(bound):
            errors.append(f"{metric_name} frozen limit is invalid")
            continue
        if direction == "upper" and max(values) > bound:
            errors.append(f"{metric_name} maximum {max(values)} exceeds upper bound {bound}")
        if direction == "lower" and statistics.median(values) < bound:
            errors.append(f"{metric_name} median {statistics.median(values)} is below lower bound {bound}")
        if metric_name == "main_thread_frame_budget":
            product = frozen.get("product_gate") if isinstance(frozen, dict) else None
            product_bound = product.get("value") if isinstance(product, dict) else None
            if not _finite(product_bound) or max(values) > product_bound:
                errors.append(f"{metric_name} maximum {max(values)} exceeds product upper bound {product_bound}")
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path, help="Existing manifest to validate")
    parser.add_argument("--run-index", type=Path, help="Complete five-probe run index to compare")
    args = parser.parse_args(argv)
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"Stage 6 performance baseline invalid: {exc}")
        return 1
    errors = validate_manifest(manifest)
    if args.run_index:
        try:
            run_index = json.loads(args.run_index.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"run index invalid: {exc}")
        else:
            errors.extend(validate_run_index(manifest, run_index))
    if errors:
        print("Stage 6 performance baseline invalid:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Stage 6 performance baseline manifest passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
