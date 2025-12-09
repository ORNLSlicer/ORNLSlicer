# Pull Requests

Well-structured pull requests (PRs) keep review fast, history clean, and quality high. This guide defines how to prepare, describe, review, and merge changes.

## Contents

- [Pre-PR Checklist](#pre-pr-checklist)
- [PR Title Format](#pr-title-format)
- [Description Template](#description-template)
- [Draft vs Ready](#draft-vs-ready)
- [Review Expectations](#review-expectations)
- [Responding to Feedback](#responding-to-feedback)
- [Keeping PRs Focused](#keeping-prs-focused)
- [Updating Your Branch](#updating-your-branch)
- [Merge Strategy](#merge-strategy)
- [Post-Merge Follow-Up](#post-merge-follow-up)
- [Common Anti-Patterns](#common-anti-patterns)

---

## Pre-PR Checklist

Complete these before marking a PR ready for review:
- Branch follows convention: see `conventional-branch.md` (e.g. `feat/path-optimizer`).
- Commits follow Conventional Commits; squash fixups locally before opening.
- Code builds locally; no new warnings introduced intentionally.
- Public APIs, structs, classes, and complex functions documented (see `documentation.md`).
- Formatting applied: `clang-format` run on all modified C++ headers and sources.
- No stray debug code, commented-out blocks, or unused includes.
- Added/updated tests where feasible (logic, parsing, geometry, algorithms). If not applicable, state why.
- Large assets or generated files are excluded from the diff.
- Rebased or updated from `develop`; resolved conflicts cleanly.

---

## PR Title Format

Use a Conventional Commit style summary:
```
<type>(<optional-scope>): <concise summary>
```
Examples:
- `feat(slicing): add adaptive layer thickness strategy`
- `fix(gcode): correct extrusion rate overflow`
- `refactor(geometry): simplify polygon stitcher`
- `docs: clarify cross_section API usage`
For breaking changes add `!` after type: `feat!: remove legacy path planner` and explain impact in description.

---

## Description Template

Provide context & rationale—copy/adapt this template:
```
Summary
Briefly state WHAT the PR does and WHY (problem + approach).

Context / Motivation
Link issues, tickets, or prior discussions. Note constraints or design decisions.

Changes
- Bullet list of key changes (APIs, algorithms, data structures, UI elements)
- Note any removed or deprecated functionality

Breaking Changes (if any)
Describe surface impact, migration steps, and affected modules.

Testing
Describe manual and automated validation: commands run, sample files sliced, edge cases.

Performance (if relevant)
State observed impact (e.g., slice time -12%, memory +8%). Include methodology.

Documentation
List added/updated Doxygen blocks or user docs pages.

Outstanding / Follow-ups (optional)
Items intentionally deferred (scope control), with links/issues.

Review Notes (optional)
Call out areas needing focused attention or risk.
```

---

## Draft vs Ready

- Open as Draft if scope may change, CI needs to pass first, or design feedback is sought early.
- Mark Ready only when checklist is met and description complete.

---

## Review Expectations

- Reviewers focus on correctness, clarity, performance, safety, and consistency with style/architecture.
- Prefer comments suggesting direction: “Consider extracting…” vs “Bad pattern”.
- Use suggestions for small textual/code edits.
- Approvals require: tests (when applicable), docs updated, no unresolved blocker comments.

---

## Responding to Feedback

1. Address each thread; push changes or explain if declined.
2. Avoid stacking unrelated refactors—open follow-up issues instead.
3. Mark conversations resolved only after change or explicit consensus.
4. Re-request review after substantive updates (force-push rebases allowed).

---

## Keeping PRs Focused

- Target a single atomic improvement or feature.
- Split out large refactors or generated changes (e.g. formatting sweeps) into separate PRs.
- If diff > ~1,000 LOC (excluding tests), reevaluate scope or split.

---

## Updating Your Branch

Keep history tidy and linear:
```
git fetch origin
git rebase origin/develop
# Resolve conflicts, run build & tests
git push --force-with-lease
```
Use `--force-with-lease` (never plain `--force`) to protect collaborators’ updates.

---

## Merge Strategy

We use squash merges for feature/fix PRs:
- Produces a single, traceable commit in `develop`.
- Keeps commit history noise low (no fixup or WIP commits).
- Encourages high-quality final commit message (auto-filled from title + description summary).
For large foundational refactors or history-sensitive work (rare), maintainers may choose a rebase merge—note this explicitly in the PR.

Final squash commit message should:
1. Keep Conventional Commit title.
2. Include a condensed “Changes” list (if helpful for future archaeology).
3. Include `BREAKING CHANGE:` footer if applicable.

---

## Post-Merge Follow-Up

- Delete the source branch after merge (GitHub UI or `git push origin --delete <branch>`).
- Verify CI status on `develop` after merge.
- Open issues for deferred items listed under “Outstanding / Follow-ups”.
- If user docs impacted, ensure wiki or guide updates are published.

---

## Common Anti-Patterns

- Combining feature + large formatting sweep.
- Hidden breaking changes (silent behavior shifts without docs).
- Unexplained magic numbers or geometry tolerances.
- Drive-by unrelated refactors increasing review surface.

---

Cross-reference: [`CONTRIBUTING.md`](../../CONTRIBUTING.md), [`conventional-commits.md`](conventional-commits.md), [`conventional-branch.md`](conventional-branch.md), [`formatting.md`](formatting.md), [`documentation.md`](documentation.md), [`style-guide.md`](style-guide.md)

