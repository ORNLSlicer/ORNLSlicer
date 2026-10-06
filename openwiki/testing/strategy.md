---
type: guide
title: Test Strategy and Validation Map
description: CTest topology, focused regression coverage, end-to-end slicing checks, commands, fixtures, and the current CI and GUI validation boundary.
tags: [testing, ctest, regression, validation, ci]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-ced6747f13d7a40f942b17d8
    resource: repo://.codex/AGENTS.md
  - id: openwiki-source-164e2da859b5277df81c7d94
    resource: repo://.github/workflows/ci.yml
  - id: openwiki-source-a60928f26402a7ffadc427b3
    resource: repo://cmake/presets/generic-llvm-ninja.json
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-cf53758b4249db7b3d333718
    resource: repo://docs/contributing/linux-appimage.md
  - id: openwiki-source-71ae1012989bc835de1e9233
    resource: repo://nix/ornlslicer/default.nix
  - id: openwiki-source-b89f433e76caa6e1aa00a3e9
    resource: repo://tests/arc_specialties_parser_tests.cpp
  - id: openwiki-source-b56e4780c1ae610b3d8ba0db
    resource: repo://tests/as_printed_model_exporter_tests.cpp
  - id: openwiki-source-60219e40c59fdb42fb41cdd0
    resource: repo://tests/common_parser_tests.cpp
  - id: openwiki-source-1d4aeeb295c375a2ff8ca729
    resource: repo://tests/gcode_settings_importer_tests.cpp
  - id: openwiki-source-06dbed28cbcd952aa1f714fc
    resource: repo://tests/helical_region_profile_tests.cpp
  - id: openwiki-source-6a59e8ac36ba9aa084b8dcdf
    resource: repo://tests/juggerbot_writer_tests.cpp
  - id: openwiki-source-2141391cb9833c33ff1c3855
    resource: repo://tests/mesh_repair_tests.cpp
  - id: openwiki-source-3308169b66ac9fd6252e5bb8
    resource: repo://tests/optimizer_empty_input_tests.cpp
  - id: openwiki-source-6949df8bebfa2cc2eb946f04
    resource: repo://tests/preferences_import_tests.cpp
  - id: openwiki-source-3adbbc35f16bcd573644d363
    resource: repo://tests/run_project_slice_regression.cmake
  - id: openwiki-source-af6398eb7d1bc9a42afbe5af
    resource: repo://tests/session_manager_tests.cpp
  - id: openwiki-source-1fd14546369693e17741b750
    resource: repo://tests/settings_version_control_tests.cpp
  - id: openwiki-source-488b08952aaa6db435f7b20d
    resource: repo://tests/test_utils.h
  - id: openwiki-source-fd42cdedbc9d871771e139f8
    resource: repo://tests/variable_layer_height_tests.cpp
  - id: openwiki-source-7a4801d8dab8724a03e566bd
    resource: repo://tests/visualization_colors_tests.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Test Strategy and Validation Map

ORNLSlicer uses small standalone C++ executables for focused regression coverage and an optional whole-application slicing check for a local project fixture. The focused tests exercise production classes directly and link the same object library as the application; they are not a separate mock implementation of the slicer. [source](repo://CMakeLists.txt#L139-L164) [source](repo://CMakeLists.txt#L229-L241)

```text
                        ornlslicer_obj
                  all shared production sources
                       /             \
                      /               \
             ornlslicer app       21 test executables
                                       |
                                      CTest
                                       |
                         optional .s2p -> app -> .gcode
```

## How the suite is wired

`include(CTest)` creates the conventional `BUILD_TESTING` option. `ORNLSLICER_BUILD_TESTS` defaults to that value, and its guarded block creates the test executables, enables CTest, and registers each executable with `add_test()`. Set `-DORNLSLICER_BUILD_TESTS=OFF` only when intentionally omitting the suite from a configuration. [source](repo://CMakeLists.txt#L17-L26) [source](repo://CMakeLists.txt#L229-L241)

Every focused binary follows the same naming split:

- the build target is prefixed with the project name, such as `ornlslicer_mesh_repair_tests`;
- the CTest name omits that prefix, such as `mesh_repair_tests`;
- the executable includes the production and generated headers and links `ornlslicer_obj`. [source](repo://CMakeLists.txt#L232-L263)

The suite does not depend on an external C++ test framework. Test files provide `main()`, accumulate boolean expectations, and return `EXIT_SUCCESS` or `EXIT_FAILURE`. Shared helpers print failed conditions to standard error and provide scalar, 2D-point, and 3D-point tolerance comparisons. [source](repo://tests/test_utils.h#L9-L69) [source](repo://tests/mesh_repair_tests.cpp#L114-L132)

## Focused validation commands

Configure in the repository's Nix development shell, then build the default target set so the test executables exist before invoking CTest:

```bash
nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug --output-on-failure
```

The preset uses Clang with the Ninja Multi-Config generator and writes to `build/generic-llvm-ninja`, so `-C Debug` selects the executable configuration for CTest. Do not substitute the unrelated top-level `build/` path. [source](repo://cmake/presets/generic-llvm-ninja.json#L1-L14) [source](repo://.codex/AGENTS.md#L39-L47)

For a narrow change, build one prefixed executable target and filter by its unprefixed CTest name:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug \
  --target ornlslicer_arc_specialties_parser_tests
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug \
  -R '^arc_specialties_parser_tests$' --output-on-failure
```

Use `ctest --test-dir build/generic-llvm-ninja -C Debug -N` to inspect the tests registered by the current configuration. Reconfigure after adding or deleting source, header, resource, or test-target declarations; source and resource discovery is glob-based even though test targets themselves are explicit. [source](repo://CMakeLists.txt#L139-L157) [source](repo://.codex/AGENTS.md#L22-L29)

## Coverage map

The 21 always-declared focused tests divide into four practical risk groups. The table summarizes the behavior protected by each CTest name, not merely the class named by the file.

| Area | CTest name | Principal regression surface |
| --- | --- | --- |
| Mesh and layer construction | `mesh_repair_tests` | Large-boundary repair is skipped without closing the mesh; a closed tetrahedron remains closed after a successful repair. [source](repo://tests/mesh_repair_tests.cpp#L114-L132) |
| Mesh and layer construction | `variable_layer_height_tests` | Fixed and adaptive layer heights, cusp refinement bounds, CGAL/non-CGAL parity, syntax gating, and support-gap slice counts. [source](repo://tests/variable_layer_height_tests.cpp#L176-L239) |
| Ordering and anchors | `point_order_optimizer_tests` | Physical-distance split selection across edges and corners, wraparound behavior, randomness fallback, and legacy vertex-only selection. [source](repo://tests/point_order_optimizer_tests.cpp#L21-L101) |
| Ordering and anchors | `optimization_anchor_tests` | Projected seam-attractor anchors for custom island, path, region-path, and point modes, while non-custom order ignores the attractor. [source](repo://tests/optimization_anchor_tests.cpp#L40-L95) |
| Ordering and anchors | `optimizer_empty_input_tests` | Empty and degenerate optimizer inputs, forward progress, planar/radial/helical ordering, reversal, arc splitting, and region-boundary flags. [source](repo://tests/optimizer_empty_input_tests.cpp#L139-L228) [source](repo://tests/optimizer_empty_input_tests.cpp#L230-L481) |
| Ordering and anchors | `global_layer_tests` | Agreement and conflict between local layer ordering settings, global order selection, shifts, and projected anchors. [source](repo://tests/global_layer_tests.cpp#L88-L219) |
| Helical and path geometry | `helical_region_profile_tests` | Band ordering, stepover fallback, infill-revolution rounding, invalid-input reasons, and retained clipping profiles. [source](repo://tests/helical_region_profile_tests.cpp#L250-L280) |
| Helical and path geometry | `waam_layer_z_tests` | WAAM surface-reference Z, non-WAAM top reference, variable height, normal direction, and Wolf travel-lift behavior. [source](repo://tests/waam_layer_z_tests.cpp#L220-L239) |
| Helical and path geometry | `path_modifier_tests` | Sharp-corner modification geometry, continuity, connector threshold behavior, and segment-local setting overrides. [source](repo://tests/path_modifier_tests.cpp#L30-L95) |
| Helical and path geometry | `spiral_path_tests` | Spiral grouping for adjacent, disjoint, nested, separated-stack, and mid-edge-start paths. [source](repo://tests/spiral_path_tests.cpp#L32-L104) |
| Helical and path geometry | `helical_clip_rounding_tests` | Exact, last-full, and complete revolution clipping; sub-revolution omission; handed endpoints; offsets; and fixed seams. [source](repo://tests/helical_clip_rounding_tests.cpp#L185-L212) |
| Settings compatibility | `gcode_settings_importer_tests` | Comment-style normalization, importer fixtures, legacy keys and version migration, unknown keys, and cancellation of missing-setting prompts. [source](repo://tests/gcode_settings_importer_tests.cpp#L64-L242) |
| Settings compatibility | `settings_version_control_tests` | Forward migration of v10/v11 helical angle keys and the v12 variable-Z boolean into the current v13 representation. [source](repo://tests/settings_version_control_tests.cpp#L13-L65) |
| Settings compatibility | `preferences_import_tests` | Transactional preference import, one aggregate unit-change signal, and observers seeing the complete final state. [source](repo://tests/preferences_import_tests.cpp#L20-L76) |
| Settings compatibility | `preferences_manager_tests` | Persistence and defaulting of manager-owned preferences, including STEP-to-STL linear deflection. [source](repo://tests/preferences_manager_tests.cpp#L14-L52) |
| Session lifecycle | `session_manager_tests` | Recent-project ordering, caps and clearing, project types, asynchronous CLI-style load, and archived settings migration. [source](repo://tests/session_manager_tests.cpp#L74-L188) |
| Parsing and visualization | `common_parser_tests` | Travel timing and scaling plus modifier-only layer and tip-wipe height inference. [source](repo://tests/common_parser_tests.cpp#L296-L316) |
| Parsing and visualization | `arc_specialties_parser_tests` | Arc Specialties parsing and writing, including optional stops, G80 scheduling, CP visualization, rejection cases, handedness, frames, comments, startup, rounding, and arc density. [source](repo://tests/arc_specialties_parser_tests.cpp#L790-L875) |
| Parsing and visualization | `juggerbot_writer_tests` | Arc bead-area extrusion scaling and zero-RPM extrusion shutdown. [source](repo://tests/juggerbot_writer_tests.cpp#L81-L93) |
| Parsing and visualization | `visualization_colors_tests` | Registry completeness, stable names/defaults, preference registration, and G-code comment/segment color mapping. [source](repo://tests/visualization_colors_tests.cpp#L93-L210) |
| Export artifacts | `as_printed_model_exporter_tests` | Bead meshes and centerlines, bounds/orientation/units, radial and LFAM behavior, filtering, arc/corner geometry, and binary/ASCII STL validity. [source](repo://tests/as_printed_model_exporter_tests.cpp#L129-L341) |

The declarations for those targets are centralized in the test block rather than inferred from filenames. Consequently, adding a new `tests/*.cpp` file alone does not add it to CTest; a corresponding executable and `add_test()` entry are required. [source](repo://CMakeLists.txt#L229-L461)

## Fixtures and isolation

Most focused cases construct geometry, settings, commands, and expected values directly in C++. File-oriented tests create disposable fixtures with `QTemporaryDir`, including preference import, settings import, session history/project loading, and model export. This keeps the routine suite independent of a developer's persistent preferences and output directories. [source](repo://tests/preferences_import_tests.cpp#L1-L42) [source](repo://tests/session_manager_tests.cpp#L74-L113) [source](repo://tests/as_printed_model_exporter_tests.cpp#L129-L157)

Some parser, writer, settings, session, WAAM, and exporter tests instantiate `QCoreApplication` because the production code uses Qt core facilities. They do not create a `QApplication`, a main window, an OpenGL context, or an interactive event-driving harness. Passing these tests therefore supports data-flow and algorithm correctness, not visual correctness or full GUI behavior. [source](repo://tests/common_parser_tests.cpp#L1-L4) [source](repo://tests/common_parser_tests.cpp#L296-L299) [source](repo://tests/session_manager_tests.cpp#L1-L4) [source](repo://tests/session_manager_tests.cpp#L74-L78)

## Optional whole-project slicing regression

The only registered whole-application slicing test is conditional. At configure time, CMake checks `ORNLSLICER_NEW_IMPACT_PROJECT`; when that path exists, it registers `new_impact_project_regression`, launches the built `ornlslicer` executable with `--input_project_file` and `--output_location`, and writes artifacts beneath the build tree. The checked-in default is a developer-local absolute path, so this test is normally absent on other machines unless explicitly configured. [source](repo://CMakeLists.txt#L463-L475) [source](repo://tests/run_project_slice_regression.cmake#L1-L31)

To enable it with an available project:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --preset generic-llvm-ninja \
  -DORNLSLICER_NEW_IMPACT_PROJECT=/absolute/path/to/project.s2p
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug \
  -R '^new_impact_project_regression$' --output-on-failure
```

Success means the application exited with status zero and produced a nonempty `.gcode` file. The script preserves stdout and stderr logs, but it does not compare G-code with a golden file or validate machine execution. [source](repo://tests/run_project_slice_regression.cmake#L19-L50)

## CI evidence and current gaps

The push workflow has three complementary signals:

- Ubuntu and macOS run `nix flake check --all-systems`;
- Ubuntu builds and bundles the Linux AppImage and runs only its `--help` command as a smoke test;
- Ubuntu cross-builds the Windows portable tree and installer and uploads both artifacts. [source](repo://.github/workflows/ci.yml#L1-L19) [source](repo://.github/workflows/ci.yml#L21-L61) [source](repo://.github/workflows/ci.yml#L63-L107)

The workflow does not explicitly invoke `ctest`, and the Nix package derivation defines neither a check phase nor a CTest command. A green packaging workflow therefore demonstrates the declared Nix/build/package steps, but it should not be reported as a run of the 21 focused tests. [source](repo://.github/workflows/ci.yml#L8-L19) [source](repo://.github/workflows/ci.yml#L42-L56) [source](repo://nix/ornlslicer/default.nix#L12-L64)

No current CTest target drives the complete widget UI, native file dialogs, drag-and-drop, rendered toolpaths, screenshots, or OpenGL behavior. The AppImage smoke check starts only the command-line help path. Validate those risks interactively on a machine with a display and suitable graphics support; for AppImage release work, follow the dedicated launch, dialog, preferences, alternate-settings, and output-ownership checklist. [source](repo://CMakeLists.txt#L229-L475) [source](repo://.github/workflows/ci.yml#L52-L56) [source](repo://docs/contributing/linux-appimage.md#L33-L72)

Likewise, parser and writer tests validate syntax transformations in process, not acceptance by a physical controller. The optional project regression proves only that one supplied project reaches a nonempty G-code artifact. Treat controller compatibility, representative-project output review, and machine motion as separate integration evidence. [source](repo://tests/arc_specialties_parser_tests.cpp#L790-L875) [source](repo://tests/juggerbot_writer_tests.cpp#L81-L93) [source](repo://tests/run_project_slice_regression.cmake#L23-L50)

Related reading: [Build and Packaging](../operations/build-and-packaging.md), [Slicing Pipeline](../architecture/slicing-pipeline.md), [G-code and Visualization](../architecture/gcode-and-visualization.md), and [Machine Syntaxes](../integrations/machine-syntaxes.md).
