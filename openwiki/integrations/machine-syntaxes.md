---
type: integration
title: Machine Syntax Writers and Parsers
description: How machine dialect metadata, writer and parser selection, companion exporters, and focused tests fit together, with a checklist for adding a syntax safely.
tags: [gcode, writers, parsers, machine-integration, export]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-95107d4ff5d6d52fa1e825b7
    resource: repo://include/gcode/gcode_meta.h
  - id: openwiki-source-e14231bda7de11e93f1bcb96
    resource: repo://include/gcode/parsers/common_parser.h
  - id: openwiki-source-7d76326440b2af435c042279
    resource: repo://include/gcode/writers/writer_base.h
  - id: openwiki-source-b682621197d014ee954484a7
    resource: repo://include/utilities/enums.h
  - id: openwiki-source-4b1359988efc6fa24f9c2ef0
    resource: repo://resources/settings/001_printer_machine_setup.yaml
  - id: openwiki-source-6f5d88049adbd611fb8e2754
    resource: repo://resources/settings/README.md
  - id: openwiki-source-ec619fd5d2a20f51e10dc899
    resource: repo://src/gcode/parsers/arc_specialties_parser.cpp
  - id: openwiki-source-94d7720f109e6629e985b28d
    resource: repo://src/gcode/parsers/common_parser.cpp
  - id: openwiki-source-78753032fb9f23e04b7e0e07
    resource: repo://src/gcode/parsers/parser_base.cpp
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-97f2984c6f7df859b5dac320
    resource: repo://src/managers/settings/settings_version_control.cpp
  - id: openwiki-source-b1134fc13dd24912e260b947
    resource: repo://src/threading/abs_slicing_thread.cpp
  - id: openwiki-source-eda4b121f1ff16dc092e8f3f
    resource: repo://src/threading/gcode_aml3d_saver.cpp
  - id: openwiki-source-4c1d7b6308a78ba9c493ce8a
    resource: repo://src/threading/gcode_loader.cpp
  - id: openwiki-source-6b09820f6916b4f199cb60a7
    resource: repo://src/threading/gcode_marlin_saver.cpp
  - id: openwiki-source-74cea8ba6a71c48d8439dfdc
    resource: repo://src/threading/slicers/cylindrical_slicer.cpp
  - id: openwiki-source-23afc284fcae4cee2de94dcb
    resource: repo://src/windows/gcode_export.cpp
  - id: openwiki-source-b89f433e76caa6e1aa00a3e9
    resource: repo://tests/arc_specialties_parser_tests.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Machine Syntax Writers and Parsers

A machine syntax is not implemented in one registry. It is a cross-cutting contract that connects a persisted enum value and display name to metadata, a writer, a loader parser, layer-time parsing, optional export workers, settings visibility, and focused tests. Several syntaxes intentionally share parts of that machinery, so adding a dialect means choosing which behavior is genuinely unique rather than copying a full stack. [source](repo://include/utilities/enums.h#L258-L295) [source](repo://src/threading/abs_slicing_thread.cpp#L70-L170) [source](repo://src/threading/gcode_loader.cpp#L590-L738)

```text
canonical syntax option ──> GcodeSyntax + public syntax string
                                  │
                                  ▼
                              GcodeMeta
                     units / comments / suffix / layers
                         ┌────────┴────────┐
                         ▼                 ▼
                WriterBase subclass   parser selection
                         │                 │
                         └──── G-code ─────┘
                                  │
                                  ▼
                     visualization / statistics / export
                                  │
                                  ▼
                     optional companion-file saver
```

## Identity and metadata

`GcodeSyntax` is the persisted numeric identity. The canonical machine-setup YAML presents matching options in enum order, while `toString()` maps an enum value to the syntax marker written into and detected from G-code. Existing settings migrations explicitly translate numeric syntax values after removals, which is why reordering or inserting values in the middle is a data-migration change rather than a cosmetic edit. [source](repo://resources/settings/001_printer_machine_setup.yaml#L1-L47) [source](repo://include/utilities/enums.h#L262-L364) [source](repo://src/managers/settings/settings_version_control.cpp#L17-L30) [source](repo://src/managers/settings/settings_version_control.cpp#L185-L209)

`GcodeMeta` carries the properties needed on both sides of the file boundary: syntax ID, comment delimiters, distance/time/angle/mass/velocity units, file suffix, whether travels are explicit, and the layer-count and layer-start delimiters. `GcodeMetaList` defines reusable metadata records and a syntax-to-metadata hash used by output-path selection. Multiple syntax IDs may deliberately share a metadata record. [source](repo://include/gcode/gcode_meta.h#L8-L27) [source](repo://include/gcode/gcode_meta.h#L308-L345) [source](repo://src/managers/session_manager.cpp#L575-L587)

This sharing is behavioral, not an alias for the entire dialect. JuggerBot, Mach4, and Marlin use Marlin metadata, but generation selects three different writer classes. Thermwood shares Cincinnati metadata and parser behavior while using a distinct writer. Haas Metric No Comments and Okuma reuse Haas Metric metadata in the writer factory. [source](repo://src/threading/abs_slicing_thread.cpp#L70-L106) [source](repo://src/threading/abs_slicing_thread.cpp#L229-L266) [source](repo://src/threading/abs_slicing_thread.cpp#L281-L295)

## Writer contract

`WriterBase` defines the emission lifecycle consumed by the slicer data model: slicer/settings headers, initial machine setup, boundaries around layers, parts, islands, regions and paths, motion writers for travel/line/optional scan/arc, shutdown, and a settings footer. It also owns shared state such as feed rate, deposition state, current output position, build maximum Z, comment delimiters, common command tokens, and coordinate-frame helpers. [source](repo://include/gcode/writers/writer_base.h#L23-L114) [source](repo://include/gcode/writers/writer_base.h#L116-L203)

`AbstractSlicingThread::setGcodeOutput()` reads the global syntax setting and constructs the concrete writer with its compatible metadata and the global settings object. The default branch creates a Cincinnati writer, so an enum value that reaches this switch without an explicit case does not fail closed. Cylindrical slicing is stricter: session dispatch only permits Arc Specialties, and `CylindricalSlicer` directly constructs `ArcSpecialtiesWriter`. [source](repo://src/threading/abs_slicing_thread.cpp#L210-L320) [source](repo://src/managers/session_manager.cpp#L47-L49) [source](repo://src/managers/session_manager.cpp#L575-L583) [source](repo://src/threading/slicers/cylindrical_slicer.cpp#L418-L421)

Use a dedicated writer when controller commands, lifecycle hooks, or motion formatting differ. Reuse metadata or inherit existing behavior when only a narrow exception differs. For example, JuggerBot has its own writer but retains Marlin units and syntax metadata; focused tests require width/height arc bead area to include the extrusion multiplier and require a zero-RPM arc to turn deposition off before motion. [source](repo://src/threading/abs_slicing_thread.cpp#L247-L252) [source](repo://tests/juggerbot_writer_tests.cpp#L47-L77) [source](repo://tests/juggerbot_writer_tests.cpp#L81-L92)

## Parser contract

`ParserBase` resets one `GcodeCommand`, extracts configured comments, tokenizes the remaining command, normalizes leading zeroes in command numbers, and dispatches parameters through a command-to-handler map installed by `config()`. Subclasses can replace mappings because `addCommandMapping()` removes an existing handler before inserting the new one. [source](repo://src/gcode/parsers/parser_base.cpp#L20-L75) [source](repo://src/gcode/parsers/parser_base.cpp#L77-L98) [source](repo://src/gcode/parsers/parser_base.cpp#L110-L157)

`CommonParser` is the shared stateful layer above that tokenizer. It imports units and layer delimiters from metadata, owns original and uppercase line arrays, configures common commands, and exposes header, footer, and motion parsing for visualization and statistics. Specialized parsers call or override this behavior when their controller changes tokens, modal state, or command meaning. [source](repo://src/gcode/parsers/common_parser.cpp#L75-L123) [source](repo://include/gcode/parsers/common_parser.h#L39-L76)

The loader scans for `G-CODE SYNTAX` or `GCODE SYNTAX`, matches the public syntax string, and chooses a specialized parser or `CommonParser` with appropriate metadata. The Marlin family shares `MarlinParser`; many conventional controllers use `CommonParser`; and Beam, Cincinnati, Mazak, MVP, Siemens, Tormach, AeroBasic, Adamantine, and Arc Specialties have specialized parser paths. Unknown declarations fall back to common Marlin parsing, while a missing declaration also constructs `CommonParser` with Marlin metadata. [source](repo://src/threading/gcode_loader.cpp#L590-L655) [source](repo://src/threading/gcode_loader.cpp#L657-L728) [source](repo://src/threading/gcode_loader.cpp#L729-L738)

Generated output has a second parser-selection site. After writing G-code, `AbstractSlicingThread` constructs a parser appropriate to the active syntax to calculate adjusted layer times and insert syntax-delimited time comments. A new parser needed for imported-file correctness may therefore also need a case in `makeLayerTimeParser()`. [source](repo://src/threading/abs_slicing_thread.cpp#L134-L175) [source](repo://src/threading/abs_slicing_thread.cpp#L400-L418)

## A focused exception: Arc Specialties

Arc Specialties demonstrates why a dialect sometimes needs both sides specialized. Its metadata selects semicolon comments, millimeters, seconds, millimeters per minute, and an `.nc` suffix. Its parser strips Beckhoff-style block numbers, normalizes `G00` through `G03`, installs controller-specific handlers, treats `G82`/`G83` as deposition state, strips orientation axes from ordinary motion, retains `CP` as an optional visualization parameter, and rejects duplicate or unknown parameters. [source](repo://include/gcode/gcode_meta.h#L296-L307) [source](repo://src/gcode/parsers/arc_specialties_parser.cpp#L17-L57) [source](repo://src/gcode/parsers/arc_specialties_parser.cpp#L59-L145) [source](repo://src/gcode/parsers/arc_specialties_parser.cpp#L168-L201)

The corresponding integration test is intentionally both parser- and writer-facing. It covers inline arc optional stops, weld-schedule speed selection, retained cylindrical position, block numbering, rotation direction, region comments, tool-frame rotations, physical path finalization, safe startup height, clip-rounding metadata, and configured cylindrical arc density. Those checks are the compatibility boundary for changes to this dialect, not merely syntax smoke tests. [source](repo://tests/arc_specialties_parser_tests.cpp#L806-L874)

## Companion outputs are a separate layer

Some machine integrations transform the completed primary G-code into additional files. `GcodeExport` selects a threaded saver from the detected metadata plus an enable setting—for example MELD, Tormach, AML3D, Sandia, Marlin command data, ORNL/AMCM, or Adamantine—and otherwise performs the ordinary file copy. This is downstream of writer/parser selection; a new companion format needs an export branch and lifecycle wiring in addition to its dialect implementation. [source](repo://src/windows/gcode_export.cpp#L337-L405) [source](repo://src/windows/gcode_export.cpp#L406-L415)

Saver behavior can be materially different from G-code emission. AML3D creates one deposit CSV per layer, ignores controller-computed travels, and numbers bead points; the Marlin saver writes a command-data stream with converted SI coordinates and per-axis feed rates. Treat these as export adapters with their own format tests and failure behavior rather than as writer subclasses. [source](repo://src/threading/gcode_aml3d_saver.cpp#L24-L78) [source](repo://src/threading/gcode_aml3d_saver.cpp#L80-L145) [source](repo://src/threading/gcode_marlin_saver.cpp#L24-L55) [source](repo://src/threading/gcode_marlin_saver.cpp#L57-L140)

## Adding a dialect safely

Work through every site below; there is no single factory that makes the rest automatic.

1. **Preserve persisted identity.** Add the `GcodeSyntax` value without silently changing existing ordinals. Add the matching canonical YAML option in the same position, update the public `SyntaxString`, and add a settings migration if stored numeric values move. [source](repo://include/utilities/enums.h#L262-L364) [source](repo://resources/settings/001_printer_machine_setup.yaml#L4-L47) [source](repo://src/utilities/constants.cpp#L153-L187)
2. **Regenerate settings artifacts.** Edit `resources/settings/*.yaml`, then run the documented generator for both `master.conf` and `setting_inputs.conf`; file and setting order control the generated UI order. [source](repo://resources/settings/README.md#L1-L15)
3. **Define metadata.** Supply correct delimiters, units, suffix, travel semantics, and layer markers, then insert the syntax into `SyntaxToMetaHash`. Verify any deliberate sharing rather than assuming each enum maps to a unique record. [source](repo://include/gcode/gcode_meta.h#L8-L27) [source](repo://include/gcode/gcode_meta.h#L308-L345)
4. **Wire generation.** Implement or reuse a `WriterBase` subclass and add the exact writer/metadata pair to `AbstractSlicingThread::setGcodeOutput()`. Audit capability gates such as cylindrical slicing rather than assuming writer support implies mode support. [source](repo://include/gcode/writers/writer_base.h#L23-L114) [source](repo://src/threading/abs_slicing_thread.cpp#L210-L315) [source](repo://src/managers/session_manager.cpp#L575-L583)
5. **Wire both read paths.** Add loader syntax detection and parser construction, then update `metaForSyntax()` and `makeLayerTimeParser()` if generated layer-time annotation needs the dialect's parser semantics. Decide explicitly whether unknown or missing syntax fallback is acceptable for this format. [source](repo://src/threading/gcode_loader.cpp#L590-L738) [source](repo://src/threading/abs_slicing_thread.cpp#L70-L170)
6. **Add optional outputs deliberately.** If the machine needs a companion artifact, add its saver and its `GcodeExport` dispatch condition; do not hide that transformation inside the primary writer. [source](repo://src/windows/gcode_export.cpp#L337-L415)
7. **Pin exceptions with focused tests.** Cover emitted machine text, parser modal behavior, units, comments, deposition/travel classification, layer-time edits, malformed parameters, and round-trip visualization metadata. Register any new executable with CTest. The existing common-parser, Arc Specialties, and JuggerBot targets show the intended pattern. [source](repo://CMakeLists.txt#L353-L384) [source](repo://tests/common_parser_tests.cpp#L295-L315) [source](repo://tests/arc_specialties_parser_tests.cpp#L806-L876)

The source and header trees are globbed with `CONFIGURE_DEPENDS`, so new implementation files are discovered by CMake after reconfiguration. Test executables are explicit and still require their own `add_executable()`, linkage, and `add_test()` entries. [source](repo://CMakeLists.txt#L139-L158) [source](repo://CMakeLists.txt#L353-L384)

Related reading: [G-Code, Parsing, Visualization, and Export](../architecture/gcode-and-visualization.md), [Settings and Preferences](../architecture/settings-and-preferences.md), [Slicing Pipeline Orchestration](../architecture/slicing-pipeline.md), and [Cylindrical and Helical Slicing](../slicing/cylindrical-and-helical.md).
