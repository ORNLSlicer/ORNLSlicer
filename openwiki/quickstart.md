---
type: guide
title: ORNLSlicer OpenWiki Quickstart
description: A task-oriented maintainer route from runtime entrypoints and canonical settings through slicing, machine output, focused tests, and packaging.
tags: [quickstart, maintainers, architecture, validation]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-ced6747f13d7a40f942b17d8
    resource: repo://.codex/AGENTS.md
  - id: openwiki-source-164e2da859b5277df81c7d94
    resource: repo://.github/workflows/ci.yml
  - id: openwiki-source-362e06c30ccfdafd87339cb0
    resource: repo://ARCHITECTURE.md
  - id: openwiki-source-a60928f26402a7ffadc427b3
    resource: repo://cmake/presets/generic-llvm-ninja.json
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-65e4b6e37d34931407f2a93d
    resource: repo://include/geometry/segment_base.h
  - id: openwiki-source-36f64ad30117d774697ab275
    resource: repo://include/step/step.h
  - id: openwiki-source-23775c3de52f3ab95a13cb8b
    resource: repo://README.md
  - id: openwiki-source-90a873fbb389104f241eb359
    resource: repo://resources/configs/configs.qrc
  - id: openwiki-source-6f5d88049adbd611fb8e2754
    resource: repo://resources/settings/README.md
  - id: openwiki-source-f008b387f2b8811ff5bc17b6
    resource: repo://src/console/command_line_processor.cpp
  - id: openwiki-source-ab3dd3ea028d9cad20143b04
    resource: repo://src/console/main_control.cpp
  - id: openwiki-source-d364d949938a433276255c32
    resource: repo://src/main.cpp
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-ee47d8a7bebb804c33b330b4
    resource: repo://src/managers/settings/settings_manager.cpp
  - id: openwiki-source-b1134fc13dd24912e260b947
    resource: repo://src/threading/abs_slicing_thread.cpp
  - id: openwiki-source-4c1d7b6308a78ba9c493ce8a
    resource: repo://src/threading/gcode_loader.cpp
  - id: openwiki-source-c9b49d4d1b98e2ebff97fee2
    resource: repo://src/threading/session_loader.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# ORNLSlicer OpenWiki Quickstart

ORNLSlicer is a Qt/C++ slicing and toolpath-planning application for additive-manufacturing workflows. Its maintainable path is deliberately shared: the GUI and command line converge on the same session, settings, geometry, slicing, writer, and G-code loading subsystems. [source](repo://README.md#L1-L21) [source](repo://ARCHITECTURE.md#L14-L39)

Use this page to choose the owning subsystem, the first production source to inspect, and the smallest relevant validation. Follow the linked page when a change crosses an ownership, threading, data-model, or file-format boundary.

## First five minutes

Start by preserving the current checkout and tracing the named behavior rather than editing from a directory guess:

```bash
git status --short --branch
rg -n 'NamedType|setting_key|signalName|command_text' include src resources tests
```

Headers and implementations are normally paired beneath `include/` and `src/`; `resources/settings/` owns setting metadata, `templates/` owns installed process/printer templates, and CMake/Nix own build and package topology. [source](repo://ARCHITECTURE.md#L41-L57) [source](repo://.codex/AGENTS.md#L3-L20)

Establish the ordinary development baseline inside the repository's Nix shell:

```bash
nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer
```

The checked-in preset uses Clang, Ninja Multi-Config, and `build/generic-llvm-ninja`. For a compile-only iteration across shared production sources, replace the final target with `ornlslicer_obj`; that object library is linked by both the application and focused test executables. [source](repo://cmake/presets/generic-llvm-ninja.json#L1-L14) [source](repo://CMakeLists.txt#L139-L164) [source](repo://.codex/AGENTS.md#L39-L47)

## The entry rule: arguments select the shell

There is one source entrypoint with a strict branch:

- with no command-line arguments, it creates `QApplication`, initializes GUI resources, obtains the singleton `MainWindow`, and enters the GUI event loop;
- with any argument, including `--help` or `--version`, it creates `QCoreApplication`, converts options into a `SettingsBase`, and runs `MainControl`. [source](repo://src/main.cpp#L69-L121)

The CLI converter defines model, support, project, settings, transform, output, export, and slice-bound options. Normal slicing requires exactly one primary input form—model files, a model directory, or a project—and also requires an output location. Inspect the live interface with the built executable's `--help` rather than copying an old invocation. [source](repo://src/console/command_line_processor.cpp#L43-L112) [source](repo://src/console/command_line_processor.cpp#L114-L203)

`MainControl` applies console settings, loads models or a project through `SessionManager`, starts slicing when all parts arrive, parses non-image G-code through `GCodeLoader`, and moves the final text to the requested output name. The GUI performs the same domain work through signal/slot connections owned by `MainWindow`. [source](repo://src/console/main_control.cpp#L26-L120) [source](repo://src/console/main_control.cpp#L123-L155)

See [Application Runtime and Coordination](architecture/application-runtime.md) for startup, asynchronous control flow, progress, cancellation, shutdown, and affinity boundaries.

## End-to-end route

```text
src/main.cpp
   ├─ no args ──> MainWindow (GUI)
   └─ any args ─> CommandLineConverter -> MainControl
                              |
                              v
                    SessionManager + SettingsManager
                              |
                 model import or .s2p project load
                              |
               planar | cylindrical/helical | image
                              |
            Step / island / region / path / segment model
                              |
                    syntax-selected WriterBase
                              |
                      temporary G-code
                              |
                         GCodeLoader
                    /         |          \
                 preview   statistics   exported output
```

`SessionManager::doSlice()` reads the active syntax and slicing mode, enforces the cylindrical/Arc-Specialties gate, selects or reuses `PlanarSlicer`, `ImageSlicer`, or `CylindricalSlicer`, then emits the worker start signal. [source](repo://src/managers/session_manager.cpp#L575-L605) [source](repo://src/managers/session_manager.cpp#L654-L685)

Toolpath modes select a concrete writer from the active machine syntax. Generated or imported G-code then passes through `GCodeLoader`, which selects a parser from the header, parses motion and timing metadata, and supplies downstream consumers. [source](repo://src/threading/abs_slicing_thread.cpp#L210-L320) [source](repo://src/threading/gcode_loader.cpp#L287-L328) [source](repo://src/threading/gcode_loader.cpp#L590-L680)

## Route a change by task

| Maintainer task | Start in current source | Follow the contract |
| --- | --- | --- |
| Change startup, CLI option handling, progress, or shutdown | `src/main.cpp`, `src/console/command_line_processor.cpp`, then `src/console/main_control.cpp` or `src/windows/main_window.cpp` | [Application Runtime](architecture/application-runtime.md) |
| Change model import, session ownership, recent files, or `.s2p` save/load | `include/managers/session_manager.h`, `src/managers/session_manager.cpp`, and the relevant loader under `src/threading/` | [Session and Project Lifecycle](architecture/session-and-project-lifecycle.md) |
| Add or alter a setting, composite input, template, preference, or migration | `resources/settings/*.yaml`, then settings/preferences managers and migration code | [Settings and Preferences](architecture/settings-and-preferences.md) |
| Change mesh, coordinate, unit, `Step`, island, path, or segment semantics | the matching declaration under `include/geometry`, `include/part`, or `include/step`, then its `src/` implementation | [Geometry and Toolpath Model](concepts/geometry-and-toolpath-model.md) |
| Change slicer dispatch, phase order, progress, cancellation, or common output | `src/managers/session_manager.cpp` and `src/threading/abs_slicing_thread.cpp` | [Slicing Pipeline](architecture/slicing-pipeline.md) |
| Change planar cross-sections, layer settings, regions, ordering, or modifiers | `src/threading/slicers/planar_slicer.cpp`, `src/threading/step_thread.cpp`, then `src/step/layer/regions`, `src/optimizers`, or `src/modifiers` | [Planar Path Generation](slicing/planar-path-generation.md) |
| Change radial/helical geometry, clipping, seam, or tool orientation | `src/threading/slicers/cylindrical_slicer.cpp`, cylindrical layer implementations, and `src/slicing/helical_region_profile.cpp` | [Cylindrical and Helical Slicing](slicing/cylindrical-and-helical.md) |
| Add or change a machine dialect | syntax enum/metadata, `src/gcode/writers`, parser selection in `GCodeLoader`, and syntax-specific tests | [Machine Syntaxes](integrations/machine-syntaxes.md) |
| Change imported/generated G-code display, timing, colors, preview, or export | `src/threading/gcode_loader.cpp` and its parser, widget, or export consumer | [G-code and Visualization](architecture/gcode-and-visualization.md) |
| Change dependencies, generated build metadata, packages, AppImage, or installer | `CMakeLists.txt`, `cmake/`, `flake.nix`, `nix/`, and packaging scripts | [Build and Packaging](operations/build-and-packaging.md) |
| Add or choose regression coverage | the explicit test target block in `CMakeLists.txt` and the matching standalone file under `tests/` | [Test Strategy](testing/strategy.md) |

Project persistence is a zip-backed contract, not merely a file extension. `SessionLoader` embeds active model data and serializes part transforms, global settings, part-local settings, ranges, and a version marker; loading restores those pieces after the settings-version preflight in `SessionManager`. [source](repo://src/threading/session_loader.cpp#L37-L128) [source](repo://src/threading/session_loader.cpp#L131-L216) [source](repo://src/managers/session_manager.cpp#L708-L740)

The geometry route is also the output route: `Step` owns settings, slicing-plane orientation, geometry and islands, while concrete steps compute paths, apply modifiers, and write through `WriterBase`. `SegmentBase` is the polymorphic movement unit and also carries metadata used to render loaded G-code. [source](repo://include/step/step.h#L18-L49) [source](repo://include/step/step.h#L75-L121) [source](repo://include/geometry/segment_base.h#L21-L35) [source](repo://include/geometry/segment_base.h#L52-L110)

## Settings are a source-to-generated contract

Edit `resources/settings/*.yaml`; do not treat the generated JSON files as the authoring surface. Sorted YAML path order and in-file setting order determine generated catalog and UI order. Generate and validate both artifacts together: [source](repo://resources/settings/README.md#L1-L15)

```bash
python3 scripts/generate_master_config.py \
  resources/settings \
  resources/configs/master.conf \
  resources/configs/setting_inputs.conf
jq empty resources/configs/master.conf resources/configs/setting_inputs.conf
```

Both generated files are embedded under the `/configs` Qt resource prefix. `SettingsManager` parses them into the master catalog, composite input metadata, and default-valued active global settings; the CLI converter also reads embedded `master.conf`. A settings change is incomplete if the YAML and both generated artifacts disagree. [source](repo://resources/configs/configs.qrc#L1-L8) [source](repo://src/managers/settings/settings_manager.cpp#L38-L68) [source](repo://src/console/command_line_processor.cpp#L34-L41)

For template, override-precedence, widget dependency, migration, and preference persistence details, use [Settings and Preferences](architecture/settings-and-preferences.md).

## Match validation to the risk

For a production-code change, start with the shared compile target, then build and run the narrow CTest executable whose name maps to the behavior:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_obj

nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug \
  --target ornlslicer_preferences_import_tests
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug \
  -R '^preferences_import_tests$' --output-on-failure
```

Build all configured targets and run full CTest when a change crosses subsystems:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug --output-on-failure
```

Focused binaries use the `ornlslicer_` build-target prefix while CTest names omit it. The test block declares 21 focused executables and one project-slice regression that appears only when its configured local `.s2p` fixture exists. [source](repo://CMakeLists.txt#L229-L461) [source](repo://CMakeLists.txt#L463-L475)

Use these additional checks according to the changed contract:

- setting metadata: regenerate both configs and run `jq empty`;
- C++: format changed files with the repository `.clang-format` and compile the affected target;
- source/header/resource additions or deletions: rerun CMake configure because production inputs are globbed;
- documentation only: run `git diff --check -- <touched-files>`;
- GUI, native-dialog, OpenGL, or controller-facing behavior: add interactive or external integration evidence because current CTest and packaging smoke checks do not cover those outcomes. [source](repo://.codex/AGENTS.md#L22-L58) [source](repo://CMakeLists.txt#L139-L157) [source](repo://.github/workflows/ci.yml#L52-L56)

The exact test-to-risk map, the optional whole-project regression, and the distinction between local CTest and CI packaging evidence are in [Test Strategy and Validation Map](testing/strategy.md).

## Page index

- [Application Runtime and Coordination](architecture/application-runtime.md): GUI/CLI startup, shared managers, workers, progress, and shutdown.
- [Session, Model, and Project Lifecycle](architecture/session-and-project-lifecycle.md): part ownership, loaders, `.s2p` contents, migrations, history, and failure behavior.
- [Settings, Templates, and Preferences](architecture/settings-and-preferences.md): generated catalogs, runtime scope, UI metadata, versioning, and persistence.
- [Geometry and Toolpath Data Model](concepts/geometry-and-toolpath-model.md): meshes, transforms, units, steps, islands, paths, and segments.
- [Slicing Pipeline Orchestration](architecture/slicing-pipeline.md): dispatch and common phase, thread, cancellation, and completion contracts.
- [Planar Cross-Sections and Path Generation](slicing/planar-path-generation.md): plane production, variable layers, regions, ordering, and modifiers.
- [Cylindrical and Helical Slicing](slicing/cylindrical-and-helical.md): radial and helical construction, clipping, seams, orientation, and Arc Specialties constraints.
- [G-Code, Parsing, Visualization, and Export](architecture/gcode-and-visualization.md): parser flow, timing, display segments, preview, and export consumers.
- [Machine Syntax Writers and Parsers](integrations/machine-syntaxes.md): dialect identity, writer/parser contracts, exceptions, and extension checklist.
- [Test Strategy and Validation Map](testing/strategy.md): CTest commands, target coverage, fixtures, end-to-end test, and current gaps.
- [Build, Generated Assets, CI, and Packaging](operations/build-and-packaging.md): CMake, Nix, generated settings, provenance, AppImage, Windows, and CI boundaries.
