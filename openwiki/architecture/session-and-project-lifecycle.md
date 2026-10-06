---
type: architecture
title: Session, Model, and Project Lifecycle
description: Ownership and persistence guide for loaded parts, raw model bytes, `.s2p` projects, application history, unsaved-change tracking, and active slicers.
tags: [session, project, model-import, persistence, threading]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-06T20:25:27.555Z
sources:
  - id: openwiki-source-6ac6f343f5f17fc1351db9c2
    resource: repo://include/managers/session_manager.h
  - id: openwiki-source-0baa0747dc689aa7f6157450
    resource: repo://include/threading/mesh_loader.h
  - id: openwiki-source-29f2d3a38a8c7e63c427aa92
    resource: repo://include/threading/session_loader.h
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-c97c50d7ad4d8d89717ac6d3
    resource: repo://src/threading/mesh_loader.cpp
  - id: openwiki-source-c9b49d4d1b98e2ebff97fee2
    resource: repo://src/threading/session_loader.cpp
  - id: openwiki-source-403d1cb683b233c4f42460d0
    resource: repo://src/windows/main_window.cpp
  - id: openwiki-source-af6398eb7d1bc9a42afbe5af
    resource: repo://tests/session_manager_tests.cpp
generated: { by: "codex", at: "2026-10-06T20:25:27.555Z" }
---

# Session, Model, and Project Lifecycle

`SessionManager` is the shared owner of the working build. It holds named `Part` objects, the raw source bytes needed to make projects self-contained, the active slicer, temporary output locations, recent-file history, and the current project path. The GUI and CLI present different workflows, but both mutate this singleton and receive its signals.

Three forms of state have separate lifecycles:

| State | Owner/location | Contents | Persistence trigger |
| --- | --- | --- | --- |
| Working session | `SessionManager` in memory | Parts, raw model bytes, active slicer, current path and temporary output | Exists for the process lifetime |
| Project | `.s2p` ZIP archive | Referenced model bytes, part transforms/types, global settings, part-local settings/ranges, build version | Explicit save, export copy, autosave, or final last-session save |
| Application history | platform application-data `app.history` | Recent directories/files and last selected templates/configuration | Written when dirty during `SessionManager` destruction |

The GUI's “unsaved project” flag is a fourth concern: it is a comparison gate over project-relevant state, not the same flag as dirty application history.

## In-memory ownership

`SessionManager` stores parts as `QMap<QString, QSharedPointer<Part>>`. Names are unique session keys; adding a duplicate derives a numbered name before emitting `partAdded`. It separately stores each model's malloc-owned raw byte buffer and size, keyed by source filename. The raw representation exists because both the import libraries and ZIP layer accept untyped buffers. The manager frees every retained buffer at destruction and before replacing a session.

The maps serve different purposes:

- `Part` is the live semantic object: mesh hierarchy, transforms, local settings, ranges, and computed steps.
- `m_models` retains source bytes so `.s2p` saving can embed the original model without re-reading a path that may have moved or disappeared.

Removing a part removes matching model-map entries by root/submesh name, while project saving performs an additional active-reference filter. New ownership logic must preserve the rule that each malloc-owned buffer is freed exactly once.

The manager also owns a `QSharedPointer<AbstractSlicingThread>`. `doSlice()` validates the active mode, creates or reuses the concrete planar/image/cylindrical slicer, updates its output path, requests current GUI transformations, and emits `startSlice`. Changing slicer clears previously computed part steps and reconnects progress, status, and completion signals.

## Model import

### Supported paths

`MeshLoader::LoadMeshes()` accepts source bytes from disk or from an `.s2p` archive. STEP/STP files follow an OpenCASCADE tessellation path; STL, 3MF, OBJ, and AMF use Assimp with format-appropriate processing. A source containing multiple meshes produces multiple live meshes while retaining one copy of the source byte buffer.

For Assimp inputs, import first attempts a CGAL polyhedron. If model repair is enabled, a repaired copy replaces the original only on successful repair; a failed or exceptional repair logs a warning and preserves the unrepaired input. A builder failure or non-closed polyhedron becomes `OpenMesh`; otherwise import constructs `ClosedMesh`. Each mesh is centered, assigned the requested mesh role, scaled from the selected import unit, and optionally receives its original centroid as an implicit translation.

STEP import tessellates in millimeters using the preference-controlled linear deflection and a fixed angular deflection. Archive-backed STEP bytes are written to a temporary file because OpenCASCADE's reader requires a path.

### GUI versus CLI

`SessionManager::loadModel()` defaults to asynchronous import. The GUI constructs `MeshLoader : QThread`; `newMesh` adds each result and stores the first raw buffer, while `error` becomes a session status message. Reload and replace operations use the same worker pattern and update the existing part only after a new mesh arrives.

The CLI passes `synchRequired = true`. It calls the static loader in the coordinator's call stack, adds every returned mesh immediately, and relies on the same `partAdded` signal to count the load barrier. Project reconstruction also calls the static loader because the raw bytes are already in `m_models`.

An empty loader result emits an asynchronous error, but `SessionManager::loadModel()` itself returns `true` after starting or attempting the operation; its boolean is not proof that a mesh was imported.

## Project archive format

`SessionLoader::saveSession()` writes a ZIP archive with these logical entries:

| Entry | Source | Role |
| --- | --- | --- |
| `model/...` | Active entries in `SessionManager::models()` | Original model bytes for self-contained reload |
| Session JSON | `SessionManager::partsJson()` | Part name, source filename, mesh role, generated-shape type, original dimensions, and one or more transforms |
| Global JSON | `SettingsManager::globalJson()` | Active project-wide settings and version header |
| Local JSON | Each part's settings plus serialized settings ranges | Per-part and per-layer overrides |
| `Version.txt` | Runtime build version | Human-readable producer/version marker |

Only raw models still referenced by an active root mesh or submesh are embedded. Generated primitives do not require model bytes: `partsJson()` records generator type and original dimensions, and `loadPartsJson()` reconstructs the corresponding box, pyramid, prism, cylinder, cone, open box, or default settings region through `MeshFactory`.

Before starting a save worker, `SessionManager` emits `requestTransformationUpdate` so GUI edits are reflected in the serialized transforms. The worker emits `saveSucceeded` only after closing a fully written archive. `SessionManager` can translate that to `sessionSaved`, but only when the caller requests a notification.

## Project load and settings preflight

Project settings are version-checked synchronously before the loader thread starts:

1. `SessionManager::loadSession()` optionally frees current raw model buffers and clears parts, then clears the active global settings.
2. A `SessionLoader` reads the archived global JSON for `SettingsManager::checkVersion()`.
3. GUI loads prompt before rolling old settings forward; declining returns `nullptr`. Headless loads use automatic in-memory migration.
4. If migration occurred, the loader receives the migrated JSON. GUI-approved migration is also written back into the archive; headless auto-migration deliberately leaves the archive unchanged.
5. The worker reads embedded model bytes, loads global settings, reconstructs parts/transforms, then applies each part's local settings and ranges before emitting `loadSucceeded`.

The CLI behavior is regression-tested: an old settings archive loads using migrated settings in memory while its stored global JSON remains at the old version. This separation lets automation consume legacy projects without silently rewriting input artifacts.

Preflight ordering has a consequence: when `shouldDelete` is true, the existing session and global settings are cleared before the version decision is accepted. A rejected or malformed project does not restore the previous session automatically.

### Thread-affinity exception

`SessionLoader` is a `QThread`, but its `run()` method directly reads and writes `CSM` and `GSM`: it iterates manager maps during save and populates their model, settings, part, and range state during load. The source itself marks this as needing cleanup. Preserve this behavior when fixing nearby code, but do not use it as precedent; new workers should return data through queued signals to the managers' owning thread.

## Load failure semantics

The lifecycle exposes `saveSucceeded`, `loadSucceeded`, and `error`, but callers must distinguish worker termination from success:

- Failure to open a save/load archive, update migrated JSON, or write embedded model bytes returns from the worker without a success signal.
- A local-settings entry whose named part was not reconstructed emits `error` and continues with other entries.
- The standard manager setup always deletes the worker on `finished`; it does not universally forward `SessionLoader::error` to a status surface.
- A normal GUI save updates the window's saved-state snapshot immediately after starting the asynchronous worker. Only close-confirmation saves explicitly wait. Therefore a cleared dirty indicator is not itself evidence that the archive write succeeded; success-sensitive UI should use `saveSucceeded`/`sessionSaved`.

## Application history and recent files

`app.history` is JSON outside the project archive. It remembers selected printer/material/profile/experimental templates; recent model, project, G-code, settings, and layer-bar directories; the recent HTTP configuration; and recent project/model lists.

Recent paths are canonicalized to absolute paths and deduplicated by moving a reopened item to the front. Both lists are capped at ten. Project history accepts `.s2p`; model history accepts STL, 3MF, OBJ, AMF, STEP, and STP, but only for build-role models. Invalid extensions and clipping/settings models do not enter the recent-model list. The focused session test protects filtering, ordering, the cap, removal/clear behavior, STEP support, and headless migration.

Changing a history field marks `m_dirty_history`; the manager writes `app.history` on destruction. This flag says nothing about whether the current `.s2p` project has changed.

## GUI dirty state and recovery

`MainWindow` snapshots the project-relevant JSON: part transforms/types, active global settings, and each part's local settings/ranges. Part add/reload/remove/parent/transform/name signals and settings changes set a cheap `m_project_modified` gate. Before prompting, `hasUnsavedProjectChanges()` refreshes transformations and compares a fresh snapshot with the saved snapshot, avoiding prompts for edits that were undone back to the saved value.

Close, project replacement, and recent-project loading call the same confirmation workflow when the preference is enabled. Save waits only when the close decision requires a durable result before shutdown. Separately, a five-minute autosave and final window teardown write `_lastsession.s2p` in application data; this recovery file is not the user's named project.

## Safe extension points

- Add project-persistent part state to both save and load representations and to the GUI snapshot used for dirty detection.
- Keep user/machine history out of `.s2p`; keep build-defining settings and transforms inside it.
- When adding a model format, update import dispatch, CLI file validation, dialogs, recent-file filtering, project reconstruction, and tests together.
- Treat `SessionLoader::finished` as cleanup, not success. Connect the explicit success/error signal needed by the caller.
- Coordinate new blocking import or persistence work through a worker without directly mutating widgets.

Related reading: [Application Runtime and Coordination](application-runtime.md), [Settings, Templates, and Preferences](settings-and-preferences.md), [Slicing Pipeline Orchestration](slicing-pipeline.md), and [Geometry and Toolpath Data Model](../concepts/geometry-and-toolpath-model.md).
