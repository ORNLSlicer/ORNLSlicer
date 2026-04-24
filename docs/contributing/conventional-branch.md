# Conventional Branch Naming Guide

This project uses the [Conventional Branch](https://conventionalbranch.org/) specification for branch names. This ensures clarity, consistency, and supports automation in version control, CI/CD, and team collaboration.

## Contents

- [Overview](#overview)
- [Branch Name Format](#branch-name-format)
- [Naming Rules](#naming-rules)

---

## Overview

- **Purpose-driven names**: Branch names clearly indicate their purpose.
- **CI/CD integration**: Consistent naming enables automated actions (e.g., deployments from release branches).
- **Team clarity**: Explicit names reduce confusion and make collaboration easier.

---

## Branch Name Format

Branch names use the following structure:

```
<type>/<description>
```

### Supported Prefixes

- `master`: Main production branch
- `develop`: Main development branch for integration
- `feat/`: New features (e.g., `feat/add-login-page`)
- `fix/`: Bug fixes (e.g., `fix/header-bug`)
- `hotfix/`: Urgent fixes (e.g., `hotfix/security-patch`)
- `release/`: Release preparation (e.g., `release/v1.2.0`)
- `chore/`: Non-code tasks (e.g., `chore/update-dependencies`)

---

## Naming Rules

1. **Allowed characters**: Use lowercase letters (`a-z`), numbers (`0-9`), and hyphens (`-`). For release branches, dots (`.`) are allowed in version numbers.
2. **No consecutive, leading, or trailing hyphens/dots**: Avoid names like `feat/new--login`, `release/v1.-2.0`, `feat/-new-login`, or `release/v1.2.0.`
3. **Be clear and concise**: Make branch names descriptive but brief.
4. **Include ticket numbers when relevant**: For example, `feat/issue-123-new-login`.
