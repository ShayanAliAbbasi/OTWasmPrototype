#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prototype_dir="$(cd "${script_dir}/.." && pwd)"
node_bin="${NODE_BIN:-$(command -v node || true)}"

if [[ -z "${node_bin}" ]]; then
  echo "Node.js 20 or newer is required for the browser benchmark harness." >&2
  exit 1
fi

node_major="$(${node_bin} -p 'Number(process.versions.node.split(".")[0])' 2>/dev/null || echo 0)"
if [[ "${node_major}" -lt 20 ]]; then
  echo "Node.js 20 or newer is required; selected executable is $(${node_bin} --version 2>/dev/null || echo unknown). Set NODE_BIN to a compatible executable." >&2
  exit 1
fi

if [[ -n "${PLAYWRIGHT_NODE_MODULES:-}" ]]; then
  export NODE_PATH="${PLAYWRIGHT_NODE_MODULES}${NODE_PATH:+:${NODE_PATH}}"
elif [[ -z "${NODE_PATH:-}" ]]; then
  if [[ -d "${prototype_dir}/node_modules/playwright" ]]; then
    export NODE_PATH="${prototype_dir}/node_modules"
  fi
fi

if ! "${node_bin}" -e "require.resolve('playwright')" >/dev/null 2>&1; then
  echo "The Playwright Node package was not found. Set NODE_PATH or PLAYWRIGHT_NODE_MODULES to its node_modules directory." >&2
  exit 1
fi

exec "${node_bin}" "${script_dir}/run-web.cjs" "$@"
