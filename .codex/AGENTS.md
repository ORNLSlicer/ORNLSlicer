# AGENTS.md

## Core Workflow

- Run `git status --short --branch` first; preserve unrelated user changes.
- Start from the exact artifact the user named: file, branch, PR, commit,
  staged diff, failing command, or error text.
- Use `rg` to trace declarations, call sites, state owners, worker boundaries,
  and final consumers before editing behavior.

## Routing

| Work | Source Of Truth |
| --- | --- |
| Public API contracts | Header Doxygen and [`documentation.md`](../docs/contributing/documentation.md) |
| Commits, PRs, contributor workflow | [`CONTRIBUTING.md`](../CONTRIBUTING.md) and [`docs/contributing/`](../docs/contributing/) |

Implementation is usually paired between [`include/`](../include/) and
[`src/`](../src/). Settings source is [`resources/settings/`](../resources/settings/);
generated settings config is [`resources/configs/`](../resources/configs/).

## Repo-Specific Rules

- Prefer existing session, settings, GUI, and worker patterns over new parallel
  state. Keep blocking mesh, project, slicing, and G-code work off the GUI
  thread.
- CMake uses globbed source/resource lists. Re-run configure after adding or
  deleting `.cpp`, `.h`, or resource files, or after stale source/PCH build
  failures.
- `resources/settings/*.yaml` is the settings source of truth. Regenerate both
  config outputs with:
  `python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf resources/configs/setting_inputs.conf`
- Validate generated settings JSON with:
  `jq empty resources/configs/master.conf resources/configs/setting_inputs.conf`
- The `ornlslicerDev` Nix shell provides the repository's `pre-commit` CLI;
  enter it with `nix develop .#ornlslicerDev` before installing or running the
  local hook.

## Validation Commands

- Configure: `nix develop .#ornlslicerDev -L --command cmake --preset generic-llvm-ninja`
- Build app: `nix develop .#ornlslicerDev -L --command cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer`
- Narrow compile check: `nix develop .#ornlslicerDev -L --command cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_obj`
- Docs-only check: `git diff --check -- <touched-files>`

Use `build/generic-llvm-ninja`, not top-level `build/`. If stronger validation
cannot run, state the exact blocker and the lighter checks that did run.

## Formatting

- Format changed C++ files with repository `.clang-format`; there is no
  repo-local formatter wrapper in `scripts/`.
- Install the local formatting hook from the development shell with
  `pre-commit install`, and run staged-file checks with `pre-commit run`.
- Keep pure formatting separate from behavior changes unless limited to touched
  lines.
- For Markdown, use relative links, keep one blank line after headings, and run
  `git diff --check`.

## Git And Reviews

- Use Conventional Commits and [`.github/pull_request_template.md`](../.github/pull_request_template.md).
- Before committing or drafting a PR, inspect the exact diff; if the user says
  staged only, inspect only the staged diff.
- For reviews, stay bounded to the named artifact, lead with actionable
  findings, cite file/line references, and say clearly when there are no
  findings.
