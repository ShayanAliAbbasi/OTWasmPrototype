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
osg_build_dir="${OPENTWIN_OSG_BUILD_DIR:-"${prototype_dir}/build/osg-macos"}"
qt_cmake="${QT_MACOS_CMAKE:-"${HOME}/Qt/${qt_version}/macos/bin/qt-cmake"}"
build_dir="${OPENTWIN_QT_MACOS_BUILD_DIR:-"${prototype_dir}/build/macos"}"
jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}"
macos_arch="${CMAKE_OSX_ARCHITECTURES:-arm64}"

reset_stale_macos_sdk_cache() {
  local cmake_build_dir="$1"
  local cache_file="${cmake_build_dir}/CMakeCache.txt"

  [[ -f "${cache_file}" ]] || return 0

  local sdk_path
  while IFS= read -r sdk_path; do
    if [[ ! -d "${sdk_path}" ]]; then
      echo "Removing stale CMake configuration referencing missing SDK: ${sdk_path}"
      cmake -E rm -f "${cache_file}"
      cmake -E rm -rf "${cmake_build_dir}/CMakeFiles"
      return 0
    fi
  done < <(
    grep -Eo '/Applications/Xcode\.app/Contents/Developer/Platforms/MacOSX\.platform/Developer/SDKs/MacOSX[^/]*\.sdk' \
      "${cache_file}" | sort -u || true
  )
}

# CMake caches absolute SDK paths discovered through Qt, Zlib, and Apple
# frameworks. Xcode removes versioned SDK directories during an update.
reset_stale_macos_sdk_cache "${osg_build_dir}"
reset_stale_macos_sdk_cache "${build_dir}"

cmake \
  -S "${osg_root}" \
  -B "${osg_build_dir}" \
  -G Ninja \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_OSG_APPLICATIONS=OFF \
  -DBUILD_OSG_EXAMPLES=OFF \
  -DBUILD_OSG_DEPRECATED_SERIALIZERS=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_GLIB=ON \
  -DDYNAMIC_OPENTHREADS=OFF \
  -DDYNAMIC_OPENSCENEGRAPH=OFF \
  -DOPENGL_PROFILE=GL2 \
  -DOSG_WINDOWING_SYSTEM=None \
  -DCMAKE_OSX_ARCHITECTURES="${macos_arch}" \
  -DCMAKE_CXX_FLAGS="-std=gnu++14"

cmake --build "${osg_build_dir}" --target \
  osgViewer osgDB osgGA osgUtil osgText osg OpenThreads \
  osgAnimation osgFX osgManipulator osgParticle osgPresentation osgShadow osgSim osgTerrain osgUI osgVolume osgWidget \
  osgdb_jpeg osgdb_osg osgdb_serializers_osg \
  osgdb_serializers_osganimation osgdb_serializers_osgfx osgdb_serializers_osgga osgdb_serializers_osgmanipulator \
  osgdb_serializers_osgparticle osgdb_serializers_osgshadow osgdb_serializers_osgsim osgdb_serializers_osgterrain \
  osgdb_serializers_osgtext osgdb_serializers_osgui osgdb_serializers_osgutil osgdb_serializers_osgviewer osgdb_serializers_osgvolume \
  -j "${jobs}"

env QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 "${qt_cmake}" \
  -S "${prototype_dir}" \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENTWIN_THIRDPARTY_ROOT="${thirdparty_root}" \
  -DOPENTWIN_OSG_ROOT="${osg_root}" \
  -DOPENTWIN_OSG_BUILD_DIR="${osg_build_dir}" \
  -DCMAKE_OSX_ARCHITECTURES="${macos_arch}"

env QTFRAMEWORK_BYPASS_LICENSE_CHECK=1 cmake --build "${build_dir}" -j "${jobs}"
