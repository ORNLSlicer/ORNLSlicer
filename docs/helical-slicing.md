# Helical Slicing

Helical slicing generates direct paths around a vertical cylindrical axis. For each radius, the slicer samples a helix:

`x(t) = r cos(t)`, `y(t) = r sin(t)`, `z(t) = z0 + (bead_width / (2 * pi)) * t`

The first radius is half a `Layer Height` outward from `Radial Initial Radius`, and later radii advance by `Layer Height`. `Default Bead Width` is used as the helix rise per full revolution.

## Usage

1. Load one or more build parts.
2. Set `Slicer Type` to `Helical Slice`.
3. Set radial spacing with `Layer Height`.
4. Set helical rise with `Default Bead Width`.
5. Set `Radial Axis Mode` if the helix should use a custom XY coordinate instead of the part centroid.
6. Set `Radial Initial Radius` if the first helix should begin away from the axis.
7. Set `Radial Boundary Handling` for helical paths that cross the model boundary.
8. When boundary handling is `Clip to Model`, set `Helical Clipping Method` to choose how intersections are retained.
9. Set `Helical Path Length` when long generated helices should be split into shorter paths. Leave it at `0` to keep each clipped helix fragment as one path.
10. Confirm the printer `Syntax` is `Arc Specialties`. Selecting `Helical Slice` defaults to `Arc Specialties` when the current syntax is not cylindrical-capable.
11. Enable `Supports G2/G3` to write the helix as arc moves, then set `Number of Arcs per Revolution` to control its angular subdivision. Leave arc support disabled to write segmented G1 moves.

## Path Generation

For each build part, helical slicing resolves the same vertical axis controls used by radial slicing. `Part Centroid` uses the part's XY centroid. `Custom XY` uses `Radial Axis X` and `Radial Axis Y`.

The slicer samples horizontal model cross sections through the retained mesh, samples a candidate helix at each radius, and keeps contiguous portions whose sampled points are inside the nearest model cross section.

Boundary handling follows the radial slicer behavior:

| Option | Behavior |
| --- | --- |
| `Clip to Model` | Clips the helix according to `Helical Clipping Method`. |
| `Keep Boundary-Crossing Path` | Outputs the original full helix when any portion is inside. |
| `Discard Boundary-Crossing Path` | Omits helices cut by the model boundary while keeping fully contained paths. |

`Helical Clipping Method` is used only with `Clip to Model`:

| Option | Behavior |
| --- | --- |
| `All Model Intersections` | Outputs every contiguous helix portion that lies inside the model. This is the default and preserves the existing clipping behavior. |
| `Highest Z Intersection` | Outputs one continuous prefix of the original helix, from its generated start through the boundary intersection with the greatest Z value. Lower outside spans within that prefix are retained so the result remains continuous, and no path is emitted above the selected intersection. |

When `Highest Z Intersection` finds no boundary crossing, a helix that is wholly inside the model is kept in full and a helix that is wholly outside is omitted. Tangential contacts that do not change the inside/outside classification are not boundary crossings.

`Keep Boundary-Crossing Path` and `Discard Boundary-Crossing Path` do not use `Helical Clipping Method`; their behavior is unchanged.

If `Helical Path Length` is greater than `0`, each generated helical fragment is split into shorter paths before print segments are emitted. Split paths stay contiguous, so no travel move is inserted between adjacent split points. Values of `0` or smaller leave the generated helical fragments unbroken.

## Output

Helical slicing supports `Arc Specialties` X/Y/Z/XR/YR/ZR/AP/CP output. With `Supports G2/G3` enabled, it writes rising helical arcs with endpoint Z values and divides each complete revolution into `Number of Arcs per Revolution` moves. Clipped or partial helical paths may end with a shorter final arc. With arc support disabled, helical paths remain segmented G1 moves. Print moves are marked with `HELICAL`, and the Arc Specialties parser strips orientation fields for XYZ preview visualization.

Helical slicing bypasses the standard polymer perimeter, infill, skin, support, and raft generation pipeline. This first implementation emits direct clipped helical paths only.
