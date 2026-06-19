# Code Formatting

We enforce a single C++ style using [clang-format](https://clang.llvm.org/docs/ClangFormat.html). Let the tool decide style—avoid manual tweaks.

## Contents

- [Configuration](#configuration)
- [How to Format Code](#how-to-format-code)
- [Pre-Commit Hook](#pre-commit-hook)
- [Best Practices](#best-practices)

---

## Configuration

Style rules live in the repository root `.clang-format` (Google-derived with project overrides: includes, spacing, braces, ordering).

Updating style:
1. Edit `.clang-format` (optionally test variants using an online configurator).
2. Reformat the codebase:
   ```bash
   python3 scripts/format_cpp.py
   ```
3. Verify the result:
   ```bash
   python3 scripts/format_cpp.py --check
   ```
4. Commit ONLY formatting changes (separate PR) to minimize noise & conflicts.

---

## How to Format Code

All tracked C++ headers and sources:
```bash
python3 scripts/format_cpp.py
```

Specific files or directories:
```bash
python3 scripts/format_cpp.py src/main.cpp include/util.h
```

Check without modifying files:
```bash
python3 scripts/format_cpp.py --check
```

Equivalent CMake targets after configuring a build tree:
```bash
cmake --build build/generic-llvm-ninja --target format
cmake --build build/generic-llvm-ninja --target format-check
```

Flags:
- `--check` verifies formatting without modifying files.
- `--clang-format` selects a specific formatter executable.
- `CLANG_FORMAT` can also point to the formatter executable.

The script formats tracked C/C++ files by default. If it is run outside a Git checkout, it falls back to scanning `src/` and `include/`.

GitHub Actions runs the same check on pushes and pull requests. Maintainers should require the formatting job before merging into `develop`.

---

## Pre-Commit Hook

Install the tracked Git hook once per checkout:
```bash
git config core.hooksPath .githooks
```

After installation, `git commit` automatically formats staged C/C++ files with `scripts/format_cpp.py`, re-stages the formatted files, and verifies them with `--check`.

The hook uses `clang-format` from the current environment when available. If it is not available, the hook retries through `nix develop --accept-flake-config`.

The hook refuses to run when a staged C/C++ file also has unstaged changes, because auto-staging after formatting could otherwise include unrelated local edits. Stage or stash those changes, then commit again.

Disable the hook for this checkout:
```bash
git config --unset core.hooksPath
```

---

## Best Practices

- Format before commit (stage, format, re-stage if needed).
- Install the tracked pre-commit hook or use format-on-save to keep changes clean while developing.
- Use the `format` CMake target for an explicit full-tree formatting pass.
- Never hand-tweak whitespace—fix `.clang-format` instead.
- Separate pure formatting PRs from logic changes.
- After style modifications, re-run full-project formatting to normalize.

Questions? Open an issue or start a discussion thread.
