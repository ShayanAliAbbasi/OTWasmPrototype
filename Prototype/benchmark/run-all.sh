#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prototype_dir="$(cd "${script_dir}/.." && pwd)"

runs=30
warmup_runs=3
entities=250
warmup_frames=60
frames=300
operations=60
plot_iterations=60
target=all
build=1
headless=0
session_dir=""

usage() {
  echo "Usage: run-all.sh [--runs N] [--warmup-runs N] [--entities N] [--frames N] [--warmup-frames N] [--operations N] [--plot-iterations N] [--target all|native|web] [--skip-build] [--headless] [--output DIR]"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --runs) runs="$2"; shift 2 ;;
    --warmup-runs) warmup_runs="$2"; shift 2 ;;
    --entities) entities="$2"; shift 2 ;;
    --warmup-frames) warmup_frames="$2"; shift 2 ;;
    --frames) frames="$2"; shift 2 ;;
    --operations) operations="$2"; shift 2 ;;
    --plot-iterations) plot_iterations="$2"; shift 2 ;;
    --target) target="$2"; shift 2 ;;
    --skip-build) build=0; shift ;;
    --headless) headless=1; shift ;;
    --output) session_dir="$2"; shift 2 ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 1 ;;
  esac
done

if [[ "${target}" != "all" && "${target}" != "native" && "${target}" != "web" ]]; then
  echo "--target must be all, native, or web" >&2
  exit 1
fi

require_positive_integer() {
  local name="$1"
  local value="$2"
  if [[ ! "${value}" =~ ^[1-9][0-9]*$ ]]; then
    echo "${name} must be a positive integer" >&2
    exit 1
  fi
}

require_positive_integer --runs "${runs}"
require_positive_integer --entities "${entities}"
require_positive_integer --warmup-frames "${warmup_frames}"
require_positive_integer --frames "${frames}"
require_positive_integer --operations "${operations}"
require_positive_integer --plot-iterations "${plot_iterations}"
if [[ ! "${warmup_runs}" =~ ^[0-9]+$ ]]; then
  echo "--warmup-runs must be a non-negative integer" >&2
  exit 1
fi

if [[ -z "${session_dir}" ]]; then
  session_dir="${prototype_dir}/BenchmarkResults/$(date -u +%Y%m%dT%H%M%SZ)"
fi
mkdir -p "${session_dir}"
session_dir="$(cd "${session_dir}" && pwd)"

if [[ "${build}" -eq 1 ]]; then
  if [[ "${target}" == "all" || "${target}" == "native" ]]; then
    "${prototype_dir}/scripts/build-native-macos.sh"
  fi
  if [[ "${target}" == "all" || "${target}" == "web" ]]; then
    "${prototype_dir}/scripts/build-wasm.sh"
  fi
fi

require_release_build() {
  local target_name="$1"
  local cache_path="$2"
  if [[ ! -f "${cache_path}" ]] || ! grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "${cache_path}"; then
    echo "${target_name} must be configured with CMAKE_BUILD_TYPE=Release before benchmark acquisition: ${cache_path}" >&2
    exit 1
  fi
}

if [[ "${target}" == "all" || "${target}" == "native" ]]; then
  require_release_build "Native application" "${prototype_dir}/build/macos/CMakeCache.txt"
  require_release_build "Native OSG" "${prototype_dir}/build/osg-macos/CMakeCache.txt"
fi
if [[ "${target}" == "all" || "${target}" == "web" ]]; then
  require_release_build "WebAssembly application" "${prototype_dir}/build/wasm/CMakeCache.txt"
  require_release_build "WebAssembly OSG" "${prototype_dir}/build/osg-wasm/CMakeCache.txt"
fi

metadata_args=(
  --output "${session_dir}/session.json"
  --prototype-dir "${prototype_dir}"
  --runs "${runs}"
  --warmup-runs "${warmup_runs}"
  --entities "${entities}"
  --warmup-frames "${warmup_frames}"
  --frames "${frames}"
  --operations "${operations}"
  --plot-iterations "${plot_iterations}"
  --target "${target}"
)
if [[ "${headless}" -eq 1 ]]; then
  metadata_args+=(--headless)
fi
python3 "${script_dir}/collect-metadata.py" "${metadata_args[@]}"

if [[ "${target}" == "all" || "${target}" == "native" ]]; then
  "${script_dir}/run-native.sh" "${session_dir}" "${runs}" "${warmup_runs}" "${entities}" "${warmup_frames}" "${frames}" "${operations}" "${plot_iterations}"
fi

if [[ "${target}" == "all" || "${target}" == "web" ]]; then
  if [[ "${headless}" -eq 1 ]]; then
    BENCHMARK_HEADLESS=1 "${script_dir}/run-web.sh" "${session_dir}" "${runs}" "${warmup_runs}" "${entities}" "${warmup_frames}" "${frames}" "${operations}" "${plot_iterations}"
  else
    "${script_dir}/run-web.sh" "${session_dir}" "${runs}" "${warmup_runs}" "${entities}" "${warmup_frames}" "${frames}" "${operations}" "${plot_iterations}"
  fi
fi

python3 "${script_dir}/summarize.py" "${session_dir}"
python3 "${script_dir}/analyze.py" "${session_dir}"
python3 "${script_dir}/validate-session.py" "${session_dir}" --prototype-dir "${prototype_dir}"
echo "Benchmark session complete: ${session_dir}"
