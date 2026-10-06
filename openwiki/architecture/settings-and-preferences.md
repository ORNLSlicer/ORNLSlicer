---
type: architecture
title: Settings, Templates, and Preferences
description: Source-of-truth, generation, migration, precedence, local override, UI, and application-preference contracts for ORNLSlicer configuration.
tags: [settings, templates, preferences, migration, ui]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-d44494ef3e497fea81240ef8
    resource: repo://CMakeLists.txt
  - id: openwiki-source-90a873fbb389104f241eb359
    resource: repo://resources/configs/configs.qrc
  - id: openwiki-source-6f5d88049adbd611fb8e2754
    resource: repo://resources/settings/README.md
  - id: openwiki-source-f3d5520098265089809f783c
    resource: repo://scripts/generate_master_config.py
  - id: openwiki-source-6fcc7143f4182d92a74a222f
    resource: repo://src/managers/preferences_manager.cpp
  - id: openwiki-source-ee47d8a7bebb804c33b330b4
    resource: repo://src/managers/settings/settings_manager.cpp
  - id: openwiki-source-23c065c078f003c0b92bf2a8
    resource: repo://src/slicing/buffered_slicer.cpp
  - id: openwiki-source-29d5dbfa478667e089493f38
    resource: repo://src/slicing/preprocessor.cpp
  - id: openwiki-source-e451362b0f73808dce5d1127
    resource: repo://src/step/layer/regions/perimeter.cpp
  - id: openwiki-source-cd03371ffb111a7bd1510481
    resource: repo://src/widgets/settings/setting_bar.cpp
  - id: openwiki-source-ab8500cd8bf29ac0c5dcbd85
    resource: repo://src/widgets/settings/setting_tab.cpp
  - id: openwiki-source-6949df8bebfa2cc2eb946f04
    resource: repo://tests/preferences_import_tests.cpp
  - id: openwiki-source-1fd14546369693e17741b750
    resource: repo://tests/settings_version_control_tests.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Settings, Templates, and Preferences

ORNLSlicer has two configuration systems with different responsibilities. **Settings** describe a build: printer capabilities, material behavior, toolpath profiles, and experimental slicing controls. They can be layered from defaults through templates and local overrides and are persisted in projects. **Preferences** describe the application and user experience: display units, import behavior, theme, visualization colors, window state, and similar choices. They are stored in an application-local preferences file and must not be treated as slicing-template values.

## Data flow and ownership

```text
resources/settings/*.yaml                 canonical setting metadata
        │ generate_master_config.py
        ▼
resources/configs/master.conf             generated scalar setting catalog
resources/configs/setting_inputs.conf     generated composite-row catalog
        │ configs.qrc embeds both as :/configs/...
        ▼
SettingsManager
  ├── master metadata + UI input metadata
  ├── defaults → active global settings
  ├── .s2c templates grouped by major tab
  └── .s2l layer-range templates
        ▼
global → part overrides → matching layer ranges → settings-region polygons
        ▼
layer, region, path, and segment settings consumed by slicers and writers

PreferencesManager
  └── app.preferences → units, import/UI behavior, colors, window state
```

`SettingsManager` is the build-settings owner. At construction it reads the embedded `master.conf` and optional `setting_inputs.conf`, creates a template map for every major category, seeds the active global base with every master default, and reads the current schema version from `versions.conf`. The embedded files are runtime inputs, not an alternate authoring surface. [source](repo://src/managers/settings/settings_manager.cpp#L38-L70) [source](repo://resources/configs/configs.qrc#L1-L6)

`PreferencesManager` is an independent singleton with compiled defaults. Its JSON contains unit selections, shifting and alignment policies, visibility choices, camera/window state, rendering controls, import tessellation tolerance, visualization colors, and UI pacing values; none of these are merged into `SettingsManager::m_global`. [source](repo://src/managers/preferences_manager.cpp#L98-L146) [source](repo://src/managers/preferences_manager.cpp#L355-L395)

## Canonical metadata and generated catalogs

The YAML files under `resources/settings/` are the canonical setting definitions. Edit those files, not `resources/configs/master.conf` or `setting_inputs.conf`, then run:

```bash
python3 scripts/generate_master_config.py \
  resources/settings \
  resources/configs/master.conf \
  resources/configs/setting_inputs.conf
```

Files are read in sorted path order and entries retain file order, so the filenames and declaration order also control generated catalog and UI order. The parser intentionally accepts only the documented small YAML subset. [source](repo://resources/settings/README.md#L1-L15)

Every scalar setting must declare `display`, `type`, `tooltip`, `depends`, `options`, `default`, `minor`, `major`, `namespace`, `symbol`, and `local`. An optional top-level `inputs` section describes composite `vector2` and `vector3` rows over existing scalar settings. [source](repo://scripts/generate_master_config.py#L18-L42)

Generation is also validation. It rejects unknown or missing fields, invalid types, malformed or unknown dependencies, invalid enumeration defaults, duplicate names, invalid composite widgets, missing or reused components, and mismatched component categories or locality. Only after all definitions validate does it emit deterministic JSON. [source](repo://scripts/generate_master_config.py#L235-L308) [source](repo://scripts/generate_master_config.py#L311-L380) [source](repo://scripts/generate_master_config.py#L427-L487)

When automatic generation is enabled, CMake watches the YAML tree and generator script, regenerates both configuration files, and makes the main object target depend on that output. A missing Python interpreter or empty settings tree is a configure-time failure. [source](repo://CMakeLists.txt#L478-L518)

## Templates and the active global base

`.s2c` files are sparse global templates. Loading one checks its schema version, ignores keys absent from the current master catalog, and stores each known value under the template filename and the setting's `major` category. The active configuration is assembled by calling `constructActiveGlobal` for selected category/template pairs; each call overlays that template's JSON on the already default-populated global base. If a requested template is absent, the manager attempts `LFAM_03in` for that category and updates recent-template history. [source](repo://src/managers/settings/settings_manager.cpp#L93-L152) [source](repo://src/managers/settings/settings_manager.cpp#L197-L233)

This means template composition is overwrite-based and call-order-sensitive when two selected files contain the same key. Normal template organization avoids overlap by assigning settings to their master `major` category, but `SettingsBase::populate` itself simply replaces matching keys in iteration order. [source](repo://src/configs/settings_base.cpp#L17-L27)

Layer-bar templates use `.s2l`. Each entry supplies a low layer, high layer, and sparse settings object; loading creates a `SettingsRange` for each entry. The range endpoints are inclusive, and multiple matching ranges are applied in their container order. [source](repo://src/managers/settings/settings_manager.cpp#L155-L194) [source](repo://src/configs/settings_range.cpp#L35-L47)

## Effective-setting precedence

For planar slicing, effective values are built from broadest to narrowest scope:

1. `Preprocessor` copies the active global base.
2. It overlays the build part's local `SettingsBase`.
3. `BufferedSlicer` copies those part settings for a slice and overlays every range that includes the zero-based slice index.
4. It applies derived local adjustments, such as alternating-layer choices.
5. Settings meshes are sliced into `SettingsPolygon` objects. Region generators split or classify geometry and apply polygon-local settings to the affected geometry or segment.

The first two steps are explicit in the preprocessor's setup for every build part. [source](repo://src/slicing/preprocessor.cpp#L29-L45) Layer-range application and local adjustment occur before the primary cross-section is computed. [source](repo://src/slicing/buffered_slicer.cpp#L43-L59) [source](repo://src/slicing/buffered_slicer.cpp#L248-L285)

A settings mesh contributes its own `SettingsBase` and sliced geometry, not another global template. `BufferedSlicer` cross-sections each settings part at the active plane and wraps the result as a `SettingsPolygon`. [source](repo://src/slicing/buffered_slicer.cpp#L326-L344) Perimeter generation, for example, cuts paths at polygon boundaries, selects the containing polygon by subsegment midpoint, overlays that polygon's settings on the parent region base, and then copies the effective values into segment settings. [source](repo://src/step/layer/regions/perimeter.cpp#L800-L847)

These scopes are sparse by design: a part, range, or region only needs to contain the keys it overrides. Later `populate` calls win for matching keys while unrelated inherited keys remain present.

## Settings UI contract

The settings UI is metadata-driven. A setting's generated `type` selects its row factory; the catalog covers booleans, enumerations, unit-aware numeric controls, paths, text, numbered lists, and other supported scalar types. Generated input metadata can replace two or three component rows with one vector control while preserving aliases for each underlying key. [source](repo://src/widgets/settings/setting_tab.cpp#L67-L100) [source](repo://src/widgets/settings/setting_tab.cpp#L134-L186)

The `local` flag controls scope eligibility. When the user edits a range or another local base, `SettingBar` hides rows that are not marked local while retaining inherited bases so rows can distinguish an explicit override from a value inherited from a broader scope. [source](repo://src/widgets/settings/setting_bar.cpp#L158-L220)

When adding or changing a setting, keep all layers aligned:

1. Define or update the scalar entry in the appropriate ordered YAML file.
2. Add an `inputs` entry only when several existing scalar keys need a single composite control.
3. Regenerate both configuration outputs and include their deterministic changes.
4. Add or update the corresponding constants and runtime consumer separately; metadata does not itself implement slicing behavior.
5. Exercise generation plus the focused consumer or migration tests.

## Versioning and migration

Saved setting files carry a master version. If an older file is loaded, `SettingsManager::checkVersion` can roll it forward automatically, ask in the GUI, or ask on the console. Accepting a prompted migration mutates the JSON; file-based template loading writes the migrated document back. Declining returns `-1`, but the current loader still continues to inspect and load the document, so callers must not describe decline as a hard rejection. [source](repo://src/managers/settings/settings_manager.cpp#L243-L300) [source](repo://src/managers/settings/settings_manager.cpp#L93-L139)

Migrations are sequential transformations in `SettingsVersionControl`. Focused tests verify concrete compatibility behavior, including renaming two historical helical start-angle keys while retaining their values, converting the former boolean variable-Z setting into its enumeration value, and advancing all three fixtures to master version 13. [source](repo://tests/settings_version_control_tests.cpp#L13-L64)

Serialization should go through the version formatter. `globalJson()` and `saveTemplate()` both attach the current versioned header rather than writing the in-memory settings array directly. [source](repo://src/managers/settings/settings_manager.cpp#L463-L466) [source](repo://src/managers/settings/settings_manager.cpp#L473-L515)

## Preference import and persistence

With no explicit path, preferences live at the platform application-data location as `app.preferences`. Export serializes the complete preference JSON and clears the dirty flag. Import is deliberately tolerant of missing keys: each field falls back to the current in-memory value, allowing older files to inherit newer defaults. If the file does not exist, the manager leaves defaults in place and marks itself dirty so a later export can create it. [source](repo://src/managers/preferences_manager.cpp#L226-L249) [source](repo://src/managers/preferences_manager.cpp#L335-L353)

Import is transactional from an observer's perspective. A `QSignalBlocker` suppresses setter notifications while the whole JSON object is applied. Once every value and color migration is complete, the manager emits the individual unit/theme notifications and exactly one `anyUnitChanged` aggregate signal. The focused import test asserts both the single emission and that its callback can already observe the final unit, camera, lag, and color state. [source](repo://src/managers/preferences_manager.cpp#L250-L333) [source](repo://tests/preferences_import_tests.cpp#L44-L73)

Preference-specific migrations and guards belong in `PreferencesManager`, not the build-setting version chain. For example, stored visualization colors are migrated by their own version marker, invalid or empty colors fall back to registered defaults, and STEP tessellation deflection is clamped to a positive supported range; the round-trip and lower-bound behavior are covered by a focused test. [source](repo://src/managers/preferences_manager.cpp#L34-L94) [source](repo://src/managers/preferences_manager.cpp#L192-L223) [source](repo://tests/preferences_manager_tests.cpp#L19-L50)

## Failure and review checklist

- Treat YAML generation errors as schema failures; fix the canonical YAML instead of patching generated JSON.
- Preserve sparse override semantics when changing `populate` order: later scopes intentionally win.
- When adding a local setting, verify that `local` is correct and that the consumer reads the effective layer/region base rather than only the global singleton.
- Add a sequential migration and focused fixture whenever a persisted key changes name, type, units, or meaning.
- Keep preference imports batch-observable: subscribers must not rebuild against a partially imported state.
- Test file-open and malformed-input behavior deliberately. Existing preference import parses JSON without a local exception boundary, while a missing file is recoverable; those are different failure contracts.
