# Cylindrical Slicing

Cylindrical slicing generates direct toolpaths around a selected vertical cylinder axis. `Cylindrical Path Pattern` selects whether those paths are radial rings/arcs or rising helices. The axis uses each build part's XY centroid by default, but it can also be set to a custom XY coordinate.

This slicer currently requires the `Arc Specialties` G-code syntax, but this page describes only cylindrical path generation and workflow. The controller dialect, motion fields, startup sequence, and parser behavior are documented in [Arc Specialties](../gcode/arc-specialties.md).

## Basic Workflow

1. Load the build part normally.
2. Set `Slicing Mode` to `Cylindrical`.
3. Set `Cylindrical Path Pattern` to `Radial` or `Helical`.
4. Set radial spacing with `Layer Height`.
5. For `Radial`, set vertical bead spacing with `Default Bead Width`.
6. For `Helical`, set helical region revolutions and stepovers in `Profile > Helical`. A region stepover of `0` uses `Default Bead Width` as that region's pitch.
7. Set `Cylinder Axis Source` if the cylinder axis should use a custom XY coordinate instead of the part centroid.
8. Set `Cylinder Inner Radius` if the first cylinder or helix should begin away from the axis.
9. Set `Cylinder Height` to limit generated cylindrical paths above the part base. Leave it at `0` to use the part height.
10. Set `Cylindrical Path Order Optimization` to choose `Next Closest` or `Next Farthest` ordering between retained cylindrical paths.
11. For `Radial`, set `Radial Path Boundary Policy` in `Profile > Radial`.
12. For `Helical`, set `Helical Z Clip Rounding` in `Profile > Helical` to choose whether the path stops at the model intersection or rounds to a full revolution.
13. Set the radial path start angle in `Profile > Radial` or the helical start angle offset in `Profile > Helical` if the first point should begin somewhere other than the default.
14. For `Helical`, set `Helical Path Handedness` in `Profile > Helical` if the helix should sweep clockwise rather than the default counter-clockwise direction as Z rises.
15. Confirm the printer `Syntax` is `Arc Specialties`. Selecting `Cylindrical` defaults to `Arc Specialties` when the current syntax is not cylindrical-capable.
16. Configure the required Arc Specialties machine output settings, including positioner axes, frame rotation, `TRAFO`, and G2/G3 center mode, using the [Arc Specialties](../gcode/arc-specialties.md) syntax documentation.
17. Slice and inspect the generated G-code preview before running the machine.

## Path Patterns

`Radial` expands cylindrical layers outward from the cylinder axis. The first radial layer centerline is offset by half of `Layer Height`, then later radial layers advance by the full `Layer Height`. On each radius, bead Z positions advance by `Default Bead Width`. `Radial Path Start Angle` selects the first sampled point around each generated ring. It defaults to `0 deg`, which starts on +X.

`Helical` samples a rising region profile at each radius. The angular position is still a continuous helix:

`x(t) = r cos(90 deg + start_angle_offset +/- t)`, `y(t) = r sin(90 deg + start_angle_offset +/- t)`

The first radius is half a `Layer Height` outward from `Cylinder Inner Radius`, and later radii advance by `Layer Height`. Each helix starts at the retained part base Z. Z is defined by the ordered helical region profile rather than one global rise-per-revolution value.

The helical profile order is:

1. Perimeter
2. Inset
3. Infill
4. Inset
5. Perimeter

`Perimeter Revolutions` and `Inset Revolutions` reserve matching shell bands at both ends of the profile. `Perimeter Stepover`, `Inset Stepover`, and `Infill Stepover` are vertical pitch per revolution for their regions. A stepover of exactly `0` falls back to `Default Bead Width`; a negative nonzero stepover is invalid. Zero-revolution perimeter or inset bands are skipped.

Infill revolutions are derived from the remaining available height after both perimeter and inset shell bands are accounted for:

`remaining_height = available_height - 2 * perimeter_revolutions * perimeter_stepover - 2 * inset_revolutions * inset_stepover`

`infill_revolutions = remaining_height / infill_stepover`

`Infill Revolutions Rounding Policy` rounds that derived value with `Round`, `Floor`, or `Ceil`. If the rounded value is not positive, no infill band is generated. `Round` and `Ceil` can make the generated rounded profile extend above the retained model top or configured `Cylinder Height`; this is allowed when model clipping finds no exit within the available-height prefix.

`Helical Start Angle Offset` shifts the first sampled point around each generated helix relative to top dead center. It defaults to `0 deg`, which starts on +Y. An offset of `10 deg` starts where the old absolute `100 deg` start angle did, and `-10 deg` starts where the old absolute `80 deg` start angle did. `Helical Path Handedness` selects the angular sweep while Z rises. `Right Handed` is the default and uses the existing counter-clockwise XY sweep. `Left Handed` mirrors the helix to a clockwise XY sweep without changing the first point, Z rise, or radius spacing.

`Cylinder Height` limits radial and helical candidate paths above the retained part base. Values of `0` or smaller use the retained part height, preserving the default model-bounded behavior.

`Cylindrical Path Order Optimization` controls how retained radial arcs or helical paths are selected after clipping. `Next Closest` minimizes travel from the current machine location to the next path. `Next Farthest` selects the farthest available path first. Closed radial paths can be rotated so travel enters at the selected segment start. Open radial arcs stay endpoint-only so they are not split. A retained helical radius is one continuous multi-region path, so it may be selected from either endpoint and reversed as a single unit, but perimeter/inset/infill boundaries do not add travel.

## Required Settings

| Setting | Location | Effect |
| --- | --- | --- |
| `Slicing Mode` | Profile > Slicing | Select `Cylindrical` to use cylindrical slicing. |
| `Cylindrical Path Pattern` | Profile > Slicing | Selects `Radial` rings/arcs or `Helical` rising paths. |
| `Syntax` | Printer > Machine Setup | Select `Arc Specialties` so the cylindrical writer and parser are used. |
| `Layer Height` | Profile > Layer | Radial distance between successive cylindrical radii. Values less than or equal to zero fall back to a physical `1 mm` default. |
| `Default Bead Width` | Profile > Layer | For `Radial`, vertical bead spacing. For `Helical`, fallback pitch for any region stepover set to exactly `0`. |
| `Cylinder Axis Source` | Profile > Slicing | Selects whether the cylinder axis is centered on each part's centroid or on a custom XY coordinate. |
| `Cylinder Axis - X` / `Cylinder Axis - Y` | Profile > Slicing | Custom cylinder axis coordinates when `Cylinder Axis Source` is `Custom XY`. |
| `Cylinder Inner Radius` | Profile > Slicing | Inner radial boundary before the half-layer offset is applied. |
| `Cylinder Height` | Profile > Slicing | Upper height limit for generated cylindrical paths above the part base. Values less than or equal to `0` use the retained part height. |
| `Cylindrical Path Order Optimization` | Profile > Optimizations | Selects `Next Closest` or `Next Farthest` ordering between retained radial or helical paths. Closed radial paths may rotate to the selected segment start. |
| `Radial Path Start Angle` | Profile > Radial | For `Radial`, angular start position around the cylinder axis. |
| `Helical Start Angle Offset` | Profile > Helical | For `Helical`, angular offset from top dead center around the cylinder axis. Defaults to `0 deg` for a +Y start. |
| `Helical Z Clip Rounding` | Profile > Helical | For `Helical`, controls whether the z-clipped path ends at the model intersection, the next complete revolution, or the previous complete revolution. |
| `Helical Path Handedness` | Profile > Helical | For `Helical`, selects `Right Handed` counter-clockwise rise or `Left Handed` clockwise rise. |
| `Perimeter Revolutions` / `Inset Revolutions` | Profile > Helical | Reserve matching shell revolutions at the beginning and end of each helical profile. |
| `Perimeter Stepover` / `Inset Stepover` / `Infill Stepover` | Profile > Helical | Set vertical pitch per revolution for helical regions. `0` falls back to `Default Bead Width`. |
| `Infill Revolutions Rounding Policy` | Profile > Helical | Rounds derived infill revolutions with `Round`, `Floor`, or `Ceil`. |
| `Arcs per Revolution` | Profile > Slicing | Sets how many G2/G3 moves represent one complete revolution when `Supports G2/G3` is enabled. |

Only relevant settings are shown for the selected path pattern. `Radial` shows its radial-only controls in `Profile > Radial`, including `Radial Path Boundary Policy` with `Clip`, `Keep`, and `Discard`, plus `Radial Path Start Angle`. `Helical` shows its helical-only controls in `Profile > Helical`, including `Helical Start Angle Offset`, `Helical Path Handedness`, and `Helical Z Clip Rounding`.

Planar-only path settings, including Perimeter, Inset, Skeleton, Skin, Infill, Support, Ordering, Platform Adhesion, and their region-specific material modifiers, are hidden or disabled while `Slicing Mode` is `Cylindrical`. Cylindrical mode shows the two-option `Cylindrical Path Order Optimization` setting instead of the planar path-order controls.

## Radial Profile Settings

When `Slicing Mode` is `Cylindrical` and `Cylindrical Path Pattern` is `Radial`, ORNLSlicer shows a dedicated `Profile > Radial` group for radial path controls:

| Setting | Effect |
| --- | --- |
| `Radial Path Boundary Policy` | Selects whether boundary-crossing radial paths are clipped, kept, or discarded. |
| `Radial Path Start Angle` | Sets the first angular position around each generated radial ring or arc. |

## Helical Profile Settings

When `Slicing Mode` is `Cylindrical` and `Cylindrical Path Pattern` is `Helical`, ORNLSlicer shows a dedicated `Profile > Helical` group for helical path controls and helical-region inputs:

| Setting | Effect |
| --- | --- |
| `Helical Path Handedness` | Selects `Right Handed` counter-clockwise rise or `Left Handed` clockwise rise. |
| `Helical Start Angle Offset` | Sets the first angular position around the cylinder axis as an offset from top dead center. |
| `Helical Z Clip Rounding` | Selects exact, next-full-revolution, or previous-full-revolution z clipping in cumulative profile-revolution coordinates. |
| `Perimeter Revolutions` | Number of perimeter revolutions reserved for helical region planning. |
| `Inset Revolutions` | Number of inset revolutions reserved for helical region planning. |
| `Perimeter Stepover` | Pitch used by perimeter revolutions. This is shown only when perimeter revolutions are nonzero. |
| `Inset Stepover` | Pitch used by inset revolutions. This is shown only when inset revolutions are nonzero. |
| `Infill Stepover` | Pitch used by helical infill region planning. |
| `Infill Revolutions Rounding Policy` | Rounds derived infill revolutions with `Round`, `Floor`, or `Ceil`. |

## Boundary Handling

For `Radial`, model clipping can split a path into one or more retained arcs:

| Option | Behavior |
| --- | --- |
| `Clip` | Outputs only the retained portions inside the model. |
| `Keep` | Outputs the original boundary-crossing path when any portion is inside the model cross section. |
| `Discard` | Omits paths when clipping removes a meaningful portion of the path. Paths fully inside the model cross section are still kept. |

For `Helical`, model clipping checks only the available-height prefix of the generated region profile, from its generated start through the retained model top or configured `Cylinder Height`. If the checked path starts outside the model, the retained output starts at the first model entry instead of consuming lower shell regions in omitted space. If the checked path exits the model, `Helical Z Clip Rounding` controls the endpoint in cumulative profile-revolution coordinates: `Exact Intersection` stops at the highest-Z model exit, `Complete Revolution` continues to the next complete profile revolution, and `Last Full Revolution` stops at the previous complete profile revolution. If the rounded endpoint is not above the retained start, that radius is omitted.

After clipping, the retained path is rebuilt from the retained start and top using the same helical region settings. Lower perimeter/inset shell bands begin at the retained path start, the retained lower perimeter starts at top dead center plus `Helical Start Angle Offset`, and `Infill Revolutions Rounding Policy` still controls the rebuilt infill region count.

When helical z clipping finds no boundary crossing, a checked prefix that is wholly inside the model keeps the full generated rounded profile, including any `Round` or `Ceil` height overrun. A checked prefix that is wholly outside, or partly outside without a detected crossing, is omitted.

## G-code Output Handoff

Cylindrical slicing hands radial and helical paths to the Arc Specialties writer. The generated header reports the cylindrical geometry, selected path pattern, cylindrical path order, radial path start angle or helical start angle offset, helical handedness, z clip rounding for helical paths, helical region revolutions, helical region pitches, infill rounding policy, and travel lift distance.

Radial cylindrical print moves continue to use generic `RADIAL` comments. Helical print moves use generated region metadata when available:

| Segment metadata | Comment |
| --- | --- |
| `Perimeter` | `HELICAL PERIMETER` |
| `Inset` | `HELICAL INSET` |
| `Infill` | `HELICAL INFILL` |
| Missing or unknown | `HELICAL` |

The G-code preview colors these comments through the existing `GCodeLoader` visualization mappings. `HELICAL PERIMETER`, `HELICAL INSET`, and `HELICAL INFILL` use their helical-specific colors, and generic `HELICAL` remains the fallback.

When `Supports G2/G3` is disabled, cylindrical print paths are written as sampled G1 segments. When `Supports G2/G3` is enabled, complete rings or helical revolutions are divided according to `Arcs per Revolution`; clipped or partial paths may include a shorter final arc. The exact G00/G01/G02/G03 syntax, positioner fields, center modes, and startup commands are described in [Arc Specialties](../gcode/arc-specialties.md).

## Current Limitations

- Cylindrical slicing generates direct radial or helical paths only.
- Standard planar polymer island regions are not generated. Helical perimeter, inset, and infill are direct-path segment annotations on one retained path per radius.
- The cylinder axis is vertical Z through each part's XY centroid or the configured custom XY coordinate.
- Candidate paths are sampled for model clipping. Output uses sampled G1 segments when arc support is disabled, or G2/G3 moves controlled by `Arcs per Revolution` when it is enabled.
- Clipping meshes are applied before cylindrical path generation, but `Slice Plane Normal` settings are not used by cylindrical slicing.
- Cylindrical mode is currently guarded to the Arc Specialties syntax.

## Quick Checks

After slicing, verify that:

- The G-code header identifies the selected `Cylindrical Path Pattern` and selected radial path start angle or helical start angle offset.
- The G-code header identifies the selected `Cylindrical Path Order Optimization`.
- For `Helical`, the G-code header identifies the selected `Helical Path Handedness`.
- For `Helical`, the G-code header identifies the selected `Helical Z Clip Rounding`.
- For `Helical`, print comments transition between `HELICAL PERIMETER`, `HELICAL INSET`, and `HELICAL INFILL` without travel moves at touching region boundaries.
- A complete radial ring or helical revolution contains the configured `Arcs per Revolution`; clipped or partial paths may include a shorter final arc.
- Printed paths lie on the part rather than above or below it.
- Travel moves and configured travel lift stay clear of the printed cylindrical paths.
- The cylinder axis is centered where expected for the selected `Cylinder Axis Source`.
