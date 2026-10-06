---
type: operations
title: Build, Generated Assets, CI, and Packaging
description: Reproducible local and CI build paths, generated settings and build metadata, compiler-cache behavior, and Linux and Windows distribution assembly.
tags: [build, cmake, nix, ci, packaging, diagnostics]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-164e2da859b5277df81c7d94
    resource: repo://.github/workflows/ci.yml
  - id: openwiki-source-d02327111261f96a065fb8de
    resource: repo://cmake/build_info.cmake
  - id: openwiki-source-5d790bd7059cfd1497fadcf0
    resource: repo://cmake/git.cmake
  - id: openwiki-source-a60928f26402a7ffadc427b3
    resource: repo://cmake/presets/generic-llvm-ninja.json
  - id: openwiki-source-5e070533cc02d87db926df69
    resource: repo://cmake/version.cmake
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-3eda5d2b85501c78fc0ba02d
    resource: repo://flake.nix
  - id: openwiki-source-41844989991110a96eb2c453
    resource: repo://nix/appimage/default.nix
  - id: openwiki-source-71ae1012989bc835de1e9233
    resource: repo://nix/ornlslicer/default.nix
  - id: openwiki-source-6f5d88049adbd611fb8e2754
    resource: repo://resources/settings/README.md
  - id: openwiki-source-dc724c400ef7006f36d0b685
    resource: repo://scripts/installer.nsi
  - id: openwiki-source-d364d949938a433276255c32
    resource: repo://src/main.cpp
  - id: openwiki-source-3a34e9951a829d6407a1f125
    resource: repo://src/utilities/runtime_diagnostics.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Build, Generated Assets, CI, and Packaging

ORNLSlicer has two cooperating build layers. CMake describes compilation, generated headers and settings, tests, installation, and platform targets. Nix supplies pinned dependencies, the developer shell, native and Windows-cross packages, the Linux AppImage bundler, and the same package entry points used by CI. [source](repo://CMakeLists.txt#L1-L36) [source](repo://flake.nix#L1-L43) [source](repo://flake.nix#L52-L108)

```text
version.json + Git state ────────> generated build_info.h
resources/settings/*.yaml ──────> master.conf + setting_inputs.conf
src/*.cpp + include/*.h + *.qrc ─> ornlslicer_obj
                                      ├─> ornlslicer GUI/CLI entrypoint
                                      ├─> Windows console entrypoint
                                      └─> focused test executables
                                                 │
                         CMake install tree <────┘
                                 │
                ┌────────────────┴────────────────┐
                ▼                                 ▼
        Nix Linux closure                  MinGW Windows tree
                │                                 │
         x86-64 AppImage                 portable artifact + NSIS
```

## Reproducible developer build

The default `ornlslicerDev` shell inherits the package's build inputs and adds Git, jq, ccache, pre-commit, Doxygen/Graphviz, LLDB, clang tools, Python utilities, and Linux-only NSIS, `cntr`, and `clazy`. The flake uses LLVM 18 for the build environment and selects separate LLVM tooling packages—18 on x86-64 Darwin and 22 elsewhere. [source](repo://flake.nix#L14-L41) [source](repo://flake.nix#L110-L146)

Configure once with the checked-in multi-config Clang/Ninja preset, then build the desired configuration and target inside the Nix shell:

```bash
nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug --output-on-failure
```

The preset fixes `clang`, `clang++`, `Ninja Multi-Config`, and `build/generic-llvm-ninja`; its Debug and Release build presets select the corresponding configuration. A `generic-llvm-ninja-fast` variant enables CMake unity builds in a separate build directory for faster clean builds. [source](repo://cmake/presets/generic-llvm-ninja.json#L1-L55)

For a narrow compile check, build `ornlslicer_obj`. The GUI executable is only `src/main.cpp` plus that object library, and every focused unit executable also links the same object library, so this target catches compilation failures across the shared production sources without linking the application or tests. [source](repo://CMakeLists.txt#L139-L164) [source](repo://CMakeLists.txt#L214-L241)

## CMake topology and dependencies

The project requires CMake 3.25 and C++23, enables Qt automoc and autorcc, and emits `compile_commands.json`. Its required dependency graph includes Qt 6 GUI/Widgets/OpenGL/Core/Concurrent/Network/Charts/Core5Compat, Assimp, Boost.System, CGAL with Eigen support, Eigen, nlohmann-json, OpenCASCADE STEP/mesh libraries, VTK, zip, polyclipping, and psimpl. OpenMP is optional: its absence produces a warning and an unoptimized build rather than a configure failure. [source](repo://CMakeLists.txt#L1-L26) [source](repo://CMakeLists.txt#L56-L113)

CMake recursively discovers implementation files, public headers, and Qt resource collections with `CONFIGURE_DEPENDS`, removes `src/main.cpp` from the common set, and compiles everything else into `ornlslicer_obj`. The normal application links that object library; Windows additionally builds `ornlslicer_cli` from the same entry point without the GUI subsystem link flag. [source](repo://CMakeLists.txt#L139-L164) [source](repo://CMakeLists.txt#L214-L227)

Tests are opt-in through `ORNLSLICER_BUILD_TESTS`, whose default follows CTest's `BUILD_TESTING`. Each test is an explicit executable linked to `ornlslicer_obj` and registered with `add_test()`. A local project-slice regression is registered only when its configured `.s2p` fixture exists, so its absence is an intentional reduction in the configured test set. [source](repo://CMakeLists.txt#L229-L241) [source](repo://CMakeLists.txt#L463-L476)

## Compiler caches and accelerated builds

Compiler caching is enabled by default. Configure searches for `sccache` first, then `ccache`, and installs the first one found as both the C and C++ compiler launcher; if neither exists, compilation continues uncached. The Nix developer shell includes ccache, while a caller can force deterministic uncached behavior with `-DORNLSLICER_ENABLE_COMPILER_CACHE=OFF` when the cache directory is unavailable or unwritable. [source](repo://CMakeLists.txt#L38-L54) [source](repo://flake.nix#L117-L127)

Precompiled headers are enabled by default for the object library. Unity builds are off by default and, when enabled, group object-library sources in batches of 16. These controls affect compilation strategy, not the installed interface; use the ordinary preset when diagnosing include-order or translation-unit-specific behavior hidden by a unity build. [source](repo://CMakeLists.txt#L166-L212)

## Generated settings resources

The split files under `resources/settings/` are canonical. The generator produces both `resources/configs/master.conf` and `resources/configs/setting_inputs.conf`; YAML file order and setting order control emitted configuration and UI order. [source](repo://resources/settings/README.md#L1-L15)

With `ORNLSLICER_AUTO_GENERATE_MASTER_CONFIG=ON`, CMake tracks every settings YAML plus the generator script, writes both outputs into the source tree, and makes `ornlslicer_obj` depend on the generation target. Missing YAML or Python is a configure error. Because the generated files live in the repository, a build after settings edits can legitimately change the working tree. [source](repo://CMakeLists.txt#L478-L523)

The Nix derivation deliberately configures with automatic generation off. Therefore a Nix or CI package consumes the checked-in generated configuration files; a settings change is not package-complete until both outputs have been regenerated and included with the canonical YAML change. [source](repo://nix/ornlslicer/default.nix#L39-L49) [source](repo://resources/settings/README.md#L1-L15)

## Version and diagnostic provenance

`version.json` is read in two forms: the CMake project version omits the suffix, while build/runtime version text uses `major.minor.patch-suffix` when a suffix exists. During configure, CMake also captures the latest Git commit, appends `-dirty` when tracked work differs, classifies the configuration as multi-config, a named single configuration, or unspecified, and accepts an overridable package type whose default is `local`. Those values are substituted into `generated/include/ornlslicer/build_info.h`. [source](repo://cmake/version.cmake#L3-L45) [source](repo://cmake/git.cmake#L3-L37) [source](repo://cmake/build_info.cmake#L1-L25) [source](repo://CMakeLists.txt#L28-L36)

Both GUI and successful CLI startup log a one-line runtime summary. It includes mode, semantic version, Git revision, build configuration, package type, Qt runtime version, and Qt compile-time version; the application version exposed to Qt comes from the same generated header. This makes a captured stderr line sufficient to distinguish source revisions and Qt/runtime mismatches, provided the build was reconfigured after provenance changed. [source](repo://src/utilities/runtime_diagnostics.cpp#L9-L27) [source](repo://src/main.cpp#L69-L106)

## Nix package graph

The flake constructs native packages and a `windows` package set using the MinGW cross toolchain. The ORNLSlicer derivation supplies CMake, pkg-config and Ninja as native tools; links the complete Qt/geometry/data dependency set; uses the Linux Qt wrapper when native; and uses the Windows Qt plugin deployment hook when cross compiling. [source](repo://flake.nix#L52-L83) [source](repo://nix/ornlslicer/default.nix#L1-L55)

The CMake install tree contains the application, the Windows console binary and runtime DLLs on Windows, the user-guide PDF, and template directory. That install tree is what Nix packages and what the Windows installer copies. [source](repo://CMakeLists.txt#L554-L564) [source](repo://scripts/installer.nsi#L49-L78)

The flake also advertises the project's Cachix substituter and trusted public key. Accepting the flake configuration lets local and CI Nix builds reuse those signed binaries; CI publishes new cache results only for the `develop` branch. [source](repo://flake.nix#L150-L153) [source](repo://.github/workflows/ci.yml#L30-L43) [source](repo://.github/workflows/ci.yml#L73-L89)

## Linux AppImage

The AppImage bundler accepts a derivation or flake app but is restricted to x86-64 Linux. Its custom `AppRun` checks `APPDIR`, then launches the packaged entry point through a pinned static PRoot while binding the bundled Nix store at `/nix`. This is path translation under the invoking user, not an elevation or security sandbox. [source](repo://flake.nix#L85-L108) [source](repo://nix/appimage/default.nix#L7-L35) [source](repo://docs/contributing/linux-appimage.md#L3-L18)

Build the same artifact shape as CI with:

```bash
nix build -L .#legacyPackages.x86_64-linux.ornl.ornlslicer --accept-flake-config
nix bundle -L --accept-flake-config --bundler .#appimage \
  .#legacyPackages.x86_64-linux.ornl.ornlslicer -o ornlslicer.appimage
```

CI builds the Linux derivation, bundles it, then smoke-tests `--help` as a non-root user on Ubuntu 24.04 with the default unprivileged-user-namespace restriction enabled. Release validation additionally covers GUI launch, native dialogs, preference import/export, additional settings locations, and output-file ownership. [source](repo://.github/workflows/ci.yml#L21-L61) [source](repo://docs/contributing/linux-appimage.md#L33-L72)

## Windows portable tree and installer

CI builds `windows.ornl.ornlslicer` on an Ubuntu runner, names a portable directory and installer from the derivation metadata and workflow run, and invokes `makensis` from the developer shell with the portable tree and version as definitions. Both the portable tree and installer executable are uploaded. [source](repo://.github/workflows/ci.yml#L63-L107)

The NSIS package installs for all users under 64-bit Program Files and requests administrator rights. It copies the full CMake/Nix install tree, records uninstall metadata and release links, creates application and uninstall Start Menu shortcuts, and removes the install and shortcut directories on uninstall. [source](repo://scripts/installer.nsi#L22-L47) [source](repo://scripts/installer.nsi#L49-L97)

## CI boundary

Every push cancels an older in-progress run for the same workflow/ref. The check matrix runs `nix flake check --all-systems` on Ubuntu and macOS. Separate jobs build and package Linux and Windows artifacts with Git LFS content present; only the Linux AppImage receives a command-line smoke test in this workflow. Treat successful packaging as evidence for those declared checks, not as proof that every CTest executable or interactive GUI workflow ran. [source](repo://.github/workflows/ci.yml#L1-L19) [source](repo://.github/workflows/ci.yml#L21-L61) [source](repo://.github/workflows/ci.yml#L63-L107)

Related reading: [Quickstart](../quickstart.md), [Settings and Preferences](../architecture/settings-and-preferences.md), and [Testing Strategy](../testing/strategy.md).
