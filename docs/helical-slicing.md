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
8. Confirm the printer `Syntax` is `Radial3Plus2`. Selecting `Helical Slice` updates this automatically.

## Path Generation

For each build part, helical slicing resolves the same vertical axis controls used by radial slicing. `Part Centroid` uses the part's XY centroid. `Custom XY` uses `Radial Axis X` and `Radial Axis Y`.

The slicer samples horizontal model cross sections through the retained mesh, samples a candidate helix at each radius, and keeps contiguous portions whose sampled points are inside the nearest model cross section.

Boundary handling follows the radial slicer behavior:

| Option | Behavior |
| --- | --- |
| `Clip to Model` | Outputs only retained portions inside the model. |
| `Keep Boundary-Crossing Path` | Outputs the original full helix when any portion is inside. |
| `Discard Boundary-Crossing Path` | Omits helices cut by the model boundary while keeping fully contained paths. |

## Output

Helical slicing uses the existing `Radial3Plus2` syntax because the machine motion requirements are the same: X/Y/Z plus A/C. Print moves are marked with `HELICAL`; the preview parser validates and strips A/C axes for XYZ visualization.

Helical slicing bypasses the standard polymer perimeter, infill, skin, support, and raft generation pipeline. This first implementation emits direct clipped helical paths only.
