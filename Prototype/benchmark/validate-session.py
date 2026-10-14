#!/usr/bin/env python3

import argparse
import csv
import hashlib
import json
import math
import statistics
import subprocess
import sys
from pathlib import Path


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_tree(prototype_dir):
    excluded = {"build", "BenchmarkResults", "__pycache__"}
    files = [
        path for path in prototype_dir.rglob("*")
        if path.is_file()
        and path.name != ".DS_Store"
        and not excluded.intersection(path.relative_to(prototype_dir).parts)
    ]
    digest = hashlib.sha256()
    total = 0
    for path in sorted(files):
        relative = path.relative_to(prototype_dir).as_posix().encode("utf-8")
        content = path.read_bytes()
        digest.update(len(relative).to_bytes(8, "big"))
        digest.update(relative)
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
        total += len(content)
    return {"file_count": len(files), "bytes": total, "sha256": digest.hexdigest()}


parser = argparse.ArgumentParser()
parser.add_argument("session_dir")
parser.add_argument("--prototype-dir")
parser.add_argument("--allow-source-change", action="store_true")
args = parser.parse_args()

session = Path(args.session_dir).resolve()
prototype = Path(args.prototype_dir).resolve() if args.prototype_dir else session.parents[1]
repo_root = subprocess.run(
    ["git", "-C", str(prototype), "rev-parse", "--show-toplevel"],
    check=False,
    capture_output=True,
    text=True,
).stdout.strip()
repo = Path(repo_root).resolve() if repo_root else prototype.parent
metadata = json.loads((session / "session.json").read_text(encoding="utf-8"))
configuration = metadata["configuration"]
errors = []
checks = {}


def check(condition, message):
    if not condition:
        errors.append(message)


check(metadata["benchmark_protocol"] == "opentwin-prototype-v3-release-osgb", "Unexpected benchmark protocol")
viewport_width = configuration.get("viewport_width", 1024)
viewport_height = configuration.get("viewport_height", 540)
check(viewport_width == 1024, "Unexpected viewport width")
check(viewport_height == 540, "Unexpected viewport height")

expected_targets = configuration["target_execution_order"]
for target in expected_targets:
    for component in ("application", "osg"):
        key = f"{target}_{component}_build_type"
        check(metadata["build_configuration"].get(key) == "Release", f"Non-Release build: {key}")
expected_configuration = {
    "entity_count": configuration["entity_count"],
    "warmup_frames": configuration["warmup_frames"],
    "measured_frames": configuration["measured_frames"],
    "modeling_operations": configuration["modeling_operations"],
    "plot_iterations": configuration["plot_iterations"],
    "viewport_width": viewport_width,
    "viewport_height": viewport_height,
    "complex_model_format": "osgb",
    "complex_model_compressor": "zlib",
    "complex_model_drawables": 64,
    "complex_model_vertices": 270400,
    "complex_model_triangles": 524288,
}

asset_metadata = next(
    (item for item in metadata["artifacts"] if item["path"].endswith("complex_surface_zlib.osgb")),
    None,
)
check(asset_metadata is not None, "Complex-model artifact metadata is missing")
expected_payload_bytes = asset_metadata["bytes"] if asset_metadata else None
run_ids = set()
timestamps = set()
run_count = 0

for target in expected_targets:
    for kind, expected_count in (
        ("warmup", configuration["warmup_runs"]),
        ("measured", configuration["measured_runs"]),
    ):
        files = sorted((session / target).glob(f"{kind}-run-*.json"))
        check(len(files) == expected_count, f"{target}/{kind}: expected {expected_count}, found {len(files)}")
        for index, path in enumerate(files, 1):
            run_count += 1
            result = json.loads(path.read_text(encoding="utf-8"))
            run_id = f"{target}-{kind}-{index:03d}"
            check(result["schema_version"] == 1, f"{path}: schema version")
            check(result["benchmark_version"] == "2.1.0", f"{path}: benchmark version")
            check(result["run_id"] == run_id, f"{path}: run id")
            check(result["configuration"] == expected_configuration, f"{path}: configuration mismatch")
            check(run_id not in run_ids, f"Duplicate run id {run_id}")
            check(result["timestamp_utc"] not in timestamps, f"Duplicate timestamp {result['timestamp_utc']}")
            run_ids.add(run_id)
            timestamps.add(result["timestamp_utc"])

            metrics = result["metrics"]
            check(metrics["complex_model_loaded"] is True, f"{path}: compressed OSGB did not load")
            check(metrics["complex_model_payload_bytes"] == expected_payload_bytes, f"{path}: payload size")
            for metric in ("complex_model_resource_read_ms", "complex_model_deserialize_ms"):
                check(math.isfinite(metrics[metric]) and metrics[metric] > 0, f"{path}: invalid {metric}")
            for metric in ("frame_render_ms", "frame_interval_ms"):
                values = metrics[metric]["samples"]
                check(len(values) == configuration["measured_frames"], f"{path}: {metric} sample count")
                check(metrics[metric]["count"] == len(values), f"{path}: {metric} declared count")
                check(all(math.isfinite(value) and value >= 0 for value in values), f"{path}: invalid {metric} sample")
                check(abs(metrics[metric]["mean"] - statistics.fmean(values)) < 1e-9, f"{path}: {metric} mean")

            runtime = result["runtime"]
            expected_runtime_target = "native-macos" if target == "native" else "wasm-webgl2"
            check(runtime["target"] == expected_runtime_target, f"{path}: runtime target")
            check(runtime["viewport_width"] == viewport_width, f"{path}: viewport width")
            check(runtime["viewport_height"] == viewport_height, f"{path}: viewport height")
            if target == "web":
                harness = result["harness"]
                check(harness["classification"] == kind, f"{path}: harness classification")
                check(harness["run_index"] == index, f"{path}: harness run index")
                check(harness["headless"] == configuration["browser_headless"], f"{path}: browser mode")
                if not configuration["browser_headless"]:
                    check("HeadlessChrome" not in harness["browser"]["user_agent"], f"{path}: headless user agent")

checks["run_files_validated"] = run_count
checks["unique_run_ids"] = len(run_ids)
checks["unique_timestamps"] = len(timestamps)

for item in metadata["artifacts"]:
    path = repo / item["path"]
    check(path.is_file(), f"Missing artifact {path}")
    if path.is_file():
        check(path.stat().st_size == item["bytes"], f"Artifact size changed: {path}")
        check(sha256(path) == item["sha256"], f"Artifact hash changed: {path}")
checks["artifact_hashes_validated"] = len(metadata["artifacts"])

current_tree = source_tree(prototype)
if not args.allow_source_change:
    check(current_tree == metadata["source"]["prototype_tree"], "Prototype source tree changed since acquisition")
checks["source_tree_at_validation"] = current_tree
checks["source_tree_match_required"] = not args.allow_source_change

diagnostic_patterns = (
    "[warning]", "[error]", "[pageerror]", "invalid_enum", "invalid_operation",
    "failed to", "still referenced", "context is lost", "no such compressor",
)
diagnostics = []
logs = sorted((session / "logs").rglob("*.log"))
for path in logs:
    for line_number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        lower = line.lower()
        if any(pattern in lower for pattern in diagnostic_patterns):
            diagnostics.append(f"{path.relative_to(session)}:{line_number}: {line[:300]}")
expected_log_count = len(expected_targets) * (configuration["warmup_runs"] + configuration["measured_runs"])
check(len(logs) == expected_log_count, f"Expected {expected_log_count} logs, found {len(logs)}")
check(not diagnostics, f"Found {len(diagnostics)} diagnostic log lines")
checks["logs_scanned"] = len(logs)
checks["diagnostic_lines"] = diagnostics

with (session / "measurements.csv").open(newline="", encoding="utf-8") as stream:
    csv_rows = list(csv.DictReader(stream))
expected_rows = len(expected_targets) * configuration["measured_runs"]
check(len(csv_rows) == expected_rows, f"Expected {expected_rows} measurement rows, found {len(csv_rows)}")
for target in expected_targets:
    count = sum(row["target"] == target for row in csv_rows)
    check(count == configuration["measured_runs"], f"CSV {target} row count")
checks["measurement_rows"] = len(csv_rows)

summary = json.loads((session / "summary.json").read_text(encoding="utf-8"))
for target in expected_targets:
    for metric, values in summary["targets"][target].items():
        check(values["count"] == configuration["measured_runs"], f"Summary count {target}/{metric}")
        check(len(values["values"]) == configuration["measured_runs"], f"Summary values {target}/{metric}")
check("memory_footprint_bytes" not in summary["web_to_native_mean_ratio"], "Memory ratio must be excluded")
check("complex_model_payload_bytes" not in summary["web_to_native_mean_ratio"], "Payload ratio must be excluded")
checks["summary_metrics_per_target"] = {
    target: len(summary["targets"][target]) for target in expected_targets
}

analysis = json.loads((session / "statistical-analysis.json").read_text(encoding="utf-8"))
check(analysis["method"]["independent_unit"] == "one measured application launch", "Analysis unit mismatch")
check(analysis["method"]["bootstrap_resamples"] >= 1000, "Insufficient bootstrap resamples")
for metric, values in analysis["metrics"].items():
    for target in expected_targets:
        check(values[target]["count"] == configuration["measured_runs"], f"Analysis count {target}/{metric}")
    if expected_targets == ["native", "web"]:
        comparison = values.get("comparison")
        check(comparison is not None, f"Missing cross-target analysis for {metric}")
        if comparison:
            check(math.isfinite(comparison["web_to_native_mean_ratio"]), f"Invalid ratio for {metric}")
with (session / "statistical-analysis.csv").open(newline="", encoding="utf-8") as stream:
    analysis_rows = list(csv.DictReader(stream))
check(len(analysis_rows) == len(analysis["metrics"]), "Statistical CSV row count")
checks["statistical_metrics"] = len(analysis["metrics"])

report = {
    "schema_version": 1,
    "status": "passed" if not errors else "failed",
    "session": str(session),
    "checks": checks,
    "errors": errors,
}
(session / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
sys.exit(0 if not errors else 1)
