---
type: guide
title: Planar Cross-Sections and Path Generation
description: How planar slicing turns meshes and layered settings into cross-sections, islands, region paths, globally ordered motion, and final modifiers.
tags: [slicing, planar, cross-section, toolpath, optimization]
sources:
  - id: openwiki-source-37878225e1b652f31b122609
    resource: repo://include/geometry/contour_connection.h
  - id: openwiki-source-8882bcf023823dd683b4b159
    resource: repo://include/slicing/buffered_slicer.h
  - id: openwiki-source-7aa3597430c5531fbd775d9a
    resource: repo://include/slicing/preprocessor.h
  - id: openwiki-source-d74e50435e998de961f6cc93
    resource: repo://src/cross_section/cross_section_object.cpp
  - id: openwiki-source-5e1407456a1bc930b497003b
    resource: repo://src/cross_section/cross_section.cpp
  - id: openwiki-source-4a7672bbc05bcfbef1d708f3
    resource: repo://src/optimizers/layer_order_optimizer.cpp
  - id: openwiki-source-23c065c078f003c0b92bf2a8
    resource: repo://src/slicing/buffered_slicer.cpp
  - id: openwiki-source-29d5dbfa478667e089493f38
    resource: repo://src/slicing/preprocessor.cpp
  - id: openwiki-source-e6313e383d53e930ab9c33c4
    resource: repo://src/slicing/slicing_utilities.cpp
  - id: openwiki-source-85901c6cd3589bcc0a1df2b0
    resource: repo://src/step/global_layer.cpp
  - id: openwiki-source-8d3b1576a257384e27959c3d
    resource: repo://src/step/layer/island/island_base.cpp
  - id: openwiki-source-f031d4676982d01e5471f267
    resource: repo://src/step/layer/island/polymer_island.cpp
  - id: openwiki-source-9537a65b88b944291a31aa8d
    resource: repo://src/step/layer/layer.cpp
  - id: openwiki-source-54164313dac767bd4d5141e6
    resource: repo://src/step/layer/regions/infill.cpp
  - id: openwiki-source-28ac42c237d5d9987f0085d5
    resource: repo://src/step/layer/regions/inset.cpp
  - id: openwiki-source-e451362b0f73808dce5d1127
    resource: repo://src/step/layer/regions/perimeter.cpp
  - id: openwiki-source-49cb237e320e24287dd4e890
    resource: repo://src/step/layer/regions/region_base.cpp
  - id: openwiki-source-1d02fec2dd72ccb1ea3b4c51
    resource: repo://src/threading/slicers/planar_slicer.cpp
  - id: openwiki-source-2b373e298658f39ccac3e9f6
    resource: repo://src/threading/step_thread.cpp
  - id: openwiki-source-9f10ba488daafec662ecc53a
    resource: repo://src/threading/traditional_ast.cpp
  - id: openwiki-source-611a07d7b5589694e83c490e
    resource: repo://tests/contour_connection_branch_tests.cpp
  - id: openwiki-source-4f43f1fde5d2866604687e66
    resource: repo://tests/contour_connection_tests.cpp
  - id: openwiki-source-adce7a1f3cfeab8995ef38b7
    resource: repo://tests/global_layer_tests.cpp
  - id: openwiki-source-fd42cdedbc9d871771e139f8
    resource: repo://tests/variable_layer_height_tests.cpp
generated: { by: "codex", at: "2026-11-10T18:02:46.284Z" }
verified:
  - by: openwiki/0.7.2
    at: 2026-11-10T18:02:46.284Z
---

# Planar Cross-Sections and Path Generation

Planar slicing is a staged transformation rather than one geometry call. `PlanarSlicer` configures a reusable `Preprocessor`; the preprocessor copies and cross-sections build meshes into `Layer` objects; dirty layers compute island regions in worker threads; `GlobalLayer` objects combine part steps; and postprocessing orders paths, inserts travel and modifiers, and restores printer coordinates before G-code emission. [source](repo://src/threading/slicers/planar_slicer.cpp#L43-L190) [source](repo://src/threading/traditional_ast.cpp#L42-L109) [source](repo://src/threading/slicers/planar_slicer.cpp#L967-L1050)

```text
global settings
      │ copy + global adjustments
      ▼
part settings ──> range overrides ──> per-layer adjustments
      │                                  │
      │                                  ▼
      └──── build mesh ──> slicing plane ──> cross-section polygons
                                             │
                              settings meshes ──> settings polygons
                                             │
                                             ▼
                           Layer ──> islands ──> ordered regions
                                             │ compute in StepThread
                                             ▼
                              polylines ──> paths and segments
                                             │
                                             ▼
                        GlobalLayer ordering, travel, modifiers
                                             │
                                             ▼
                           reorientation and G-code writer hooks
```

## Preprocessor lifecycle

`Preprocessor::processAll()` divides session parts into build, clipping, and settings groups and runs callbacks in a fixed order: initial processing, per-part processing, per-mesh processing, step building for each slice, cross-section completion, progress, and final processing. Each build part receives a copy of global settings populated with that part's overrides, and each mesh is copied before slicer-specific preprocessing so clipping does not mutate the loaded mesh. [source](repo://include/slicing/preprocessor.h#L16-L34) [source](repo://src/slicing/preprocessor.cpp#L17-L65)

The planar configuration applies global adjustments first, clears old build steps because the cache is disabled, and clips each copied build mesh with enabled root clippers plus its enabled direct child clippers. Buffered settings masks use the same target-part scope. Other build objects, including nested build objects, do not inherit a parent's child modifier. After cross-sectioning it derives skin, support, raft, brim, skirt and scan additions, then global layers. [source](repo://src/threading/slicers/planar_slicer.cpp#L47-L74) [source](repo://src/slicing/preprocessor.cpp#L49-L66) [source](repo://src/slicing/slicing_utilities.cpp#L164-L174) [source](repo://src/threading/slicers/planar_slicer.cpp#L171-L212)

Settings conflicts are checked between actual settings polygons inside each build cross-section, including disconnected islands and transformed planes. Overlap outside build geometry is harmless. A conflict reports both modifiers, the target object and slice; preprocessing calls `failPreProcess`, clears steps/global layers/output, and emits `sliceFailed` before worker dispatch. [source](repo://src/threading/slicers/planar_slicer.cpp#L77-L94) [source](repo://src/threading/slicers/planar_slicer.cpp#L208-L212) [source](repo://src/threading/traditional_ast.cpp#L40-L45) [source](repo://src/threading/traditional_ast.cpp#L116-L124)

The step builder stores a complete slice snapshot: effective settings, settings polygons, cross-section geometry, average normal, slicing plane, and the flattening shifts. New geometry is split into disconnected `PolygonList` parts, with one `PolymerIsland` created for each part. When an existing step is reused within preprocessing, changed settings or settings polygons mark it dirty and cause replacement. [source](repo://include/slicing/buffered_slicer.h#L30-L42) [source](repo://src/threading/slicers/planar_slicer.cpp#L97-L164)

## Slicing planes and cross-sections

`BufferedSlicer` starts at the mesh minimum along the configured normalized slicing vector. Successive planes move by half the previous layer height plus half the current layer height, so each plane samples the center of its layer band even when heights vary. A nonpositive effective layer height terminates slicing, and a plane beyond the mesh maximum is not returned. [source](repo://src/slicing/slicing_utilities.cpp#L201-L224) [source](repo://src/slicing/buffered_slicer.cpp#L248-L293)

For ordinary cross-sectioning, every non-ignored triangle is classified against the plane and intersecting edges produce directed segments. Those segments are shifted and rotated into a flat XY frame; the layer retains the plane and shifts so paths can be transformed back before output. The cross-section builder joins segments by face connectivity, connects and stitches nearby open polylines, optionally smooths them, removes degenerate vertices, and simplifies the resulting polygons. [source](repo://src/cross_section/cross_section.cpp#L21-L184) [source](repo://src/cross_section/cross_section_object.cpp#L43-L53) [source](repo://src/cross_section/cross_section_object.cpp#L62-L201)

The alternative CGAL path calls `MeshBase::intersect()` and extracts polygons and optimization polylines directly. Both paths can apply an oversize offset after cross-sectioning. Settings meshes are always cross-sectioned with the primary mesh's base shift preserved, keeping their 2D masks aligned with the build geometry. [source](repo://src/slicing/buffered_slicer.cpp#L295-L345)

When support is enabled for an elevated part and the slicing direction is +Z, planar preprocessing can move the initial plane to Z = 0. This retains otherwise empty layers below the model so generated support can reach the physical build plate. [source](repo://src/slicing/buffered_slicer.cpp#L61-L73)

## Effective settings precedence

The effective layer settings are assembled in this order:

1. Copy global settings and apply global adjustments in `PlanarSlicer`.
2. Populate the copy with part overrides in `Preprocessor`.
3. For each slice index, populate matching settings-range overrides.
4. Apply index-based local adjustments such as layer-varying angles.
5. Cross-section settings parts into `SettingsPolygon` masks for region- or segment-local behavior.

The first four stages create the `SliceMeta::settings` snapshot. The fifth is intentionally not a blanket `SettingsBase::populate()`: each region decides which local values affect its geometry or segment metadata. [source](repo://src/threading/slicers/planar_slicer.cpp#L51-L56) [source](repo://src/slicing/preprocessor.cpp#L29-L48) [source](repo://src/slicing/buffered_slicer.cpp#L43-L59) [source](repo://src/slicing/buffered_slicer.cpp#L326-L345)

For example, infill removes masks whose relevant infill settings differ from the base, fills the remaining geometry with base settings, then fills intersections with enabled local masks using their line spacing and angle. Perimeters instead split contour segments at settings-polygon boundaries and populate each subsegment from the first containing mask, so speed, material, width policy, and modifier inputs can change along one contour. The per-slice overlap check makes conflicting masks inside build geometry an error rather than relying on iteration order. [source](repo://src/step/layer/regions/infill.cpp#L43-L80) [source](repo://src/step/layer/regions/perimeter.cpp#L1582-L1627) [source](repo://src/step/layer/regions/perimeter.cpp#L1641-L1680) [source](repo://src/threading/slicers/planar_slicer.cpp#L77-L94)

## Variable layer height

Variable layer height is currently gated to JuggerBot syntax as well as its enable setting. For valid minimum height, standard height, and target surface error, the slicer examines face-normal projection within the candidate layer slab, computes `target error / maximum projection`, and clamps the result to the configured minimum and standard heights. Invalid limits, vertical-wall geometry, or a disabled/unsupported configuration use the standard height. [source](repo://src/slicing/buffered_slicer.cpp#L35-L41) [source](repo://src/slicing/buffered_slicer.cpp#L75-L145)

`computeSliceCount()` repeats the same range/local-setting, cusp-limit, and plane-shift decisions without producing cross-section geometry. The focused tests require that this count match production slicing, that sloped geometry introduce bounded intermediate heights, that vertical walls retain standard height, that CGAL and non-CGAL paths agree on layer count, and that non-JuggerBot syntax ignore the option. [source](repo://src/slicing/buffered_slicer.cpp#L212-L246) [source](repo://tests/variable_layer_height_tests.cpp#L176-L237)

## Layers, islands, and regions

A `PolymerIsland` instantiates enabled perimeter, inset, skin, infill, and skeleton regions and records their configured order. `Layer::compute()` asks each island to compute, then sorts polymer regions by that order index. During island computation, each region receives the remaining polygon geometry, generates its own intermediate geometry, and passes its remainder to the next region. Region ordering therefore affects both emission order and what geometry remains available downstream. [source](repo://src/step/layer/island/polymer_island.cpp#L26-L63) [source](repo://src/step/layer/layer.cpp#L151-L158) [source](repo://src/step/layer/island/island_base.cpp#L175-L183)

Representative region behavior:

- Perimeters and insets iteratively offset closed boundaries, reject undersized paths and segments, and can adapt bead width within configured bounds while subtracting each deposited footprint from the remaining geometry. [source](repo://src/step/layer/regions/perimeter.cpp#L324-L430) [source](repo://src/step/layer/regions/inset.cpp#L299-L367)
- Infill offsets the remainder for overlap, derives line spacing from density unless manual spacing is enabled, and delegates line, grid, concentric, triangle, or honeycomb geometry to `PatternGenerator`. Grid directions stay in separate optimizer groups to avoid unnecessary cross-direction travel. [source](repo://src/step/layer/regions/infill.cpp#L43-L143)
- Skin preprocessing supplies upper, lower, and gradual neighbor geometry only to dirty layers, giving the skin region the context needed to classify top, bottom, and gradual-fill areas. [source](repo://src/threading/slicers/planar_slicer.cpp#L215-L258)

Layer additions are separate island or step types built around the model layers. Rafts prepend new shifted layers and shift the original steps upward; brims and skirts add islands; thermal scans add a bounding scan island; and laser scans create or update paired `ScanLayer` steps with copied build-layer settings and orientation. [source](repo://src/threading/slicers/planar_slicer.cpp#L261-L362) [source](repo://src/slicing/layer_additions.cpp#L33-L68) [source](repo://src/slicing/layer_additions.cpp#L181-L221)

### Support demand, restored contacts and bridges

Support is a preprocessing pass, separate from normal polymer regions. It derives demand from adjacent cross-sections, applies settings-part blockers/enforcers, propagates collision-cleared support toward a foundation, optionally builds organic branches or hollow tapered walls, validates connectivity and replaces each layer's support islands. [source](repo://src/threading/slicers/planar_slicer.cpp#L384-L405) [source](repo://src/threading/slicers/planar_slicer.cpp#L500-L520) [source](repo://src/threading/slicers/planar_slicer.cpp#L662-L810) [source](repo://src/threading/slicers/planar_slicer.cpp#L895-L964)

The angle threshold expands the lower model by vertical spacing times the tangent of the threshold to classify unsupported contact. That classification can leave only the tip of an arched/sloped footprint. Restoration grows classified contact within each unsupported component's convex trapezoid cells, using horizontal and vertical partitions, and clips growth to both cell and component. It preserves contact coverage without jumping empty gaps or reaching another arm merely because the arms join elsewhere. Minimum-area filtering follows restoration. [source](repo://src/threading/slicers/planar_slicer.cpp#L524-L581)

Bridge suppression tests each restored component against the model below. It requires full exterior anchor bands on opposing ends, within the configured span length, in candidate directions from world axes and polygon edges. The test uses the span's projected frame with a two-micron clipping tolerance, so rotated spans are supported; partial transverse anchors and attached cantilevers remain support demand. Enforcers are added afterward and blockers are subtracted at the target layer. [source](repo://src/threading/slicers/planar_slicer.cpp#L429-L498) [source](repo://src/threading/slicers/planar_slicer.cpp#L583-L605)

Organic branch seeds start from the original angle-classified contact rather than the enlarged interface footprint. Forced contact joins the seeds, blockers clip them to surviving demand, and a surviving restored component with no seed falls back to its own footprint. This retains branch roots and XY clearance while preserving printable contact interfaces. [source](repo://src/threading/slicers/planar_slicer.cpp#L539-L540) [source](repo://src/threading/slicers/planar_slicer.cpp#L622-L659)

`support_generation_tests` covers sloped interfaces, separate/joined contact components, open/enclosed gaps under rotation, taper footprints, organic clearance and blockers, opposing versus partial bridge anchors, rotated bridges and attached cantilevers, and build-plate/model landings. [source](repo://tests/support_generation_tests.cpp#L375-L407)

## Compute boundary

After preprocessing, `TraditionalAST` queues every dirty non-clipping step and creates up to the smaller of the step count and ideal thread count. Each `StepThread` invokes exactly that step's `compute()`. Postprocessing does not begin until the queue is empty and all worker objects have finished; if no step is dirty, the slicer goes directly to postprocessing. [source](repo://src/threading/traditional_ast.cpp#L48-L109) [source](repo://src/threading/traditional_ast.cpp#L127-L183) [source](repo://src/threading/step_thread.cpp#L12-L40)

This is the extension seam between polygon construction and motion planning: a new island or region must be installed during preprocessing, implement computation from input geometry to paths, and leave enough segment settings for the later optimizer and writer. It should not depend on worker completion order because dirty steps compute independently.

## Global layers and part ordering

`LayerOrderOptimizer` groups part step pairs into `GlobalLayer` objects using one of three policies. By-height chooses the lowest next plane and groups planes within the configured tolerance; by-layer-number groups equal step indices; by-part emits every layer of one part before advancing to the next. [source](repo://src/optimizers/layer_order_optimizer.cpp#L24-L114) [source](repo://src/optimizers/layer_order_optimizer.cpp#L115-L159)

A global layer carries printing and optional scan steps from multiple parts. For island ordering, it uses a common per-layer island-order method when all orderable layers agree. Conflicting methods—or custom-point methods whose projected anchors disagree—fall back to global settings. The anchor is evaluated in a representative layer's slicing frame, so harmless part shifts do not automatically create a conflict. [source](repo://src/step/global_layer.cpp#L32-L101) [source](repo://src/step/global_layer.cpp#L184-L208) [source](repo://tests/global_layer_tests.cpp#L88-L219)

Within a global layer, scans are ordered first. A skirt precedes the main print sequence; raft layers exclude ordinary build/support ordering; otherwise support and polymer precedence follows the effective support-first setting. Brims create parent-child containment order around the enclosed islands, and the selected island optimizer chooses among candidates inside each precedence level. [source](repo://src/step/global_layer.cpp#L210-L259) [source](repo://src/step/global_layer.cpp#L261-L398)

## Path ordering and continuity

Region optimizers turn computed polylines into `Path` and segment objects, choose path and point starts, apply region-level modifiers, prepend travel from the current location, and update that location. Perimeter, inset, and skin may override the default path-order mode; custom anchors are flattened into the same frame as the cross-section before comparison. Consecutive modes can use the previous layer's first printing point for seam continuity. [source](repo://src/step/layer/regions/region_base.cpp#L88-L152) [source](repo://src/step/layer/regions/infill.cpp#L146-L203) [source](repo://src/step/layer/island/island_base.cpp#L236-L252)

Optimizers must make progress on imperfect generated input. Focused tests pin empty island results to `-1`, filter empty paths and degenerate polylines, consume zero-distance candidates even under next-farthest ordering, and distinguish monotonic open-line linking from explicit path-order linking. [source](repo://tests/optimizer_empty_input_tests.cpp#L139-L228)

### Connected contours

`ContourConnection::linkClosedPolylineGroups` joins eligible nested loops into open extrusion groups. Unrelated loops and gaps beyond the width-based connector limit terminate a group, leaving travel between groups. Point annotations identify the outgoing connector without mutating the original loop's settings. The current helper and tests replace the removed `SpiralPath` header and test target; connected contours are distinct from spiralized layer printing. [source](repo://include/geometry/contour_connection.h#L275-L330) [source](repo://include/geometry/contour_connection.h#L780-L878) [source](repo://include/geometry/connection.h#L10-L24) [source](repo://tests/contour_connection_tests.cpp#L32-L103)

Perimeter and inset controls independently select connection enablement, completion of the loop before connection, and connection after a forward tip wipe. The post-wipe branch code rotates seams, prefers a local angled approach, validates both emitted loops and the final branch geometry, and flushes the current group when a connection is unsafe. This preserves separate paths rather than assuming every requested connection exists. [source](repo://resources/settings/022_profile_perimeter.yaml#L235-L282) [source](repo://resources/settings/023_profile_inset.yaml#L148-L183) [source](repo://include/geometry/contour_connection.h#L538-L618) [source](repo://src/step/layer/regions/inset.cpp#L409-L491)

`PolymerIsland::optimize` can pass computed inset geometry and widths to the perimeter stage when both connection modes and Connect to insets are enabled, inset geometry exists, and the perimeter is immediately before the inset in region order. Effective multi-material settings must permit the same material for both regions, including overrides from settings polygons that actually intersect this island. Only consumed inset output is suppressed; unavailable chains retain the normal inset path. [source](repo://src/step/layer/island/polymer_island.cpp#L28-L54) [source](repo://src/step/layer/island/polymer_island.cpp#L104-L142)

Perimeter-to-inset chains retain destination-region settings, local bead widths, and connection identity. The perimeter tracks consumed inset geometry and applies localized inset settings to the corresponding segments; writer transitions can change region state without ending the physical path. Regression coverage includes localized materials, unrelated masks, intervening regions, fallback paths, and reoptimization after eligibility changes. [source](repo://src/step/layer/regions/perimeter.cpp#L1630-L1749) [source](repo://tests/contour_connection_branch_tests.cpp#L1228-L1458)

## Postprocessing, modifiers, and output coordinates

For every dirty global layer, planar postprocessing performs this exact sequence:

1. `unorient()` clean cached paths into the flattened optimization frame.
2. `connectPaths()` orders scans, islands, regions, and paths while threading the current point and previous-region history across global layers.
3. `calculateModifiers()` adds global-layer lead-in or spiral lift and fits eligible line runs to arcs.
4. `reorient()` restores each layer to its slicing plane and printer frame.

[source](repo://src/threading/slicers/planar_slicer.cpp#L967-L1023) [source](repo://src/step/global_layer.cpp#L110-L181)

Region-level modifiers include sharp-corner extension, trajectory slowdown, startup, tip wipe, and optional spiral lift. Modifier code reads segment-local settings before its fallback settings where supported; the sharp-corner regression proves that local settings can enable an extension even when the fallback is disabled, and also guards continuity and the close-connector threshold. [source](repo://src/geometry/path_modifier.cpp#L77-L116) [source](repo://src/step/layer/regions/perimeter.cpp#L1409-L1508) [source](repo://tests/path_modifier_tests.cpp#L30-L95)

Arc fitting is deliberately narrow: it requires G3 support, an emitting syntax whose writer implements arcs, planar mode, and the exact +Z slicing normal. After reorientation, the layer also raises ordinary material paths to a printable minimum Z and redirects clearance modifiers that would cross the build plate; wire-arc surface-referenced and spiralized paths use separate height conventions. [source](repo://src/step/layer/regions/region_base.cpp#L29-L55) [source](repo://src/step/layer/regions/region_base.cpp#L258-L262) [source](repo://src/step/layer/layer.cpp#L410-L465) [source](repo://src/step/layer/layer.cpp#L486-L522)

Finally, `PlanarSlicer::writeGCode()` wraps each global layer with writer layer hooks, emits scan and ordered island paths through `GlobalLayer`, marks every contained step clean, and finishes with `writeAfterPart()`. A global layer with neither a scan nor a valid path emits the writer's empty-step representation. [source](repo://src/threading/slicers/planar_slicer.cpp#L1026-L1050) [source](repo://src/step/global_layer.cpp#L440-L504)

## Change checklist

When extending planar path generation, trace the stage whose contract actually changes:

- Cross-section changes must preserve the flatten/reorient transform and settings-mask alignment.
- Layer-height changes must keep `processSingleSlice()` and `computeSliceCount()` decisions identical.
- New local settings need an explicit region-level interpretation; storing a `SettingsPolygon` alone does not apply every key.
- New islands or regions need stable dirty-step computation and an intentional position in region, island, and global-layer precedence.
- Optimizer changes must handle empty and degenerate candidates while updating current location and prior-layer references.
- Geometry modifiers run after path creation, and global modifiers run after connection; verify segment metadata survives both.
- Output-frame changes must be checked for angled slicing, wire-arc surface reference, minimum-Z correction, and build-plate clearance.

Related reading: [Slicing Pipeline Orchestration](../architecture/slicing-pipeline.md), [Settings and Preferences](../architecture/settings-and-preferences.md), [Geometry and Toolpath Model](../concepts/geometry-and-toolpath-model.md), and [Testing Strategy](../testing/strategy.md).
