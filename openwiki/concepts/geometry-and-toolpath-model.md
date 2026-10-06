---
type: concept
title: Geometry and Toolpath Data Model
description: Ownership, coordinate frames, units, and invariants from imported meshes through parts, layers, islands, regions, paths, and motion segments.
tags: [geometry, mesh, toolpath, transforms, ownership]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-8cb3f06f029677597f888cb5
    resource: repo://include/geometry/mesh/mesh_base.h
  - id: openwiki-source-e086f074a8c3472745b4505c
    resource: repo://include/geometry/path.h
  - id: openwiki-source-be16f9dd0b4104481b6fa5db
    resource: repo://include/part/part.h
  - id: openwiki-source-af95f1695777b5e0b5e4ed10
    resource: repo://include/units/unit.h
  - id: openwiki-source-5e1407456a1bc930b497003b
    resource: repo://src/cross_section/cross_section.cpp
  - id: openwiki-source-034506ce199cb451a9a8a72c
    resource: repo://src/geometry/mesh/mesh_base.cpp
  - id: openwiki-source-b79ac60f55187d7cb361c2a6
    resource: repo://src/geometry/path_modifier.cpp
  - id: openwiki-source-b59298805f0b4883fe586e89
    resource: repo://src/geometry/point.cpp
  - id: openwiki-source-165fea8ffaca12491bd296b9
    resource: repo://src/geometry/segment_base.cpp
  - id: openwiki-source-b441330c759fe652a2df3d75
    resource: repo://src/part/part.cpp
  - id: openwiki-source-85901c6cd3589bcc0a1df2b0
    resource: repo://src/step/global_layer.cpp
  - id: openwiki-source-8d3b1576a257384e27959c3d
    resource: repo://src/step/layer/island/island_base.cpp
  - id: openwiki-source-9537a65b88b944291a31aa8d
    resource: repo://src/step/layer/layer.cpp
  - id: openwiki-source-49cb237e320e24287dd4e890
    resource: repo://src/step/layer/regions/region_base.cpp
  - id: openwiki-source-adce7a1f3cfeab8995ef38b7
    resource: repo://tests/global_layer_tests.cpp
  - id: openwiki-source-3308169b66ac9fd6252e5bb8
    resource: repo://tests/optimizer_empty_input_tests.cpp
  - id: openwiki-source-0bbbbe27c7cb5a34caf9db5a
    resource: repo://tests/path_modifier_tests.cpp
  - id: openwiki-source-7c7eaa23810381aad20aac88
    resource: repo://tests/waam_layer_z_tests.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Geometry and Toolpath Data Model

ORNLSlicer's data model deliberately separates **source geometry** from **generated manufacturing intent**. Meshes and parts describe what was loaded and how it is placed. Steps, layers, islands, regions, paths, and segments describe what will be manufactured. The slicer bridges those halves by cross-sectioning transformed meshes into planar polygon geometry, generating ordered motion, then restoring that motion to the layer's machine-space frame.

## Ownership graph

```text
SessionManager
└── Part (stable UUID, part-local settings and ranges)
    ├── root MeshBase
    ├── zero or more sub-MeshBase objects
    └── ordered StepPair list
        ├── printing Layer (ordinary layer or raft)
        │   └── IslandBase objects by IslandType
        │       └── ordered RegionBase stages
        │           └── QVector<Path> values
        │               └── QSharedPointer<SegmentBase> motions
        └── optional ScanLayer

GlobalLayer (one cross-part output layer)
└── copied StepPair handles keyed by Part UUID
    └── shares the same Layer / ScanLayer objects owned from each Part
```

A `Part` requires a root mesh, may contain submeshes, owns a sparse local settings base and layer ranges, and groups generated work into `StepPair` records. Each pair can hold one printing layer—ordinary or raft—and one scan layer at the corresponding output position. [source](repo://include/part/part.h#L23-L36) [source](repo://include/part/part.h#L142-L180) [source](repo://include/part/part.h#L205-L269)

Appending or prepending a `Step` creates a new pair and places it in the slot implied by `StepType`; adding a scan later fills the scan slot of an existing pair. Asking for all steps flattens every pair in printing-then-scan order. The pair count, rather than the raw number of `Step` objects, is the layer-indexing unit used by the slicer. [source](repo://src/part/part.cpp#L295-L364) [source](repo://src/part/part.cpp#L460-L493)

`GlobalLayer` is a cross-part coordination view. It stores a copy of each part's `StepPair` under that part's UUID; those pair copies retain shared pointers to the original layers. Consequently, ordering, modifier, dirty-bit, and output operations performed through a global layer affect the same layer objects held by the parts. [source](repo://src/step/global_layer.cpp#L104-L108) [source](repo://src/step/global_layer.cpp#L496-L509)

## Mesh state and part transforms

`MeshBase` stores three vertex/face representations:

- original import geometry;
- aligned geometry, which is the baseline after axis alignment;
- current geometry, rebuilt from the aligned baseline whenever a transformation is set.

It also tracks the current transformation, replayable transformation history, import unit, mesh role, dimensions, and axis-aligned bounds. Concrete `ClosedMesh` and `OpenMesh` implementations supply topology-specific conversion and intersection behavior behind this interface. [source](repo://include/geometry/mesh/mesh_base.h#L27-L89) [source](repo://include/geometry/mesh/mesh_base.h#L222-L289)

`MeshBase::setTransformation()` does not compound a matrix onto already transformed vertices. It resets current vertices/faces from the aligned copies, applies the supplied matrix, rebuilds the concrete geometry representation, and recomputes bounds. Replaying a saved transformation list deliberately restores the supplied list afterward so a load/save cycle does not duplicate rotation history. [source](repo://src/geometry/mesh/mesh_base.cpp#L74-L98) [source](repo://src/geometry/mesh/mesh_base.cpp#L106-L129)

At part scope, a transformation is propagated to the root and every submesh and marks all existing steps dirty. Moving a settings mesh is broader: because it can affect paths on any build part, the operation marks every session part dirty. [source](repo://src/part/part.cpp#L255-L275)

Mesh roles are behavioral, not merely visual labels. Planar preprocessing partitions build, clipping, and settings parts before it creates layers; other pipelines select support meshes separately. Code that adds another role must update every role-based filter, not just rendering. [source](repo://src/slicing/preprocessor.cpp#L17-L26)

## Units and points

The unit system encodes physical dimensions in the `Unit` template type and performs conversions through typed constants. The internal distance base is one micron; `mm` is 1,000 internal units and `in` is 25.4 mm. [source](repo://include/units/unit.h#L18-L63) [source](repo://include/units/unit.h#L630-L653)

`Point` stores three `float` components. Constructing a point from `Distance` copies the unit object's internal numeric value, while conversions expose `Distance2D`, `Distance3D`, Qt, Clipper, and CGAL forms. Therefore a raw `Point(1, 0, 0)` means one internal distance unit, not one millimeter; code at API boundaries should prefer explicit expressions such as `1.0 * mm`. [source](repo://include/geometry/point.h#L23-L74) [source](repo://src/geometry/point.cpp#L27-L49)

## Cross-section geometry and coordinate frames

The lightweight 2D geometry types have separate semantics:

| Type | Meaning |
| --- | --- |
| `Polyline` | Ordered points forming an open path; supports length, reversal, simplification, clipping, and closing. |
| `Polygon` | One closed boundary; orientation distinguishes outer and hole conventions for area operations. |
| `PolygonList` | Multiple boundaries with even/odd containment, Boolean operations, offsets, and split-into-part behavior. |
| `SettingsPolygon` | A `PolygonList` cross-section paired with a local `SettingsBase`. |

`PolygonList::splitIntoParts()` returns connected groups whose first polygon is the outline and remaining polygons are holes; planar preprocessing uses those groups as islands. [source](repo://include/geometry/polyline.h#L18-L23) [source](repo://include/geometry/polygon.h#L18-L44) [source](repo://include/geometry/polygon_list.h#L20-L29) [source](repo://include/geometry/polygon_list.h#L108-L114)

The triangle cross-sectioner evaluates each face's vertices against the slicing plane and emits an intersection segment only for a genuine face crossing. Before polygon assembly, it translates around a plane midpoint and rotates the slicing normal onto +Z so subsequent Clipper operations happen in a flat 2D frame. It returns assembled polygons and a normalized average of intersected-face normals. [source](repo://src/cross_section/cross_section.cpp#L42-L88) [source](repo://src/cross_section/cross_section.cpp#L148-L184)

Every `Step` retains the slicing plane and shift alongside its flattened `PolygonList`, settings, typed islands, and dirty bit. A newly constructed step is dirty; comparing a replacement settings base marks it dirty when JSON differs. [source](repo://include/step/step.h#L42-L101) [source](repo://include/step/step.h#L131-L159) [source](repo://src/step/step.cpp#L18-L60)

`Layer::unorient()` and `reorient()` make that frame transition explicit. Existing clean paths are brought back into the flat optimization frame before cross-part ordering; afterward all paths receive the saved origin correction, plane rotation, and orientation shift. The reorientation pass also enforces the minimum printable Z and redirects clearance moves consistently with the slicing normal. [source](repo://src/step/layer/layer.cpp#L486-L525)

Machine semantics affect that restoration. Wire-arc paths represent the deposition surface, so the layer subtracts half the current layer height along the slicing normal; ordinary deposition retains the top-of-layer convention. Focused tests verify the 0, 2.8, 5.6 mm WAAM sequence, variable-layer-height handling, a non-WAAM 2.8 mm top reference, and adjustment along a tilted slicing normal. [source](repo://src/step/layer/layer.cpp#L410-L434) [source](repo://tests/waam_layer_z_tests.cpp#L90-L132)

## From layer geometry to segments

`Layer::compute()` asks each island to compute and then lets polymer islands reorder their region stages. An island passes its geometry through regions sequentially: each region receives the polygons left by the previous region, computes its paths, and returns the remaining geometry to the next stage. This is why perimeter, inset, skin, infill, skeleton, support, and adhesion regions are stages over one island rather than sibling copies of the original outline. [source](repo://src/step/layer/layer.cpp#L151-L158) [source](repo://src/step/layer/island/island_base.cpp#L171-L183)

The lower toolpath hierarchy is intentionally mixed value/shared ownership:

- an island owns shared region objects;
- a region owns `Path` values and its current/uncut polygon geometry;
- a `Path` owns an ordered list of shared polymorphic segments;
- each `SegmentBase` owns start/end points and a shared settings base.

Changing a layer settings pointer propagates it through islands to regions, but segment settings are populated when paths are created and may contain narrower spatial or modifier-specific values. [source](repo://src/step/layer/layer.cpp#L364-L369) [source](repo://src/step/layer/island/island_base.cpp#L185-L198) [source](repo://src/step/layer/regions/region_base.cpp#L67-L120) [source](repo://include/geometry/path.h#L18-L34) [source](repo://include/geometry/segment_base.h#L200-L208)

`Path` provides ordered mutation, reversal, transforms, length variants, travel removal, closure metadata, and optional replacement of compatible contiguous line runs with arcs. Reversing a path reverses both segment order and each segment's direction; arc implementations additionally reverse their angular direction. [source](repo://src/geometry/path.cpp#L247-L284) [source](repo://src/geometry/path.cpp#L339-L388) [source](repo://include/geometry/segments/arc.h#L61-L72)

The main motion classes are `LineSegment`, `ArcSegment`, and `TravelSegment`, each responsible for its own length/geometry and writer dispatch. “Printing” is a semantic predicate, not simply “not a travel”: `SegmentBase::isPrintingSegment()` also rejects deposition-disabled segments and coasting, tip-wipe, or spiral-lift modifiers. [source](repo://include/geometry/segments/line.h#L13-L36) [source](repo://include/geometry/segments/arc.h#L14-L72) [source](repo://include/geometry/segments/travel.h#L12-L39) [source](repo://src/geometry/segment_base.cpp#L21-L30) [source](repo://src/geometry/segment_base.cpp#L160-L169)

This predicate is used when computing deposition-only length, finding printable minima, and performing multi-material transitions. New segment classes must implement cloning, writer dispatch, minimum Z, and any extra geometry transforms such as an arc center or cylindrical bead axis.

## Cross-part ordering and invariants

`GlobalLayer::connectPaths()` is where per-part layers become one machine sequence. It installs each layer's optimization frame on its islands, orders scan layers and parts, applies skirt/raft/brim/support/polymer precedence, invokes island/region optimizers, and inserts travels while carrying the current point and previous regions forward. [source](repo://src/step/global_layer.cpp#L184-L228) [source](repo://src/step/global_layer.cpp#L285-L399)

Island-order settings follow a compatibility rule. If all orderable layers agree on the method—and on the effective projected custom anchor when custom ordering is selected—the global layer may use a representative layer's settings. Conflicting methods or effective anchors fall back to the global settings and an anchor layer. Raw per-part shifts are not compared directly; the projected anchor is. [source](repo://src/step/global_layer.cpp#L33-L100) [source](repo://src/step/global_layer.cpp#L200-L208)

The focused global-layer tests pin down that behavior: common layer settings win, horizontally shifted frames with the same effective custom anchor remain compatible, differing raw custom coordinates can remain compatible after projection, and actual setting/frame conflicts use global fallback. [source](repo://tests/global_layer_tests.cpp#L88-L219)

After ordering, modifiers can rewrite paths. Global-layer modifiers add lead-ins and spiral lifts and then fit circular arcs; region logic can split segments for exact multi-material transition distances. Modifier code resolves segment-local settings before broader fallback settings, and the sharp-corner tests verify both continuity after insertion and that local segment settings can enable a modifier disabled in the fallback base. [source](repo://src/step/global_layer.cpp#L130-L181) [source](repo://src/step/layer/regions/region_base.cpp#L205-L261) [source](repo://src/geometry/path_modifier.cpp#L77-L116) [source](repo://tests/path_modifier_tests.cpp#L30-L93)

Optimizers must be total over degenerate inputs. Focused tests require an empty island set to return `-1`, empty paths and one-point polylines to be filtered, and zero-distance but structurally valid paths/polylines to be consumed so an optimization loop still makes progress. [source](repo://tests/optimizer_empty_input_tests.cpp#L139-L196)

Finally, `GlobalLayer` assumes co-planar constituent layers have one common layer height, emits scans before ordered deposition islands, writes an empty step only when neither exists, and can clear dirty bits through the shared step handles after output. [source](repo://src/step/global_layer.cpp#L440-L493) [source](repo://src/step/global_layer.cpp#L496-L529)

## Safe extension checklist

- Keep imported/aligned/current mesh representations distinct; transformations must remain replayable and must refresh concrete acceleration/topology structures and bounds.
- Mark affected steps dirty whenever geometry or spatial settings change. A settings mesh can invalidate every build part.
- Preserve the flattened optimization frame until cross-part ordering and path connection are complete, then reorient exactly once.
- Attach final effective settings to new segments. Later modifiers and writers read segment-local width, height, speed, material, region, and modifier metadata.
- Preserve path continuity when inserting, splitting, reversing, or fitting segments; clone settings when a new segment must diverge safely.
- Handle empty and degenerate collections explicitly so order optimizers either return a sentinel or consume an item and cannot loop forever.
