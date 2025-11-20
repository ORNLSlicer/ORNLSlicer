# Contributing

This guide summarizes expectations for code style, formatting, documentation, and workflow so contributions remain consistent, maintainable, and easy to review.

---

## Quick Workflow

1. Create a branch using the conventional branch scheme (e.g. `feat/feature-name`).
2. Implement changes following the style & formatting guides.
3. Add/adjust Doxygen docs for public APIs.
4. Run `clang-format` on changed files.
5. Write commits using Conventional Commits.
6. Rebase or update from `develop` if needed; resolve conflicts.
7. Open a pull request with a clear summary and rationale.
8. Address review feedback promptly; keep diffs focused.

---

## [Conventional Branch Naming](/doc/contributing/conventional-branch.md)

Name branches with `<type>/<description>` for clarity and CI/CD automation.

## [Conventional Commits](/doc/contributing/conventional-commits.md)

Use structured commit messages (type, scope, description, optional breaking change).

## [Pull Requests](/doc/contributing/pull-requests.md)

Prepare focused PRs using the checklist & description template; we squash merge features/fixes.

## [Issue Submissions](/doc/contributing/issue-submissions.md)

Submit all tasks, bugs, and features as issues first for tracking and discussion.

## [Style Guide](/doc/contributing/style-guide.md)

Follow naming, layout, and structural conventions; favor clarity over cleverness.

## [Formatting](/doc/contributing/formatting.md)

Use `clang-format` before committing; never hand-adjust whitespace.

## [Documentation](/doc/contributing/documentation.md)

Document public APIs with Doxygen blocks (purpose, constraints, usage).
