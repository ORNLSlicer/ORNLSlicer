# Conventional Commits Guide

This project uses the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) specification for commit messages. This improves clarity, consistency, changelog generation, release automation, and team collaboration.

## Contents

- [Commit Message Format](#commit-message-format)
- [Commit Types](#commit-types)
- [Commit Scopes](#commit-scopes)
- [Breaking Changes](#breaking-changes)

---

## Commit Message Format

Each commit message should follow this structure:

```
<type>(<optional scope>): <description>

<optional body>

<optional footer>
```

---

## Commit Types

Use the following types to categorize your changes:

- `build`: Build system or dependency changes
- `chore`: Non-source or non-test changes (e.g., config updates)
- `ci`: CI configuration or scripts
- `docs`: Documentation changes only
- `feat`: New feature
- `fix`: Bug fix
- `perf`: Performance improvement
- `refactor`: Code refactoring (no bug fix or feature)
- `revert`: Revert a previous commit
- `style`: Code style changes (formatting, whitespace, etc.)
- `test`: Add or update tests
- `wip`: Work in progress

---

## Commit Scopes

Scopes specify the module, component, or area affected. Example scopes include:

- `algorithms`: Slicing algorithms (KNN, TSP, etc.)
- `clang-format`
- `cmake`
- `configs`: Configuration and settings
- `console`: CLI and main control
- `cross-section`: Cross-section processing
- `docs`
- `doxygen`
- `external-files`: Parsers and exporters
- `gcode`: G-code management
- `geometry`: Geometric primitives
- `git`
- `graphics`: Rendering and visualization
- `managers`: Resource/state managers
- `nix`
- `optimizers`: Path/toolpath optimization
- `part`: Part management
- `readme`
- `slicing`: Slicing functionality
- `step`: Manufacturing step processing
- `templates`: Config templates
- `tests`
- `threading`: Multi-threading
- `ui`: User interface/widgets
- `units`: Unit conversion
- `utilities`: Utility functions
- `windows`: Application windows

For multiple scopes, separate with a comma and space:

```
feat(gcode, slicing): add support for adaptive layer heights
```

---

## Breaking Changes

Indicate breaking changes in one of two ways:

1. **Footer**: Add a footer with `BREAKING CHANGE:` followed by a description.

	```
	feat: allow provided config object to extend other configs

	BREAKING CHANGE: `extends` key in config file is now used for extending other config files
	```

2. **Type/Scope Prefix**: Add `!` before the colon in the type/scope prefix. The commit description must explain the breaking change. The footer is optional.

	```
	feat!: send an email to the customer when a product is shipped
	```
