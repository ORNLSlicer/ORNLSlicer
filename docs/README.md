# ORNLSlicer Documentation

This directory is the canonical documentation source for ORNLSlicer.

`Slicer-2` is now `ORNLSlicer`, and the renamed product line starts at version `2.0.0`.

## Start Here

- [ORNLSlicer User Guide](ornlslicer-user-guide.md)
- [Getting Started](wiki/Getting-Started.md)
- [Cylindrical Slicing](slicing/cylindrical-slicing.md)
- [Arc Specialties G-code](gcode/arc-specialties.md)
- [Contributing Guide](../CONTRIBUTING.md)
- [Legacy Wiki Content](wiki/Home.md)

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
  --lua-filter=docs/pandoc/red-diagram-placeholders.lua \
  --include-in-header=docs/pandoc/user-guide-header.tex \
  --metadata title-meta="ORNLSlicer User Guide" \
  --output=docs/ornlslicer-user-guide.pdf
```

CI uses the same stable timestamp and timezone when building generated PDF
artifacts to reduce build-time metadata churn.

The Lua filter colors diagram-placeholder callouts red in generated PDF/HTML
outputs so unreplaced manual figures are easy to spot before release.

The guide's replaceable figure placeholders live in
`docs/user-guide-images/figureNN.png`, using zero-padded names such as
`figure01.png` so the files stay sorted. Replace the image contents while
keeping the same filename, then regenerate the guide; the Markdown fallback and
PDF will pick up the updated figure automatically.

## Architecture

- [Architecture Overview](../ARCHITECTURE.md)
- [Application Runtime](architecture/application-runtime.md)
- [Slicing Pipeline](architecture/slicing-pipeline.md)
- [Settings System](architecture/settings-system.md)
- [G-code and Visualization](architecture/gcode-and-visualization.md)

## Contributor Docs

- [Conventional Branch Naming](contributing/conventional-branch.md)
- [Conventional Commits](contributing/conventional-commits.md)
- [Pull Requests](contributing/pull-requests.md)
- [Issue Submissions](contributing/issue-submissions.md)
- [Style Guide](contributing/style-guide.md)
- [Formatting](contributing/formatting.md)
- [Documentation](contributing/documentation.md)

## Migrated Wiki Content

- [Legacy Wiki Home](wiki/Home.md)
- [ORNL WSL2 Installation](wiki/ORNL-WSL2-Installation.md)
- [Troubleshooting](wiki/Troubleshooting.md)
- [Generating the Master Settings File](wiki/Generating-the-Master-Settings-File.md)
- [Adding a New User Setting](wiki/Adding-a-New-User-Setting.md)
- [Adding Images to Settings Tooltips](wiki/Adding-Images-to-Settings-Tooltips.md)
- [Relevant Papers](wiki/Relevant-Papers.md)
- [License and Library Licenses](wiki/License-and-Library-Licenses.md)
- [Citation and Copyright Information](wiki/Citation-and-Copyright-Information.md)
- [SLUG Archive](wiki/SLUG.md)

## Compatibility Notes

The rename intentionally keeps several legacy technical identifiers in place for compatibility:

- `.s2p` and `.s2c` file extensions
