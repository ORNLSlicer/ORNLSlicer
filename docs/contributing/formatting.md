# Code Formatting

We enforce a single C++ style using [clang-format](https://clang.llvm.org/docs/ClangFormat.html). Let the tool decide style—avoid manual tweaks.

## Contents

- [Configuration](#configuration)
- [How to Format Code](#how-to-format-code)
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

## Best Practices

- Format before commit (stage, format, re-stage if needed).
- Use format-on-save or the `format` CMake target to keep changes clean while developing.
- Never hand-tweak whitespace—fix `.clang-format` instead.
- Separate pure formatting PRs from logic changes.
- After style modifications, re-run full-project formatting to normalize.

Questions? Open an issue or start a discussion thread.
