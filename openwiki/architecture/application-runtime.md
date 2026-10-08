---
type: architecture
title: Application Runtime and Coordination
description: Runtime map for ORNLSlicer's GUI and command-line shells, shared managers, worker threads, progress and cancellation signals, and output completion paths.
tags: [runtime, qt, gui, cli, threading]
verified:
  - by: openwiki/0.7.0
    at: 2026-10-08T21:29:05.545Z
sources:
  - id: openwiki-source-3eee793d88e31c2c26304c07
    resource: repo://include/threading/abs_slicing_thread.h
  - id: openwiki-source-3c39dad600af45d983ab53aa
    resource: repo://include/threading/gcode_loader.h
  - id: openwiki-source-0baa0747dc689aa7f6157450
    resource: repo://include/threading/mesh_loader.h
  - id: openwiki-source-ab3dd3ea028d9cad20143b04
    resource: repo://src/console/main_control.cpp
  - id: openwiki-source-d364d949938a433276255c32
    resource: repo://src/main.cpp
  - id: openwiki-source-1c931b6d3c850e1860fde20c
    resource: repo://src/managers/session_manager.cpp
  - id: openwiki-source-b1134fc13dd24912e260b947
    resource: repo://src/threading/abs_slicing_thread.cpp
  - id: openwiki-source-c9b49d4d1b98e2ebff97fee2
    resource: repo://src/threading/session_loader.cpp
  - id: openwiki-source-c8bd61f0cfef6efd243b1e22
    resource: repo://src/widgets/main_toolbar.cpp
  - id: openwiki-source-403d1cb683b233c4f42460d0
    resource: repo://src/windows/main_window.cpp
  - id: openwiki-source-eca38ea4c3df70e00aa49450
    resource: repo://tests/main_toolbar_tests.cpp
generated: { by: "codex", at: "2026-10-08T21:29:05.545Z" }
---

# Application Runtime and Coordination

ORNLSlicer has one executable and two application shells. The split happens before any window is created: an invocation with at least one command-line argument uses a `QCoreApplication` and `MainControl`, while an invocation with no extra arguments uses a `QApplication` and the singleton `MainWindow`. Both shells coordinate the same settings and session managers and ultimately use the same slicers, writers, and G-code loader.

## Runtime shape

```text
src/main.cpp
├── argc > 1: QCoreApplication → CommandLineConverter → MainControl
│   └── load models/project → SessionManager → slicer → GCodeLoader → output file → quit
└── argc == 1: QApplication → MainWindow
    └── user actions → SessionManager → slicer → GCodeLoader → views/export widgets
```

Startup registers the Qt metatypes carried across queued signal/slot connections before choosing a shell. This includes shared `Part` and mesh pointers, toolpath segments, unit types, G-code metadata, JSON, and mesh-loader results. Both branches also publish a runtime diagnostic summary, but only the GUI initializes the embedded icon, shader, style, and configuration resources.

| Invocation | Qt application | Coordinator | Completion |
| --- | --- | --- | --- |
| No extra arguments | `QApplication` | `MainWindow` | The user closes the window; close confirmation and preference persistence run before the event loop exits. |
| One or more arguments | `QCoreApplication` | `MainControl` | `MainControl::finished` is queued to `QCoreApplication::quit`; parse or option failures return before the event loop. |

Because the branch condition is simply `argc > 1`, informational options such as `--help` and `--version` also enter the CLI branch.

## Shared ownership

The coordinators do not own duplicate application state. `SessionManager` (`CSM`) owns the loaded parts, raw model payloads used by projects, current project path and active slicer. `SettingsManager` (`GSM`) owns available templates and the active global settings. `PreferencesManager` holds application-local choices such as import behavior, theme, and window geometry.

The GUI additionally owns its QObject/widget tree through normal Qt parent ownership. `MainWindow` itself is a manually managed singleton: `main()` obtains it, shows it, runs the event loop, and deletes it after the loop returns. Shared model objects passed among managers, workers, and views use `QSharedPointer` so queued deliveries can safely retain them.

## GUI startup and event wiring

`MainWindow::continueStartup()` performs the stateful boot sequence before building widgets:

1. Import preferences.
2. Search installed/development locations for base `.s2c` templates.
3. Load templates from the most recently used settings directory.
4. Construct the active global settings selection and load the layer-bar template.
5. Create non-widget helpers, compose the UI, connect events, and establish the initial saved-project state.

The window is the UI router, not the data owner. Its event graph connects session additions to the part widget, session status text to the status surfaces, and slice completion to G-code import, export enablement, and a switch to the G-code view. Settings changes fan out to part, G-code, layer, toolbar, and window consumers. A five-minute timer writes the recoverable `_lastsession.s2p` project when parts exist and no busy operation is active.

### GUI slice sequence

1. `PartWidget::slice` invokes `MainWindow::doSlice()`.
2. The window clears old G-code views, requires at least one build part, creates a `SliceDialog`, and connects progress and cancellation.
3. `SessionManager::doSlice()` validates the selected mode/syntax, selects or reuses the concrete slicer, requests current GUI transforms, and emits `startSlice`.
4. The active `AbstractSlicingThread` receives `doSlice()` in its internal worker thread. Status and completion signals return through `SessionManager`.
5. `forwardSliceComplete` makes the GUI start a `GCodeLoader`. That worker parses the file and independently supplies visualization segments, formatted text, layer-time statistics, export metadata, and status text.
6. While the G-code worker is active, the slice dialog's cancel action is reconnected from the slicer to the loader.

An invalid cylindrical configuration is rejected before work starts: current source permits cylindrical slicing only with the Arc Specialties syntax. The manager emits a user-visible status and returns `false`, allowing the GUI to discard the slice dialog.

## Runtime UI extensions

`MainWindow` owns auxiliary dialogs as children, including the settings-file comparison dialog. It constructs that dialog during window setup, exposes **Compare Settings Files** in the Settings menu, and brings the existing dialog to the foreground when the action is triggered. The comparison dialog therefore remains a UI-owned helper; it does not introduce a second settings-state owner.

The main toolbar is likewise a child of the main container and emits intent through signals that `MainWindow` connects to the active view and session consumers. Its command controls are `QAction`s (with only the view tabs represented by a widget action), allowing Qt to move trailing commands into its built-in overflow menu when width is constrained. At the normal startup view width the controls remain visible; the focused offscreen Qt test covers both that baseline and the constrained-overflow behavior.

## Command-line state machine

`CommandLineConverter` validates options into a `SettingsBase`. `MainControl` installs those console settings, optionally constructs global settings from a supplied configuration, and subscribes to the same session progress and completion signals used by the GUI.

The loading path differs by input:

- Explicit model and support files call the synchronous `MeshLoader::LoadMeshes` route through `SessionManager::loadModel(..., synchRequired = true)`. `MainControl` counts resulting `partAdded` notifications.
- A project starts a `SessionLoader`; its reported part count establishes the same completion barrier.

When the last part arrives, `MainControl` asks the session manager to slice. Image slicing finishes after the slicer's image output. Toolpath modes start a `GCodeLoader`, even in headless mode, so generated G-code receives the same parsing and minimum-layer-time adjustment pass used by the GUI. The loader reports the detected metadata and temporary path; `MainControl` then writes the requested basename with the syntax-specific suffix and emits `finished`.

Progress is reduced to a console status line keyed by `StatusUpdateStepType`. Repeated updates for the same phase rewrite the line; a 100% update terminates it.

## Worker and affinity boundaries

| Work | Mechanism | Coordination contract |
| --- | --- | --- |
| GUI model import/reload | `MeshLoader : QThread` | Emits `newMesh` or `error`; the session manager updates owned state in receiving slots. The CLI can deliberately call the static loader synchronously. |
| Project save/load | `SessionLoader : QThread` | Created by `SessionManager`, deleted on `finished`, and optionally emits save success. The current implementation reads and mutates `CSM`/`GSM` directly inside the worker; treat this as an existing exception, not a pattern for new workers. |
| Slicing | `AbstractSlicingThread : QObject` moved to an owned `QThread` | `SessionManager::startSlice` queues `doSlice`; progress, status, and completion flow back through the manager. Destruction quits and joins the internal thread. |
| Per-step planar computation | `StepThread` workers managed by the active slicer | The slicer queues steps, gathers completion, and advances to postprocessing/output. See [Slicing Pipeline Orchestration](slicing-pipeline.md). |
| G-code parse and visualization | `GCodeLoader : QThread` | Emits distinct payloads for text, OpenGL segments, timing, export, and status; the GUI and CLI connect only the consumers they need. |
| Machine-specific export helpers | `GCode*Saver : QThread` implementations | Long-running transformation/export stays outside the GUI thread. See [G-Code, Parsing, Visualization, and Export](gcode-and-visualization.md). |

Signals carrying complex values depend on the metatype registrations in `main()`. New cross-thread payload types must be registered before any queued delivery.

## Progress, cancellation, and failures

`SessionManager` is the status relay. Slicers emit typed percentage updates and free-form messages; the manager forwards them to a GUI dialog/status surface or to `MainControl`. Cancellation is cooperative: the dialog calls `SessionManager::cancelSlice()`, which sets the active slicer's flag; once parsing begins, the same dialog calls `GCodeLoader::cancelSlice()` instead.

Failure behavior follows the boundary that detects it:

- CLI option conversion prevents coordinator startup and returns a nonzero process status, except for a successful version request.
- Mesh workers emit textual errors, which the session manager forwards to the active shell.
- Mode/syntax validation fails synchronously before `startSlice`.
- Parser errors are emitted by `GCodeLoader` to the GUI status surface. Headless completion depends on the loader reaching its normal `finished` path.
- Project workers may return without a success signal when an archive cannot be opened or written; callers must not infer success merely from starting the thread.

## Shutdown and persisted shell state

GUI close first asks whether a modified project may be discarded. It records changed window geometry preferences, then quits the application. During `MainWindow` destruction, dirty preferences are exported and a nonempty session is saved to `_lastsession.s2p`; this final save is explicitly waited on before widget teardown completes. `SessionManager` persists its separate `app.history` record when destroyed.

The CLI has no window-owned state. Its `finished` signal quits the event loop through a queued connection, after which `main()` deletes `MainControl` and returns the Qt event-loop status.

## Where to extend or verify

- Add an application-wide state owner to an existing manager unless it has a genuinely separate lifecycle.
- Put blocking file, mesh, slicing, parsing, or export work behind an existing worker boundary and return state through signals rather than mutating widgets from a worker.
- When adding a shell-visible phase, update both typed progress consumers and failure completion paths.
- Verify non-UI session coordination with `session_manager_tests`; verify a complete headless load/slice/write route with the registered project-slice regression. Interactive window wiring, OpenGL context behavior, close prompts, and cancellation timing still require GUI-level testing.

Related reading: [Session, Model, and Project Lifecycle](session-and-project-lifecycle.md), [Settings, Templates, and Preferences](settings-and-preferences.md), [Slicing Pipeline Orchestration](slicing-pipeline.md), and [G-Code, Parsing, Visualization, and Export](gcode-and-visualization.md).
