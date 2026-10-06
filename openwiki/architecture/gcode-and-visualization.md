---
type: architecture
title: G-Code, Parsing, Visualization, and Export
description: End-to-end guide to ORNLSlicer's generated and imported G-code pipeline, including parser selection, layer-time adjustment, visualization segments, statistics, and export artifacts.
tags: [gcode, parsing, visualization, export, qt]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-a2768c251f812c51a0a8788f
    resource: repo://include/gcode/as_printed_model_exporter.h
  - id: openwiki-source-95107d4ff5d6d52fa1e825b7
    resource: repo://include/gcode/gcode_meta.h
  - id: openwiki-source-9130512ad0c7d187b1aad05c
    resource: repo://include/graphics/objects/gcode_object.h
  - id: openwiki-source-a68e0b041dc4b591977503bd
    resource: repo://src/gcode/as_printed_model_exporter.cpp
  - id: openwiki-source-94d7720f109e6629e985b28d
    resource: repo://src/gcode/parsers/common_parser.cpp
  - id: openwiki-source-ceebec80a0c2f9b8339a0fb0
    resource: repo://src/graphics/objects/gcode_object.cpp
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-4c1d7b6308a78ba9c493ce8a
    resource: repo://src/threading/gcode_loader.cpp
  - id: openwiki-source-23afc284fcae4cee2de94dcb
    resource: repo://src/windows/gcode_export.cpp
  - id: openwiki-source-403d1cb683b233c4f42460d0
    resource: repo://src/windows/main_window.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# G-Code, Parsing, Visualization, and Export

G-code is a second internal representation in ORNLSlicer, not merely the final text file. Toolpath slicers write a temporary machine-specific file, and `GCodeLoader` parses that file back into commands, timing and material statistics, formatted source text, and a layered graph of display segments. Imported G-code enters at the same loader. Downstream UI and export features consume independent pieces of that parse result.

```text
Step / Layer / Island / Region / Path / Segment
                    │
                    ▼
          machine-specific WriterBase
                    │
             temporary G-code ───────── imported G-code
                    │                         │
                    └──────────┬──────────────┘
                               ▼
                          GCodeLoader
                ┌──────────────┼───────────────┐
                ▼              ▼               ▼
          source text     display segments   timing/material metadata
                │              │               │
          GcodeBar/editor  GCodeView/export  LayerTimes/export/status
```

## Metadata is the dialect contract

`GcodeMeta` is the shared description of a machine dialect. It binds a `GcodeSyntax` to comment delimiters, distance/time/angle/mass/velocity units, file suffix, travel support, and layer delimiters. Writers use this metadata to format generated text; parsers use it to interpret values and comments; `GCodeLoader` uses it to convert coordinates and statistics; and `GcodeExport` uses it to choose filenames and wrap operator notes correctly.

The active slicer selects a writer from the global machine-syntax setting. On the read side, `GCodeLoader::setParser()` scans the file for `G-CODE SYNTAX` or `GCODE SYNTAX`, then chooses a specialized parser or `CommonParser` plus the matching metadata. Several dialects intentionally share parser machinery. An unknown declared syntax falls back to common Marlin parsing and metadata; a file with no syntax declaration also attempts common Marlin parsing.

See [Machine Syntax Writers and Parsers](../integrations/machine-syntaxes.md) for the multi-site dialect extension contract.

## Loader phases

`GCodeLoader` is a `QThread`. Its constructor snapshots the active global settings and visualization colors; `run()` performs these phases:

1. Read the complete file into original-case lines and a parallel uppercase list used for matching.
2. Select the parser, then parse the header, settings footer, and commands.
3. Collect per-layer original and adjusted times, feed-rate modifiers, volume, total/travel/printing distances, and parser-provided layer-start line numbers.
4. Convert volume to mass with the configured or named material density and emit timing, export metadata, and formatted summary text.
5. Build a `QVector<QVector<QSharedPointer<SegmentBase>>>`, preserving the layer grouping.
6. Emit the visualization graph and formatted source text independently.
7. For alterable files, append calculated export comments and replace the original temporary file with the adjusted result.

The settings footer is important for imported files: it supplies the geometry and machine values needed to reconstruct widths, heights, offsets, units, and material calculations. While building visualization, the loader starts each layer with the active global settings. When exactly one part is loaded, part layer-range settings are overlaid for their covered layers; this lets generated previews retain range-specific bead geometry.

## Parsing and minimum-layer-time adjustment

`CommonParser` owns modal motion state, deposition state, layer boundaries, command construction, distance/time/volume accounting, and the original/uppercase source-line arrays. Specialized parsers configure or override that common behavior for their dialect.

At each layer boundary, the common parser can enforce the settings embedded in the file when all of the following are true:

- minimum-layer-time enforcement is enabled;
- the `GCodeLoader` was constructed with `alterFile = true`;
- the current layer has adjustable motion time; and
- the chosen policy permits the required operation.

The slow-feed-rate policy calculates a modifier and clamps it against machine minimum/maximum speeds before materializing feed-rate transitions and scaling related extrusion parameters. The dwell policy inserts time when a layer is too short but cannot reduce an overlong layer. Any edit marks the parser modified, and the loader refreshes its text from the parser's changed source lines before emitting or rewriting the file.

Generated slicer output passes `alterFile = true`, so this parse is part of final output production. Manually imported G-code passes `false`, which keeps visualization and statistics read-only. This distinction must be preserved when adding new loader call sites.

## Display segment production

Each parsed motion command becomes zero or more `SegmentBase` instances:

- line, arc, and spline command IDs choose the geometric segment type;
- dialect metadata decides whether explicit non-deposition travels are expected;
- region and path-modifier comments select display color, travel/support flags, and bead-width settings;
- current layer height and region-specific width determine true-width display geometry;
- line and layer numbers link the geometry back to source text;
- feed/extruder speed, endpoints, length, width, deposition state, and cylindrical-axis data populate inspection metadata.

Coordinates are converted from the dialect's units into view space. The loader applies X/Y/Z offsets and table motion, and has additional Arc Specialties handling for cylindrical-axis and optional current-position information. Cancellation is checked while commands and layers are converted so large previews can stop without publishing a partial segment graph.

## Independent consumers

`MainWindow::importGCodeHelper()` deliberately fans out loader signals rather than passing a monolithic result:

| Loader output | Consumer | Purpose |
| --- | --- | --- |
| `gcodeLoadedVisualization` | `GCodeWidget` / `GCodeView` | OpenGL preview, picking, segment visibility, and layer/segment ranges |
| `gcodeLoadedVisualization` | `GcodeExport` | Optional as-printed STL generation from the same parsed geometry |
| `gcodeLoadedText` | `GcodeBar` | Source display, syntax color, and layer-to-line navigation |
| Layer timing signal | `LayerTimesWindow` | Original/adjusted per-layer durations and feed-rate modifiers |
| Export information | `GcodeExport` | Temporary source path plus detected `GcodeMeta` |
| Summary/status signals | `MainWindow` and slice dialog | File, time, distance, volume, mass, progress, warnings, and errors |

This separation allows headless `MainControl` to connect only the export metadata and completion signals it needs.

## OpenGL preview and interaction

`GCodeObject` packs all segments into shared buffers rather than creating one OpenGL object per motion. It keeps per-segment metadata for hiding by type, layer and segment range filtering, source-line selection, hover highlighting, and the segment-information panel.

Printable moves may be rendered as true-width bead triangles or as lightweight lines. In automatic mode the object estimates the true-width vertex count and switches the full preview to lines when it exceeds the preference threshold. `GCodeView` can then construct a true-width overlay only for the currently visible range when that smaller subset fits the same threshold. Travel geometry uses a separate line buffer even when printed moves use bead meshes.

The G-code bar and view remain bidirectionally linked: range controls change visible geometry, while picked segments update selected source lines. A segment stores the one-based G-code line used by the view; text widgets account for their own zero-based indexing at the boundary.

## Export contracts

`GcodeExport` receives two pieces of state at different times: the temporary G-code path/dialect metadata and the parsed visualization graph. As-printed export stays disabled until at least one visualization segment has arrived, preventing an empty or stale STL.

An export can produce:

- the dialect-suffixed G-code file, with optional operator and description comments;
- an `.s2p` project copy;
- generated sensor/auxiliary files;
- an as-printed or centerline STL; and
- syntax-specific companion/simulation files through dedicated `GCode*Saver` workers.

The ordinary path reads the temporary file, prepends operator notes using the dialect's comment delimiters, and renames a newly written temporary file to the requested destination. Several metadata/setting combinations instead dispatch threaded companion exporters. Adamantine also uses a special `_scan_path` basename.

### As-printed STL

`AsPrintedModelExporter` expands the parsed display segments, not the pre-G-code path model. The default binary output uses true bead widths, blends connected corners, excludes travel and support, excludes non-depositing or degenerate moves, converts to millimeters, and normalizes the mesh to a local origin. Options can include travel/support, preserve origin, emit ASCII, or replace bead geometry with fixed-diameter centerline tubes. Cylindrical bead-center metadata travels with segments so radial bead orientation survives export.

The focused exporter tests cover printable filtering, optional support/travel, local-origin and unit conversion, true-width versus centerline geometry, cylindrical orientation, full-circle arcs, corner blending, and binary/ASCII file structure.

## Cancellation and failure behavior

Cancellation sets both the loader flag and the parser's flag. The parser stops its line loop and returns an empty command list; visualization construction also checks the loader flag within command and layer loops. The `QThread` then ends normally and emits its standard `finished` signal, but no complete visualization payload is promised.

Parser-domain `ExceptionBase` failures are converted to `GCodeLoader::error` and shown in the GUI status surface. An unreadable source file currently logs a debug message and returns without emitting `error`; callers should not treat loader start as proof that a file was parsed. File replacement in the alterable path checks whether the temporary file opens but does not surface remove/rename failure through the loader error signal.

As-printed writes return a boolean and error string; the export window turns a failure into a warning. Machine companion exporters have their own completion behavior and should be tested with their exact metadata/setting combination.

## Extension checklist

When a change affects G-code flow, inspect all of these boundaries:

1. Writer output and `GcodeMeta` units, delimiters, suffix, and layer markers.
2. Loader syntax detection and the appropriate parser class.
3. Parser modal state, command parameters, layer timing, and cancellation.
4. Segment type, bead dimensions, coordinate transforms, source-line mapping, and display metadata.
5. Text, preview, timing, summary, and export consumers.
6. Read-only imported files versus alterable generated files.
7. Focused parser/writer tests and, for geometry output, `as_printed_model_exporter_tests`.

Related reading: [Application Runtime and Coordination](application-runtime.md), [Slicing Pipeline Orchestration](slicing-pipeline.md), [Geometry and Toolpath Data Model](../concepts/geometry-and-toolpath-model.md), and [Machine Syntax Writers and Parsers](../integrations/machine-syntaxes.md).
