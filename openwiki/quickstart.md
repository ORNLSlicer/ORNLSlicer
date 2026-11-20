---
type: guide
title: ORNLSlicer OpenWiki Quickstart
description: A task-oriented maintainer route from runtime entrypoints and canonical settings through slicing, machine output, focused tests, and packaging.
tags: [quickstart, maintainers, architecture, validation]
sources:
  - id: openwiki-source-164e2da859b5277df81c7d94
    resource: repo://.github/workflows/ci.yml
  - id: openwiki-source-8037e2358a2c4f9b2c722a11
    resource: repo://AGENTS.md
  - id: openwiki-source-a60928f26402a7ffadc427b3
    resource: repo://cmake/presets/generic-llvm-ninja.json
  - id: openwiki-source-c9d11891f93487ed035fc596
    resource: repo://cmake/validate_resources.cmake
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-f5502ee67e629f24f1284e65
    resource: repo://docs/contributing/development-setup.md
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
  - id: openwiki-source-97f2984c6f7df859b5dac320
    resource: repo://src/managers/settings/settings_version_control.cpp
  - id: openwiki-source-e6313e383d53e930ab9c33c4
    resource: repo://src/slicing/slicing_utilities.cpp
  - id: openwiki-source-b1134fc13dd24912e260b947
    resource: repo://src/threading/abs_slicing_thread.cpp
  - id: openwiki-source-4c1d7b6308a78ba9c493ce8a
    resource: repo://src/threading/gcode_loader.cpp
  - id: openwiki-source-c9b49d4d1b98e2ebff97fee2
    resource: repo://src/threading/session_loader.cpp
  - id: openwiki-source-403d1cb683b233c4f42460d0
    resource: repo://src/windows/main_window.cpp
  - id: openwiki-source-c104bb75a5f8622243458c6e
    resource: repo://tests/gcode_shading_tests.cpp
generated: { by: "codex", at: "2026-11-10T17:47:51.624Z" }
verified:
  - by: openwiki/0.7.2
    at: 2026-11-10T17:47:51.624Z
---

# ORNLSlicer OpenWiki Quickstart

ORNLSlicer is a Qt/C++ slicing and toolpath-planning application for additive-manufacturing workflows. Its maintainable path is deliberately shared: the GUI and command line converge on the same session, settings, geometry, slicing, writer, and G-code loading subsystems. [source](repo://README.md#L1-L21) [source](repo://src/main.cpp#L72-L129) [source](repo://src/console/main_control.cpp#L26-L40) [source](repo://src/console/main_control.cpp#L60-L120) [source](repo://src/managers/session_manager.cpp#L592-L660) [source](repo://src/threading/abs_slicing_thread.cpp#L210-L320) [source](repo://src/threading/gcode_loader.cpp#L287-L320)

Use this page to choose the owning subsystem, the first production source to inspect, and the smallest relevant validation. Follow the linked page when a change crosses an ownership, threading, data-model, or file-format boundary.

## First five minutes

Start by preserving the current checkout and tracing the named behavior rather than editing from a directory guess:

```bash
git status --short --branch
rg -n 'NamedType|setting_key|signalName|command_text' include src resources tests
```

Headers and implementations are normally paired beneath `include/` and `src/`; `resources/settings/` owns setting metadata, `templates/` owns installed process/printer templates, and CMake/Nix own build and package topology. [source](repo://AGENTS.md#L159-L162) [source](repo://resources/settings/README.md#L1-L15) [source](repo://CMakeLists.txt#L139-L164) [source](repo://CMakeLists.txt#L763-L801) [source](repo://CMakeLists.txt#L841-L851)

Follow [Developer Setup](../docs/contributing/development-setup.md) for platform prerequisites, Git LFS restoration, Nix and optional direnv. Restore LFS assets before configuring each checkout; CMake rejects unresolved pointers in bundled resources. Establish the build baseline inside the repository's Nix shell:

```bash
nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer -j 32
```

The checked-in preset uses Clang, Ninja Multi-Config, and `build/generic-llvm-ninja`. For a compile-only iteration across shared production sources, replace the final target with `ornlslicer_obj`; that object library is linked by both the application and focused test executables. [source](repo://cmake/presets/generic-llvm-ninja.json#L1-L14) [source](repo://CMakeLists.txt#L139-L164) [source](repo://AGENTS.md#L166-L173)

## The entry rule: arguments select the shell

There is one source entrypoint with a strict branch:

- with no command-line arguments, it creates `QApplication`, initializes GUI resources, obtains the singleton `MainWindow`, and enters the GUI event loop;
- with any argument, including `--help` or `--version`, it creates `QCoreApplication`, converts options into a `SettingsBase`, and runs `MainControl`. [source](repo://src/main.cpp#L72-L129)

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

`SessionManager::doSlice()` reads the active syntax and slicing mode, enforces the cylindrical/Arc-Specialties gate, selects or reuses `PlanarSlicer`, `ImageSlicer`, or `CylindricalSlicer`, then emits the worker start signal. [source](repo://src/managers/session_manager.cpp#L592-L660) [source](repo://src/managers/session_manager.cpp#L717-L750)

Toolpath modes select a concrete writer from the active machine syntax. Generated or imported G-code then passes through `GCodeLoader`, which selects a parser from the header, parses motion and timing metadata, and supplies downstream consumers. [source](repo://src/threading/abs_slicing_thread.cpp#L210-L320) [source](repo://src/threading/gcode_loader.cpp#L287-L328) [source](repo://src/threading/gcode_loader.cpp#L590-L680)

## Route a change by task

| Maintainer task | Start in current source | Follow the contract |
| --- | --- | --- |
| Change startup, CLI option handling, progress, or shutdown | `src/main.cpp`, `src/console/command_line_processor.cpp`, then `src/console/main_control.cpp` or `src/windows/main_window.cpp` | [Application Runtime](architecture/application-runtime.md) |
| Change model import, session ownership, recent files, or `.s2p` save/load | `include/managers/session_manager.h`, `src/managers/session_manager.cpp`, and the relevant loader under `src/threading/` | [Session and Project Lifecycle](architecture/session-and-project-lifecycle.md) |
| Add or alter a setting, composite input, template, preference, migration, or settings-file comparison | `resources/settings/*.yaml` for metadata; otherwise the settings/preferences managers or comparison dialog/comparator | [Settings and Preferences](architecture/settings-and-preferences.md) |
| Change mesh, coordinate, unit, `Step`, island, path, or segment semantics | the matching declaration under `include/geometry`, `include/part`, or `include/step`, then its `src/` implementation | [Geometry and Toolpath Model](concepts/geometry-and-toolpath-model.md) |
| Change slicer dispatch, phase order, progress, cancellation, or common output | `src/managers/session_manager.cpp` and `src/threading/abs_slicing_thread.cpp` | [Slicing Pipeline](architecture/slicing-pipeline.md) |
| Change planar cross-sections, layer settings, regions, ordering, or modifiers | `src/threading/slicers/planar_slicer.cpp`, `src/threading/step_thread.cpp`, then `src/step/layer/regions`, `src/optimizers`, or `src/modifiers` | [Planar Path Generation](slicing/planar-path-generation.md) |
| Change radial/helical geometry, clipping, seam, or tool orientation | `src/threading/slicers/cylindrical_slicer.cpp`, cylindrical layer implementations, and `src/slicing/helical_region_profile.cpp` | [Cylindrical and Helical Slicing](slicing/cylindrical-and-helical.md) |
| Add or change a machine dialect | syntax enum/metadata, `src/gcode/writers`, parser selection in `GCodeLoader`, and syntax-specific tests | [Machine Syntaxes](integrations/machine-syntaxes.md) |
| Change imported/generated G-code display, timing, colors, preview, or export | `src/threading/gcode_loader.cpp` and its parser, widget, or export consumer | [G-code and Visualization](architecture/gcode-and-visualization.md) |
| Change dependencies, generated build metadata, packages, AppImage, or installer | `CMakeLists.txt`, `cmake/`, `flake.nix`, `nix/`, and packaging scripts | [Build and Packaging](operations/build-and-packaging.md) |
| Change connected contours, forward-wipe connections, or perimeter-to-inset chains | `include/geometry/contour_connection.h`, perimeter/inset regions and `PolymerIsland::optimize` | [Planar Path Generation](slicing/planar-path-generation.md#connected-contours) |
| Change standalone layer export or preview connection identity | `src/gcode/gcode_layer_export.cpp`, `src/threading/gcode_layer_saver.cpp`, writer hooks and loader comments | [G-code and Visualization](architecture/gcode-and-visualization.md) |
| Prepare a release, adjust version rules, or maintain release history | `scripts/version.py`, `version.json`, `CHANGELOG.md`, release workflows | [Build and Packaging](operations/build-and-packaging.md#release-preparation-and-changelog) |
| Change the offline Help changelog viewer | `src/windows/changelog.cpp`, `resources/docs/docs.qrc`, Help-menu wiring | [Application Runtime](architecture/application-runtime.md#runtime-ui-extensions) |
| Change parent-child edits, transforms, copy/paste or child modifier scope | `src/widgets/part_widget/model/part_meta_model.cpp`, `src/part/part.cpp`, `src/slicing/slicing_utilities.cpp` | [Session Lifecycle](architecture/session-and-project-lifecycle.md#parent-child-objects-and-reversible-edits) |
| Change support contacts or bridge suppression | `PlanarSlicer::processSupport`, `tests/support_generation_tests.cpp` | [Planar Support](slicing/planar-path-generation.md#support-demand-restored-contacts-and-bridges) |
| Change ARC weld schedule values, units or companion exports | `src/threading/gcode_arc_specialties_saver.cpp`, `include/slicing/region_speed.h`, ARC writer and export snapshots | [Machine Syntaxes](integrations/machine-syntaxes.md#generated-arc-weld-schedules) |
| Change themes, fonts or disabled toolbar controls | `src/utilities/theme_tool.cpp`, root/view styles, `MainToolbar::setupStyle` | [Application Runtime](architecture/application-runtime.md#runtime-ui-extensions) |
| Add or choose regression coverage | the explicit test target block in `CMakeLists.txt` and the matching standalone file under `tests/` | [Test Strategy](testing/strategy.md) |

Project persistence is a zip-backed contract. `SessionLoader` embeds active model data and serializes part transforms, global settings, part-local settings, ranges, and a version marker; loading restores those pieces after the settings-version preflight in `SessionManager`. [source](repo://src/threading/session_loader.cpp#L37-L128) [source](repo://src/threading/session_loader.cpp#L131-L216) [source](repo://src/managers/session_manager.cpp#L773-L808)

The geometry route is also the output route: `Step` owns settings, slicing-plane orientation, geometry and islands, while concrete steps compute paths, apply modifiers, and write through `WriterBase`. `SegmentBase` is the polymorphic movement unit and also carries metadata used to render loaded G-code. [source](repo://include/step/step.h#L18-L49) [source](repo://include/step/step.h#L75-L121) [source](repo://include/geometry/segment_base.h#L21-L35) [source](repo://include/geometry/segment_base.h#L52-L110)

Recent contracts to preserve: project JSON records parent links, modifier enablement and aligned/current transform state; child modifiers apply only to their direct build parent. Individual Layer Export is opt-in before slicing and stores standalone programs in memory. Generated ARC export retains the slice-time companion decision; wire feed uses in/min while travel override and schedule motion speed use mm/min. Starting Layer Number changes external labels and filenames while indexes remain zero-based. Follow the owning pages for limits and failure paths. [source](repo://src/managers/session_manager.cpp#L442-L468) [source](repo://src/slicing/slicing_utilities.cpp#L164-L174) [source](repo://src/threading/abs_slicing_thread.cpp#L327-L403) [source](repo://src/threading/gcode_arc_specialties_saver.cpp#L46-L64) [source](repo://src/gcode/writers/writer_base.cpp#L349-L369)

## Settings are a source-to-generated contract

Edit `resources/settings/*.yaml`; do not treat the generated JSON files as the authoring surface. Sorted YAML path order and in-file setting order determine generated catalog and UI order. Generate and validate both artifacts together: [source](repo://resources/settings/README.md#L1-L15) [source](repo://AGENTS.md#L162-L173)

```bash
python3 scripts/generate_master_config.py \
  resources/settings \
  resources/configs/master.conf \
  resources/configs/setting_inputs.conf
jq empty resources/configs/master.conf resources/configs/setting_inputs.conf
```

Both generated files are embedded under the `/configs` Qt resource prefix. `SettingsManager` parses them into the master catalog, composite input metadata, and default-valued active global settings; the CLI converter also reads embedded `master.conf`. A settings change is incomplete if the YAML and both generated artifacts disagree. [source](repo://resources/configs/configs.qrc#L1-L8) [source](repo://src/managers/settings/settings_manager.cpp#L73-L103) [source](repo://src/console/command_line_processor.cpp#L34-L41)

Current persisted settings are schema version 15. Helical tool offsets are signed arc lengths evaluated at each actual radius; legacy angles migrate with first-radius reference geometry. Connected contour keys replace former spiral-connection names while spiralized printing remains separate. [source](repo://src/managers/settings/settings_version_control.cpp#L331-L383) [source](repo://src/managers/settings/settings_version_control.cpp#L424-L474)

For template, override-precedence, widget dependency, migration, and preference persistence details, use [Settings and Preferences](architecture/settings-and-preferences.md).

## Match validation to the risk

For a production-code change, start with the shared compile target, then build and run the narrow CTest executable whose name maps to the behavior:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_obj -j 32

nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug \
  --target ornlslicer_preferences_import_tests -j 32
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug \
  -R '^preferences_import_tests$' --output-on-failure
```

Build all configured targets and run full CTest when a change crosses subsystems:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug -j 32
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug --output-on-failure
```

Focused binaries use the `ornlslicer_` build-target prefix while CTest names omit it. The test block declares 45 focused C++ executables plus Python metadata and CMake resource checks (47 unconditional CTest entries), and one project-slice regression when its configured local `.s2p` fixture exists. For settings comparison, template reload and toolbar theme/overflow contracts, build `ornlslicer_settings_file_comparator_tests`, `ornlslicer_settings_manager_tests`, and `ornlslicer_main_toolbar_tests`, then filter CTest with their three unprefixed names. [source](repo://CMakeLists.txt#L239-L746) [source](repo://CMakeLists.txt#L748-L760)

Use these additional checks according to the changed contract:

- setting metadata: regenerate both configs and run `jq empty`;
- C++: format changed files with the repository `.clang-format` and compile the affected target;
- source/header/resource additions or deletions: rerun CMake configure because production inputs are globbed;
- documentation only: run `git diff --check -- <touched-files>`;
- GUI, native-dialog, OpenGL, or controller-facing behavior: use the bounded toolbar, changelog, color and shader tests where applicable, then add interactive or external integration evidence for the remaining outcomes. [source](repo://AGENTS.md#L166-L175) [source](repo://CMakeLists.txt#L139-L157) [source](repo://.github/workflows/ci.yml#L86-L90)

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
