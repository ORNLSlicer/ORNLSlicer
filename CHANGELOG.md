# Changelog

Notable changes to ORNLSlicer are recorded here. This changelog starts with
changes since [v2.1.0](https://github.com/ORNLSlicer/ORNLSlicer/releases/tag/v2.1.0).
Release versions follow the project's
[versioning policy](https://github.com/ORNLSlicer/ORNLSlicer/blob/develop/docs/contributing/versioning.md).

## Unreleased

### Added

- Added cube and stacked-layer icons beside global settings to distinguish part and layer overrides, showing both when both apply.
- Added a Starting Layer Number setting under Printer > G-Code for continuation builds, with consistent layer, bead, block, and individual-layer export numbering ([#347](https://github.com/ORNLSlicer/ORNLSlicer/issues/347)).

### Changed

- Changed individual-layer export to a disabled-by-default setting under Experimental > File Output. Enable Individual Layer Export before slicing to prepare separate layer files; reopened G-code must be re-sliced to export individual layers.

### Fixed

- Fixed generated G-code containing embedded standalone layer programs and export-only markers, including when individual-layer export is enabled.
- Fixed incomplete Git LFS checkouts silently building with missing GUI icons, setting arrows, and Slice toolbar access by rejecting unresolved bundled resources with recovery instructions ([#351](https://github.com/ORNLSlicer/ORNLSlicer/issues/351)).
- Fixed regular CI not running for pull requests from fork branches, including Linux and Windows builds and Linux and macOS flake checks.
- Fixed STEP assemblies of touching solids, and solids with internal voids, importing as open meshes; vertices are now welded per shell ([#341](https://github.com/ORNLSlicer/ORNLSlicer/issues/341)).
- Fixed building against CGAL 6 while keeping CGAL 5 support, and a null dereference when a mesh intersection is not a single point ([#343](https://github.com/ORNLSlicer/ORNLSlicer/issues/343)).

---

## 2.2.0 - 2026-10-20

### Release highlights

- Added connected perimeter and inset contours, with connection segments distinguished in G-code preview.
- Added standalone individual-layer G-code export for supported printers.
- Expanded cylindrical and helical slicing with regional profiles, tool-frame rotations, clipping, and path ordering.
- Added variable layer heights driven by surface error and minimum layer height.
- Added settings-file comparison and an offline **Help > Changelog** viewer.

### Added

- Added an offline changelog window under **Help > Changelog** and a shared change history for release notes ([#309](https://github.com/ORNLSlicer/ORNLSlicer/issues/309)).
- Added connected perimeter and inset contours, including connections after forward tip wipe and a distinct G-code preview color ([#273](https://github.com/ORNLSlicer/ORNLSlicer/pull/273)).
- Added individual-layer G-code export with standalone entry and shutdown commands for supported printers ([#308](https://github.com/ORNLSlicer/ORNLSlicer/pull/308)).
- Added a settings-file comparison tool for inspecting differences between configuration files ([#305](https://github.com/ORNLSlicer/ORNLSlicer/pull/305)).
- Added variable layer heights controlled by surface error and minimum layer height ([#268](https://github.com/ORNLSlicer/ORNLSlicer/pull/268)).
- Added regional helical profiles, tool-frame rotations, clipping controls, and cylindrical path ordering ([#274](https://github.com/ORNLSlicer/ORNLSlicer/pull/274), [#275](https://github.com/ORNLSlicer/ORNLSlicer/pull/275), [#259](https://github.com/ORNLSlicer/ORNLSlicer/pull/259)).
- Added Arc Specialties block numbering, G80 weld schedule configuration, and helical optional-stop output ([#277](https://github.com/ORNLSlicer/ORNLSlicer/pull/277), [#276](https://github.com/ORNLSlicer/ORNLSlicer/pull/276), [#282](https://github.com/ORNLSlicer/ORNLSlicer/pull/282)).
- Added part renaming, part-name overlays, recent files, STEP tessellation preferences, and adaptive bead-width information ([#286](https://github.com/ORNLSlicer/ORNLSlicer/pull/286), [#270](https://github.com/ORNLSlicer/ORNLSlicer/pull/270), [#263](https://github.com/ORNLSlicer/ORNLSlicer/pull/263), [#269](https://github.com/ORNLSlicer/ORNLSlicer/pull/269), [#264](https://github.com/ORNLSlicer/ORNLSlicer/pull/264)).
- Added a travel-feedrate scaling exemption and Cincinnati selection of variable 200 or 201 for Z position ([#260](https://github.com/ORNLSlicer/ORNLSlicer/pull/260), [#287](https://github.com/ORNLSlicer/ORNLSlicer/pull/287)).
- Added reviewable release-version preparation from Conventional Commits with MAJOR capped at 2 ([#332](https://github.com/ORNLSlicer/ORNLSlicer/pull/332)).
- Added a reproducible Windows installer and persistent release outputs in Cachix ([#323](https://github.com/ORNLSlicer/ORNLSlicer/pull/323)).

### Changed

- Changed helical and radial slicing to use a shared cylindrical slicing workflow ([#265](https://github.com/ORNLSlicer/ORNLSlicer/pull/265)).
- Changed the helical tool start offset from an angle to a signed arc length evaluated at each path radius; legacy settings migrate using the first generated radius ([#317](https://github.com/ORNLSlicer/ORNLSlicer/pull/317)).
- Changed G-code preview shading to preserve configured path hues.
- Changed architecture documentation to a generated, source-grounded OpenWiki index ([#310](https://github.com/ORNLSlicer/ORNLSlicer/pull/310)).

### Fixed

- Fixed release promotion validation for PRs into `main` and automatic attachment of Linux and Windows downloads after release checks pass ([#338](https://github.com/ORNLSlicer/ORNLSlicer/pull/338)).
- Fixed toolbar commands becoming inaccessible on scaled displays ([#314](https://github.com/ORNLSlicer/ORNLSlicer/pull/314)).
- Fixed Wolf settings headers failing to reload in RGCodeTools ([#316](https://github.com/ORNLSlicer/ORNLSlicer/pull/316)).
- Fixed Wire Arc paths using incorrect deposition-surface Z coordinates ([#293](https://github.com/ORNLSlicer/ORNLSlicer/pull/293)).
- Fixed consecutive seam selection, global island ordering, and seam-attractor drag updates ([#299](https://github.com/ORNLSlicer/ORNLSlicer/pull/299), [#298](https://github.com/ORNLSlicer/ORNLSlicer/pull/298), [#300](https://github.com/ORNLSlicer/ORNLSlicer/pull/300)).
- Fixed weight estimates when bead height must be inferred from path modifiers ([#301](https://github.com/ORNLSlicer/ORNLSlicer/pull/301)).
- Fixed repeated UI reloads during preference import and incorrect flowrate-calculator unit conversions ([#292](https://github.com/ORNLSlicer/ORNLSlicer/pull/292), [#261](https://github.com/ORNLSlicer/ORNLSlicer/pull/261)).
- Fixed AppImage launches for unprivileged users on Ubuntu ([#296](https://github.com/ORNLSlicer/ORNLSlicer/pull/296)).
- Fixed JuggerBot bead-area multiplier handling, including zero-RPM arcs ([#262](https://github.com/ORNLSlicer/ORNLSlicer/pull/262)).

### Breaking changes

- Removed the former maximum helical path length controls. Configure regional revolutions, stepovers, and helical Z clipping instead ([#274](https://github.com/ORNLSlicer/ORNLSlicer/pull/274)).
- Changed the helical tool start offset from an angle to a signed physical distance, which stays constant as path radius changes. Existing versioned settings migrate on import using the first generated radius. Integrations that generate settings must use `helical_tool_start_arc_length_offset` and supply a distance; do not reuse angular values as distances ([#317](https://github.com/ORNLSlicer/ORNLSlicer/pull/317)).
