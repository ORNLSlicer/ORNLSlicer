# Arc Specialties

Arc Specialties is the ORNLSlicer G-code dialect for the Arc Specialties wire-arc machine workflow. It can format the normal planar segment stream and the direct radial/helical paths from [Cylindrical Slicing](../slicing/cylindrical-slicing.md).

The implementation lives in:

- [`ArcSpecialtiesWriter`](../../include/gcode/writers/arc_specialties_writer.h)
- [`arc_specialties_writer.cpp`](../../src/gcode/writers/arc_specialties_writer.cpp)
- [`ArcSpecialtiesParser`](../../include/gcode/parsers/arc_specialties_parser.h)
- [`arc_specialties_parser.cpp`](../../src/gcode/parsers/arc_specialties_parser.cpp)

## File Format

Arc Specialties uses the `Arc Specialties` syntax identifier, `.nc` file extension, semicolon comments, millimeter distance units, degree angle units, seconds for dwell time, and millimeters per minute for feedrate output.

Generated motion uses `KEY=value` fields:

```gcode
G00 X=12.0000 Y=0.0000 Z=150.0000 XR=180.0000 YR=0.0000 ZR=-90.0000 AP=0.0000 CP=0.0000 ;TRAVEL
G01 X=12.0000 Y=1.0000 Z=0.5000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=5.0000 F600.0000 ;RADIAL
G03 X=0.0000 Y=12.0000 Z=1.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=90.0000 I=0.0000 J=0.0000 F600.0000 ;HELICAL
```

The writer emits `G00` for rapid travel, `G01` for feed moves, and `G02`/`G03` for clockwise/counter-clockwise arc moves when G2/G3 support is enabled. If G2/G3 support is disabled, arc segments fall back to `G01`.

## Motion Fields

| Field | Meaning |
| --- | --- |
| `X`, `Y`, `Z` | Endpoint coordinates relative to the active work offset after the configured G-code frame rotation is applied. |
| `XR`, `YR`, `ZR` | Tool-frame orientation angles. The writer emits `XR=180.0000`, `YR=0.0000`, and either feed or rapid `ZR`. |
| `AP` | Positioner tilt from `Printer > Machine Setup > Axis A`. |
| `CP` | Positioner rotation. Planar output uses `Axis C`; cylindrical output computes it from the endpoint angle around the cylinder axis plus `Axis C`. |
| `I`, `J` | G2/G3 arc center parameters. The values are absolute center coordinates in G161 mode and relative start-to-center offsets in G162 mode. |
| `F` | Feedrate in millimeters per minute. It is emitted on `G01`, `G02`, and `G03`; rapid `G00` moves do not include `F`. |

Feed moves use `ZR=-135.0000`. Rapid travel moves use `ZR=-90.0000`. For the Arc Specialties partner frame, set `Printer > Machine Setup > G-Code Frame Rotation Z` to `-90 deg`.

## AP And CP

`AP` is the configured `Axis A` positioner tilt.

For planar slicing, `CP` is the configured `Axis C` value normalized to `[0, 360)`. The planar region stream is otherwise preserved, so print comments continue to use region names such as `Perimeter`, `Inset`, and `Skeleton`.

For cylindrical slicing, the writer rotates the endpoint and cylinder center into the configured G-code frame, computes the endpoint angle around that transformed center, adds `Axis C`, and normalizes the result to `[0, 360)`.

For helical cylindrical paths, `CP` reports positive angular sweep from the transformed `Helical Path Start Angle` plus `Axis C`. `Right Handed` helices advance with counter-clockwise G03 arcs, and `Left Handed` helices advance with clockwise G02 arcs. With no additional frame rotation, the default `90 deg` helical start angle, and `Axis C=0`, four equal quarter-revolution endpoints report `CP` values of `90`, `180`, `270`, and `0`.

## Arc Center Modes

`Printer > Machine Setup > G2/G3 Center Point Interpretation` controls G2/G3 I/J output when `Supports G2/G3` is enabled.

| Mode | Startup command | I/J output |
| --- | --- | --- |
| `Absolute` | `G161` | The configured `G2/G3 Absolute Center` I and J values. |
| `Relative` | `G162` | The frame-rotated offset from the arc start point to the arc center point. |

When absolute center mode was enabled during startup, shutdown emits `G164` to leave that mode. The parser also treats `G164` as disabling absolute-center parsing.

## Startup Sequence

`ArcSpecialtiesWriter::writeInitialSetup()` writes the weld schedule variables, contour mode, robot-home/channel setup, absolute positioning, and a TRAFO-off initial state:

```gcode
V.E.Sch.Preflow = 3  ;Preflow Time in Seconds
...
#CONTOUR MODE [DEV PATH_DEV=2 CONST_VEL=1]
;M06 T1   ;Select Tool 1
M49 ;Send Robot Home
#CHANNEL INIT [CMDPOS]

G90
#TRAFO OFF
#FLUSH WAIT
```

The work-object kinematics block is deferred until the first travel target is known. The first travel emits an `INITIAL WORLD APPROACH` section with a TRAFO-off `G00 ... ;WORLD APPROACH TRAVEL` move, then writes:

```gcode
#KIN ID [9]
#FLUSH WAIT

V.G.KIN[9].PROGRAMMING_MODE            = -1
V.G.KIN[9].RTCP                        = 0
#ORI MODE [ANGLE]
V.G.WZ_AKT.L = 0
M01
#FLUSH WAIT
#TRAFO ON
#FLUSH WAIT
#CHANNEL INIT [CMDPOS]
#FLUSH WAIT
G161
```

The `#TRAFO` line is `#TRAFO ON` or `#TRAFO OFF` according to `Enable TRAFO`. The final modal line is `G161` for absolute arc centers or `G162` for relative arc centers.

The layer marker is held until after the initial world approach and kinematics block. The first normal lifted travel is then emitted before the first bead marker and travel-lower move. For cylindrical slicing, the initial world approach uses the cylinder center XY and the build maximum Z plus a 100 mm buffer. For planar slicing, it uses the first travel XY and the same Z clearance policy.

## Travel And Welding Commands

Travel moves use `G00`. Cylindrical travel lift moves outward from the cylinder axis; planar travel lift follows the slice-plane normal. Non-first cylindrical travel may be split into `TRAVEL ARC` waypoints around the cylinder axis when the angular move is long enough. Travel lower is emitted as a feed move:

```gcode
;G80 ;OPTIONAL STOP ROUTINE
G01 X=... Y=... Z=... XR=180.0000 YR=0.0000 ZR=-135.0000 AP=... CP=... F... ;TRAVEL LOWER
```

The writer turns welding and blending on before print motion and off before longer travel or shutdown:

```gcode
M150 ;WIRE ARC WELDER ON
G261 ;BLENDING ON
...
G260 ;BLENDING OFF
M151 ;WIRE ARC WELDER OFF
;M160 ;CLIP WIRE
#CHANNEL INIT [CMDPOS]
```

`M06`, `G80`, and `M160` are currently emitted as comments because the inline writer TODOs identify controller issues with those commands as of 2026-07-29. They should be re-enabled or replaced only after the controller behavior is resolved.

Shutdown writes any configured end code, emits `G164` when absolute center mode was active, sends the robot home, initializes the channel, and ends the program with `M02`.

## Parser Behavior

`ArcSpecialtiesParser` is selected for generated or imported files whose header identifies the `Arc Specialties` syntax. It normalizes `G00`, `G01`, `G02`, and `G03` to the shared parser's `G0`, `G1`, `G2`, and `G3` handlers after Arc Specialties-specific preprocessing.

The parser accepts:

- Common motion fields `X`, `Y`, `Z`, `I`, `J`, `K`, `R`, and `F`.
- Orientation and positioner fields `XR`, `YR`, `ZR`, `AP`, and `CP`.
- Both equals form, such as `X=1.0000`, and compact form, such as `X1.0000`; orientation keys use their two-letter prefix in compact form.

For known common and orientation fields, the parser validates that each key appears at most once and that its value is numeric. It strips `XR`, `YR`, `ZR`, `AP`, and `CP` before delegating XYZ motion to `CommonParser`, because the preview only models XYZ geometry. Orientation-only moves are valid machine-positioning commands but do not create visible XYZ preview segments.

Unknown `KEY=value` fields raise an Arc Specialties illegal-parameter error. Non-key tokens without `=` are passed through to the common parser.

When `G161` absolute-center mode is active, the parser converts absolute `I` and `J` values into relative offsets from the current X/Y position before delegating arcs to `CommonParser`. `G162` and `G164` disable that conversion. Comments containing `RADIAL` or `HELICAL`, excluding travel comments, temporarily force print-state handling so cylindrical preview moves are classified as bead motion.
