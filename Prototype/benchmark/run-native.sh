#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prototype_dir="$(cd "${script_dir}/.." && pwd)"

session_dir="${1:?Usage: run-native.sh SESSION_DIR RUNS WARMUP_RUNS ENTITIES WARMUP_FRAMES FRAMES OPERATIONS PLOT_ITERATIONS}"
runs="${2:?}"
warmup_runs="${3:?}"
entities="${4:?}"
warmup_frames="${5:?}"
frames="${6:?}"
operations="${7:?}"
plot_iterations="${8:?}"
binary="${OPENTWIN_NATIVE_BINARY:-${prototype_dir}/build/macos/OpenTwinPrototype}"

if [[ ! -x "${binary}" ]]; then
  echo "Native prototype executable not found: ${binary}" >&2
  exit 1
fi

mkdir -p "${session_dir}/native" "${session_dir}/logs/native"

run_one() {
  local kind="$1"
  local index="$2"
  local padded
  local output
  local log
  local run_id

  padded="$(printf '%03d' "${index}")"
  run_id="native-${kind}-${padded}"
  output="${session_dir}/native/${kind}-run-${padded}.json"
  log="${session_dir}/logs/native/${kind}-run-${padded}.log"

  "${binary}" \
    --benchmark \
    --benchmark-output "${output}" \
    --benchmark-run-id "${run_id}" \
    --benchmark-entities "${entities}" \
    --benchmark-warmup-frames "${warmup_frames}" \
    --benchmark-frames "${frames}" \
    --benchmark-operations "${operations}" \
    --benchmark-plot-iterations "${plot_iterations}" \
    >"${log}" 2>&1

  if [[ ! -s "${output}" ]]; then
    echo "Native benchmark did not produce ${output}; see ${log}" >&2
    exit 1
  fi
  python3 -B -c 'import json,sys; result=json.load(open(sys.argv[1])); assert result["metrics"]["complex_model_loaded"] is True, "compressed OSGB benchmark model did not load"' "${output}"
  echo "Native ${kind} run ${padded} complete"
}

for ((i = 1; i <= warmup_runs; ++i)); do
  run_one warmup "${i}"
done

for ((i = 1; i <= runs; ++i)); do
  run_one measured "${i}"
done
