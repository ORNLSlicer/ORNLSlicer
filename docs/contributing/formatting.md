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
   find . -name '*.cpp' -o -name '*.h' | xargs clang-format -i --style=file
   ```
3. Commit ONLY formatting changes (separate PR) to minimize noise & conflicts.

---

## How to Format Code

Single file:
```bash
clang-format -i --style=file src/main.cpp
```

Multiple files:
```bash
clang-format -i --style=file src/main.cpp include/util.h
```

All C++ headers and sources:
```bash
find . -name '*.cpp' -o -name '*.h' | xargs clang-format -i --style=file
```

Changed files only (staged):
```bash
git diff --cached --name-only -- '*.cpp' '*.h' | xargs clang-format -i --style=file
```

Flags:
- `-i` in-place modify
- `--style=file` use repository `.clang-format`

Enable editor format-on-save to avoid manual runs.

---

## Best Practices

- Format before commit (stage, format, re-stage if needed).
- Use format-on-save / pre-commit hook to enforce style.
- Never hand-tweak whitespace—fix `.clang-format` instead.
- Separate pure formatting PRs from logic changes.
- After style modifications, re-run full-project formatting to normalize.

Questions? Open an issue or start a discussion thread.
