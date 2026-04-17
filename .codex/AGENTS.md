# AGENTS.md

## Project overview

ORNLSlicer is a 3D printing slicer developed by Oak Ridge National Laboratory (ORNL) that focuses on high-performance slicing for large-format additive manufacturing. It is designed to handle complex geometries and optimize print paths for improved print quality and reduced print times.


## Development expectations

- Document any behavior changes in the codebase with an update to the relevant documentation.


## Git and PR conventions

- Always create branches from `develop` to ensure a clean history and avoid merge conflicts.
- Name branches as `<type>/<description>`. Use one of `feat`, `fix`, `build`, `chore`, `ci`, `docs`, `style`, `refactor`, `perf`, `test` for `<type>`, and keep `<description>` short, hyphenated, and issue-linked when applicable (for example, `feat/123-add-login-feature`).
- Format commit messages using the Conventional Commit format: `<type>(<scope>): <description>`. Use the same `<type>` values as branch names, and keep `<scope>` optional but descriptive of the area of the codebase affected (for example, `feat(auth): add login feature`).
- Title pull requests using the same Conventional Commit format as commit messages, and include a description of the change in the PR body using the PR template in `.github/pull_request_template.md`.


## Formatting

- Always leave a blank line after a heading in markdown files.
