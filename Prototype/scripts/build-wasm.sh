#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prototype_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${prototype_dir}/.." && pwd)"
development_root="$(cd "${repo_root}/.." && pwd)"

thirdparty_root="${OPENTWIN_THIRDPARTY_ROOT:-}"
if [[ -z "${thirdparty_root}" ]]; then
  for candidate in "${repo_root}/ThirdParty" "${development_root}/ThirdParty"; do
    if [[ -d "${candidate}/OpenSceneGraph/OpenSceneGraph-OpenSceneGraph-3.6.3" && -d "${candidate}/qwt/qwt-6.2.0" ]]; then
      thirdparty_root="${candidate}"
      break
    fi
  done
fi
if [[ -z "${thirdparty_root}" ]]; then
  echo "OpenTwin ThirdParty sources were not found. Set OPENTWIN_THIRDPARTY_ROOT." >&2
  exit 1
fi

qt_version="${QT_VERSION:-6.11.0}"
osg_root="${OPENTWIN_OSG_ROOT:-"${thirdparty_root}/OpenSceneGraph/OpenSceneGraph-OpenSceneGraph-3.6.3"}"
osg_build_dir="${OPENTWIN_OSG_BUILD_DIR:-"${prototype_dir}/build/osg-wasm"}"
qt_cmake="${QT_WASM_CMAKE:-"${HOME}/Qt/${qt_version}/wasm_singlethread/bin/qt-cmake"}"
build_dir="${OPENTWIN_QT_WASM_BUILD_DIR:-"${prototype_dir}/build/wasm"}"
em_cache="${OPENTWIN_EM_CACHE:-"${prototype_dir}/build/emscripten-cache"}"
jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}"

OPENTWIN_THIRDPARTY_ROOT="${thirdparty_root}" OPENTWIN_OSG_ROOT="${osg_root}" OPENTWIN_OSG_BUILD_DIR="${osg_build_dir}" OPENTWIN_EM_CACHE="${em_cache}" "${script_dir}/build-osg-wasm.sh"

export EM_CACHE="${em_cache}"

env QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 "${qt_cmake}" \
  -S "${prototype_dir}" \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENTWIN_THIRDPARTY_ROOT="${thirdparty_root}" \
  -DOPENTWIN_OSG_ROOT="${osg_root}" \
  -DOPENTWIN_OSG_BUILD_DIR="${osg_build_dir}" \
  -DOPENTWIN_EM_CACHE="${em_cache}"

env QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 cmake --build "${build_dir}" -j "${jobs}"
