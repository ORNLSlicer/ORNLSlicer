---
type: operations
title: Build, Generated Assets, CI, and Packaging
description: Reproducible local and CI build paths, generated settings and build metadata, compiler-cache behavior, and Linux and Windows distribution assembly.
tags: [build, cmake, nix, ci, packaging, diagnostics]
sources:
  - id: openwiki-source-a836451b0e5d245b3d5c916a
    resource: repo://.codex/environments/environment.toml
  - id: openwiki-source-c3d475a24efcb0fdbdb0aaf7
    resource: repo://.envrc
  - id: openwiki-source-22672afc9bd1f970cd00e64d
    resource: repo://.github/workflows/auto-rebase.yaml
  - id: openwiki-source-164e2da859b5277df81c7d94
    resource: repo://.github/workflows/ci.yml
  - id: openwiki-source-6d4b4e707b8d60b6ccfa3425
    resource: repo://.github/workflows/openwiki-update.yml
  - id: openwiki-source-958d20e1c524bd4847a88ba0
    resource: repo://.github/workflows/prepare-release.yml
  - id: openwiki-source-d02327111261f96a065fb8de
    resource: repo://cmake/build_info.cmake
  - id: openwiki-source-5d790bd7059cfd1497fadcf0
    resource: repo://cmake/git.cmake
  - id: openwiki-source-a60928f26402a7ffadc427b3
    resource: repo://cmake/presets/generic-llvm-ninja.json
  - id: openwiki-source-c9d11891f93487ed035fc596
    resource: repo://cmake/validate_resources.cmake
  - id: openwiki-source-5e070533cc02d87db926df69
    resource: repo://cmake/version.cmake
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-f5502ee67e629f24f1284e65
    resource: repo://docs/contributing/development-setup.md
  - id: openwiki-source-6021bafd170e50774c4e66a0
    resource: repo://docs/contributing/versioning.md
  - id: openwiki-source-3eda5d2b85501c78fc0ba02d
    resource: repo://flake.nix
  - id: openwiki-source-41844989991110a96eb2c453
    resource: repo://nix/appimage/default.nix
  - id: openwiki-source-71ae1012989bc835de1e9233
    resource: repo://nix/ornlslicer/default.nix
  - id: openwiki-source-afb703049218d4fb87ac4eb5
    resource: repo://nix/windows-installer/default.nix
  - id: openwiki-source-6f5d88049adbd611fb8e2754
    resource: repo://resources/settings/README.md
  - id: openwiki-source-dc724c400ef7006f36d0b685
    resource: repo://scripts/installer.nsi
  - id: openwiki-source-d469b03ab78fae94381a088e
    resource: repo://scripts/version.py
  - id: openwiki-source-d364d949938a433276255c32
    resource: repo://src/main.cpp
  - id: openwiki-source-3a34e9951a829d6407a1f125
    resource: repo://src/utilities/runtime_diagnostics.cpp
generated: { by: "codex", at: "2026-11-10T17:47:51.624Z" }
verified:
  - by: openwiki/0.7.2
    at: 2026-11-10T17:47:51.624Z
---

# Build, Generated Assets, CI, and Packaging

ORNLSlicer has two cooperating build layers. CMake describes compilation, generated headers and settings, tests, installation, and platform targets. Nix supplies pinned dependencies, the developer shell, native and Windows-cross packages, the Linux AppImage bundler, and the same package entry points used by CI. [source](repo://CMakeLists.txt#L1-L36) [source](repo://flake.nix#L1-L43) [source](repo://flake.nix#L46-L127)

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

The default `ornlslicerDev` shell inherits the package's build inputs and adds Git, jq, ccache, pre-commit, Doxygen/Graphviz, LLDB, clang tools, Python utilities, and Linux-only NSIS, `cntr`, and `clazy`. The flake uses LLVM 18 for the build environment and selects separate LLVM tooling packages—18 on x86-64 Darwin and 22 elsewhere. [source](repo://flake.nix#L14-L41) [source](repo://flake.nix#L130-L166)

The canonical cross-platform procedure is [Developer Setup](../../docs/contributing/development-setup.md). Restore Git LFS assets in each checkout before configuration. Enter `nix develop .#ornlslicerDev -L --accept-flake-config`, wait for it to succeed, then configure this checkout and build. For later terminals, the checked-in `.envrc` uses `use flake`; install the appropriate shell hook and run `direnv allow` for each trusted checkout. The guide also owns WSL2/macOS prerequisites and managed-network certificate recovery. [source](repo://docs/contributing/development-setup.md#L138-L217)

The equivalent explicit commands are:

```bash
nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer -j 32
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug -j 32
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug --output-on-failure
```

The preset fixes `clang`, `clang++`, `Ninja Multi-Config`, and `build/generic-llvm-ninja`; its Debug and Release build presets select the corresponding configuration. A `generic-llvm-ninja-fast` variant enables CMake unity builds in a separate build directory for faster clean builds. [source](repo://cmake/presets/generic-llvm-ninja.json#L1-L55)

For a narrow compile check, build `ornlslicer_obj`. The GUI executable is only `src/main.cpp` plus that object library, and every focused unit executable also links the same object library, so this target catches compilation failures across the shared production sources without linking the application or tests. [source](repo://CMakeLists.txt#L139-L164) [source](repo://CMakeLists.txt#L224-L245)

## Managed Codex worktrees

Codex worktrees have a generated repository-local environment definition rather than a second dependency stack. Its Linux setup requires Git, Git LFS, and Nix; verifies that it is running at this repository's physical worktree root; checks the flake and CMake preset files; materializes LFS assets; and configures `build/generic-llvm-ninja` through `ornlslicerDev` with flake configuration accepted and lock-file writes disabled. Setup is considered complete only when the expected `CMakeCache.txt` exists. Treat `.codex/environments/environment.toml` as generated configuration, not as a hand-maintained substitute for `flake.nix` or the checked-in presets. [source](repo://.codex/environments/environment.toml#L1-L58)

The matching cleanup intentionally performs no shared-store deletion. The build directory is ignored and worktree-local, while the Nix store, Git LFS object store, and ccache can serve other ORNLSlicer worktrees and are preserved when this checkout is removed. [source](repo://.codex/environments/environment.toml#L60-L72)

## CMake topology and dependencies

The project requires CMake 3.25 and C++23, enables Qt automoc and autorcc, and emits `compile_commands.json`. Its required dependency graph includes Qt 6 GUI/Widgets/OpenGL/Core/Concurrent/Network/Charts/Core5Compat, Assimp, Boost.System, CGAL with Eigen support, Eigen, nlohmann-json, OpenCASCADE STEP/mesh libraries, VTK, zip, polyclipping, and psimpl. OpenMP is optional: its absence produces a warning and an unoptimized build rather than a configure failure. [source](repo://CMakeLists.txt#L1-L31) [source](repo://CMakeLists.txt#L64-L124)

CMake recursively discovers implementation files, public headers, and Qt resource collections with `CONFIGURE_DEPENDS`, removes `src/main.cpp` from the common set, and compiles everything else into `ornlslicer_obj`. The normal application links that object library; Windows additionally builds `ornlslicer_cli` from the same entry point without the GUI subsystem link flag. [source](repo://CMakeLists.txt#L139-L164) [source](repo://CMakeLists.txt#L224-L237)

Tests are opt-in through `ORNLSLICER_BUILD_TESTS`, whose default follows CTest's `BUILD_TESTING`. Each test is an explicit executable linked to `ornlslicer_obj` and registered with `add_test()`. A local project-slice regression is registered only when its configured `.s2p` fixture exists, so its absence is an intentional reduction in the configured test set. [source](repo://CMakeLists.txt#L239-L245) [source](repo://CMakeLists.txt#L748-L760)

The focused settings and toolbar lanes are named `settings_file_comparator_tests`, `settings_manager_tests`, and `main_toolbar_tests`. Build the specific executables before invoking CTest, especially after changing shared production sources:

```bash
nix develop .#ornlslicerDev -L --command \
  cmake --build build/generic-llvm-ninja --config Debug --target \
  ornlslicer_settings_file_comparator_tests ornlslicer_settings_manager_tests ornlslicer_main_toolbar_tests
nix develop .#ornlslicerDev -L --command \
  ctest --test-dir build/generic-llvm-ninja -C Debug \
  -R '^(settings_file_comparator_tests|settings_manager_tests|main_toolbar_tests)$' --output-on-failure
```

All three executables link `ornlslicer_obj`, so the build also compiles the shared implementation they exercise. [source](repo://CMakeLists.txt#L309-L340) [source](repo://CMakeLists.txt#L452-L461)

CMake checks every `.qrc`-referenced resource at configure time and through a `validate_resources` dependency of `ornlslicer_obj`. An unresolved Git LFS pointer fails with examples and recovery commands, preventing RCC from silently bundling pointer text as image/font data. Run `git lfs pull` (or `git lfs checkout` for cached objects), then configure and rebuild. The font license is installed with the documentation. [source](repo://cmake/validate_resources.cmake#L1-L35) [source](repo://CMakeLists.txt#L19-L20) [source](repo://CMakeLists.txt#L149-L157) [source](repo://CMakeLists.txt#L848-L851)

## Compiler caches and accelerated builds

Compiler caching is enabled by default. Configure searches for `sccache` first, then `ccache`, and installs the first one found as both the C and C++ compiler launcher; if neither exists, compilation continues uncached. The Nix developer shell includes ccache, while a caller can force deterministic uncached behavior with `-DORNLSLICER_ENABLE_COMPILER_CACHE=OFF` when the cache directory is unavailable or unwritable. [source](repo://CMakeLists.txt#L38-L54) [source](repo://flake.nix#L117-L127)

Precompiled headers are enabled by default for the object library. Unity builds are off by default and, when enabled, group object-library sources in batches of 16. These controls affect compilation strategy, not the installed interface; use the ordinary preset when diagnosing include-order or translation-unit-specific behavior hidden by a unity build. [source](repo://CMakeLists.txt#L166-L212)

## Generated settings resources

The split files under `resources/settings/` are canonical. The generator produces both `resources/configs/master.conf` and `resources/configs/setting_inputs.conf`; YAML file order and setting order control emitted configuration and UI order. [source](repo://resources/settings/README.md#L1-L15)

With `ORNLSLICER_AUTO_GENERATE_MASTER_CONFIG=ON`, CMake tracks every settings YAML plus the generator script, writes both outputs into the source tree, and makes `ornlslicer_obj` depend on the generation target. Missing YAML or Python is a configure error. Because the generated files live in the repository, a build after settings edits can legitimately change the working tree. [source](repo://CMakeLists.txt#L763-L807)

The Nix derivation deliberately configures with automatic generation off. Therefore a Nix or CI package consumes the checked-in generated configuration files; a settings change is not package-complete until both outputs have been regenerated and included with the canonical YAML change. [source](repo://nix/ornlslicer/default.nix#L39-L49) [source](repo://resources/settings/README.md#L1-L15)

## Version and diagnostic provenance

`version.json` is read in two forms: the CMake project version omits the suffix, while build/runtime version text uses SemVer prerelease/build-metadata syntax (`2.2.0-rc.1`, `2.2.0+build.5`, or both). During configure, CMake also captures the latest Git commit, appends `-dirty` when tracked work differs, classifies the configuration as multi-config, a named single configuration, or unspecified, and accepts an overridable package type whose default is `local`. Those values are substituted into `generated/include/ornlslicer/build_info.h`. [source](repo://cmake/version.cmake#L3-L80) [source](repo://cmake/git.cmake#L3-L37) [source](repo://cmake/build_info.cmake#L1-L25) [source](repo://CMakeLists.txt#L33-L41)

Both GUI and successful CLI startup log a one-line runtime summary. It includes mode, semantic version, Git revision, build configuration, package type, Qt runtime version, and Qt compile-time version; the application version exposed to Qt comes from the same generated header. This makes a captured stderr line sufficient to distinguish source revisions and Qt/runtime mismatches, provided the build was reconfigured after provenance changed. [source](repo://src/utilities/runtime_diagnostics.cpp#L9-L27) [source](repo://src/main.cpp#L72-L109)

## Nix package graph

The flake constructs native packages and a `windows` package set using the MinGW cross toolchain. The ORNLSlicer derivation supplies CMake, pkg-config and Ninja as native tools; links the complete Qt/geometry/data dependency set; uses the Linux Qt wrapper when native; and uses the Windows Qt plugin deployment hook when cross compiling. [source](repo://flake.nix#L46-L103) [source](repo://nix/ornlslicer/default.nix#L1-L55)

The CMake install tree contains the application, the Windows console binary and runtime DLLs on Windows, the user-guide PDF, and template directory. That install tree is what Nix packages and what the Windows installer copies. [source](repo://CMakeLists.txt#L841-L851) [source](repo://scripts/installer.nsi#L49-L78)

The application source fileset includes `CHANGELOG.md`, CMake inputs, `include/`, `src/`, `tests/`, `resources/`, `templates/`, `version.json`, and the installed user guide. Workflow, wiki, and non-installed documentation changes therefore do not change that application source path. The changelog is included because the GUI embeds it as a Qt resource. Nix expressions and dependencies still independently affect derivations. [source](repo://flake.nix#L49-L83) [source](repo://resources/docs/docs.qrc#L1-L5)

The flake advertises the signed Cachix substituter and public key. CI accepts flake configuration; cache write credentials are supplied only for pushes to `develop` and published releases. Develop pushes also realize the Linux development shell. Releases synchronously push four outputs before creating permanent version-specific pins for Linux, AppImage, Windows, and the Windows installer. Each release tag must equal `v` plus the package version. Other branches consume the cache without its write token. See [Cachix Workflow](../../docs/contributing/cachix.md) for developer publishing and retention verification. [source](repo://flake.nix#L170-L173) [source](repo://.github/workflows/ci.yml#L55-L99) [source](repo://.github/workflows/ci.yml#L123-L157)

## Linux AppImage

The AppImage bundler accepts a derivation or flake app but is restricted to x86-64 Linux. Its custom `AppRun` checks `APPDIR`, then launches the packaged entry point through a pinned static PRoot while binding the bundled Nix store at `/nix`. This is path translation under the invoking user, not an elevation or security sandbox. [source](repo://flake.nix#L105-L127) [source](repo://nix/appimage/default.nix#L7-L35) [source](repo://docs/contributing/linux-appimage.md#L3-L18)

Build the same artifact shape as CI with:

```bash
nix build -L .#legacyPackages.x86_64-linux.ornl.ornlslicer --accept-flake-config
nix bundle -L --accept-flake-config --bundler .#appimage \
  .#legacyPackages.x86_64-linux.ornl.ornlslicer -o ornlslicer.appimage
```

CI builds the Linux derivation, bundles it, then smoke-tests `--help` as a non-root user on Ubuntu 24.04 with the default unprivileged-user-namespace restriction enabled. Release validation additionally covers GUI launch, native dialogs, preference import/export, additional settings locations, and output-file ownership. [source](repo://.github/workflows/ci.yml#L21-L61) [source](repo://docs/contributing/linux-appimage.md#L33-L72)

## Windows portable tree and installer

CI builds `windows.ornl.ornlslicer` and the first-class `windowsInstaller` derivation on Ubuntu. The installer derivation runs native NSIS against the versioned Windows tree with normalized input timestamps and a deterministic `ornlslicer-<version>-installer.exe` filename. CI adds its run number only to the copied upload name; both portable and installer outputs remain independently cacheable. [source](repo://flake.nix#L93-L103) [source](repo://nix/windows-installer/default.nix#L10-L46) [source](repo://.github/workflows/ci.yml#L110-L175)

The NSIS package installs for all users under 64-bit Program Files and requests administrator rights. It copies the full CMake/Nix install tree, records uninstall metadata and release links, creates application and uninstall Start Menu shortcuts, and removes the install and shortcut directories on uninstall. [source](repo://scripts/installer.nsi#L22-L47) [source](repo://scripts/installer.nsi#L49-L97)

## CI boundary

Pushes, fork pull requests, manual dispatches, and published releases run Nix CI. Same-repository PR events skip the version gate because their branch pushes already run CI. Version validation runs first and gates the Ubuntu/macOS flake checks and Linux/Windows packaging jobs. The declared runtime smoke test is Linux AppImage `--help` under a non-root user; the workflow does not declare a full CTest or interactive GUI pass. Concurrency includes event and PR/ref identity and cancels older non-release runs; published release runs are retained. [source](repo://.github/workflows/ci.yml#L1-L40) [source](repo://.github/workflows/ci.yml#L76-L90) [source](repo://.github/workflows/ci.yml#L110-L175)

Auto-rebase uses a repository App token, checks out `develop`, and limits PR targets to branches in the same repository. Fork PRs are skipped; preserve this boundary whenever changing its `pull_request_target` handling. [source](repo://.github/workflows/auto-rebase.yaml#L44-L101)

## Release preparation and changelog

`version.json` contains exactly four string fields (`major`, `minor`, `patch`, `suffix`), canonical numeric components, and MAJOR at most 2. CMake and Nix use validated readers; product versions are separate from settings schema versions. `scripts/version.py validate` checks schema and optional base/tag constraints. `prepare` defaults to a JSON dry run; only `--write` changes `version.json` and it refuses existing edits to that file. [source](repo://cmake/version.cmake#L3-L80) [source](repo://nix/version.nix#L1-L27) [source](repo://scripts/version.py#L78-L126) [source](repo://scripts/version.py#L214-L254)

Release inference uses eligible canonical tags and full Conventional Commit messages: `fix`/`perf` select PATCH, `feat` selects MINOR, and breaking messages require MAJOR below 2. At MAJOR 2, breaking changes require an explicit MINOR bump plus a compatibility note; the tool never produces MAJOR 3. Missing/mismatched tags, ambiguous metadata, and non-increasing candidates fail. Read [Versioning and Release Preparation](../../docs/contributing/versioning.md) for tag eligibility and prerelease rules. [source](repo://scripts/version.py#L113-L211)

The manually dispatched Prepare release workflow runs on `develop` and defaults to dry run. Write mode creates a fresh `release/<version>` branch, commits only the version file, opens a PR to `develop`, and explicitly dispatches Nix CI; it refuses to overwrite an existing release branch. Maintainers finalize `CHANGELOG.md` on that PR branch after the version-only commit and carry compatibility notes into release notes. The bundled Help viewer consumes that same file. [source](repo://.github/workflows/prepare-release.yml#L31-L105) [source](repo://docs/contributing/changelog.md#L1-L52) [source](repo://resources/docs/docs.qrc#L1-L5)

After the release PR merges into `develop`, promote `develop` to `main` with a merge commit so version-baseline ancestry remains intact. Finalize the changelog and follow the canonical [release procedure](../../docs/contributing/versioning.md) for tag/publication. Published-release CI waits for checks and Linux/Windows packaging, then uploads AppImage, installer and portable ZIP downloads with `gh release upload --clobber`. [source](repo://docs/contributing/versioning.md#L98-L109) [source](repo://.github/workflows/ci.yml#L177-L197)

## Automated OpenWiki maintenance

The OpenWiki workflow is a separate documentation-maintenance lane. It can be dispatched manually and is scheduled for 08:00 UTC each day. The job checks out full history so OpenWiki can diff against its last documented commit, installs Node.js 22 plus OpenWiki 0.7.0 and optional Mermaid validation dependencies, and runs `openwiki code --update --print`. Provider and connector credentials come from repository secrets; the workflow file contains only their secret references and model/provider configuration. [source](repo://.github/workflows/openwiki-update.yml#L1-L48)

OpenWiki execution is allowed to finish with a recorded failure so the workflow can remove transient `.run.json` state and open or update the `openwiki/update` pull request. That pull request includes generated wiki content, the OpenWiki-managed `AGENTS.md` setup block, the workflow itself, and `CLAUDE.md` only when present. A failed generation deliberately preserves pages completed before the failure in the PR, after which the final step returns a failing workflow status. This lane refreshes documentation evidence; it does not compile ORNLSlicer, run CTest, or replace the Nix CI signals above. [source](repo://.github/workflows/openwiki-update.yml#L33-L88)

Related reading: [Quickstart](../quickstart.md), [Settings and Preferences](../architecture/settings-and-preferences.md), and [Testing Strategy](../testing/strategy.md).
