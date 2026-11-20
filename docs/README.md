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
