---
type: guide
title: Cylindrical and Helical Slicing
description: How ORNL Slicer generates radial and helical paths directly, clips them to build geometry, orders them, and emits Arc Specialties machine motion.
tags: [slicing, cylindrical, radial, helical, arc-specialties]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-9eef3e22b2b57db54842729c
    resource: repo://include/slicing/helical_path_rounding.h
  - id: openwiki-source-83bd812a869b83fe85ad9e0c
    resource: repo://include/threading/slicers/cylindrical_slicer.h
  - id: openwiki-source-f59b754c28024e0b0622af41
    resource: repo://resources/settings/020_profile_slicing.yaml
  - id: openwiki-source-9dd742dabfc9abff0fdd3277
    resource: repo://resources/settings/028_profile_radial.yaml
  - id: openwiki-source-49b18848ab782915839c94e7
    resource: repo://resources/settings/029_profile_helical.yaml
  - id: openwiki-source-b5e19d8b2e881b1c706e1c44
    resource: repo://src/gcode/writers/arc_specialties_writer.cpp
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-9691825f79c753541a758204
    resource: repo://src/slicing/helical_path_rounding.cpp
  - id: openwiki-source-8c60a8d33451696f8f70f5c5
    resource: repo://src/slicing/helical_region_profile.cpp
  - id: openwiki-source-fa5d3d31fe13fb75f358585a
    resource: repo://src/slicing/helical_tool_start_angle.cpp
  - id: openwiki-source-742f62690d2691cdfaabdf08
    resource: repo://src/step/layer/cylindrical_layer.cpp
  - id: openwiki-source-74cea8ba6a71c48d8439dfdc
    resource: repo://src/threading/slicers/cylindrical_slicer.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Cylindrical and Helical Slicing

Cylindrical slicing is a specialized direct-generation pipeline. It does not create conventional islands and regions for later `StepThread` computation: `CylindricalSlicer` constructs `CylindricalLayer` paths during preprocessing, orders those paths during postprocessing, and writes them with `ArcSpecialtiesWriter`. `CylindricalLayer::compute()` is consequently a no-op. [source](repo://include/threading/slicers/cylindrical_slicer.h#L21-L47) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L418-L457) [source](repo://src/step/layer/cylindrical_layer.cpp#L190-L192)

```text
build parts + clipping meshes + effective settings
                       │
                       ▼
        copy and clip each supported part mesh
                       │
             ┌─────────┴─────────┐
             ▼                   ▼
       radial pattern       helical pattern
   horizontal sections     region/pitch profile
   concentric radii        sampled rising curve
             │                   │
             └─────────┬─────────┘
                       ▼
        clip candidates to horizontal sections
                       │
                       ▼
       CylindricalLayer paths and path ordering
                       │
                       ▼
       Arc Specialties setup, motion, and shutdown
```

## Entry conditions and shared preprocessing

Session dispatch permits cylindrical slicing only when the selected G-code syntax is Arc Specialties. The slicer reinforces that boundary by setting its syntax and constructing `ArcSpecialtiesWriter` itself. This is an explicit current-code restriction, not a statement that the geometry algorithms are intrinsically tied to one controller. [source](repo://src/managers/session_manager.cpp#L47-L49) [source](repo://src/managers/session_manager.cpp#L575-L583) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L418-L421)

Preprocessing begins from globally adjusted settings, then overlays each build part's local settings. It gathers clipping meshes separately, clears old part steps, copies each supported build mesh, and clips the copy. Unsupported mesh types, empty mesh collections, and meshes without usable bounds are skipped. This preserves the loaded mesh while making the generated path geometry reflect clipping meshes. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L459-L527) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L903-L944)

The cylinder center comes from the root build mesh centroid unless the axis-source setting selects custom X/Y; its Z coordinate is the part's lower bound. A nonpositive cylinder height means “use the part height,” while a positive height is capped at the mesh top. The maximum candidate radius is measured from that center to mesh vertices. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L408-L415) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L917-L944) [source](repo://resources/settings/020_profile_slicing.yaml#L55-L142)

Each part selects either the radial or helical generator after local settings are applied. This means different parts can use different effective patterns, handedness, clipping policy, and profile parameters even though the first global pattern is retained for the final no-path diagnostic. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L465-L474) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L499-L576)

## Radial path generation

Radial slicing treats each layer as a cylindrical shell at a fixed radius. It first caches horizontal mesh cross-sections from half a bead width above the base through the configured top, stepping by bead width. It then advances radii from `inner radius + layer height / 2` through the farthest mesh vertex, stepping by layer height. Local layer adjustments are applied once per radial shell. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L582-L643)

At every cached Z, the generator creates a sampled circle at the shell radius and configured radial start angle, clips it against the cross-section, and applies one of three boundary policies:

| Policy | Result when the circle meets a boundary |
| --- | --- |
| `Clip` | Keep the clipped fragments. |
| `Keep` | Keep the original full circle when any intersection exists. |
| `Discard` | Remove a circle that crosses the boundary. |

The policy implementation distinguishes a fully retained candidate from boundary-crossing fragments, and the settings describe zero degrees as +X and 90 degrees as +Y. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L78-L106) [source](repo://resources/settings/028_profile_radial.yaml#L4-L30)

Candidate circles are sampled to target segments no longer than half a bead width, bounded to 64–720 samples. When the writer supports counterclockwise arcs, printable circular fragments are converted to configured arc segments; otherwise they remain line segments. A travel is inserted only when the next print start is more than 10 microns from the current location. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L946-L978) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L981-L1034)

## Helical region profile

Helical slicing builds a variable-pitch profile before sampling geometry. The profile consists, in order, of bottom perimeter revolutions, bottom inset revolutions, middle infill revolutions, top inset revolutions, and top perimeter revolutions. Each band retains its region type, revolution count, and pitch. A configured zero stepover falls back to bead width; a negative stepover is invalid. The available middle height determines the infill revolution count, which is rounded down, to nearest, or up according to the infill-rounding setting. A nonpositive infill span is omitted. [source](repo://src/slicing/helical_region_profile.cpp#L21-L69) [source](repo://src/slicing/helical_region_profile.cpp#L300-L361) [source](repo://resources/settings/029_profile_helical.yaml#L45-L119)

The generator starts at the part base and uses the same half-layer radial offset as radial slicing. Right-handed paths rise counterclockwise; left-handed paths rise clockwise. Invalid height, negative shell counts, invalid pitches, or a profile with no positive-revolution bands stop generation for that part and surface a reason for an invalid profile. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L681-L727) [source](repo://src/slicing/helical_region_profile.cpp#L300-L361) [source](repo://resources/settings/029_profile_helical.yaml#L4-L44)

The profile's generated top can differ from the nominal mesh top because infill revolutions are rounded. Geometry sampling therefore uses the lower of the profile-generated top and configured/mesh top. Horizontal sections are spaced at half the profile's minimum pitch, with a 100-micron minimum spacing and explicit endpoint samples. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L730-L780)

## Clipping and Z rounding

For each candidate radius, the helical sampler maps revolution position to angle, Z, and region using the profile. The angular direction follows handedness, while the geometric seam always begins at 90 degrees (+Y). Region bands are sampled separately so their identities survive flattening into the curve used for model-intersection tests. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L191-L274) [source](repo://src/slicing/helical_tool_start_angle.cpp#L3-L18)

Every sample is classified against the nearest cached horizontal cross-section. When consecutive samples cross the model boundary, the transition is refined with 12 bisection iterations. If the generated start is outside the model, retention begins at the first entry; if the end is outside, retention stops at the highest exit. A curve wholly outside yields no path. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L276-L399)

The retained Z interval is rebuilt as a region profile. `Exact Intersection` keeps its clipped endpoint. `Complete Revolution` rounds up to the next full revolution, while `Last Full Revolution` rounds down and can eliminate a path shorter than one complete turn. During rounded reconstruction, shell pitches remain fixed and only a middle infill band can absorb the remaining height; reconstruction fails if the fixed shell regions exceed the rounded height. [source](repo://src/slicing/helical_region_profile.cpp#L130-L225) [source](repo://src/slicing/helical_region_profile.cpp#L364-L386) [source](repo://resources/settings/029_profile_helical.yaml#L4-L44)

`HelicalPathRounding` is also kept as a focused uniform-pitch clipping/rounding utility and regression surface. Its exact, ceil-to-complete, and floor-to-last-full behavior is tested directly, but the current cylindrical generator performs its variable-region reconstruction through `HelicalRegionProfile`; do not assume the standalone utility is the call path for profile-driven slicing. [source](repo://include/slicing/helical_path_rounding.h#L10-L26) [source](repo://src/slicing/helical_path_rounding.cpp#L59-L129) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L789-L867)

## Seam, ordering, and tool orientation

The helical start-angle setting is a tool-orientation offset, not a geometry-seam control. Geometry starts at +Y. The effective tool offset changes sign only when all three conditions hold: Z clipping requests a complete revolution, cylindrical path ordering is next-closest, and the optimizer chooses the generated end as the path start. [source](repo://src/slicing/helical_tool_start_angle.cpp#L3-L18) [source](repo://resources/settings/029_profile_helical.yaml#L4-L44)

Postprocessing removes previously generated travels and delegates ordering to `PathOrderOptimizer`. Direct cylindrical layers support next-closest and next-farthest ordering; helical linking may reverse a path. After selection, the layer restores print settings, creates settings for optimizer-generated travels, recomputes region-start flags, and applies the direction-aware helical tool offset. [source](repo://src/step/layer/cylindrical_layer.cpp#L54-L113) [source](repo://src/step/layer/cylindrical_layer.cpp#L194-L227)

A helical layer stores the full retained curve as one path whose segments carry perimeter, inset, or infill settings. Emission opens and closes writer region hooks as those segment regions change without injecting travel at an internal region boundary. Radial layers, by contrast, emit every path under the perimeter region. [source](repo://src/step/layer/cylindrical_layer.cpp#L116-L188)

## Arc Specialties output and Z safety

Arc Specialties output records the cylindrical pattern, ordering, coordinate-frame values, helical handedness, clipping-rounding choice, tool-frame rotations, and arcs per revolution in its header. During motion it selects region schedules, transforms cylindrical coordinates into machine/work frames, and can subdivide long angular travels into arc waypoints. These controller-specific behaviors explain the current capability gate around this writer. [source](repo://src/gcode/writers/arc_specialties_writer.cpp#L216-L270) [source](repo://src/gcode/writers/arc_specialties_writer.cpp#L365-L389) [source](repo://src/gcode/writers/arc_specialties_writer.cpp#L536-L698)

Printable line and arc starts establish the layer, kinematics, and weld schedule before enabling deposition; unsupported arcs fall back to line output. Helical generation tracks the highest actual generated Z after clipping and rounding, and `doSlice()` passes the larger of that value and any existing writer maximum to setup before motion is written. That keeps the writer's safe startup height aligned with the path that will actually be emitted. [source](repo://src/gcode/writers/arc_specialties_writer.cpp#L700-L747) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L423-L451) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L842-L850)

## Failure, cancellation, and diagnostics

- Starting with no loaded parts logs a warning and returns. Cancellation is checked after preprocessing, postprocessing, and output. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L423-L456)
- Nonpositive layer height and bead width use safe fallbacks; a negative inner radius is clamped to zero. Empty cross-sections and profiles with no usable height generate no layer. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L582-L634) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L681-L735)
- An invalid helical profile emits its specific reason. If all parts produce no path, the final warning points to the inner radius, axis source, clipping meshes, and the pattern-specific boundary or Z-rounding setting. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L720-L727) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L560-L576)
- Progress is aggregated across build parts for preprocessing and compute, even though direct path generation occurs inside preprocessing rather than a conventional compute queue. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L476-L558)

## Regression map

Use the focused tests according to the behavior being changed:

| Change area | Primary regression coverage |
| --- | --- |
| Region sequence, pitch fallback, infill rounding, invalid inputs | `helical_region_profile_tests` verifies cumulative Z, floor/nearest/ceil choices, omitted nonpositive infill, and diagnostic reasons. [source](repo://tests/helical_region_profile_tests.cpp#L29-L139) |
| Retained clipping profile | The same target checks clipped starts, preserved shell ordering and pitches, rounded endpoints, and last-full behavior without overshoot. [source](repo://tests/helical_region_profile_tests.cpp#L160-L280) |
| Uniform-pitch clipping and start orientation | `helical_clip_rounding_tests` covers exact/complete/last-full results, sub-one-turn elimination, handed endpoints, fixed +Y geometry start, and tool-offset sign changes. [source](repo://tests/helical_clip_rounding_tests.cpp#L71-L213) |
| Writer/parser integration | `arc_specialties_parser_tests` covers block numbering, rotation direction, region comments, tool frames, safe startup Z, clip-rounding metadata, travel behavior, and configured arc density. [source](repo://tests/arc_specialties_parser_tests.cpp#L806-L874) |

When modifying this subsystem, check the whole chain: settings meaning, per-part overlay, profile construction, geometry classification, retained-profile reconstruction, optimizer reversal, segment region metadata, header reporting, safe-height propagation, and the Arc Specialties parser-facing output. A change that is locally correct in the sampler can still become wrong after ordering or machine-frame conversion.

Related reading: [Slicing Pipeline Orchestration](../architecture/slicing-pipeline.md), [Geometry and Toolpath Model](../concepts/geometry-and-toolpath-model.md), [G-Code, Parsing, Visualization, and Export](../architecture/gcode-and-visualization.md), and [Machine Syntax Writers and Parsers](../integrations/machine-syntaxes.md).
