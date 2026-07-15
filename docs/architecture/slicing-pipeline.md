# Slicing Pipeline

This page describes slicer selection, geometry preprocessing, toolpath
computation, the path data model, and G-code handoff. See the
[architecture overview](../../ARCHITECTURE.md) for the surrounding application
and [G-code and Visualization](gcode-and-visualization.md) for the downstream
writer/parser path.

## Slicer Selection

[`SessionManager::doSlice()`](../../src/managers/session_manager.cpp) reads the
active settings, validates mode-specific constraints, chooses the temporary
output suffix, and emits `startSlice` to the active slicer.

| `SlicingMode` | Additional Selection | Implementation | Output Model |
| --- | --- | --- | --- |
| `kPlanar` | None | `PlanarSlicer` | Cross-sectioned layers with islands and regions |
| `kCylindrical` | `CylindricalPathPattern::kRadial` | `RadialSlicer` | Concentric radial paths stored directly on `RadialLayer` |
| `kCylindrical` | `CylindricalPathPattern::kHelical` | `HelicalSlicer` | Rising helical paths stored directly on `HelicalLayer` |
| `kImage` | None | `ImageSlicer` | Image slices rather than ordinary machine G-code |

Cylindrical slicing currently requires the Arc Specialties G-code syntax. The
guard lives in `SessionManager::doSlice()`, and the cylindrical slicers select
`ArcSpecialtiesWriter` directly. User-facing cylindrical behavior is documented
in [Cylindrical Slicing](../cylindrical-slicing.md).

`SessionManager::changeSlicer()` replaces the slicer when the mode or
cylindrical pattern changes, clears existing part steps, and reconnects
the start trigger plus progress/status and completion signals. Cancellation is
requested separately through `SessionManager::cancelSlice()`, which calls the
active slicer's `setCancel()` method.

## Pipeline Phases

The concrete slicers derive from
[`TraditionalAST`](../../include/threading/traditional_ast.h), which derives from
[`AbstractSlicingThread`](../../include/threading/abs_slicing_thread.h).
`AbstractSlicingThread` is a `QObject` moved to an internal `QThread`; it owns
the cancellation flag, output file, selected writer, bounds, and common G-code
setup/shutdown behavior.

```mermaid
flowchart LR
    Select["Select slicer and writer"] --> Pre["preProcess()"]
    Pre --> Queue["Queue dirty Step objects"]
    Queue --> Compute["StepThread: Step::compute()"]
    Compute --> Post["postProcess()"]
    Post --> Skip{"skip G-code?"}
    Skip -->|"no"| Setup["writeGCodeSetup()"]
    Setup --> Body["Concrete writeGCode()"]
    Body --> Shutdown["writeGCodeShutdown()"]
    Shutdown --> Complete["sliceComplete"]
    Skip -->|"yes"| Complete
```

### Preprocess

Toolpath slicers convert meshes and effective settings into their `Step` model.
Image slicing writes image stencils during preprocessing without populating the
ordinary toolpath-step hierarchy.

Planar slicing uses [`Preprocessor`](../../src/slicing/preprocessor.cpp) and
[`BufferedSlicer`](../../src/slicing/buffered_slicer.cpp). `Preprocessor`
provides ordered hooks around the whole-part cross-sectioning loop:

1. initial processing across all build, clipping, and settings parts;
2. per-part setup and effective settings construction;
3. per-mesh copying and clipping;
4. step construction from each `BufferedSlicer::SliceMeta` result;
5. post-cross-section processing after each mesh's slice loop;
6. a status update after all meshes for one part have been processed;
7. final processing after all parts have steps.

`PlanarSlicer` builds `Layer` objects, splits layer geometry into polymer
islands, and adds optional support, raft, brim, skirt, laser-scan, and
thermal-scan structures. It then uses `LayerOrderOptimizer` to group compatible
steps from multiple parts into `GlobalLayer` objects.

Radial and helical slicers have specialized preprocessors. They copy and clip
meshes, compute horizontal cross-sections, generate circular or helical
polylines against those sections, and store resulting `Path` objects directly
on their specialized layers. They intentionally do not create polymer islands
or regions.

Image slicing owns its image-generation preprocessing and does not use the
ordinary machine G-code body.

### Compute

[`TraditionalAST::doSlice()`](../../src/threading/traditional_ast.cpp) collects
dirty steps from every non-clipping part. It creates up to
`QThread::idealThreadCount()`
[`StepThread`](../../src/threading/step_thread.cpp) workers, assigns one step to
each worker, and feeds the next queued step to a worker when it emits
`completed`.

Each `StepThread` worker calls only `Step::compute()`. In the common planar and
scan hierarchy, computation therefore belongs in the step, island, region, and
pathing types, not in a GUI callback. `TraditionalAST` owns queue progress and
decides when it is safe to start postprocessing.

The specialized modes are intentional exceptions to that common hierarchy.
`RadialLayer::compute()` and `HelicalLayer::compute()` are currently no-ops
because their paths are generated during preprocessing and modified later.
Image slicing creates no `Step` objects; it generates VTK image stencils during
preprocessing with its own `std::jthread` worker pool.

### Postprocess

Postprocessing performs work that depends on computed paths or cross-step
ordering.

- Planar `GlobalLayer` objects have already been assembled and ordered by
  `LayerOrderOptimizer` in the preprocessor's final hook. Postprocessing
  unorients each global layer, orders and connects its islands/paths and
  travels, applies path modifiers, and restores printer coordinates.
- Radial and helical slicing apply modifiers over their directly owned paths
  while preserving the current tool location between layers.
- Concrete slicers emit postprocess progress and honor the shared cancellation
  flag before output begins.

### Write

For slicers that do not set `m_skip_gcode`, `AbstractSlicingThread` writes the
file header, settings header, and initial machine setup through its selected
`WriterBase`. The concrete slicer then walks its ordered steps or global layers
and delegates segment output to the writer. The base class appends shutdown and,
where supported, the settings footer.

The completed temporary file is returned to `SessionManager`, then normally
sent to `GCodeLoader` for parsing, visualization, statistics, possible
layer-time adjustment, and export.

## Settings Scope by Mode

The slicers do not all consume the same override scopes.

| Mode | Effective Settings Scope |
| --- | --- |
| Planar | Adjusted global copy, part overrides, matching layer ranges, local adjustments, and spatial settings polygons |
| Radial/helical | Adjusted global copy, part overrides, and per-layer local adjustments; layer ranges and settings-part polygons are not currently applied |
| Image | Active global values read directly while generating image stencils |

When adding an override mechanism, trace the concrete slicer's preprocessing
instead of assuming the planar settings path is shared by every mode.

## Toolpath Data Model

[`Part`](../../include/part/part.h) owns meshes for geometry and `StepPair`
groups for computed output. A `StepPair` can associate a printing `Layer` with a
corresponding `ScanLayer` at the same sequence position.

The common planar toolpath hierarchy is:

```text
Part
└── StepPair
    ├── Layer (printing)
    │   └── IslandBase
    │       └── RegionBase
    │           └── Path
    │               └── SegmentBase
    └── ScanLayer (optional)
        └── IslandBase
            └── RegionBase
                └── Path
                    └── SegmentBase
```

Responsibilities are deliberately layered:

- [`Step`](../../include/step/step.h) is a computable and writable unit with
  settings, geometry, orientation, and dirty state.
- [`Layer`](../../include/step/layer/layer.h) owns islands, inter-island order,
  travel connection, modifiers, and orientation restoration.
- [`IslandBase`](../../include/step/layer/island/island_base.h) groups regions
  for one connected area and coordinates region computation/optimization.
- [`RegionBase`](../../include/step/layer/regions/region_base.h) implements a
  pathing purpose such as perimeter, inset, skin, infill, skeleton, support, or
  scan behavior and owns the resulting paths.
- [`Path`](../../include/geometry/path.h) is an ordered collection of shared
  `SegmentBase` objects.
- [`SegmentBase`](../../include/geometry/segment_base.h) and its line, arc,
  travel, scan, and other subclasses carry geometric and process data and
  delegate machine text to the writer.

`GlobalLayer` is not a `Step`. It is a planar cross-part aggregate that groups
`StepPair` objects for ordering, postprocessing, and G-code generation.

`RadialLayer` and `HelicalLayer` derive through `Layer`, but their specialized
implementations own `Path` objects directly. Do not assume every layer has an
island/region hierarchy.

## Geometry and Coordinates

- `geometry/mesh/` owns closed/open mesh representations and mesh utilities.
- `cross_section/` turns meshes and planes into stitched polygon geometry.
- `geometry/` owns points, polylines, polygons, polygon lists, paths, planes,
  and common path modification utilities.
- `slicing/` coordinates preprocessing, buffered cross-sectioning, step
  additions, and shared slicer utilities.
- `optimizers/` owns ordering algorithms at layer, island, path, polyline, and
  point levels.

Planar layers may be flattened for path computation and later reoriented into
printer coordinates. Preserve the existing orientation, shift, and minimum-Z
restoration stages when adding geometry operations. Applying an operation in
the wrong coordinate space commonly produces correct-looking 2-D paths at the
wrong 3-D location.

## Extending the Pipeline

### Add or Change a Slicing Mode

1. Define or reuse the mode and any submode in `utilities/enums.h` and settings
   metadata.
2. Implement a concrete `TraditionalAST` or `AbstractSlicingThread` subclass
   with preprocess, postprocess, and output behavior.
3. Route it through `SessionManager::changeSlicer()` and define mode-specific
   validation in `doSlice()`.
4. Establish its step model explicitly: common islands/regions or a documented
   direct-path specialization.
5. Forward status and completion through the shared base, and honor direct
   `setCancel()` requests and the shared cancellation flag.

### Add a Region or Island

1. Derive from `RegionBase` or `IslandBase` under `step/layer/`.
2. Add the enum/string mappings used for ordering, comments, and
   visualization.
3. Construct the type during preprocessing or a `LayerAdditions` stage.
4. Implement computation, optimization, modifiers, and writer delegation.
5. Confirm that settings-region and layer-range overrides reach the new type.

### Add a Segment or Modifier

Keep geometry and process metadata on the segment, and keep syntax formatting
on `WriterBase` implementations. A new segment must be handled by generation,
ordering/modification code, G-code writing, parsing where applicable, and the
preview path. See [G-code and Visualization](gcode-and-visualization.md) before
adding syntax-specific behavior to a geometry class.

## Invariants to Preserve

- The slicer and each `StepThread` own separate worker threads; GUI state stays
  on the GUI thread.
- Build, clipping, support, and settings parts have different preprocessing
  roles. Do not treat every loaded part as printable geometry.
- In toolpath modes, effective settings are copied before supported local
  adjustments; avoid mutating the global settings object from a region
  algorithm. Check the mode-specific scope before relying on an override.
- Cross-part ordering belongs to `GlobalLayer`/`LayerOrderOptimizer`, not to a
  single part's `Layer`.
- Not every layer uses islands and regions; check the concrete slicer before
  relying on that hierarchy.
- Progress/status and completion are forwarded through slicer signals;
  cancellation is a direct `SessionManager` request to the active slicer.
