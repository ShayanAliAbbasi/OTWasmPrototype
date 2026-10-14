#!/usr/bin/env python3

import argparse
import csv
import json
import math
import random
import statistics
from pathlib import Path


METRIC_METADATA = {
    "startup_to_first_frame_ms": ("ms", "lower"),
    "scene_setup_ms": ("ms", "lower"),
    "modeling_workload_total_ms": ("ms", "lower"),
    "modeling_operation_mean_ms": ("ms", "lower"),
    "plot_workload_total_ms": ("ms", "lower"),
    "plot_iteration_mean_ms": ("ms", "lower"),
    "complex_model_resource_read_ms": ("ms", "lower"),
    "complex_model_deserialize_ms": ("ms", "lower"),
    "frame_render_mean_ms": ("ms", "lower"),
    "frame_render_median_ms": ("ms", "lower"),
    "frame_render_p95_ms": ("ms", "lower"),
    "frame_interval_mean_ms": ("ms", "lower"),
    "frame_interval_p95_ms": ("ms", "lower"),
    "frame_rate_fps": ("fps", "higher"),
}


def percentile(values, probability):
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def interval(values, confidence_level):
    tail = (1.0 - confidence_level) / 2.0
    return {
        "lower": percentile(values, tail),
        "upper": percentile(values, 1.0 - tail),
    }


def cliffs_delta(native_values, web_values):
    greater = 0
    lower = 0
    for web_value in web_values:
        for native_value in native_values:
            if web_value > native_value:
                greater += 1
            elif web_value < native_value:
                lower += 1
    return (greater - lower) / (len(native_values) * len(web_values))


def effect_magnitude(delta):
    absolute = abs(delta)
    if absolute < 0.147:
        return "negligible"
    if absolute < 0.33:
        return "small"
    if absolute < 0.474:
        return "medium"
    return "large"


def target_statistics(values, bootstrap_means, confidence_level):
    mean = statistics.fmean(values)
    standard_deviation = statistics.stdev(values) if len(values) > 1 else None
    return {
        "count": len(values),
        "mean": mean,
        "median": statistics.median(values),
        "sample_standard_deviation": standard_deviation,
        "coefficient_of_variation_percent": (
            100.0 * standard_deviation / mean if standard_deviation is not None and mean else None
        ),
        "bootstrap_mean_confidence_interval": interval(bootstrap_means, confidence_level),
    }


parser = argparse.ArgumentParser()
parser.add_argument("session_dir")
parser.add_argument("--bootstrap-resamples", type=int, default=20000)
parser.add_argument("--confidence-level", type=float, default=0.95)
parser.add_argument("--seed", type=int, default=20260907)
args = parser.parse_args()

if args.bootstrap_resamples < 1000:
    raise SystemExit("--bootstrap-resamples must be at least 1000")
if not 0.0 < args.confidence_level < 1.0:
    raise SystemExit("--confidence-level must be between zero and one")

session_dir = Path(args.session_dir).resolve()
summary = json.loads((session_dir / "summary.json").read_text(encoding="utf-8"))
rng = random.Random(args.seed)
analysis = {
    "schema_version": 1,
    "method": {
        "independent_unit": "one measured application launch",
        "confidence_interval": "independent nonparametric percentile bootstrap",
        "confidence_level": args.confidence_level,
        "bootstrap_resamples": args.bootstrap_resamples,
        "random_seed": args.seed,
        "effect_size": "Cliff's delta, web relative to native",
        "outlier_policy": summary["outlier_policy"],
        "multiple_testing": "No null-hypothesis significance tests are reported.",
    },
    "comparison_cautions": [
        "Targets were acquired sequentially (native, then web), not randomized or interleaved.",
        "Frame samples within a launch are summarized before cross-launch analysis to avoid treating nested frames as independent observations.",
        "Startup-to-first-frame is application-local; browser navigation and WebAssembly download/instantiation are recorded separately.",
        "The graphics stacks differ by target, so render results characterize these complete deployment paths rather than WebAssembly CPU cost alone.",
    ],
    "excluded_cross_target_metrics": summary["cross_target_ratio_exclusions"],
    "metrics": {},
}

csv_rows = []
for metric, (unit, preferred_direction) in METRIC_METADATA.items():
    metric_analysis = {
        "unit": unit,
        "preferred_direction": preferred_direction,
    }
    row = {
        "metric": metric,
        "unit": unit,
        "preferred_direction": preferred_direction,
    }
    bootstrap_means = {}
    values_by_target = {}
    for target in ("native", "web"):
        if target not in summary["targets"]:
            continue
        values = summary["targets"][target][metric]["values"]
        values_by_target[target] = values
        count = len(values)
        means = [
            sum(rng.choice(values) for _ in range(count)) / count
            for _ in range(args.bootstrap_resamples)
        ]
        bootstrap_means[target] = means
        target_result = target_statistics(values, means, args.confidence_level)
        metric_analysis[target] = target_result
        row.update({
            f"{target}_count": target_result["count"],
            f"{target}_mean": target_result["mean"],
            f"{target}_mean_ci_lower": target_result["bootstrap_mean_confidence_interval"]["lower"],
            f"{target}_mean_ci_upper": target_result["bootstrap_mean_confidence_interval"]["upper"],
            f"{target}_cv_percent": target_result["coefficient_of_variation_percent"],
        })

    if "native" in values_by_target and "web" in values_by_target:
        native_values = values_by_target["native"]
        web_values = values_by_target["web"]
        differences = [
            web_mean - native_mean
            for native_mean, web_mean in zip(bootstrap_means["native"], bootstrap_means["web"])
        ]
        ratios = [
            web_mean / native_mean
            for native_mean, web_mean in zip(bootstrap_means["native"], bootstrap_means["web"])
            if native_mean
        ]
        native = metric_analysis["native"]
        web = metric_analysis["web"]
        mean_ratio = web["mean"] / native["mean"]
        ratio_interval = interval(ratios, args.confidence_level)
        delta = cliffs_delta(native_values, web_values)
        comparison = {
            "web_minus_native_mean": web["mean"] - native["mean"],
            "web_minus_native_mean_confidence_interval": interval(differences, args.confidence_level),
            "web_to_native_mean_ratio": mean_ratio,
            "web_to_native_mean_ratio_confidence_interval": ratio_interval,
            "web_percent_change": 100.0 * (mean_ratio - 1.0),
            "web_percent_change_confidence_interval": {
                "lower": 100.0 * (ratio_interval["lower"] - 1.0),
                "upper": 100.0 * (ratio_interval["upper"] - 1.0),
            },
            "cliffs_delta": delta,
            "cliffs_delta_magnitude": effect_magnitude(delta),
        }
        metric_analysis["comparison"] = comparison
        row.update({
            "web_to_native_mean_ratio": comparison["web_to_native_mean_ratio"],
            "ratio_ci_lower": ratio_interval["lower"],
            "ratio_ci_upper": ratio_interval["upper"],
            "web_percent_change": comparison["web_percent_change"],
            "cliffs_delta": delta,
            "cliffs_delta_magnitude": comparison["cliffs_delta_magnitude"],
        })

    analysis["metrics"][metric] = metric_analysis
    csv_rows.append(row)

json_path = session_dir / "statistical-analysis.json"
csv_path = session_dir / "statistical-analysis.csv"
json_path.write_text(json.dumps(analysis, indent=2) + "\n", encoding="utf-8")
with csv_path.open("w", newline="", encoding="utf-8") as stream:
    fieldnames = list(dict.fromkeys(key for row in csv_rows for key in row))
    writer = csv.DictWriter(stream, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(csv_rows)

print(f"Wrote {json_path}")
print(f"Wrote {csv_path}")
