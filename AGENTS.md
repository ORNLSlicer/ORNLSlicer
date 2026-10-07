<!-- OPENWIKI:START -->

## OpenWiki

This repository has a generated `openwiki/` evidence index. It is optional just-in-time context, not required startup reading.

- Do not enumerate, preload, or search wikis at task start. Use retrieval when the user asks for it, when unfamiliar architecture or dependency behavior materially affects the task, or when source inspection leaves an important uncertainty. Stop once the question is grounded.
- When those conditions apply and OpenWiki retrieval tools are available, use `openwiki_search` for just-in-time context and `openwiki_read` for the relevant complete sections. If search returns `workspace_required`, ask which listed workspace to use and retry with its ID.
- Use `openwiki_list_workspaces` or `openwiki_list_wikis` when workspace membership itself needs to be discovered.
- If the retrieval tools are unavailable, read `openwiki/quickstart.md` and follow its links to the relevant pages.
- Treat source code and tests as authoritative. A brief's unknowns and review items are verification gaps, not automatic requirements.
- Prefer the narrowest quiet validation that proves the changed behavior. Preserve complete failure output.

The scheduled OpenWiki GitHub Actions workflow refreshes the repository wiki. Do not hand-edit generated OpenWiki pages unless explicitly asked; prefer updating source code/docs and letting OpenWiki regenerate.

<!-- OPENWIKI:END -->

## Repository Instructions

### Working Rules

- Before modifying files, run `git status --short --branch` and preserve
  unrelated user changes.
- Start from the exact artifact the user named: file, branch, PR, commit,
  staged diff, failing command, or error text.
- For behavioral changes, use `rg` to trace relevant declarations, call sites,
  state owners, worker boundaries, and final consumers before editing.
- For reviews, explanations, and diagnoses, remain read-only unless the user
  also requests implementation.

### Routing

| Work | Start With |
| --- | --- |
| Architecture or dependency investigation | OpenWiki just in time, then verify against source and tests |
| C++ implementation | [`include/`](include/), [`src/`](src/), and relevant [`tests/`](tests/) |
| Tests and test registration | [`tests/`](tests/) and [`CMakeLists.txt`](CMakeLists.txt) |
| Settings | [`resources/settings/`](resources/settings/) and [`generate_master_config.py`](scripts/generate_master_config.py) |
| CMake and Nix builds | [`CMakePresets.json`](CMakePresets.json), [`cmake/`](cmake/), [`flake.nix`](flake.nix), and [`nix/`](nix/) |
| CI and packaging | [`.github/workflows/`](.github/workflows/) and [`scripts/`](scripts/) |
| Public API contracts | Header Doxygen and [`documentation.md`](docs/contributing/documentation.md) |
| Commits, PRs, contributor workflow | [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`docs/contributing/`](docs/contributing/) |

### Repository Invariants

- Prefer existing session, settings, GUI, and worker patterns over new parallel
  state. Keep blocking mesh, project, slicing, and G-code work off the GUI
  thread.
- CMake uses globbed source/resource lists. Configure once per worktree; re-run
  configure after adding or deleting `.cpp`, `.h`, or resource files, or after
  stale source/PCH build failures.
- `resources/settings/*.yaml` is the settings source of truth. Never hand-edit
  `resources/configs/master.conf` or `resources/configs/setting_inputs.conf`;
  regenerate both with:
  `python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf resources/configs/setting_inputs.conf`
- The `ornlslicerDev` Nix shell provides the repository's `pre-commit` CLI;
  enter it with `nix develop .#ornlslicerDev` before installing or running the
  local hook.

### Validation

- Configure: `nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja`
- Shared production compile: `nix develop .#ornlslicerDev -L --command cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_obj`
- Application integration: `nix develop .#ornlslicerDev -L --command cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer`
- Focused test example: `nix develop .#ornlslicerDev -L --command cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_mesh_repair_tests`, then `nix develop .#ornlslicerDev -L --command ctest --test-dir build/generic-llvm-ninja -C Debug -R '^mesh_repair_tests$' --output-on-failure`. Substitute the relevant registered test name.
- Settings: regenerate both config outputs, then run `jq empty resources/configs/master.conf resources/configs/setting_inputs.conf`.
- CMake or preset changes: reconfigure, then build an affected target.
- Documentation only: run `git diff --check -- <touched-files>`.

Use `build/generic-llvm-ninja`, not top-level `build/`. If stronger validation
cannot run, state the exact blocker and the lighter checks that did run.

### Formatting

- Follow [`docs/contributing/formatting.md`](docs/contributing/formatting.md).
  Format changed C++ files with the repository `.clang-format` and keep
  unrelated formatting out of behavioral changes.
- For Markdown, use relative links, keep one blank line after headings, and run
  `git diff --check`.

### Git Workflow

- Use Conventional Commits and [`.github/pull_request_template.md`](.github/pull_request_template.md).
- Before committing or drafting a PR, inspect the exact diff; if the user says
  staged only, inspect only the staged diff.

## Code Review Rules

- Stay bounded to the named artifact and remain read-only unless fixes are
  requested.
- Prioritize correctness, regressions, thread safety, ownership, and missing
  tests over style-only observations.
- Build newly added or modified focused test targets before relying on CTest or
  CI results.
- Lead with actionable findings, cite exact file and line references, and say
  clearly when there are no findings.
