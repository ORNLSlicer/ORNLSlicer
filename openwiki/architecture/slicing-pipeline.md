---
type: architecture
title: Slicing Pipeline Orchestration
description: End-to-end orchestration of slicer selection, preprocessing, parallel step computation, postprocessing, output generation, progress, cancellation, and completion.
tags: [slicing, pipeline, threading, cancellation, progress]
sources:
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-29d5dbfa478667e089493f38
    resource: repo://src/slicing/preprocessor.cpp
  - id: openwiki-source-e6313e383d53e930ab9c33c4
    resource: repo://src/slicing/slicing_utilities.cpp
  - id: openwiki-source-b1134fc13dd24912e260b947
    resource: repo://src/threading/abs_slicing_thread.cpp
  - id: openwiki-source-74cea8ba6a71c48d8439dfdc
    resource: repo://src/threading/slicers/cylindrical_slicer.cpp
  - id: openwiki-source-e7dd70d203530610f141b507
    resource: repo://src/threading/slicers/image_slicer.cpp
  - id: openwiki-source-1d02fec2dd72ccb1ea3b4c51
    resource: repo://src/threading/slicers/planar_slicer.cpp
  - id: openwiki-source-2b373e298658f39ccac3e9f6
    resource: repo://src/threading/step_thread.cpp
  - id: openwiki-source-9f10ba488daafec662ecc53a
    resource: repo://src/threading/traditional_ast.cpp
generated: { by: "codex", at: "2026-11-10T17:47:51.624Z" }
verified:
  - by: openwiki/0.7.2
    at: 2026-11-10T17:47:51.624Z
---

# Slicing Pipeline Orchestration

Slicing begins in `SessionManager`, but the work runs in a mode-specific `AbstractSlicingThread`. Planar and image modes use the common `TraditionalAST` lifecycle; cylindrical mode overrides that lifecycle because radial and helical paths are generated directly rather than through the ordinary queue of dirty `Step` objects.

```text
SessionManager::doSlice
  ├── validate mode/syntax
  ├── choose or reuse slicer
  ├── request current model transforms
  └── emit startSlice
        │ queued to slicer's internal QThread
        ▼
  Planar / Image: TraditionalAST::doSlice
      preprocess → dirty Step queue → postprocess → optional G-code → sliceComplete

  Cylindrical: CylindricalSlicer::doSlice
      generate radial or helical layers → modifiers → G-code → sliceComplete
        │
        ▼
SessionManager::sliceComplete → forwardSliceComplete(temporary path, alterFile=true)
```

## Dispatch and early validation

`SessionManager::doSlice()` reads the active syntax and slicing mode, derives the temporary output suffix from the syntax metadata, and either constructs a new slicer or refreshes the existing slicer's output file. Before dispatch it requests the GUI's current transforms so slicing observes the latest model placement. The `startSlice` signal is connected to the selected slicer's `doSlice()` slot. [source](repo://src/managers/session_manager.cpp#L592-L660) [source](repo://src/managers/session_manager.cpp#L717-L750)

Mode selection has three concrete branches:

| Mode | Concrete slicer | Orchestration |
| --- | --- | --- |
| Planar | `PlanarSlicer` | `TraditionalAST` preprocessing, parallel dirty-step computation, planar postprocessing, G-code |
| Image | `ImageSlicer` | `TraditionalAST`, but image generation happens during preprocessing and G-code is skipped |
| Cylindrical | `CylindricalSlicer` | Direct radial or helical layer generation, then modifiers and G-code |

An unknown mode logs a warning and falls back to planar. Changing modes also clears every part's existing steps before reconnecting progress, status-message, and completion signals. [source](repo://src/managers/session_manager.cpp#L717-L750)

Starting-layer validation and nonpositive active helical-region speeds are rejected in manager preflight. [source](repo://src/managers/session_manager.cpp#L592-L638) Two further validation paths stop before normal completion. A request with no loaded parts returns from the slicer after a warning and does not emit `sliceComplete`. A cylindrical request with a syntax other than Arc Specialties is rejected earlier by `SessionManager`, which emits a user-facing message and returns `false` without dispatching work. [source](repo://src/threading/traditional_ast.cpp#L24-L28) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L423-L427) [source](repo://src/managers/session_manager.cpp#L601-L607)

## Thread and state boundaries

`AbstractSlicingThread` is a `QObject`, not a `QThread` subclass. Its constructor opens/configures the output and moves the slicer object onto an owned internal `QThread`; destruction quits and joins that thread. Bounds, writer, syntax, temporary file, elapsed time, maximum step count, and the cancellation flag live on this slicer object. [source](repo://src/threading/abs_slicing_thread.cpp#L179-L191) [source](repo://include/threading/abs_slicing_thread.h#L121-L157)

For traditional slicing, each `StepThread` is another `QObject` with its own thread. It holds one shared `Step`, invokes `Step::compute()`, and emits `completed`; `stop()` queues a thread quit and waits when called from another thread. [source](repo://src/threading/step_thread.cpp#L12-L40)

The resulting ownership model is:

- `SessionManager` owns the active slicer through `QSharedPointer<AbstractSlicingThread>`.
- The slicer owns its lifecycle thread and, while computing, raw `StepThread` workers.
- Parts continue to own their shared layer/step objects; workers receive `QSharedPointer<Step>` handles.
- Progress and completion cross thread boundaries through Qt signals.

## Common traditional lifecycle

`TraditionalAST::doSlice()` resets its queue and workers, starts the elapsed timer, clears the maximum-step count, and calls the mode's `preProcess()` on the slicer thread. It then counts all part step pairs, creates up to `QThread::idealThreadCount()` workers, and queues every distinct dirty step except steps belonging to clipping parts. [source](repo://src/threading/traditional_ast.cpp#L24-L70)

Initial workers each receive one queued step before a broadcast `stepStart`. As workers finish, `cleanThread()` either destroys an unneeded worker or assigns the next queued step back to that worker. Compute progress is derived from queue depletion. Only after the queue is empty and every worker has completed and been destroyed does the pipeline enter postprocessing. [source](repo://src/threading/traditional_ast.cpp#L72-L114) [source](repo://src/threading/traditional_ast.cpp#L127-L183)

If preprocessing creates no dirty steps, the pipeline reports compute as complete and proceeds synchronously without allocating workers. This is the normal route for image slicing and can also occur when planar steps are already clean. [source](repo://src/threading/traditional_ast.cpp#L46-L57) [source](repo://src/threading/traditional_ast.cpp#L88-L107)

After postprocessing, a traditional slicer writes output only when `m_skip_gcode` is false:

1. `writeGCodeSetup()` writes the slicer/settings headers and writer-specific initial setup.
2. The mode's `writeGCode()` serializes its ordered layers and paths.
3. `writeGCodeShutdown()` writes shutdown and settings footer content, optionally annotates layers with estimated times, flushes, and closes the file.
4. A final cancellation check gates `sliceComplete`.

The setup derives XY bounds and maximum Z from all non-clipping parts. Layer-time comments are a best-effort post-pass: parser, seek, or rewrite failures log warnings and leave slicing completion intact. [source](repo://src/threading/abs_slicing_thread.cpp#L290-L369) [source](repo://src/threading/abs_slicing_thread.cpp#L372-L434)

## Planar mode

Planar preprocessing uses `Preprocessor` as a callback-driven traversal over build, clipping, and settings parts. For each build part it copies global settings, overlays part settings, clones each source mesh, applies mesh processing, and feeds the mesh through `BufferedSlicer`. Each cross-section becomes a `Layer`; its disconnected polygon components become polymer islands, and settings-region polygons are retained with that layer. [source](repo://src/slicing/preprocessor.cpp#L17-L94) [source](repo://src/threading/slicers/planar_slicer.cpp#L43-L150)

The planar callbacks also clip build meshes, add skin/support and enabled raft, brim, skirt, laser-scan, and thermal-scan structures, update the maximum step count, and finally assemble cross-part `GlobalLayer` objects. The implementation currently clears each part's steps on every run, despite retaining dirty-step machinery elsewhere. [source](repo://src/threading/slicers/planar_slicer.cpp#L59-L74) [source](repo://src/threading/slicers/planar_slicer.cpp#L152-L189)

During the compute phase, each dirty layer step generates its regions and paths. Planar postprocessing then works across global layers: it temporarily unorients each layer, connects paths using the ending state of the previous layer, calculates modifiers, and restores orientation. The writer serializes global layers in order and clears each layer's dirty bit after writing. [source](repo://src/threading/slicers/planar_slicer.cpp#L967-L1023) [source](repo://src/threading/slicers/planar_slicer.cpp#L1026-L1050)

The preprocessor's callbacks use `true` to stop traversal. Planar settings conflicts now have an explicit failure path: each slice intersects settings polygons with the actual build cross-section, records the conflicting objects and slice, then calls `failPreProcess`. That clears part steps, output and standalone programs and emits `sliceFailed`; `TraditionalAST` checks the failure flag before starting workers or completing. [source](repo://src/threading/slicers/planar_slicer.cpp#L77-L94) [source](repo://src/threading/slicers/planar_slicer.cpp#L208-L212) [source](repo://src/threading/traditional_ast.cpp#L40-L45) [source](repo://src/threading/traditional_ast.cpp#L116-L124)

For each target build part, settings and clipping selection includes enabled root modifiers and enabled direct children of that part. A child modifier does not affect another build object. Planar processing passes that target into both clipping and buffered settings selection; cylindrical preprocessing uses the same targeted selection. [source](repo://src/slicing/slicing_utilities.cpp#L157-L174) [source](repo://src/slicing/preprocessor.cpp#L49-L66) [source](repo://src/threading/slicers/planar_slicer.cpp#L68-L72) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L486-L499)

## Cylindrical and helical mode

Helical slicing is not a fourth slicer. It is a `CylindricalPathPattern` handled inside `CylindricalSlicer`, alongside radial paths. The cylindrical constructor pins the Arc Specialties writer, and its `doSlice()` bypasses `TraditionalAST`'s `StepThread` queue: it runs preprocessing, postprocessing, and writing directly with cancellation checks between phases. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L418-L456)

Preprocessing copies global and part settings, clones and clips build meshes, computes their bounds, and dispatches each part to either `generateHelicalLayers` or `generateRadialLayers`. These routines directly create `CylindricalLayer` objects and their paths while reporting both preprocessing and compute progress across parts. Effective helical rounding and handedness values are passed to the Arc Specialties writer for output metadata. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L459-L564)

If no printable cylindrical paths are produced, the slicer emits a warning identifying the selected pattern and relevant boundary setting, but it does not fail the slice. Postprocessing either immediately reports 100% for the empty case or calculates modifiers layer by layer. G-code is then written in cylindrical-layer order. [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L566-L580) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L887-L929)

## Standalone layer output

Standalone generation is opt-in through Individual Layer Export and skipped for Adamantine. Planar and cylindrical writers use a fresh machine writer for each layer and retain complete programs in memory, separate from continuous G-code. `writeStandaloneLayer` supplies isolated startup, layer entry and shutdown behavior. Filenames use `WriterBase::layerNumber` with the configured start; the common settings footer is added to the last standalone program. Export snapshots and file writing belong to the downstream [G-code export contract](gcode-and-visualization.md#individual-layer-programs). [source](repo://src/threading/abs_slicing_thread.cpp#L346-L418) [source](repo://src/threading/slicers/planar_slicer.cpp#L1026-L1050) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L909-L929)

`writeGCodeFile` clears prior snapshots, writes setup/body/shutdown, and records whether the resulting ARC program requires a weld-schedule companion. An invalid-argument failure truncates/closes output, clears standalone programs and emits `sliceFailed` instead of success. Resetting output and consuming cancellation also clear export state. [source](repo://src/threading/abs_slicing_thread.cpp#L284-L309) [source](repo://src/threading/abs_slicing_thread.cpp#L327-L343)

## Image mode

`ImageSlicer` passes `skipGcode = true` to `TraditionalAST`. Its preprocessing is the product: it selects build and support parts, copies their root meshes, applies build offsets, orders build meshes deterministically by rounded Z/Y/X minima, assigns numeric material/object IDs, and reserves `65535` for support. [source](repo://src/threading/slicers/image_slicer.cpp#L59-L134)

It constructs all or explicitly requested slicing planes, then uses a dynamic batch counter and `std::jthread`s to cross-section and render those planes as zero-padded PNG files in the output directory. It also writes `idFileLinks.dat`, mapping emitted IDs to original model names. [source](repo://src/threading/slicers/image_slicer.cpp#L136-L178) [source](repo://src/threading/slicers/image_slicer.cpp#L192-L234) [source](repo://src/threading/slicers/image_slicer.cpp#L307-L335)

Because image preprocessing creates files directly and does not populate ordinary part steps, `TraditionalAST` takes its zero-step branch. `postProcess()` and `writeGCode()` only report completion, and the latter is not called because G-code is skipped. [source](repo://src/threading/slicers/image_slicer.cpp#L337-L343) [source](repo://src/threading/traditional_ast.cpp#L88-L107)

## Progress, cancellation, and completion

The shared progress vocabulary is `PreProcess`, `Compute`, `PostProcess`, and `GcodeGeneraton`. Mode code emits phase percentages; `AbstractSlicingThread::forwardStatus` exposes the signal, and `SessionManager` forwards it to the UI. Cylindrical mode synthesizes preprocessing and compute percentages during its direct path-generation loops rather than from worker completion. [source](repo://include/threading/abs_slicing_thread.h#L51-L74) [source](repo://src/managers/session_manager.cpp#L677-L683) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L476-L495)

Cancellation is cooperative. `SessionManager::cancelSlice()` sets a boolean on the active slicer. `shouldCancel()` consumes and resets that flag and closes the temporary G-code file. Traditional slicing checks it after preprocessing, when each worker reports completion, after postprocessing, and after writing; an already-running `Step::compute()` is not interrupted, and worker shutdown waits for its callback to return. [source](repo://src/managers/session_manager.cpp#L681-L683) [source](repo://src/threading/abs_slicing_thread.cpp#L299-L309) [source](repo://src/threading/traditional_ast.cpp#L80-L106) [source](repo://src/threading/traditional_ast.cpp#L127-L135)

On successful completion, the slicer emits `sliceComplete`; `SessionManager` forwards its temporary path with `alterFile = true`. That handoff begins the downstream G-code parse/visualization and minimum-layer-time workflow for toolpath modes. Image mode uses the output path primarily as a directory anchor for its PNG set and companion map. [source](repo://src/managers/session_manager.cpp#L671-L674) [source](repo://src/threading/abs_slicing_thread.cpp#L317-L324)

## Extension checklist

- Add a new `SlicingMode` branch in `SessionManager::changeSlicer()` and decide explicitly whether the mode fits `TraditionalAST` or needs a direct lifecycle.
- Ensure every early rejection has a user-visible status and a defined completion policy; a plain return leaves callers waiting unless they already handled the `false` result.
- Keep long operations cancellation-aware. A phase-boundary check cannot interrupt work inside one long compute or rendering callback.
- Emit monotonic progress for every phase the UI presents, including empty-work cases.
- Write products under the configured temporary output location and emit `sliceComplete` only after files are flushed and stable for the next consumer.
- Preserve the distinction between a warning with usable output, such as no cylindrical paths, and a pre-dispatch failure, such as an incompatible syntax.
