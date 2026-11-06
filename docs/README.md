# ORNLSlicer Documentation

This directory is the canonical documentation source for ORNLSlicer.
`Slicer-2` is now `ORNLSlicer`, with `2.0.0` as the renamed product baseline.

## Using ORNLSlicer

- [ORNLSlicer User Guide](ornlslicer-user-guide.pdf)
- [Cylindrical Slicing](slicing/cylindrical-slicing.md)
- [Arc Specialties G-code](gcode/arc-specialties.md)
- [Changelog](../CHANGELOG.md)

## Development and contribution

Start with [Developer Setup](contributing/development-setup.md) for Linux,
Windows (WSL2), and macOS, then follow the [Contributing Guide](../CONTRIBUTING.md).
The setup guide includes build, test, editor, and troubleshooting instructions.

- [ORNL-Managed WSL2 Installation (Historical)](contributing/ornl-wsl2.md)
- [Creating and Updating Settings](contributing/settings.md)
- [Adding Images to Settings Tooltips](contributing/settings-tooltips.md)
- [Conventional Branch Naming](contributing/conventional-branch.md)
- [Conventional Commits](contributing/conventional-commits.md)
- [Pull Requests](contributing/pull-requests.md)
- [Issue Submissions](contributing/issue-submissions.md)
- [Style Guide](contributing/style-guide.md)
- [Formatting](contributing/formatting.md)
- [API Documentation](contributing/documentation.md)
- [Linux AppImage Packaging](contributing/linux-appimage.md)
- [Cachix Workflow](contributing/cachix.md)
- [Versioning and Releases](contributing/versioning.md)
- [Maintaining the Changelog](contributing/changelog.md)

Add the below pandoc documentation into a seperate contributing md file

The Markdown user guide is the canonical manual source. Its exhaustive settings
appendix is generated from `resources/settings/*.yaml`; choice-level and
implementation notes that are not part of the UI schema live in
`scripts/generate_settings_reference.py`. After changing either source, refresh
and verify the appendix from the repository root:

```bash
python3 scripts/generate_master_config.py \
  resources/settings \
  resources/configs/master.conf \
  resources/configs/setting_inputs.conf
python3 -B scripts/generate_settings_reference.py
python3 -B scripts/generate_settings_reference.py --check
```

Regenerate the same-stem `ornlslicer-user-guide.pdf` for a release rather than
editing the PDF directly. With Pandoc and a LaTeX engine installed, run:

```bash
FORCE_SOURCE_DATE=1 SOURCE_DATE_EPOCH=1735689600 TZ=UTC \
pandoc --from=gfm docs/ornlslicer-user-guide.md \
  --resource-path=docs \
  --lua-filter=docs/pandoc/red-diagram-placeholders.lua \
  --include-in-header=docs/pandoc/user-guide-header.tex \
  --metadata title-meta="ORNLSlicer User Guide" \
  --output=docs/ornlslicer-user-guide.pdf
```

CI uses the same stable timestamp and timezone when building generated PDF
artifacts to reduce build-time metadata churn.

The Lua filter colors diagram-placeholder callouts red in generated PDF/HTML
outputs so unreplaced manual figures are easy to spot before release.

Documentation screenshots live under `docs/user-guide-images/` using semantic
names matching the GUI components (e.g. `main_window_overview.png`,
`settings_sidebar.png`). Screenshots can be generated via the
`ornlslicer_gui_capture` tool, and the user guide embeds them directly.

For source-grounded architecture, runtime flows, ownership boundaries, extension
points, and validation paths, start with the [OpenWiki quickstart](../openwiki/quickstart.md).
Source code and tests remain authoritative; generated OpenWiki pages are maintained
separately from these guides.

## Reference and archive

- [Relevant Papers](reference/papers.md)
- [Citation and Copyright Information](reference/citation.md)
- [License and Library Licenses](reference/licenses.md)
- [Slicer User Group (SLUG) Presentations](archive/slug/README.md)

## Maintaining these docs

Place contributor procedures in `contributing/`, slicing guides in `slicing/`,
G-code dialect documentation in `gcode/`, research/legal reference material in
`reference/`, and historical presentations in `archive/`. Link new guides from
this index, use relative links for repository files, and update callers when
renaming a page. Keep each procedure in one canonical guide and link to it
instead of duplicating its commands.

## Compatibility notes

The rename retains `.s2p` and `.s2c` file extensions for compatibility.
