#!/usr/bin/env python3

import argparse
import csv
import json
import math
import statistics
from pathlib import Path


METRICS = {
    "startup_to_first_frame_ms": lambda m, r: m["startup_to_first_frame_ms"],
    "scene_setup_ms": lambda m, r: m["scene_setup_ms"],
    "modeling_workload_total_ms": lambda m, r: m["modeling_workload_total_ms"],
    "modeling_operation_mean_ms": lambda m, r: m["modeling_operation_mean_ms"],
    "plot_workload_total_ms": lambda m, r: m["plot_workload_total_ms"],
    "plot_iteration_mean_ms": lambda m, r: m["plot_iteration_mean_ms"],
    "complex_model_resource_read_ms": lambda m, r: m["complex_model_resource_read_ms"],
    "complex_model_deserialize_ms": lambda m, r: m["complex_model_deserialize_ms"],
    "complex_model_payload_bytes": lambda m, r: m["complex_model_payload_bytes"],
    "frame_render_mean_ms": lambda m, r: m["frame_render_ms"]["mean"],
    "frame_render_median_ms": lambda m, r: m["frame_render_ms"]["median"],
    "frame_render_p95_ms": lambda m, r: m["frame_render_ms"]["p95"],
    "frame_interval_mean_ms": lambda m, r: m["frame_interval_ms"]["mean"],
    "frame_interval_p95_ms": lambda m, r: m["frame_interval_ms"]["p95"],
    "frame_rate_fps": lambda m, r: 1000.0 / m["frame_interval_ms"]["mean"],
    "memory_footprint_bytes": lambda m, r: m["memory_footprint_bytes"],
    "browser_navigation_to_result_ms": lambda m, r: r.get("harness", {}).get("browser_navigation_to_result_ms"),
}

CROSS_TARGET_RATIO_EXCLUSIONS = {
    "memory_footprint_bytes": "Native peak RSS and WebAssembly linear-memory allocation are not equivalent measures.",
    "browser_navigation_to_result_ms": "Browser navigation has no native counterpart in this harness.",
    "complex_model_payload_bytes": "Payload size is a workload identity check, not a performance measurement.",
}


def describe(values):
    ordered = sorted(values)
    count = len(ordered)
    p95_index = max(0, math.ceil(count * 0.95) - 1)
    return {
        "count": count,
        "mean": statistics.fmean(ordered),
        "median": statistics.median(ordered),
        "sample_standard_deviation": statistics.stdev(ordered) if count > 1 else None,
        "minimum": ordered[0],
        "maximum": ordered[-1],
        "p95": ordered[p95_index],
        "values": ordered,
    }


def load_target(session_dir, target):
    rows = []
    for path in sorted((session_dir / target).glob("measured-run-*.json")):
        result = json.loads(path.read_text(encoding="utf-8"))
        metrics = result["metrics"]
        row = {"target": target, "run_file": str(path.relative_to(session_dir)), "run_id": result.get("run_id", "")}
        for name, getter in METRICS.items():
            value = getter(metrics, result)
            row[name] = value
        rows.append(row)
    return rows


parser = argparse.ArgumentParser()
parser.add_argument("session_dir")
args = parser.parse_args()
session_dir = Path(args.session_dir).resolve()

rows = load_target(session_dir, "native") + load_target(session_dir, "web")
if not rows:
    raise SystemExit(f"No measured run files found under {session_dir}")

csv_path = session_dir / "measurements.csv"
with csv_path.open("w", newline="", encoding="utf-8") as stream:
    writer = csv.DictWriter(stream, fieldnames=["target", "run_file", "run_id", *METRICS.keys()])
    writer.writeheader()
    writer.writerows(rows)

summary = {
    "schema_version": 1,
    "outlier_policy": "No observations removed",
    "targets": {},
    "web_to_native_mean_ratio": {},
    "cross_target_ratio_exclusions": CROSS_TARGET_RATIO_EXCLUSIONS,
}
for target in ("native", "web"):
    target_rows = [row for row in rows if row["target"] == target]
    if not target_rows:
        continue
    summary["targets"][target] = {}
    for metric in METRICS:
        values = [float(row[metric]) for row in target_rows if row[metric] is not None]
        if values:
            summary["targets"][target][metric] = describe(values)

if "native" in summary["targets"] and "web" in summary["targets"]:
    for metric in METRICS:
        if metric in CROSS_TARGET_RATIO_EXCLUSIONS:
            continue
        native_metric = summary["targets"]["native"].get(metric)
        web_metric = summary["targets"]["web"].get(metric)
        if native_metric and web_metric and native_metric["mean"] != 0:
            summary["web_to_native_mean_ratio"][metric] = web_metric["mean"] / native_metric["mean"]

(session_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
print(f"Wrote {csv_path}")
print(f"Wrote {session_dir / 'summary.json'}")
