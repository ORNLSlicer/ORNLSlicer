# Cylindrical Slicing

Cylindrical slicing generates direct toolpaths around a selected vertical cylinder axis. `Cylindrical Path Pattern` selects whether those paths are radial rings/arcs or rising helices. The axis uses each build part's XY centroid by default, but it can also be set to a custom XY coordinate.

This slicer currently requires the `Arc Specialties` G-code syntax, but this page describes only cylindrical path generation and workflow. The controller dialect, motion fields, startup sequence, and parser behavior are documented in [Arc Specialties](../gcode/arc-specialties.md).

## Basic Workflow

1. Load the build part normally.
2. Set `Slicing Mode` to `Cylindrical`.
3. Set `Cylindrical Path Pattern` to `Radial` or `Helical`.
4. Set radial spacing with `Layer Height`.
5. For `Radial`, set vertical bead spacing with `Default Bead Width`.
6. For `Helical`, set helical rise per full revolution with `Default Bead Width`.
7. Set `Cylinder Axis Source` if the cylinder axis should use a custom XY coordinate instead of the part centroid.
8. Set `Cylinder Inner Radius` if the first cylinder or helix should begin away from the axis.
9. Set `Cylinder Height` to limit generated cylindrical paths above the part base. Leave it at `0` to use the part height.
10. Set `Cylindrical Path Order Optimization` to choose `Next Closest` or `Next Farthest` ordering between retained cylindrical paths.
11. Set the path boundary policy for the selected `Cylindrical Path Pattern`.
12. For `Helical` with `Clip Z`, set `Helical Z Clip Rounding` to choose whether the path stops at the model intersection or rounds to a full revolution.
13. Set the radial or helical path start angle if the first point should begin somewhere other than the default.
14. For `Helical`, set `Helical Path Handedness` if the helix should sweep clockwise rather than the default counter-clockwise direction as Z rises.
15. For `Helical`, set `Max Helical Path Length` when long generated helices should be split into shorter paths. Leave it at `0` to keep each clipped helix fragment as one path.
16. Confirm the printer `Syntax` is `Arc Specialties`. Selecting `Cylindrical` defaults to `Arc Specialties` when the current syntax is not cylindrical-capable.
17. Configure the required Arc Specialties machine output settings, including positioner axes, frame rotation, `TRAFO`, and G2/G3 center mode, using the [Arc Specialties](../gcode/arc-specialties.md) syntax documentation.
18. Slice and inspect the generated G-code preview before running the machine.

## Path Patterns

`Radial` expands cylindrical layers outward from the cylinder axis. The first radial layer centerline is offset by half of `Layer Height`, then later radial layers advance by the full `Layer Height`. On each radius, bead Z positions advance by `Default Bead Width`. `Radial Path Start Angle` selects the first sampled point around each generated ring. It defaults to `0 deg`, which starts on +X.

`Helical` samples a rising helix at each radius:

`x(t) = r cos(start_angle +/- t)`, `y(t) = r sin(start_angle +/- t)`, `z(t) = z0 + (bead_width / (2 * pi)) * t`

The first radius is half a `Layer Height` outward from `Cylinder Inner Radius`, and later radii advance by `Layer Height`. `Default Bead Width` is the rise per full revolution.

`Helical Path Start Angle` selects the first sampled point around each generated helix. It defaults to `90 deg`, which starts on +Y. `Helical Path Handedness` selects the angular sweep while Z rises. `Right Handed` is the default and uses the existing counter-clockwise XY sweep. `Left Handed` mirrors the helix to a clockwise XY sweep without changing the first point, Z rise, or radius spacing.

If `Max Helical Path Length` is greater than `0`, each generated helical fragment is split into shorter paths before print segments are emitted. Split paths stay contiguous, so no travel move is inserted between adjacent split points. Values of `0` or smaller leave the generated helical fragments unbroken.

`Cylinder Height` limits radial and helical candidate paths above the retained part base. Values of `0` or smaller use the retained part height, preserving the default model-bounded behavior.

`Cylindrical Path Order Optimization` controls how retained radial arcs or helical fragments are selected after clipping. `Next Closest` minimizes travel from the current machine location to the next path. `Next Farthest` selects the farthest available path first. Closed radial paths can be rotated so travel enters at the selected segment start. Open radial arcs stay endpoint-only so they are not split. Helical fragments are selected by start or end endpoint and may be reordered or reversed.

## Required Settings

| Setting | Location | Effect |
| --- | --- | --- |
| `Slicing Mode` | Profile > Slicing | Select `Cylindrical` to use cylindrical slicing. |
| `Cylindrical Path Pattern` | Profile > Slicing | Selects `Radial` rings/arcs or `Helical` rising paths. |
| `Syntax` | Printer > Machine Setup | Select `Arc Specialties` so the cylindrical writer and parser are used. |
| `Layer Height` | Profile > Layer | Radial distance between successive cylindrical radii. Values less than or equal to zero fall back to a physical `1 mm` default. |
| `Default Bead Width` | Profile > Layer | For `Radial`, vertical bead spacing. For `Helical`, rise per revolution. |
| `Cylinder Axis Source` | Profile > Slicing | Selects whether the cylinder axis is centered on each part's centroid or on a custom XY coordinate. |
| `Cylinder Axis - X` / `Cylinder Axis - Y` | Profile > Slicing | Custom cylinder axis coordinates when `Cylinder Axis Source` is `Custom XY`. |
| `Cylinder Inner Radius` | Profile > Slicing | Inner radial boundary before the half-layer offset is applied. |
| `Cylinder Height` | Profile > Slicing | Upper height limit for generated cylindrical paths above the part base. Values less than or equal to `0` use the retained part height. |
| `Cylindrical Path Order Optimization` | Profile > Optimizations | Selects `Next Closest` or `Next Farthest` ordering between retained radial or helical paths. Closed radial paths may rotate to the selected segment start. |
| `Radial Path Start Angle` | Profile > Slicing | For `Radial`, angular start position around the cylinder axis. |
| `Helical Path Start Angle` | Profile > Slicing | For `Helical`, angular start position around the cylinder axis. Defaults to `90 deg` for a +Y start. |
| `Helical Z Clip Rounding` | Profile > Slicing | For `Helical` with `Clip Z`, controls whether the path ends at the model intersection, the next complete revolution, or the previous complete revolution. |
| `Helical Path Handedness` | Profile > Slicing | For `Helical`, selects `Right Handed` counter-clockwise rise or `Left Handed` clockwise rise. |
| `Max Helical Path Length` | Profile > Slicing | For `Helical`, maximum length of each generated helical path segment before it is split. |
| `Arcs per Revolution` | Profile > Slicing | Sets how many G2/G3 moves represent one complete revolution when `Supports G2/G3` is enabled. |

Only relevant settings are shown for the selected path pattern. `Radial` shows `Radial Path Boundary Policy` with `Clip`, `Keep`, and `Discard`, plus `Radial Path Start Angle`. `Helical` shows `Helical Path Boundary Policy` with `Clip` and `Clip Z`, plus helical-only controls such as `Helical Path Start Angle` and `Helical Path Handedness`. `Helical Z Clip Rounding` is shown only when `Helical Path Boundary Policy` is `Clip Z`.

Planar-only path settings, including Perimeter, Inset, Skeleton, Skin, Infill, Support, Ordering, Platform Adhesion, and their region-specific material modifiers, are hidden or disabled while `Slicing Mode` is `Cylindrical`. Cylindrical mode shows the two-option `Cylindrical Path Order Optimization` setting instead of the planar path-order controls.

## Boundary Policies

For `Radial`, model clipping can split a path into one or more retained arcs:

| Option | Behavior |
| --- | --- |
| `Clip` | Outputs only the retained portions inside the model. |
| `Keep` | Outputs the original boundary-crossing path when any portion is inside the model cross section. |
| `Discard` | Omits paths when clipping removes a meaningful portion of the path. Paths fully inside the model cross section are still kept. |

For `Helical`, model clipping retains helix portions according to the selected mode:

| Option | Behavior |
| --- | --- |
| `Clip` | Outputs every contiguous helix portion that lies inside the model. |
| `Clip Z` | Outputs one continuous prefix of the original helix, from its generated start through the boundary intersection with the greatest Z value. |

`Helical Z Clip Rounding` refines `Clip Z`. `Exact Intersection` stops at the highest-Z model intersection. `Complete Revolution` continues to the next complete revolution and may extend above the model or configured `Cylinder Height`. `Last Full Revolution` stops at the previous complete revolution; if the intersection occurs before the first complete revolution, that radius is omitted.

When `Clip Z` finds no boundary crossing, a helix that is wholly inside the model is kept in full and a helix that is wholly outside is omitted.

## G-code Output Handoff

Cylindrical slicing hands radial and helical paths to the Arc Specialties writer. The generated header reports the cylindrical geometry, selected path pattern, cylindrical path order, path start angle, helical handedness, boundary policy, Clip Z rounding when applicable, and travel lift distance. Print moves are marked with `RADIAL` or `HELICAL` comments so the preview path can classify cylindrical bead motion.

When `Supports G2/G3` is disabled, cylindrical print paths are written as sampled G1 segments. When `Supports G2/G3` is enabled, complete rings or helical revolutions are divided according to `Arcs per Revolution`; clipped or partial paths may include a shorter final arc. The exact G00/G01/G02/G03 syntax, positioner fields, center modes, and startup commands are described in [Arc Specialties](../gcode/arc-specialties.md).

## Current Limitations

- Cylindrical slicing generates direct radial or helical paths only.
- Standard polymer regions are not generated.
- The cylinder axis is vertical Z through each part's XY centroid or the configured custom XY coordinate.
- Candidate paths are sampled for model clipping. Output uses sampled G1 segments when arc support is disabled, or G2/G3 moves controlled by `Arcs per Revolution` when it is enabled.
- Clipping meshes are applied before cylindrical path generation, but `Slice Plane Normal` settings are not used by cylindrical slicing.
- Cylindrical mode is currently guarded to the Arc Specialties syntax.

## Quick Checks

After slicing, verify that:

- The G-code header identifies the selected `Cylindrical Path Pattern` and selected path start angle.
- The G-code header identifies the selected `Cylindrical Path Order Optimization`.
- For `Helical`, the G-code header identifies the selected `Helical Path Handedness`.
- For `Helical` with `Clip Z`, the G-code header identifies the selected `Helical Z Clip Rounding`.
- A complete radial ring or helical revolution contains the configured `Arcs per Revolution`; clipped or partial paths may include a shorter final arc.
- Printed paths lie on the part rather than above or below it.
- Travel moves and configured travel lift stay clear of the printed cylindrical paths.
- The cylinder axis is centered where expected for the selected `Cylinder Axis Source`.
