# Radial Slicing

Radial slicing generates toolpaths by expanding cylindrical layers outward from a selected vertical radial axis. By default, the axis uses each build part's XY centroid, but it can also be set to a custom XY coordinate. Each cylindrical layer is clipped against horizontal cross sections of the model, producing ring arc beads that follow the part boundary at that radius.

This slicer is intended for radial machine output using `Arc Specialties` X/Y/Z/XR/YR/ZR/AP/CP motion. It is an initial direct-path implementation and does not run the standard perimeter, infill, skin, support, or raft generation logic.

## Basic Workflow

1. Load the build part normally.
2. Set `Slicer Type` to `Radial Slice`.
3. Set the radial spacing with `Layer Height`.
4. Set the vertical bead spacing and bead width with `Default Bead Width`.
5. Set `Radial Axis Mode` if the cylinder axis should use a custom XY coordinate instead of the part centroid.
6. Set `Radial Initial Radius` if the first cylinder should begin away from the radial axis. Leave it at `0` for the default center-out behavior.
7. Set `Radial Boundary Handling` for radial paths that cross the model boundary.
8. Confirm the printer `Syntax` is `Arc Specialties`. Selecting `Radial Slice` defaults to `Arc Specialties` when the current syntax is not cylindrical-capable. Selecting `Arc Specialties` preserves an existing radial or helical mode and otherwise defaults `Slicer Type` to `Radial Slice`.
9. Set `Axis A` to the desired fixed tilt and set `Axis C` if the machine coordinate frame needs an angular offset.
10. Slice and inspect the generated G-code preview before running the machine.

## How Paths Are Generated

For each build part, radial slicing resolves a vertical cylinder axis from `Radial Axis Mode`. `Part Centroid` uses the part's XY centroid. `Custom XY` uses `Radial Axis X` and `Radial Axis Y`. The base Z is the retained mesh minimum Z and the top Z is the retained mesh maximum Z.

The first radial layer centerline is offset by half of `Layer Height`, then later radial layers advance by the full `Layer Height`. For example, with `Radial Initial Radius = 0 mm` and `Layer Height = 2 mm`, generated radii are `1 mm`, `3 mm`, `5 mm`, and so on until the retained mesh radial extent is exceeded.

On each radius, the first bead centerline is offset from the retained mesh base by half of `Default Bead Width`, then later beads advance by the full `Default Bead Width`. For example, with a retained mesh base at `0 mm` and `Default Bead Width = 1 mm`, bead Z positions are `0.5 mm`, `1.5 mm`, `2.5 mm`, and so on until the retained mesh maximum Z is reached.

The profile values above follow the application's active distance unit in the settings UI. Internally, radial slicing stores them as unit-aware distances before path generation. The generated `Arc Specialties` G-code uses metric distance and angle units, so X/Y/Z output is in millimeters.

At each bead Z, the slicer:

1. Cross sections the mesh with a horizontal plane.
2. Builds an approximate circle at the current radial layer radius.
3. Clips that circle against the cross-section polygons.
4. Applies `Radial Boundary Handling`.
5. Converts the selected arcs or rings into print paths. With `Supports G2/G3` enabled, the paths are divided into circular moves using `Number of Arcs per Revolution`; otherwise they remain line-segment paths.

## Required Settings

| Setting | Location | Effect |
| --- | --- | --- |
| `Slicer Type` | Profile > Slicing | Select `Radial Slice` to use the radial slicer. This automatically selects `Arc Specialties` syntax unless it is already selected. |
| `Syntax` | Printer > Machine Setup | Select `Arc Specialties` so the cylindrical writer and parser are used. An existing radial or helical slicer selection is preserved; other slicer types default to `Radial Slice`. |
| `Layer Height` | Profile > Layer | Radial distance between successive cylindrical layers. Values less than or equal to zero fall back to a physical `1 mm` default. |
| `Default Bead Width` | Profile > Layer | Vertical distance between beads on each cylindrical layer. Values less than or equal to zero fall back to `Layer Height`. |
| `Radial Axis Mode` | Profile > Slicing | Selects whether radial cylinders are centered on each part's centroid or on a custom XY coordinate. |
| `Radial Axis X` / `Radial Axis Y` | Profile > Slicing | Custom radial cylinder axis coordinates when `Radial Axis Mode` is `Custom XY`. |
| `Radial Initial Radius` | Profile > Slicing | Inner radial boundary before the half-layer offset is applied. Values less than or equal to zero place the first printed radius at half of `Layer Height`. |
| `Radial Boundary Handling` | Profile > Slicing | Controls radial paths that cross the model boundary. |
| `Number of Arcs per Revolution` | Profile > Slicing | Sets how many G2/G3 moves represent one complete revolution when `Supports G2/G3` is enabled. Clipped or partial paths may end with a shorter final arc. |

## Boundary Handling

When a radial path crosses the model boundary at a bead Z, model clipping can split it into one or more retained arcs. `Radial Boundary Handling` controls that case:

| Option | Behavior |
| --- | --- |
| `Clip to Model` | Outputs only the retained portions inside the model. This is the default and matches the original radial slicing behavior. |
| `Keep Boundary-Crossing Path` | Outputs the original full radial path when any portion of it is inside the model cross section. |
| `Discard Boundary-Crossing Path` | Omits radial paths when clipping removes a meaningful portion of the path. Paths fully inside the model cross section are still kept. |

## G-code And Machine Settings

`Arc Specialties` output keeps X, Y, and Z as user-frame endpoint coordinates relative to the active work offset and emits
`XR`, `YR`, `ZR`, `AP`, and `CP` fields on every travel and print move. The first Arc Specialties implementation fixes
`XR=180`, `YR=0`, and `ZR=0`; maps `Axis A` to `AP`; and computes `CP` from the radial endpoint angle plus `Axis C`,
normalized to the 0-360 degree range. Enabling `Supports G2/G3` writes radial and helical print paths as G2/G3 moves
divided according to `Number of Arcs per Revolution`; those moves use equals-form `I=`/`J=` offsets and place the
feedrate at the end of the motion fields. When arc support is disabled, print paths remain segmented G1 moves.

The radial settings header reports radial geometry, boundary handling, and rotary or positioner settings only, so it does not
include process-specific nozzle, filament, extrusion, or standard polymer region comments. Radial print moves are marked with
`RADIAL`.

When generated `Arc Specialties` G-code is loaded for preview, the Arc Specialties parser validates `XR`, `YR`, `ZR`,
`AP`, and `CP`, normalizes `X=...`, `Y=...`, `Z=...`, `I=...`, `J=...`, `K=...`, `R=...`, and `F...` fields for the
common XYZ preview parser, and treats `RADIAL` and `HELICAL` print comments as printable moves.

| Setting | Location | Effect |
| --- | --- | --- |
| `Axis A` | Printer > Machine Setup | Positioner tilt emitted as `AP`. |
| `Axis C` | Printer > Machine Setup | Added to the point angle around the radial center. `Arc Specialties` emits CP normalized to 0-360 degrees and relies on controller shortest-way movement. |
| `Supports G2/G3` | Printer > Machine Setup | Enables G2/G3 radial and helical print moves. When disabled, cylindrical print paths use segmented G1 moves. |
| `Number of Arcs per Revolution` | Profile > Slicing | Controls the angular subdivision of radial and helical G2/G3 paths. A complete revolution is written with this many arcs. |
| `Default Print Speed` | Profile > Layer | Print feedrate for radial bead segments. |
| `Travel Speed` | Profile > Travel | Feedrate for non-print travel moves. If unset, the writer falls back to machine max XY speed. |
| `Travel Lift Height` | Profile > Travel | Radial-safe travel lift distance. The lift moves outward from the cylinder axis before the travel, then lowers at the destination when enabled. |
| `Minimum Travel Length for Lifting` | Profile > Travel | Suppresses travel lifting for shorter travel moves. |
| `Z Speed` | Printer > Machine Speed | Speed used for travel lift and lower moves when available. |

## Path Ordering

Radial paths are grouped by same-circle bead position before they are ordered. This prevents disconnected arcs on one circle from being connected to arcs on a different circle.

`Path Order Optimization` applies within each same-circle group:

| Option | Radial behavior |
| --- | --- |
| `Next Closest` | Selects the arc with the nearest open endpoint to the current location. |
| `Next Farthest` | Selects the arc with the farthest nearest endpoint from the current location. |
| `Random` | Selects a random remaining arc. |
| `Outside In` | Orders arcs by angular sweep around the radial center in one direction. |
| `Inside Out` | Orders arcs by angular sweep around the radial center in the opposite direction. |
| `Custom Location` | Selects the arc nearest to `Custom Path Point X/Y Location`. |

`Point Order Optimization` selects which open endpoint of the chosen arc is printed first. `Custom Location` uses `Custom Point X/Y Location`.

## Travel Behavior

Radial travel moves are written by the Arc Specialties writer. For angular moves that are long enough to benefit from
segmentation, the writer connects travel endpoints with an arc-like segmented path around the radial center instead of a straight chord.
Very short moves and moves at the radial center fall back to direct travel.

When a lift is required, the start and destination are moved outward from the radial cylinder axis by `Travel Lift Height`
before the segmented traverse, then lowered at the destination. This avoids straight-line travel moves cutting through
previously printed radial layers.

Short travels below `Minimum Travel Length for Lifting` do not lift.

## Current Limitations

- Radial slicing v1 generates only direct radial ring or arc paths.
- Standard polymer regions are not generated. Settings for perimeter count, infill, skin, support, and raft do not create additional radial features.
- The cylinder axis is vertical Z through each part's XY centroid or the configured custom XY coordinate.
- The top of the radial slice is the retained mesh maximum Z. There is no separate radial cylinder height setting.
- Candidate circles are sampled for model clipping. The output uses that sampled path as G1 segments when arc support is disabled, or G2/G3 moves controlled by `Number of Arcs per Revolution` when it is enabled.
- Clipping meshes are applied before radial path generation, but slicing vector settings are not used by radial slicing.
- Arc Specialties work-offset and touch-probe setup commands are not emitted in the first pass. The generated header documents that an appropriate user frame must already be active.

## Quick Checks

After slicing, verify that:

- The G-code header identifies `Arc Specialties`.
- `Arc Specialties` motion lines use `G00`/`G01`, or `G02`/`G03` when `Supports G2/G3` is enabled. They contain `X=`, `Y=`, `Z=`, `XR=`, `YR=`, `ZR=`, `AP=`, `CP=`, and include `F` at the end of every motion line.
- A complete radial ring or helical revolution contains the configured `Number of Arcs per Revolution`; clipped or partial paths may include a shorter final arc.
- Printed arcs lie on the part rather than above or below it.
- Travel moves arc around the radial axis when the angular move is long enough, and configured travel lift offsets them outward before traversing.
- The radial axis is centered where expected for the selected `Radial Axis Mode`.
- The first printed radius is half a `Layer Height` outward from `Radial Initial Radius`.
