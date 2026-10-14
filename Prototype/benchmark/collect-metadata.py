#!/usr/bin/env python3

import argparse
import hashlib
import json
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def command_output(command):
    try:
        return subprocess.run(command, check=True, capture_output=True, text=True).stdout.strip()
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None


def artifact(path):
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest.hexdigest()}


def first_line(command):
    output = command_output(command)
    return output.splitlines()[0] if output else None


def cmake_cache_value(path, name):
    if not path.is_file():
        return None
    prefix = f"{name}:"
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1] if "=" in line else None
    return None


def source_tree(prototype_dir):
    excluded_parts = {"build", "BenchmarkResults", "__pycache__"}
    files = [
        path for path in prototype_dir.rglob("*")
        if path.is_file()
        and path.name != ".DS_Store"
        and not excluded_parts.intersection(path.relative_to(prototype_dir).parts)
    ]
    digest = hashlib.sha256()
    total_bytes = 0
    for path in sorted(files):
        relative = path.relative_to(prototype_dir).as_posix().encode("utf-8")
        content = path.read_bytes()
        digest.update(len(relative).to_bytes(8, "big"))
        digest.update(relative)
        digest.update(len(content).to_bytes(8, "big"))
        digest.update(content)
        total_bytes += len(content)
    return {"file_count": len(files), "bytes": total_bytes, "sha256": digest.hexdigest()}


parser = argparse.ArgumentParser()
parser.add_argument("--output", required=True)
parser.add_argument("--prototype-dir", required=True)
parser.add_argument("--runs", type=int, required=True)
parser.add_argument("--warmup-runs", type=int, required=True)
parser.add_argument("--entities", type=int, required=True)
parser.add_argument("--warmup-frames", type=int, required=True)
parser.add_argument("--frames", type=int, required=True)
parser.add_argument("--operations", type=int, required=True)
parser.add_argument("--plot-iterations", type=int, required=True)
parser.add_argument("--target", required=True)
parser.add_argument("--headless", action="store_true")
args = parser.parse_args()

prototype_dir = Path(args.prototype_dir).resolve()
repo_root = command_output(["git", "-C", str(prototype_dir), "rev-parse", "--show-toplevel"])
repo_dir = Path(repo_root).resolve() if repo_root else prototype_dir.parent
git_status = command_output(["git", "-C", str(repo_dir), "status", "--porcelain", "--", str(prototype_dir)]) or ""
artifacts = []
for candidate in (
    prototype_dir / "benchmark/assets/complex_surface_zlib.osgb",
    prototype_dir / "build/macos/OpenTwinPrototype",
    prototype_dir / "build/wasm/OpenTwinPrototype.html",
    prototype_dir / "build/wasm/OpenTwinPrototype.js",
    prototype_dir / "build/wasm/OpenTwinPrototype.wasm",
):
    value = artifact(candidate)
    if value:
        value["path"] = str(candidate.relative_to(repo_dir))
        artifacts.append(value)

metadata = {
    "schema_version": 1,
    "benchmark_protocol": "opentwin-prototype-v3-release-osgb",
    "created_utc": datetime.now(timezone.utc).isoformat(),
    "target_selection": args.target,
    "configuration": {
        "measured_runs": args.runs,
        "warmup_runs": args.warmup_runs,
        "entity_count": args.entities,
        "warmup_frames": args.warmup_frames,
        "measured_frames": args.frames,
        "modeling_operations": args.operations,
        "plot_iterations": args.plot_iterations,
        "viewport_width": 1024,
        "viewport_height": 540,
        "browser_headless": args.headless,
        "target_execution_order": [target for target in ("native", "web") if args.target in ("all", target)],
        "run_order_within_target": "warmup launches followed by measured launches",
    },
    "host": {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "macos_version": command_output(["sw_vers", "-productVersion"]),
        "cpu_brand": command_output(["sysctl", "-n", "machdep.cpu.brand_string"]),
        "logical_cpu_count": command_output(["sysctl", "-n", "hw.logicalcpu"]),
        "physical_memory_bytes": command_output(["sysctl", "-n", "hw.memsize"]),
        "power_status": command_output(["pmset", "-g", "batt"]),
        "chrome_version": command_output(["/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", "--version"]),
    },
    "toolchain": {
        "cmake_version": first_line(["cmake", "--version"]),
        "native_compiler_version": first_line(["clang++", "--version"]),
        "emscripten_version": first_line(["emcc", "--version"]),
        "shell_node_version": command_output(["node", "--version"]),
        "python_version": platform.python_version(),
    },
    "build_configuration": {
        "native_application_build_type": cmake_cache_value(
            prototype_dir / "build/macos/CMakeCache.txt", "CMAKE_BUILD_TYPE"
        ),
        "web_application_build_type": cmake_cache_value(
            prototype_dir / "build/wasm/CMakeCache.txt", "CMAKE_BUILD_TYPE"
        ),
        "native_osg_build_type": cmake_cache_value(
            prototype_dir / "build/osg-macos/CMakeCache.txt", "CMAKE_BUILD_TYPE"
        ),
        "web_osg_build_type": cmake_cache_value(
            prototype_dir / "build/osg-wasm/CMakeCache.txt", "CMAKE_BUILD_TYPE"
        ),
    },
    "source": {
        "git_commit": command_output(["git", "-C", str(repo_dir), "rev-parse", "HEAD"]),
        "prototype_worktree_dirty": bool(git_status),
        "prototype_changed_path_count": len(git_status.splitlines()),
        "prototype_tree": source_tree(prototype_dir),
    },
    "artifacts": artifacts,
}

Path(args.output).write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
