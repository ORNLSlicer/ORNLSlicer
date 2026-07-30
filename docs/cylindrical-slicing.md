# Cylindrical Slicing

Cylindrical slicing generates direct toolpaths around a selected vertical cylinder axis. `Cylindrical Path Pattern` selects whether those paths are radial rings/arcs or rising helices. The axis uses each build part's XY centroid by default, but it can also be set to a custom XY coordinate.

This slicer is intended for `Arc Specialties` X/Y/Z/XR/YR/ZR/AP/CP output. It does not run the standard perimeter, infill, skin, support, or raft generation logic. The same Arc Specialties syntax can also format normal planar output; this page describes only cylindrical mode.

## Basic Workflow

1. Load the build part normally.
2. Set `Slicing Mode` to `Cylindrical`.
3. Set `Cylindrical Path Pattern` to `Radial` or `Helical`.
4. Set radial spacing with `Layer Height`.
5. For `Radial`, set vertical bead spacing with `Default Bead Width`.
6. For `Helical`, set helical rise per full revolution with `Default Bead Width`.
7. Set `Cylinder Axis Source` if the cylinder axis should use a custom XY coordinate instead of the part centroid.
8. Set `Cylinder Inner Radius` if the first cylinder or helix should begin away from the axis.
9. Set the path boundary policy for the selected `Cylindrical Path Pattern`.
10. Set the radial or helical path start angle if the first point should begin somewhere other than the default.
11. For `Helical`, set `Helical Path Handedness` if the helix should sweep clockwise rather than the default counter-clockwise direction as Z rises.
12. For `Helical`, set `Max Helical Path Length` when long generated helices should be split into shorter paths. Leave it at `0` to keep each clipped helix fragment as one path.
13. Confirm the printer `Syntax` is `Arc Specialties`. Selecting `Cylindrical` defaults to `Arc Specialties` when the current syntax is not cylindrical-capable.
14. Set `Axis A` to the desired positioner tilt and set `Axis C` if the machine coordinate frame needs an angular offset.
15. Set `G-Code Frame Rotation X/Y/Z` if the generated coordinates need to be rotated into the machine frame.
16. Confirm whether the program should enable `TRAFO`, and select the required G2/G3 center interpretation when arc moves are enabled.
17. Slice and inspect the generated G-code preview before running the machine.

## Path Patterns

`Radial` expands cylindrical layers outward from the cylinder axis. The first radial layer centerline is offset by half of `Layer Height`, then later radial layers advance by the full `Layer Height`. On each radius, bead Z positions advance by `Default Bead Width`. `Radial Path Start Angle` selects the first sampled point around each generated ring. It defaults to `0 deg`, which starts on +X.

`Helical` samples a rising helix at each radius:

`x(t) = r cos(start_angle +/- t)`, `y(t) = r sin(start_angle +/- t)`, `z(t) = z0 + (bead_width / (2 * pi)) * t`

The first radius is half a `Layer Height` outward from `Cylinder Inner Radius`, and later radii advance by `Layer Height`. `Default Bead Width` is the rise per full revolution.

`Helical Path Start Angle` selects the first sampled point around each generated helix. It defaults to `90 deg`, which starts on +Y. `Helical Path Handedness` selects the angular sweep while Z rises. `Right Handed` is the default and uses the existing counter-clockwise XY sweep. `Left Handed` mirrors the helix to a clockwise XY sweep without changing the first point, Z rise, or radius spacing.

If `Max Helical Path Length` is greater than `0`, each generated helical fragment is split into shorter paths before print segments are emitted. Split paths stay contiguous, so no travel move is inserted between adjacent split points. Values of `0` or smaller leave the generated helical fragments unbroken.

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
| `Radial Path Start Angle` | Profile > Slicing | For `Radial`, angular start position around the cylinder axis. |
| `Helical Path Start Angle` | Profile > Slicing | For `Helical`, angular start position around the cylinder axis. Defaults to `90 deg` for a +Y start. |
| `Helical Path Handedness` | Profile > Slicing | For `Helical`, selects `Right Handed` counter-clockwise rise or `Left Handed` clockwise rise. |
| `Max Helical Path Length` | Profile > Slicing | For `Helical`, maximum length of each generated helical path segment before it is split. |
| `Arcs per Revolution` | Profile > Slicing | Sets how many G2/G3 moves represent one complete revolution when `Supports G2/G3` is enabled. |

Only relevant settings are shown for the selected path pattern. `Radial` shows `Radial Path Boundary Policy` with `Clip`, `Keep`, and `Discard`, plus `Radial Path Start Angle`. `Helical` shows `Helical Path Boundary Policy` with `Clip` and `Clip Z`, plus helical-only controls such as `Helical Path Start Angle` and `Helical Path Handedness`.

Planar-only path settings, including Perimeter, Inset, Skeleton, Skin, Infill, Support, Ordering, Platform Adhesion, and their region-specific material modifiers, are hidden or disabled while `Slicing Mode` is `Cylindrical`.

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

When `Clip Z` finds no boundary crossing, a helix that is wholly inside the model is kept in full and a helix that is wholly outside is omitted.

## G-code And Machine Settings

`Arc Specialties` output keeps X, Y, and Z as user-frame endpoint coordinates relative to the active work offset, applies the configured G-code coordinate frame rotation, and emits `XR`, `YR`, `ZR`, `AP`, and `CP` fields on every travel and print move. Feed moves use `XR=180`, `YR=0`, and `ZR=-135`; rapid travel moves use `ZR=-90`. The writer maps `Axis A` to `AP` and computes cylindrical `CP` from the transformed endpoint angle plus `Axis C`, normalized to 0-360 degrees. For helical output, `CP` reports positive angular sweep from the transformed `Helical Path Start Angle` plus `Axis C`; with the default `90 deg` start angle, four equal arc endpoints advance as `90`, `180`, `270`, `0` degrees. For the Arc Specialties partner frame, set `G-Code Frame Rotation Z` to `-90 deg`.

For planar slicing with the Arc Specialties syntax, the normal planar regions and segment stream are preserved. Planar motion still emits the Arc Specialties orientation fields, uses the slice-plane normal for travel lift, and writes `CP` as the configured `Axis C` value rather than deriving it from a cylinder axis.

Enabling `Supports G2/G3` writes radial and helical print paths as G2/G3 moves divided according to `Arcs per Revolution`. Right-handed helical arcs are emitted as counter-clockwise moves, and left-handed helical arcs are emitted as clockwise moves. Those moves use equals-form `I=`/`J=` fields and place the feedrate at the end of the motion fields. `G2/G3 Center Point Interpretation` controls whether I/J are emitted as absolute center coordinates or relative offsets from the arc start point. Relative offsets are rotated with the configured G-code coordinate frame. Absolute centers use the configured `G2/G3 Absolute Center` I/J values.

Arc Specialties startup writes the machine setup block, leaves the program in `G90` with `#TRAFO OFF`, then defers the work-object kinematics block until the first travel target is known. The first lifted travel is emitted as a world approach with the configured `AP`/`CP` positioner values, followed by `#FLUSH WAIT`; after that the writer emits `#KIN ID [9]`, `#TRAFO ON` or `#TRAFO OFF` according to `Enable TRAFO`, and `G161` for absolute arc-center mode or `G162` for relative arc-center mode. The first layer marker is emitted after that startup approach and kinematics block so startup positioning remains outside the bead/layer block. When absolute center mode was enabled, shutdown writes `G164` to leave that mode. `M06` tool selection and `G80` optional-stop travel commands are currently emitted as comments because those controller commands did not work as of July 29, 2026; the inline TODOs mark where they should be re-enabled or replaced after the controller behavior is resolved.

The generated header reports cylindrical geometry, path pattern, helical handedness, boundary policy, travel lift distance, and positioner settings. Print moves are marked with `RADIAL` or `HELICAL`, and the Arc Specialties parser strips orientation fields for XYZ preview visualization.

| Setting | Location | Effect |
| --- | --- | --- |
| `Axis A` | Printer > Machine Setup | Positioner tilt emitted as `AP`. |
| `Axis C` | Printer > Machine Setup | Added to the endpoint angle around the cylinder axis before writing cylindrical `CP`; planar Arc Specialties output writes it as the fixed `CP` value. |
| `G-Code Frame Rotation X/Y/Z` | Printer > Machine Setup | Rotates emitted G-code endpoint coordinates and G2/G3 center offsets. Set Z to `-90 deg` for the Arc Specialties partner frame. |
| `Supports G2/G3` | Printer > Machine Setup | Enables G2/G3 cylindrical print moves. When disabled, cylindrical print paths use segmented G1 moves. |
| `Enable TRAFO` | Printer > Machine Setup | Selects whether startup writes `#TRAFO ON` or `#TRAFO OFF`. |
| `G2/G3 Center Point Interpretation` | Printer > Machine Setup | Selects `Absolute` G161 mode or `Relative` G162 mode for arc I/J fields when G2/G3 support is enabled. |
| `G2/G3 Absolute Center` | Printer > Machine Setup | Absolute I/J center values used when `G2/G3 Center Point Interpretation` is `Absolute`. |
| `Default Print Speed` | Profile > Layer | Print feedrate for cylindrical print segments. |
| `Travel Speed` | Profile > Travel | Feedrate for non-print travel moves. If unset, the writer falls back to machine max XY speed. |
| `Travel Lift Height` | Profile > Travel | Cylindrical-safe travel lift distance. The lift moves outward from the cylinder axis before the travel, then lowers at the destination when enabled. |
| `Minimum Travel Length for Lifting` | Profile > Travel | Suppresses travel lifting for shorter travel moves. |
| `Z Speed` | Printer > Machine Speed | Speed used for travel lift and lower moves when available. |

## Current Limitations

- Cylindrical slicing generates direct radial or helical paths only.
- Standard polymer regions are not generated.
- The cylinder axis is vertical Z through each part's XY centroid or the configured custom XY coordinate.
- Candidate paths are sampled for model clipping. Output uses sampled G1 segments when arc support is disabled, or G2/G3 moves controlled by `Arcs per Revolution` when it is enabled.
- Clipping meshes are applied before cylindrical path generation, but `Slice Plane Normal` settings are not used by cylindrical slicing.
- Arc Specialties work-offset and touch-probe setup commands are not emitted in the first pass. The generated header documents that an appropriate user frame must already be active.

## Quick Checks

After slicing, verify that:

- The G-code header identifies `Arc Specialties`, the selected `Cylindrical Path Pattern`, and the selected path start angle.
- For `Helical`, the G-code header identifies the selected `Helical Path Handedness`.
- Motion lines use `G00`/`G01`, or `G02`/`G03` when `Supports G2/G3` is enabled. They contain `X=`, `Y=`, `Z=`, `XR=`, `YR=`, `ZR=`, `AP=`, and `CP=`.
- A complete radial ring or helical revolution contains the configured `Arcs per Revolution`; clipped or partial paths may include a shorter final arc.
- Printed paths lie on the part rather than above or below it.
- Travel moves arc around the cylinder axis when the angular move is long enough, and configured travel lift offsets them outward before traversing.
- The cylinder axis is centered where expected for the selected `Cylinder Axis Source`.
