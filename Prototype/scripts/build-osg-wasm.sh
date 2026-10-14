#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prototype_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${prototype_dir}/.." && pwd)"
development_root="$(cd "${repo_root}/.." && pwd)"

thirdparty_root="${OPENTWIN_THIRDPARTY_ROOT:-}"
if [[ -z "${thirdparty_root}" ]]; then
  for candidate in "${repo_root}/ThirdParty" "${development_root}/ThirdParty"; do
    if [[ -d "${candidate}/OpenSceneGraph/OpenSceneGraph-OpenSceneGraph-3.6.3" ]]; then
      thirdparty_root="${candidate}"
      break
    fi
  done
fi
if [[ -z "${thirdparty_root}" ]]; then
  echo "OpenTwin ThirdParty sources were not found. Set OPENTWIN_THIRDPARTY_ROOT." >&2
  exit 1
fi

osg_root="${OPENTWIN_OSG_ROOT:-"${thirdparty_root}/OpenSceneGraph/OpenSceneGraph-OpenSceneGraph-3.6.3"}"
build_dir="${OPENTWIN_OSG_BUILD_DIR:-"${prototype_dir}/build/osg-wasm"}"
em_cache="${OPENTWIN_EM_CACHE:-"${prototype_dir}/build/emscripten-cache"}"
jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}"

export EM_CACHE="${em_cache}"

emcc --cflags -sUSE_LIBJPEG=1 -sUSE_ZLIB=1 >/dev/null
jpeg_include_dir="${em_cache}/ports/libjpeg/jpeg-9f"
jpeg_library="${em_cache}/sysroot/lib/wasm32-emscripten/libjpeg.a"
zlib_include_dir="${em_cache}/ports/zlib/zlib-1.3.1"
zlib_library="${em_cache}/sysroot/lib/wasm32-emscripten/libz.a"

emcmake cmake \
  -S "${osg_root}" \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_OSG_APPLICATIONS=OFF \
  -DBUILD_OSG_EXAMPLES=OFF \
  -DBUILD_OSG_DEPRECATED_SERIALIZERS=OFF \
  -DCMAKE_DISABLE_FIND_PACKAGE_GLIB=ON \
  -DDYNAMIC_OPENTHREADS=OFF \
  -DDYNAMIC_OPENSCENEGRAPH=OFF \
  -DOPENGL_PROFILE=GLES2 \
  -DOSG_WINDOWING_SYSTEM=None \
  -DEGL_LIBRARY=GL \
  -DJPEG_INCLUDE_DIR="${jpeg_include_dir}" \
  -DJPEG_LIBRARY="${jpeg_library}" \
  -DZLIB_INCLUDE_DIR="${zlib_include_dir}" \
  -DZLIB_LIBRARY_RELEASE="${zlib_library}" \
  -D_OPENTHREADS_ATOMIC_USE_GCC_BUILTINS_EXITCODE=0 \
  -DCMAKE_CXX_FLAGS="-std=gnu++14"

cmake --build "${build_dir}" --target \
  osgViewer osgDB osgGA osgUtil osgText osg OpenThreads \
  osgAnimation osgFX osgManipulator osgParticle osgPresentation osgShadow osgSim osgTerrain osgUI osgVolume osgWidget \
  osgdb_jpeg osgdb_osg osgdb_serializers_osg \
  osgdb_serializers_osganimation osgdb_serializers_osgfx osgdb_serializers_osgga osgdb_serializers_osgmanipulator \
  osgdb_serializers_osgparticle osgdb_serializers_osgshadow osgdb_serializers_osgsim osgdb_serializers_osgterrain \
  osgdb_serializers_osgtext osgdb_serializers_osgui osgdb_serializers_osgutil osgdb_serializers_osgviewer osgdb_serializers_osgvolume \
  -j "${jobs}"
