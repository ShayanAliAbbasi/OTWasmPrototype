## Prerequisites

The repository contains the application source, the exact reused OpenTwin viewer files, required icons, benchmark model, harness, and retained thesis benchmark result dataset. Toolchains and upstream third-party source distributions are external prerequisites and are not duplicated in this repository.

- macOS on Apple Silicon with Xcode command-line tools, CMake, and Ninja.
- Qt 6.11.0 for both `macos` and `wasm_singlethread`. The default location is `$HOME/Qt/6.11.0`; set `QT_MACOS_CMAKE` or `QT_WASM_CMAKE` for another installation.
- Emscripten 4.0.7 activated in the shell for WebAssembly builds.
- OpenTwin ThirdParty sources containing OpenSceneGraph 3.6.3 and Qwt 6.2.0. Place the checkout at `ThirdParty/`, beside the repository as `../ThirdParty/`, or set `OPENTWIN_THIRDPARTY_ROOT`.
- Python 3, Google Chrome, and Node.js 20 or newer. Run `npm install --prefix Prototype` once to install the Playwright harness locally.

## OSG coverage

OSG 3.6.3 is rebuilt from an OpenTwin ThirdParty checkout for each target. The build scripts look first for `ThirdParty/` inside the repository and then for a sibling `../ThirdParty/` directory. Set `OPENTWIN_THIRDPARTY_ROOT` when the checkout is elsewhere. The build is static and uses `OSG_WINDOWING_SYSTEM=None` because Qt owns the window and graphics context. The Wasm build also enables Emscripten zlib so it can decode the compressed `.osgb` streams emitted by OpenTwin's visualization services.

The linked reader plugins are limited to formats observed in the original frontend: the `osg2` registration from `osgdb_osg` for `.osgb` scene payloads and `osgdb_jpeg` for JPG textures. The build also covers the OSG node-kit and serializer surface linked by the frontend projects: OpenThreads, osg, osgDB, osgGA, osgText, osgUtil, osgViewer, osgAnimation, osgFX, osgManipulator, osgParticle, osgPresentation, osgShadow, osgSim, osgTerrain, osgUI, osgVolume, and osgWidget.

## Build and run

From the repository root, build native macOS:

```sh
Prototype/scripts/build-native-macos.sh
Prototype/build/macos/OpenTwinPrototype
```

Build and serve WebAssembly:

```sh
Prototype/scripts/build-wasm.sh
python3 -m http.server 8084 --bind 127.0.0.1 --directory Prototype/build/wasm
```

Open `http://127.0.0.1:8084/OpenTwinPrototype.html`.

The scripts accept environment overrides including `QT_VERSION`, `QT_MACOS_CMAKE`, `QT_WASM_CMAKE`, `OPENTWIN_THIRDPARTY_ROOT`, `OPENTWIN_OSG_ROOT`, `OPENTWIN_OSG_BUILD_DIR`, `OPENTWIN_QT_MACOS_BUILD_DIR`, `OPENTWIN_QT_WASM_BUILD_DIR`, and `CMAKE_BUILD_PARALLEL_LEVEL`.

## Benchmark workflow

Run benchmark with thirty measured launches and three separate warm-up launches per target:

```sh
Prototype/benchmark/run-all.sh
```

The acquisition command requires Release-mode application and OSG builds. The build scripts configure `CMAKE_BUILD_TYPE=Release`, and `run-all.sh` stops before measurement if any selected target has a non-Release CMake cache.

The default browser run uses a visible system Chrome window so WebGL can use the normal browser GPU path. `--headless` is available for CI and smoke testing, but those measurements must not be mixed with headed GPU measurements. Other useful controls are:

```sh
Prototype/benchmark/run-all.sh \
  --runs 30 \
  --warmup-runs 3 \
  --entities 250 \
  --warmup-frames 60 \
  --frames 300 \
  --operations 60 \
  --plot-iterations 60
```

Use `--target native` or `--target web` for one target, `--skip-build` after a known build, and `--output PATH` for an explicit session directory. Browser automation uses Playwright and system Chrome. Set `PLAYWRIGHT_NODE_MODULES`, `NODE_BIN`, or `CHROME_PATH` when they are not in the default locations.

Each session under `BenchmarkResults/<UTC timestamp>/` contains:

- `session.json`: protocol, execution order, host, toolchains, source revision/tree hash, benchmark parameters, and artifact hashes/sizes.
- `native/` and `web/`: one JSON file for every warm-up and measured launch.
- `logs/`: complete native and browser console logs for each launch.
- `measurements.csv`: one row per measured launch.
- `summary.json`: mean, median, sample standard deviation, minimum, maximum, p95, raw values, valid Web/native mean ratios, and explicit ratio exclusions.
- `statistical-analysis.json` and `.csv`: deterministic 95% bootstrap confidence intervals, coefficient of variation, Web/native mean differences and ratios, and Cliff's delta effect sizes.
- `validation.json`: run-count, configuration, sample-array, model-load, artifact-hash, source-tree, log-diagnostic, CSV, and summary integrity checks.

Warm-up launches are stored but excluded from aggregation. No outlier is removed automatically. The raw JSON files are the primary observationssummaries are derived data.