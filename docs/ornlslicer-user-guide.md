# ORNLSlicer User Guide

This guide is the canonical user-facing manual for ORNLSlicer. It describes the
current graphical and command-line workflows, the major software components,
and the features available in the application. Release PDFs should be generated
from this Markdown source so that the documentation and packaged manual stay in
sync.

This edition documents the current development branch and is intended for the
next ORNLSlicer release. Earlier release packages can differ in available
settings, controller syntaxes, and workflows.

The exact settings shown in the application depend on the selected machine,
syntax, slicing mode, and other settings. When a setting is disabled or hidden,
hover over it for its current description and check the settings on which it
depends.

> **Safety notice:** ORNLSlicer produces machine instructions, but it does not
> certify that a toolpath is safe for a particular machine, material, fixture,
> or process. Review the preview and output, verify coordinate systems and
> machine limits, and follow the machine manufacturer's simulation, dry-run,
> and operating procedures before production use.

## Table of contents

- [1. ORNLSlicer at a glance](#1-ornlslicer-at-a-glance)
  - [1.1 What ORNLSlicer does](#11-what-ornlslicer-does)
  - [1.2 How the software is organized](#12-how-the-software-is-organized)
  - [1.3 Processing flow](#13-processing-flow)
- [2. Install and start ORNLSlicer](#2-install-and-start-ornlslicer)
  - [2.1 Release packages](#21-release-packages)
  - [2.2 First-launch checklist](#22-first-launch-checklist)
- [3. Quick-start workflow](#3-quick-start-workflow)
  - [3.1 Create a toolpath](#31-create-a-toolpath)
  - [3.2 Supported files](#32-supported-files)
- [4. Main window and navigation](#4-main-window-and-navigation)
  - [4.1 Main window layout](#41-main-window-layout)
  - [4.2 Main control toolbar](#42-main-control-toolbar)
  - [4.3 Part View](#43-part-view)
  - [4.4 G-Code View](#44-g-code-view)
  - [4.5 Settings, editor, timing, and status panels](#45-settings-editor-timing-and-status-panels)
  - [4.6 Menus and shortcuts](#46-menus-and-shortcuts)
  - [4.7 Customize the workspace](#47-customize-the-workspace)
- [5. Models and the build scene](#5-models-and-the-build-scene)
  - [5.1 Load models and point clouds](#51-load-models-and-point-clouds)
  - [5.2 Model roles](#52-model-roles)
  - [5.3 Create primitive geometry](#53-create-primitive-geometry)
  - [5.4 Select and organize parts](#54-select-and-organize-parts)
  - [5.5 Transform and align parts](#55-transform-and-align-parts)
  - [5.6 Inspect model geometry](#56-inspect-model-geometry)
- [6. Settings and templates](#6-settings-and-templates)
  - [6.1 Settings scopes and precedence](#61-settings-scopes-and-precedence)
  - [6.2 Settings panels](#62-settings-panels)
  - [6.3 Edit and find settings](#63-edit-and-find-settings)
  - [6.4 Load and save templates](#64-load-and-save-templates)
  - [6.5 Part settings](#65-part-settings)
  - [6.6 Layer and layer-range settings](#66-layer-and-layer-range-settings)
  - [6.7 Spatial settings regions](#67-spatial-settings-regions)
- [7. Slicing and toolpath features](#7-slicing-and-toolpath-features)
  - [7.1 Choose a slicing mode](#71-choose-a-slicing-mode)
  - [7.2 Planar slicing](#72-planar-slicing)
  - [7.3 Planar path regions](#73-planar-path-regions)
  - [7.4 Infill and skin patterns](#74-infill-and-skin-patterns)
  - [7.5 Support structures](#75-support-structures)
  - [7.6 Platform adhesion](#76-platform-adhesion)
  - [7.7 Travel and path modifiers](#77-travel-and-path-modifiers)
  - [7.8 Ordering and optimization](#78-ordering-and-optimization)
  - [7.9 Laser and thermal scanning](#79-laser-and-thermal-scanning)
  - [7.10 Cylindrical slicing](#710-cylindrical-slicing)
  - [7.11 Image slicing](#711-image-slicing)
  - [7.12 Run or cancel a slice](#712-run-or-cancel-a-slice)
- [8. G-code generation and inspection](#8-g-code-generation-and-inspection)
  - [8.1 Controller syntax](#81-controller-syntax)
  - [8.2 G-code preview](#82-g-code-preview)
  - [8.3 G-code editor](#83-g-code-editor)
  - [8.4 Layer times and statistics](#84-layer-times-and-statistics)
  - [8.5 Import G-code](#85-import-g-code)
  - [8.6 Export G-code and companion files](#86-export-g-code-and-companion-files)
- [9. Projects and recovery](#9-projects-and-recovery)
  - [9.1 Save and load projects](#91-save-and-load-projects)
  - [9.2 Start a new project](#92-start-a-new-project)
  - [9.3 Restore the last session](#93-restore-the-last-session)
- [10. Preferences and utilities](#10-preferences-and-utilities)
  - [10.1 Application preferences](#101-application-preferences)
  - [10.2 Flowrate Calculator](#102-flowrate-calculator)
  - [10.3 Xtrude Calculator](#103-xtrude-calculator)
- [11. Command-line workflow](#11-command-line-workflow)
  - [11.1 Command-line structure](#111-command-line-structure)
  - [11.2 Common examples](#112-common-examples)
  - [11.3 Command-line options](#113-command-line-options)
- [12. Troubleshooting and support](#12-troubleshooting-and-support)
- [Appendix A. Settings feature map](#appendix-a-settings-feature-map)
- [Appendix B. Controller syntaxes](#appendix-b-controller-syntaxes)
- [Appendix C. Shortcut reference](#appendix-c-shortcut-reference)
- [Appendix D. Glossary](#appendix-d-glossary)

## 1. ORNLSlicer at a glance

### 1.1 What ORNLSlicer does

ORNLSlicer converts 3-D geometry into manufacturing instructions. It is aimed
at additive-manufacturing research and production workflows, including pellet
and filament extrusion, wire-arc and laser-wire deposition, concrete and
thermoset deposition, and machine-specific research processes.

The application can:

- load mesh, CAD, point-cloud, project, template, and G-code files;
- arrange multiple build parts and apply clipping or spatial settings geometry;
- generate planar, cylindrical, or image-based slices;
- create perimeters, insets, skeletons, skin, infill, support, adhesion, scan,
  travel, and process-modifier paths where the selected mode supports them;
- order and optimize paths across parts and layers;
- write several controller-specific G-code dialects;
- parse generated or imported G-code for text, timing, statistics, and 2-D or
  3-D visualization; and
- save complete projects and export controller files with optional companion
  and project files.

> **Diagram placeholder — Overview:** Add a representative image showing a
> model in Part View beside its generated toolpath in G-Code View.

### 1.2 How the software is organized

ORNLSlicer has two user entry points that share the same session, settings, and
slicing implementations. Toolpath modes also share writers and G-code parsing;
Image mode writes raster output directly.

| Area | User-facing responsibility |
| --- | --- |
| Graphical application | Interactive model setup, settings, slicing, preview, editing, and export |
| Command-line application | Repeatable or automated model/project slicing and file output |
| Session and project layer | Loaded parts, transforms, local settings, project save/load, and active slicer |
| Settings layer | Defaults, templates, machine/material/profile settings, and local overrides |
| Preferences layer | Workstation theme, units, notifications, camera, and visualization choices |
| Slicing layer | Geometry preprocessing, cross-sections, regions, path generation, modifiers, and ordering |
| Writer and parser layer | Controller-specific output, imported/generated G-code parsing, and statistics |
| Visualization layer | Part geometry, slicing overlays, G-code segments, text selection, and timing views |

The graphical application is organized around a main window. Its central area
switches between **Part View** and **G-Code View**. The right side contains
tabbed **Settings**, **G-Code Editor**, and **Layer Times** panels, while a
**Status** panel reports operations and errors. Managers retain session,
settings, and preferences state so both the GUI and command-line paths use the
same slicing implementation.

For implementation-level ownership and extension points, see the
[architecture overview](https://github.com/ORNLSlicer/ORNLSlicer/blob/develop/ARCHITECTURE.md).

> **Diagram placeholder — Structure:** Add a component diagram showing GUI and
> CLI entry points sharing session, settings, and slicers, with a toolpath
> branch through writers/parsers and a direct Image-output branch.

### 1.3 Processing flow

A normal toolpath workflow moves through these stages:

1. Load one or more models, or restore a project.
2. Assign model roles and arrange the build scene.
3. Load a template and adjust global or local settings.
4. Select a slicing mode and preprocess the geometry.
5. Generate, modify, and order paths.
6. Translate the paths into the selected controller syntax.
7. Parse the generated file into preview geometry, text, timing, and summary
   statistics.
8. Inspect the result and export the required files.

Image slicing branches after preprocessing: it creates numbered image slices
and a part-ID map instead of ordinary machine G-code. Cylindrical slicing also
uses specialized radial or helical paths rather than the planar region stack.

> **Diagram placeholder — Processing flow:** Add an import-to-export flowchart
> with separate planar, cylindrical, and image branches.

## 2. Install and start ORNLSlicer

### 2.1 Release packages

Download a supported package from the
[ORNLSlicer releases page](https://github.com/ORNLSlicer/ORNLSlicer/releases).
Use the installer or portable package provided for Windows. On Linux, make the
AppImage executable before starting it:

```bash
chmod +x ORNLSlicer-*.AppImage
./ORNLSlicer-*.AppImage
```

Package names change between releases, so substitute the downloaded filename.
Developers building from source should follow the
[development environment guide](https://github.com/ORNLSlicer/ORNLSlicer/blob/develop/docs/wiki/Getting-Started.md)
instead of the release steps.

> **Diagram placeholder — Installation:** Add one Windows package-selection
> screenshot and one Linux AppImage launch example.

### 2.2 First-launch checklist

Before loading a production part:

1. Open **Settings > Application Preferences** and set the imported-model unit
   used by the source CAD files.
2. Set the preferred display units and theme.
3. Load an appropriate `.s2c` machine/process template with
   **Settings > Load from Template**.
4. Confirm **Printer > Machine Setup > Syntax**, machine type, build-volume
   dimensions, coordinate offsets, and speed limits.
5. Confirm the material, layer, bead-width, speed, and extrusion settings.
6. Save edited values as a new template rather than overwriting a packaged
   template.

Changing display units changes how values are shown. The selected controller
syntax and template determine the units and conventions used in output.

> **Diagram placeholder — First launch:** Add an annotated Preferences window
> and the Printer > Machine Setup settings panel.

## 3. Quick-start workflow

### 3.1 Create a toolpath

1. Select **File > Load Model for Building** or use the model button in the
   main toolbar.
2. Choose a supported model file. Repeat to add more parts.
3. In **Part View**, select each part and use the translate, rotate, scale,
   align, center, or drop-to-floor controls as needed.
4. Confirm that every printable part is inside the displayed build volume and
   intersects the expected build surface.
5. Load a template, then review the Printer, Material, Profile, and relevant
   Experimental settings.
6. Select **Profile > Slicing > Slicing Mode**. Use **Planar** for ordinary
   layered toolpaths, **Cylindrical** for radial/helical paths around a vertical
   axis, or **Image** for image slices.
7. Use the slicing-geometry and overhang overlays to inspect the setup.
8. Select **SLICE**, **File > Slice**, or press **Ctrl+G**.
9. After toolpath slicing completes, inspect the result in G-Code View. Review
   layers, travels, support, bead widths, segment details, text, timing, and
   summary statistics.
10. Select **Project > Export G-Code** and choose the output and companion-file
    options. Save an `.s2p` project if the setup must be reproduced later.

Always verify the generated file independently before running it on a machine.

> **Diagram placeholder — Quick start:** Add a numbered, eight-panel workflow
> from model load through export, combining adjacent inspection steps where
> needed.

### 3.2 Supported files

| Purpose | Extensions | Notes |
| --- | --- | --- |
| Model import | `.stl`, `.3mf`, `.obj`, `.amf`, `.step`, `.stp` | The GUI model picker exposes these formats. STEP/STP geometry is triangulated during import. |
| Point-cloud import | `.matrix`, `.xyz` | Available through **File > Load Point Cloud**. |
| ORNLSlicer project | `.s2p` | ZIP-based project archive containing geometry and settings state. |
| Global template | `.s2c` | Machine/process settings configuration. |
| Layer-bar template | `.s2l` | Reusable layer or layer-range settings. |
| G-code import | `.nc`, `.gcode`, `.mpf`, `.eia`, `.txt`, `.cli` | Successful preview depends on the parser understanding the file's syntax and metadata. |
| Generated output | Syntax dependent | The selected controller syntax determines the normal extension and companion files. |
| Image slicing | `.png`, plus `idFileLinks.dat` | Numbered layer images and a mapping from image IDs to source model names. |

File extensions describe the accepted container or input route, not guaranteed
compatibility with every producer. Validate complex CAD imports, third-party
G-code, and controller-specific files after loading.

> **Diagram placeholder — File types:** Add an input/output diagram grouping
> model, project, settings, G-code, and image files around ORNLSlicer.

## 4. Main window and navigation

### 4.1 Main window layout

The main window has these primary areas:

1. **Menu bar** for file, edit, view, settings, project, tools, help, and
   developer actions.
2. **Main control toolbar** for view switching, model/shape creation, overlays,
   preview controls, export, and slicing.
3. **Central view** switching between Part View and G-Code View.
4. **Layer bar** for layer-specific and layer-range settings.
5. **Right-side dock area** containing Settings, G-Code Editor, and Layer
   Times tabs.
6. **Status dock and status bar** for messages, statistics, errors, and progress.

The Settings, G-Code Editor, and Layer Times panels share a dock area by
default. Select their tabs to bring one to the front.

> **Diagram placeholder — Main window:** Add a current, numbered screenshot
> identifying all six main-window areas.

### 4.2 Main control toolbar

The main toolbar changes with the active view. Its common and Part View tools
include:

- **Part View / G-Code View tabs** to change the central view;
- a model menu for build, clipping, and settings models;
- a shape menu for generated build shapes and a box settings region;
- slicing-geometry, layer-range, optimization-point, overhang, and part-name
  overlays;
- **SLICE** to start slicing.

Once G-code is loaded, the toolbar also enables tools for segment information,
orthographic 2-D preview, ghosted source models, and G-code export. A disabled
tool normally means that the active view or loaded data does not support it.

> **Diagram placeholder — Main toolbar:** Add separate annotated toolbar strips
> for Part View and G-Code View, including enabled and disabled examples.

### 4.3 Part View

Part View is the build-scene editor. It displays the configured printer volume,
loaded geometry, generated primitives, part names, and optional diagnostic
overlays. Use it to:

- select one or more objects in the view or object tree;
- translate, rotate, scale, align, center, and drop selected parts;
- create parent/child relationships in the object tree;
- change model roles and visualization style through the context menu;
- position settings regions and clipping models;
- inspect support overhangs, slicing geometry, layer ranges, labels, and
  optimization points; and
- use isometric, front, side, or top camera projections.

Mouse navigation and selection behavior may vary with platform and the
**Invert Camera** preference. Middle-drag pans, right-drag rotates the camera,
and the wheel zooms. Arrow keys rotate the camera; modified arrow keys pan it.
When a part is selected, left-drag moves it in XY and a right-drag rotates it
with angle snapping. Use **View > Reset Camera** if the model is lost from view.

> **Diagram placeholder — Part View:** Add a labeled screenshot with the
> printer volume, object tree, transform toolbar, view controls, and three model
> roles visible.

### 4.4 G-Code View

G-Code View renders generated or imported motion. It supports:

- adjustable lower and upper layer limits;
- adjustable first and last segment limits;
- layer and segment playback;
- lightweight line or true-bead-width rendering;
- travel and support visibility filters;
- orthographic 2-D viewing;
- optional ghosted source models;
- segment picking and bead/segment information; and
- synchronization between picked segments and G-code source lines.

The preview is a verification aid. Controller state, macros, work offsets, and
commands that do not map to geometric motion may not be represented visually.

> **Diagram placeholder — G-Code View:** Add one screenshot showing colored
> paths and another showing the selected segment synchronized with its text
> line.

### 4.5 Settings, editor, timing, and status panels

| Panel | Purpose |
| --- | --- |
| Settings | Edits global, part, layer, layer-range, or settings-region values. Tabs group Printer, Material, Profile, and Experimental settings. |
| G-Code Editor | Displays and edits loaded G-code; searches text; filters preview types; controls visible layers/segments; refreshes the preview after edits. |
| Layer Times | Shows estimated timing and feed-rate adjustment information by layer. |
| Status | Reports load/slice/export progress, warnings, errors, and summary information. |

Selecting an object, layer marker, range, or settings region changes the scope
shown by the Settings panel. Confirm the active scope before editing.

> **Diagram placeholder — Dock panels:** Add a four-part figure showing each
> panel with its scope or key controls highlighted.

### 4.6 Menus and shortcuts

The main menus are:

| Menu | Common uses |
| --- | --- |
| File | Load models or point clouds, restore the last session, slice, take a screenshot, exit |
| Edit | Undo/redo, copy/paste, reload, delete |
| View | Switch views, reset camera, zoom, restore hidden settings, show/hide toolbars |
| Settings | Load/save templates, add settings locations, open preferences |
| Project | Create/load/save a project, import G-code, export G-code |
| Tools | Open the Flowrate and Xtrude process calculators |
| Scripts | Reserved integration menu; scripting is disabled in the standard application |
| Help | Open this manual, the repository, issue reporting, and version information |
| Debug | Developer diagnostics; not required for normal operation |

See [Appendix C](#appendix-c-shortcut-reference) for the current application
shortcuts.

> **Diagram placeholder — Menus:** Add a composite screenshot of File,
> Settings, Project, and View menus; use captions for the remaining menus.

### 4.7 Customize the workspace

The Settings, G-Code Editor, Layer Times, and Status panels are Qt dock widgets.
Depending on platform, they can be resized, moved, floated, tabbed together, or
hidden. Use **View > Toolbars** to restore a hidden dock. Use
**View > Hidden Settings** to restore settings categories hidden from the
Settings panel.

Workspace layout is separate from process settings. Moving a dock does not
change a project or generated toolpath.

> **Diagram placeholder — Workspace layout:** Add before-and-after screenshots
> of the default dock layout and a customized two-column layout.

## 5. Models and the build scene

### 5.1 Load models and point clouds

Use **File > Load Model for Building**, **Ctrl+O**, or the main toolbar's model
menu to import a printable model. The model menu also loads clipping and
settings models directly. Multiple files can be selected in one operation.

Use **File > Load Point Cloud** for `.matrix` or `.xyz` point data. Model import
uses the **Imported Model Unit** preference, so set it before loading files that
do not carry reliable unit metadata. Additional models may be shifted on load
to avoid collisions, according to the notification preferences.

> **Diagram placeholder — Model import:** Add the model-role menu, supported
> file dialog, and imported-model-unit preference in a three-panel figure.

### 5.2 Model roles

Every scene object has a role:

| Role | Behavior |
| --- | --- |
| Build | Printable geometry. The slicer creates output for it and can apply local settings. |
| Clipping | Subtractive geometry. Intersections are removed from build geometry; the clipping object does not create its own output. |
| Settings | Spatial override geometry. Local settings apply where it overlaps build geometry in planar slicing. |
| Support | Support input geometry consumed by Image mode, where it is rasterized with the reserved support ID instead of as an ordinary build part. |

Right-click selected objects and choose **Switch to Build**, **Switch to
Clipper**, or **Switch to Setting** to change the role. The same model can serve
different purposes without being re-imported.

> **Diagram placeholder — Model roles:** Add one build object intersected by a
> clipping object and a settings object, followed by the resulting sliced
> geometry.

### 5.3 Create primitive geometry

The shape menu can create geometry without an external CAD file:

- box settings region;
- rectangular prism;
- hexagonal prism;
- open-top rectangular prism;
- triangular pyramid;
- cylinder; and
- cone.

Enter the requested dimensions and a unique name. Generated build primitives
can be transformed and sliced like imported models. The box settings region is
created with the Settings role.

> **Diagram placeholder — Primitive geometry:** Add a labeled grid showing all
> generated shape types and the box settings region.

### 5.4 Select and organize parts

Select parts in the 3-D view or object tree. Double-click a part in the 3-D
view to toggle its selection. In the object tree, use Ctrl-click or the
platform's extended-selection gestures to select more than one. The object-tree
context menu supports:

- role changes;
- lock/unlock;
- reset transformation;
- replace model;
- reload from the original file;
- delete;
- transparency;
- wireframe; and
- solid wireframe.

The object tree also marks open geometry, floating parts, and objects outside
the configured build volume. Resolve these warnings before slicing unless they
are deliberate for the selected process.

Drag one object onto another in the object tree to make it a child. Translating
or rotating the parent also affects its children, which is useful for keeping
build, clipping, and settings geometry registered as a group. Drag the child
back to the top level to remove the relationship.

Copy and paste duplicate selected parts. Reload keeps the scene object but
re-reads its source geometry; replace selects a different source file.

> **Diagram placeholder — Part organization:** Add an object-tree example with
> multiple selection, a parent/child group, role icons, and the context menu.

### 5.5 Transform and align parts

The Part View transform toolbar provides:

| Tool | Use |
| --- | --- |
| Translation | Enter X, Y, and Z position changes in the preferred distance unit. |
| Rotation | Enter rotations about X, Y, and Z. Applying rotation commits any pending scale into the current transform and resets scale factors. |
| Scale | Apply independent X, Y, and Z scale factors. Values must be greater than zero. |
| Align | Choose a face/direction to align with the build surface. |
| Center | Center selected geometry in the configured build volume. |
| Drop to floor | Move selected geometry onto the build surface. |

Confirm transformed geometry remains inside the printer volume. If a model
loads at the wrong size, correct the import-unit preference and reload it where
possible instead of relying on an unexplained scale factor.

> **Diagram placeholder — Transform tools:** Add a six-panel sequence showing
> translation, rotation, scale, align, center, and drop-to-floor results.

### 5.6 Inspect model geometry

Use the Part View overlays before slicing:

- **Slicing geometry** previews planar slice planes or cylindrical radii and
  axes for each part.
- **Layer settings range** shows the vertical extent of the selected layer
  range.
- **Optimization points** shows draggable custom points used by enabled path
  ordering options.
- **Support overhangs** colors faces relevant to support-angle decisions.
- **Part names** adds labels to distinguish scene objects.
- **Wireframe, solid wireframe, and transparency** reveal intersections and
  internal registration.

These overlays are visual aids and are not written as manufacturing paths.
Use **File > Take Screenshot** to save the current Part View framebuffer as
PNG, JPG, GIF, or TIF; PNG is used when no extension is entered.

> **Diagram placeholder — Model inspection:** Add one labeled screenshot per
> overlay, using the same model and camera position for comparison.

## 6. Settings and templates

### 6.1 Settings scopes and precedence

For planar slicing, effective values are built from broadest to narrowest:

1. built-in defaults;
2. the active global template and global edits;
3. selected part settings;
4. matching layer or layer-range settings; and
5. overlapping spatial settings-region values.

A narrower supported scope overrides the broader value for the affected
geometry. Not every setting is local, and not every slicing mode consumes every
scope. Cylindrical slicing supports global and part overrides but does not
currently apply Layer Bar markers, layer ranges, or spatial settings polygons.
Image slicing reads active global values.

Confirm the active scope label in the Settings panel before making a change.

> **Diagram placeholder — Settings precedence:** Add a stacked diagram from
> defaults through global, part, layer/range, and spatial-region values, with
> mode-specific exceptions called out.

### 6.2 Settings panels

Settings are grouped by intent:

| Panel | Main responsibilities |
| --- | --- |
| Printer | Controller syntax, machine type, coordinate setup, build volume, auxiliary equipment, speed/acceleration limits, and machine G-code |
| Material | Density, path start/slowdown/wipe/lift modifiers, purge and extrusion behavior, filament/retraction, temperature/cooling, adhesion, and multi-material behavior |
| Profile | Slicing mode, layer geometry, path regions, support/travel, local G-code, special modes, ordering/optimization, and scanners |
| Experimental | Auto speed ramping, companion-file output, and cross-section stitching controls |

The complete category map is in
[Appendix A](#appendix-a-settings-feature-map). Settings that do not apply to
the current choices are disabled or hidden according to preferences.

> **Diagram placeholder — Settings panels:** Add one screenshot of each major
> settings panel with its category tabs labeled.

### 6.3 Edit and find settings

Select a major panel and category, then edit a value using its generated input
control. Use the settings search to locate a display name across categories,
and hover over a setting for its description. Composite inputs, such as the
slice-plane normal or custom cylinder axis, group independently stored
components into one row.

Dependencies prevent invalid or irrelevant combinations. For example, image
pixel size is shown only in Image mode and cylindrical boundary policies are
shown only for the matching radial or helical pattern. In
**Application Preferences > Settings**, choose whether unavailable settings are
greyed out or hidden.

Individual category tabs can be hidden. Restore them with
**View > Hidden Settings**, or use **Show All Settings**.

> **Diagram placeholder — Setting dependencies:** Add a before-and-after image
> showing settings enabled by changing Slicing Mode and the hidden-settings
> recovery menu.

### 6.4 Load and save templates

Use **Settings > Load from Template** or **Ctrl+T** to load a `.s2c` global
template. Use **Settings > Save as Template** or **Ctrl+Shift+T** to save the
current values under a new name. The save dialog can choose which settings
and categories are included and can search the available values.

Packaged templates provide starting points and may be refreshed by updates.
Save site-, machine-, material-, or process-specific changes as a separate user
template. Use **Additional Setting Location** to search another directory for
global templates. Layer-bar templates use `.s2l` and have a separate additional
location.

A template is not a substitute for machine validation. Review dimensions,
coordinates, syntax, speeds, and startup/shutdown commands whenever a template
moves between systems.

> **Diagram placeholder — Templates:** Add the template load dialog, save
> dialog with panel selection, and additional-location chooser.

### 6.5 Part settings

Select one build part and edit a setting marked as local to override the global
value for that part. Select multiple compatible parts to edit their shared
scope. Part settings travel with the part inside an `.s2p` project.

Use part settings when separate build objects need different bead widths,
speeds, infill, path options, or other locally supported values. Keep machine
configuration global unless the UI explicitly permits a local override.

> **Diagram placeholder — Part settings:** Add two adjacent parts using
> different local infill or speed values, with the active part scope visible.

### 6.6 Layer and layer-range settings

Selecting a build part populates the layer bar from the current layer height.
Add and select a marker to edit one layer. Select and pair two markers to edit a
continuous range. The toolbar's layer-range overlay displays the selected
height interval in Part View.

Marker context actions allow the layer number to be changed, local values to be
cleared, or the marker/range to be deleted. The layer bar also supports
multi-selection and range grouping, pairing, splitting, and unpairing where
applicable. Recheck layer markers after changing layer height or geometry
because the physical layer positions may change.

Reusable layer-bar templates use `.s2l`. Place site-provided templates in an
installed or additional layer-bar settings location when the same process
transition is used repeatedly.

> **Diagram placeholder — Layer settings:** Add a layer bar with one single
> layer and one paired range, plus the corresponding 3-D range overlay.

### 6.7 Spatial settings regions

Load a Settings model or create a box settings region, place it over a build
part, and select it to edit locally supported values. During planar slicing,
overlapping geometry receives the region's effective settings. Paths may be
split at the region boundary where required so each portion carries the correct
process values.

Settings regions are useful for localized reinforcement, slower critical
faces, different infill near holes, or other process zones. Keep them registered
with a build part by using parent/child relationships when appropriate.

Spatial settings regions are a planar-slicing feature; do not assume that
cylindrical or image modes apply them.

> **Diagram placeholder — Settings regions:** Add a transparent settings box
> intersecting a part and a preview showing the changed path density or speed
> inside the box.

## 7. Slicing and toolpath features

### 7.1 Choose a slicing mode

Set **Profile > Slicing > Slicing Mode**:

| Mode | Output model | Typical use |
| --- | --- | --- |
| Planar | Flat cross-sections and region-based toolpaths | Conventional layered additive manufacturing and most advanced path features |
| Cylindrical | Radial rings/arcs or rising helices around a vertical cylinder axis | Rotary/cylindrical deposition using Arc Specialties output |
| Image | Numbered raster cross-sections | Image-based processes and downstream image consumers |

Changing the mode reveals its specific controls. Confirm all dependent values
before slicing.

> **Diagram placeholder — Slicing modes:** Add a decision tree and one sample
> result for Planar, Cylindrical/Radial, Cylindrical/Helical, and Image.

### 7.2 Planar slicing

Planar mode intersects each build model with a sequence of planes. The
**Slice Plane Normal** sets their orientation; `{0, 0, 1}` creates ordinary
horizontal XY layers. **Layer Height** sets the plane spacing.

For each cross-section, Planar mode can create printable islands and enabled
regions, combine compatible layers across multiple parts, order the resulting
paths, insert travels, apply modifiers, and write controller output. Clipping
models remove intersecting geometry, while supported part, layer-range, and
settings-region overrides affect the result.

Use **Enable Fix Model** cautiously for geometry repair and inspect the result.
It is applied when a model is loaded or reloaded, not when **SLICE** is pressed;
enable it before import or reload the model after changing it. Use smoothing,
spiralize, or oversize options only after confirming how they change the
intended geometry.

> **Diagram placeholder — Planar slicing:** Add a model cut into planes, one
> polygon cross-section, and the resulting ordered regions for a single layer.

### 7.3 Planar path regions

Planar layers support these primary region types:

| Region | Purpose |
| --- | --- |
| Perimeter | Exterior boundary contours, with optional lead-in, flying start, reversal, and spiral behavior |
| Inset | Additional inward contours with configurable count, overlap, width, speed, and spiral behavior |
| Skeleton | Centerline-style paths from segment or point input, with cleaning, adaptive bead-width, and prestart options |
| Skin | Solid top/bottom coverage and optional gradual-infill transitions |
| Infill | Interior sparse or dense fill with density/spacing, pattern, angle, overlap, and combine-layer controls |
| Support | Generated structures below overhangs, including interfaces, bases, and optional organic branching |
| Scan | Optional laser or thermal scan motion associated with print layers |

Each enabled region can have its own bead width, speed, extrusion value,
minimum path length, start/end G-code, and supported path modifiers. The
**Region Order** setting controls their requested order within an island.

> **Diagram placeholder — Path regions:** Add one color-coded layer containing
> perimeter, inset, skeleton, skin, infill, support, and scan examples with a
> legend.

### 7.4 Infill and skin patterns

Infill can be controlled by density or manual line spacing. Available infill
patterns are Lines, Grid, Concentric, Triangles, Hexagons and Triangles,
Honeycomb, and Radial Hatch. Angle and per-layer angle rotation orient the
pattern. Optional controls can anchor patterns to printer coordinates, order
partitioned line segments, combine infill across layers, and limit path length.

Skin covers configured top and bottom layers. Its pattern, angle, overlap,
width, speed, and extrusion can be controlled separately. Gradual infill adds
transitional dense layers below skin where enabled.

Pattern availability and behavior depend on geometry and other settings.
Inspect narrow features, small islands, overlaps, and short-path filtering in
the preview.

> **Diagram placeholder — Fill patterns:** Add a comparison grid for every
> infill pattern and a cutaway showing skin plus gradual-infill layers.

### 7.5 Support structures

Enable **Profile > Support > Enable Support** to generate support under
qualifying overhangs. Major controls include:

- grid or organic/tree structure;
- support everywhere or build-plate-only placement;
- overhang threshold angle and XY/layer separation;
- sparse line/grid fill and wall contours;
- dense interface layers and interface expansion;
- solid base layers and base expansion;
- taper angle and retained tube-wall contours;
- bridge suppression and connectivity validation; and
- organic branch diameter, contact spacing, and branch angle.

Use the support-overhang overlay before slicing, then use **Hide Support** in
G-Code View to compare the build paths with and without support. Generated
planar support is distinct from support-role model files, which are currently
consumed only by Image mode.

> **Diagram placeholder — Support:** Add the overhang overlay and side-by-side
> grid and organic support previews, labeling interface and base layers.

### 7.6 Platform adhesion

**Material > Platform Adhesion** can add:

- a **raft** below the part, with offset, layer count, and bead width;
- a **brim** attached around initial layers, with width, layer count, and bead
  width; or
- a **skirt** around the object, with loop count, distance, layer count,
  minimum length, and bead width.

These structures change first-layer geometry and may change required bed area,
material use, start position, and removal procedure. Confirm they remain inside
the build volume.

> **Diagram placeholder — Adhesion:** Add one first-layer comparison showing
> raft, brim, and skirt with dimensions labeled.

### 7.7 Travel and path modifiers

Travel settings control non-print moves, including travel speed, minimum travel
lengths, lift height, final lift, and optional pauses. Retraction can be enabled
for qualifying travels with separate retract and prime behavior.

Process modifiers can alter the beginning or end of region paths:

- start-up and ramp-up;
- slow down and lift;
- tip wipe;
- spiral lift;
- purge behavior;
- dynamic acceleration; and
- experimental automatic speed ramping.

Most modifiers have region-specific enablement and speed/extrusion controls.
Their availability also depends on machine type and syntax. Inspect the G-code
text as well as the geometric preview because some modifier effects are
controller commands rather than visible paths.

> **Diagram placeholder — Travel and modifiers:** Add an annotated path showing
> prestart, print, slowdown, tip wipe, lift, travel, retract, and prime phases.

### 7.8 Ordering and optimization

ORNLSlicer can order work at several levels:

- layers across parts, including grouping tolerance;
- islands within a layer;
- region types within an island;
- paths within a region; and
- points or directions within a path.

Optimization strategies include nearest, next-closest, custom-point, and other
available choices. Custom optimization points can be displayed and dragged in
Part View or G-Code View. Point-order controls can use minimum-distance rules,
segment breaking, local randomness, secondary locations, alternating layers,
and per-layer point increments.

Optimization changes travel and seam placement. Compare travel distance,
surface starts, thermal sequence, collision risk, and machine kinematics rather
than assuming the shortest path is always the best process path.

> **Diagram placeholder — Optimization:** Add the same multi-island layer
> before and after path ordering, with custom optimization points and travels
> visible.

### 7.9 Laser and thermal scanning

Optional scan settings can add measurement motion to planar workflows:

- laser scanning supports offsets, scan dimensions and resolution, scanner
  axis/orientation, bed/global scan choices, buffering, layer skip, and height
  map transmission;
- thermal scanning supports offsets and a temperature cutoff.

Scanner paths and auxiliary data depend on the selected syntax, hardware, and
export options. Confirm the writer emits the expected machine commands and
save auxiliary files when applicable.

> **Diagram placeholder — Scanning:** Add one print/scan layer pair showing the
> scanner footprint, offsets, scan direction, and generated auxiliary data.

### 7.10 Cylindrical slicing

Cylindrical mode creates paths around a vertical axis through each part's XY
centroid or a configured custom XY location. It requires **Arc Specialties**
syntax.

Choose a path pattern:

- **Radial** creates concentric rings or retained arcs. **Layer Height** is the
  radial spacing and **Default Bead Width** is the vertical bead spacing.
- **Helical** creates rising paths. **Layer Height** is the radial spacing and
  **Default Bead Width** is the rise per revolution.

Boundary policies control whether intersections are clipped, kept, discarded,
or retained through the highest Z intersection. **Arcs per Revolution** controls
G2/G3 subdivision when arc output is enabled. Helical paths can be split by
maximum path length.

Cylindrical mode does not generate the ordinary planar perimeter, inset, skin,
infill, support, or raft regions. Review axis placement, radial bounds, AP/CP
orientation fields, travel lifts, and active work coordinates carefully. See
the dedicated
[Cylindrical Slicing guide](https://github.com/ORNLSlicer/ORNLSlicer/blob/develop/docs/cylindrical-slicing.md)
for settings, boundary policies, output fields, limitations, and checks.

> **Diagram placeholder — Cylindrical slicing:** Add axis/radius notation and
> side-by-side radial and helical paths, including clipped boundary examples.

### 7.11 Image slicing

Image mode cross-sections build and support geometry into numbered PNG layers.
Pixel values identify source build models; support uses a reserved ID. The
`idFileLinks.dat` companion maps IDs back to model names. **Image Pixel Size**
sets the physical X and Y resolution, while **Layer Height** sets vertical
spacing. Set both pixel-size components to positive, nonzero values before
slicing.

Image mode skips normal G-code generation, parsing, timing, and G-Code View. In
the command-line workflow, model input plus an Image-mode `.s2c` template uses
`--output_location` for the image directory. A project that changes the mode to
Image is loaded after current command-line output routing, so do not rely on its
requested location; verify the generated directory before using the files.
Check image dimensions, origin, resolution, layer numbering, and ID mapping in
the downstream consumer.

> **Diagram placeholder — Image slicing:** Add a 3-D model, two labeled raster
> layers, pixel-ID legend, and example `idFileLinks.dat` mapping.

### 7.12 Run or cancel a slice

Start slicing with **SLICE**, **File > Slice**, or **Ctrl+G**. The progress
dialog reports preprocessing, path computation, postprocessing, G-code
generation, and loading stages as applicable. Select cancel to request that the
active operation stop.

Toolpath modes automatically load the generated file and switch to G-Code View
when successful. A failed validation or load reports details in the status
area. Correct the model, syntax, or settings and slice again. Each slice
currently rebuilds the selected mode's path data.

> **Diagram placeholder — Slice progress:** Add the progress dialog with every
> stage labeled and a successful transition to G-Code View.

## 8. G-code generation and inspection

### 8.1 Controller syntax

**Printer > Machine Setup > Syntax** chooses the writer and G-code metadata,
including units, comments, travel conventions, and file suffix. The available
syntaxes are listed in [Appendix B](#appendix-b-controller-syntaxes).

Syntax-specific settings may control tool and base coordinates, rotary axes,
G2/G3 support, forced G1 travel, default startup code, material-load commands,
custom start/layer/end code, and companion-file generation. Some syntaxes share
a common writer or parser while others have specialized behavior.

Switching syntax is a machine-level change. Reload the correct template and
review all dependent settings, commands, units, and output fields.

> **Diagram placeholder — Controller syntax:** Add a flow from machine-neutral
> paths through three example writers to differently formatted controller
> files.

### 8.2 G-code preview

After loading G-code, use the editor controls and main toolbar to inspect:

- selected layer and segment ranges;
- travel and support visibility;
- thin-line or true-bead-width rendering;
- layer-by-layer and segment-by-segment playback;
- source model ghosts;
- orthographic 2-D or 3-D projection;
- segment type, direction, print speed, extruder speed, length, layer, and
  source line in the segment-information panel;
- position, color, and bead width in the geometric preview; and
- selectable optimization points where enabled.

In **Application Preferences > Visualization**, Auto mode uses the configured
vertex threshold to avoid building an excessively large true-width preview.
True Bead Widths can bypass that threshold when the toolbar toggle is enabled;
Thin Lines disables true-width rendering. Large files may use a lightweight
base with true-width detail only for a smaller visible range.

> **Diagram placeholder — Preview modes:** Add the same layer rendered as thin
> lines, true bead widths, orthographic 2-D, and a ghosted 3-D view.

### 8.3 G-code editor

The G-Code Editor displays colorized source and links motion lines to preview
segments. Use the search field to find text. Selecting a compatible line
highlights its segment; picking a segment highlights its source line.

The editor is editable. After changing text, select **Refresh** to write the
edited document to a temporary file, parse it again, and update visualization
and statistics. Refresh does not rerun slicing and cannot validate arbitrary
controller macros.

Keep an external, reviewed copy of important manual edits. Reslicing replaces
the current generated text.

> **Diagram placeholder — G-code editor:** Add a search result, an edited line
> with Refresh enabled, and the synchronized preview selection.

### 8.4 Layer times and statistics

The G-code loader estimates per-layer and total motion time and reports
distance, material volume, mass, and related summary values when sufficient
metadata is available. The Layer Times panel shows original and adjusted timing
information, including minimum/maximum layer-time behavior where enabled.

Estimates depend on configured speeds, acceleration assumptions, command
interpretation, density, and controller behavior. Treat them as planning
values, not guaranteed cycle times or material measurements.

> **Diagram placeholder — Timing:** Add the Layer Times plot/table and the
> corresponding status summary for a multi-layer file.

### 8.5 Import G-code

Use **Project > Import G-Code** to load a supported controller file without
slicing a model. Generated ORNLSlicer files identify their syntax in the header.
For other files, parser detection depends on recognized headers and commands;
files without recognized metadata fall back to common parsing behavior and may
not have deterministic export units or naming.

Review parser warnings, path scale, coordinates, layer boundaries, travels,
arcs, extrusion state, and timing before relying on an imported preview.

> **Diagram placeholder — G-code import:** Add a recognized-header file and an
> unrecognized-header warning, each with its resulting preview.

### 8.6 Export G-code and companion files

Select **Project > Export G-Code** or the toolbar export button. The export
dialog can:

- add operator and description lines to the header;
- save the main G-code file;
- save applicable auxiliary files;
- save an `.s2p` project beside the output; and
- create a named subdirectory that bundles the selected files.

The selected syntax determines the normal extension. Experimental file-output
settings can create syntax-specific companion files for supported systems.
Sensor workflows may create numbered `.dat` files. Inspect every file in a
bundle and preserve their relative naming when the controller workflow depends
on it.

> **Diagram placeholder — Export:** Add the export dialog and an expanded
> output bundle showing G-code, project, sensor, and syntax-specific companion
> files.

## 9. Projects and recovery

### 9.1 Save and load projects

Use **Project > Save Project** or **Ctrl+S** to create an `.s2p` file. Use
**Project > Load Project** or **Ctrl+Shift+O** to restore it. A project archive
stores the loaded model data, part names and roles, transforms, global settings,
per-part settings, layer ranges, and version information.

Project files preserve a slicing setup but do not remove the need to validate
it after application, template, or machine changes. When loading an older
project, ORNLSlicer may migrate settings or ask how to handle part positions.
Review the scene and settings before slicing.

> **Diagram placeholder — Project archive:** Add a cutaway of an `.s2p` ZIP
> showing models, session data, global settings, local settings, and version
> metadata.

### 9.2 Start a new project

Use **Project > New Project** to clear the active scene and begin a new setup.
This action clears the parts without an unsaved-changes prompt and preserves
the active settings. Save the current project first when its scene must be
retained; load a different template if the next setup needs different settings.

Save reusable machine and process values as a template; save a complete
arrangement, including geometry and local overrides, as a project.

> **Diagram placeholder — New project:** Add the populated scene before the
> action and the empty scene afterward, with unchanged active settings called
> out.

### 9.3 Restore the last session

ORNLSlicer periodically keeps a last-session project in its application-data
location. Use **File > Restore Last Session** to recover the latest available
autosaved state after closing or restarting the application.

Autosave is a recovery aid, not a versioned backup. Save named projects at
meaningful milestones and retain exported machine files through the site's
normal revision and approval process.

> **Diagram placeholder — Recovery:** Add a restore-last-session sequence from
> application start to the recovered model, settings, and project title.

## 10. Preferences and utilities

### 10.1 Application preferences

Open **Settings > Application Preferences** or press **Ctrl+P**. Preferences are
workstation/user choices rather than manufacturing settings.

| Preference area | Controls |
| --- | --- |
| Theme | Application appearance |
| Units | Imported-model, distance, velocity, acceleration, angle, time, temperature, voltage, mass, rotation, and density display units |
| Notifications | Project-load shifting, pre-slice alignment, file-load collision shifting, unsaved-project warnings |
| Camera | Camera-direction inversion |
| Parts | Implicit transforms and automatic drop-to-bed behavior |
| Settings | Grey or hide disabled settings |
| Visualization | G-code preview mode and true-width vertex threshold |
| Visualization Colors | Colors assigned to path, region, travel, and modifier types |
| Lag | Layer and segment playback delays |

Preferences can be imported or exported as `.preferences` files from the
Preferences window's File menu. Restart the application if a theme or
integration does not update fully.

> **Diagram placeholder — Preferences:** Add the tab strip and a representative
> control from every preference area.

### 10.2 Flowrate Calculator

Open **Tools > Flowrate Calculator** to relate measured extruder RPM and mass
flow to bead width, layer height, desired print rate, material density, gantry
speed, and spindle speed. Choose a known material or enter another density.

The calculator is a process-planning aid. Transfer results to the appropriate
settings deliberately and validate them with material characterization and
machine trials.

> **Diagram placeholder — Flowrate Calculator:** Add a completed example with
> input and calculated fields distinguished by color or callouts.

### 10.3 Xtrude Calculator

Open **Tools > Xtrude Calculator** to estimate feed-per-revolution and explore
minimum-layer-time-, screw-speed-, or feed-rate-driven combinations using a
material density and two-minute test mass.

The calculator uses the preferred units. Confirm units before copying a result
to a template or machine program.

> **Diagram placeholder — Xtrude Calculator:** Add one completed example for
> each of the three calculation modes.

## 11. Command-line workflow

### 11.1 Command-line structure

Launching ORNLSlicer without extra arguments opens the GUI. Supplying arguments
starts the command-line controller. On Windows builds, the console executable
may be packaged as `ornlslicer_cli`; on other platforms it is normally the
`ornlslicer` executable. Use the name supplied by the installed package.

Run help before scripting a version:

```bash
ornlslicer --help
```

Each run must provide exactly one primary input source:

- one or more `--input_stl_files` values;
- `--input_stl_files_directory`; or
- `--input_project_file`.

An output location is required. For toolpath modes, treat it as the desired
output base path without an extension; the selected syntax adds its suffix.
Image mode with model input and an Image-mode `.s2c` instead uses it as the
directory that receives numbered images. A global `.s2c` settings file can be
supplied with model input but not with a project, because the project already
contains global settings. For reproducible machine output, supply a `.s2c` or
`.s2p`; model-only input uses the embedded master defaults, not the GUI's last
active template.

> **Diagram placeholder — CLI structure:** Add a terminal-to-pipeline diagram
> showing model/template and project alternatives converging on output.

### 11.2 Common examples

Slice one model with a global template:

```bash
ornlslicer \
  --input_stl_files part.stl \
  --input_global_settings machine-and-process.s2c \
  --output_location output/part
```

Slice several models by repeating the option:

```bash
ornlslicer \
  --input_stl_files body.stl \
  --input_stl_files insert.step \
  --input_global_settings machine-and-process.s2c \
  --output_location output/assembly
```

Slice every supported model in a directory:

```bash
ornlslicer \
  --input_stl_files_directory models \
  --input_global_settings machine-and-process.s2c \
  --output_location output/batch
```

Reproduce a saved project:

```bash
ornlslicer \
  --input_project_file build.s2p \
  --output_location output/reproduced-build
```

For Image mode, use model input with an Image-mode `.s2c`; this example also
adds support-role geometry:

```bash
ornlslicer \
  --input_stl_files build.stl \
  --input_support_stl_files support.stl \
  --input_global_settings image.s2c \
  --output_location output/images
```

PNG layers and `idFileLinks.dat` are written into the output location. Do not
use a project-based Image run when exact CLI output routing is required in the
current implementation.

> **Diagram placeholder — CLI examples:** Add terminal captures for a normal
> G-code run and an Image-mode output directory.

### 11.3 Command-line options

Use `--help` as the authoritative list for the installed version. Current
option groups include:

| Group | Options |
| --- | --- |
| Primary input | `--input_stl_files`, `--input_stl_files_directory`, `--input_project_file` |
| Support input | `--input_support_stl_files`, `--input_support_stl_files_directory` |
| Settings and transforms | `--input_global_settings`, `--input_stl_transform` |
| Output | `--output_location` |
| Placement | `--shift_parts_on_load`, `--align_parts`, `--use_implicit_transforms` |
| Image-layer selection | `--single_slice_height`, `--single_slice_layer_number` |
| Information | `--version`, `--help` |

Boolean and repeated-option syntax is shown by `--help`. Heights and layer
numbers for single-slice Image output are mutually exclusive. Model-directory
input recognizes the same model extensions listed in the supported-files
table. Although `--help` currently advertises export-control options and
`--slice_bounds`, the command-line controller does not apply them. Use the GUI
export dialog when header text, project copies, auxiliary files, bundled
output, or explicit slice bounds are required.

> **Diagram placeholder — CLI options:** Add a command anatomy figure labeling
> executable, primary input, template, placement, output, and advanced options.

## 12. Troubleshooting and support

| Symptom | Check |
| --- | --- |
| Model is the wrong size | Set **Imported Model Unit** before loading, then reload. Verify the source CAD export unit. |
| Slice is disabled | Load at least one object with the Build role. |
| Model is outside or above the printer | Check build-volume bounds and offsets; center, align, or drop the model. |
| Setting is grey or missing | Review its dependencies, switch disabled-setting visibility, or restore its category through **View > Hidden Settings**. |
| Cylindrical slicing refuses to start | Set **Printer > Machine Setup > Syntax** to Arc Specialties and verify the axis and pattern settings. |
| Preview is slow or uses thin lines | Reduce the visible layer/segment range or review the preview mode and true-width vertex threshold. |
| Imported G-code has missing or incorrect geometry | Check the syntax header, units, supported commands, arcs, layer markers, and parser warnings. |
| Edited G-code is not reflected in the preview | Select **Refresh** in the G-Code Editor. |
| Export lacks an expected companion file | Check the syntax, **Experimental > File Output** settings, auxiliary-file checkbox, and whether the workflow generated the source data. |
| Old project behaves differently | Review migrated settings, template version, part transforms, and every locally overridden scope before reslicing. |
| Application closed before a named save | Try **File > Restore Last Session**. |

For development-environment problems, see
[Troubleshooting](https://github.com/ORNLSlicer/ORNLSlicer/blob/develop/docs/wiki/Troubleshooting.md).
For behavior not covered here,
search or open an issue in the
[ORNLSlicer issue tracker](https://github.com/ORNLSlicer/ORNLSlicer/issues).
Include the application version from **Help > About**, operating system,
template or project details, exact steps, status/error text, and a minimal
reproducer when possible. Do not attach proprietary geometry or machine files
to a public issue without authorization.

> **Diagram placeholder — Troubleshooting:** Add a decision flow for load,
> setup, slice, preview, and export failures, pointing to the Status panel and
> issue-report details.

## Appendix A. Settings feature map

The Settings panel remains the detailed reference: hover over a value for its
description and use the active dependency state to determine applicability.
This appendix summarizes the current categories.

| Panel | Category | Features |
| --- | --- | --- |
| Printer | Machine Setup | Syntax, machine type, travel motion, arc support, tool/base coordinates, rotary axes |
| Printer | Dimensions | Rectangular/cylindrical volume, bounds/radius, offsets, W axis, doffing/purge locations, grids |
| Printer | Auxiliary | Tamper control |
| Printer | Machine Speeds | XY/extruder limits, table and Z speeds, gear ratio |
| Printer | Acceleration | Default and region-specific dynamic acceleration |
| Printer | G-Code | Default startup, material load, wait, bounds demo, settings footer, custom start/layer/end code |
| Material | Density | Material selection and custom density |
| Material | Start-Up | Region-specific prestart and ramp-up behavior |
| Material | Slow Down | Region-specific end slowdown, lift, speed, extrusion, and bead-area controls |
| Material | Tip Wipe | Region-specific wipe direction, distance, speed, angle, cutoff, lift, and voltage |
| Material | Spiral Lift | Region/end-layer spiral lift geometry and speed |
| Material | Purge | Initial and dwell purge timing, screw speed, wipe delay, purge path |
| Material | Extruder | Initial/prime settings, region delays, servo behavior, M3 control |
| Material | Filament | Diameter, relative extrusion, G92 removal, alternate filament axis |
| Material | Retraction | Travel thresholds, retract/prime length and speed, open-space/layer-change rules |
| Material | Temperatures | Bed, standby, and one- through five-zone extrusion temperatures |
| Material | Cooling | Fan range, layer-time controls, extrusion scaling, pre/post pause G-code |
| Material | Platform Adhesion | Raft, brim, and skirt geometry |
| Material | Multi-Material | Region material assignment, transition distances, controller command |
| Profile | Slicing | Planar/cylindrical/image mode, plane normal, cylinder axis/pattern/policies, image pixels |
| Profile | Layer | Layer height, nozzle and bead width, print/extruder speed, minimum extrusion length |
| Profile | Perimeter | Count/boundaries, width/speed/extrusion, minimum length, lead-in/flying start/spiral |
| Profile | Inset | Enable/count, width/speed/extrusion, minimum length, overlap, spiral |
| Profile | Skeleton | Input/cleaning, adaptive width, speed/extrusion, minimum length, prestart |
| Profile | Skin | Top/bottom count, patterns/angles, overlap, width/speed/extrusion, gradual infill |
| Profile | Infill | Density/spacing, patterns/angles, ordering, overlap, width/speed/extrusion, path limits, layer combining |
| Profile | Support | Grid/organic structure, placement, thresholds, fill/walls/interfaces/base, tapering, bridges/connectivity |
| Profile | Travel | Speeds/thresholds, lift/final lift, optional pause and centroid move |
| Profile | G-Code | Region-specific start and end snippets |
| Profile | Special Modes | Smoothing, spiralize, model repair, oversize geometry |
| Profile | Optimizations | Layer/island/path/point ordering, grouping, custom points, distance/randomness rules |
| Profile | Ordering | Region order and perimeter/inset direction reversal |
| Profile | Laser Scanner | Scan path, offsets, dimensions/resolution, orientation, bed/global scan, buffering |
| Profile | Thermal Scanner | Enablement, offsets, temperature cutoff |
| Experimental | Auto Speed Ramping | Angle trigger, ramp distances, speeds, extrusion speeds |
| Experimental | File Output | Syntax-specific companion and simulation outputs |
| Experimental | Cross-Sectioning | Gap and stitch tolerances |

> **Diagram placeholder — Settings map:** Add a foldout-style map of the four
> panels and their categories, suitable for both Markdown and PDF navigation.

## Appendix B. Controller syntaxes

The current **Syntax** setting offers:

- AeroBasic
- Adamantine
- AML3D
- Arc Specialties
- Beam
- Cincinnati
- Common
- Dmg Dmu
- Gudel
- Haas Inch
- Haas Metric
- Haas Metric No Comments
- Hurco
- Ingersoll
- JuggerBot3D
- KraussMaffei
- Mach4
- Marlin
- Mazak
- Meld
- Meltio
- MVP
- Okuma
- ORNL
- ORNL Metric
- RepRap
- RomiFanuc
- Sandia
- Siemens
- Thermwood
- Tormach
- Wolf

This list indicates selectable output integrations, not identical feature
support. Confirm units, suffix, comments, arcs, axes, startup/shutdown behavior,
parser support, and companion outputs for the chosen machine.

> **Diagram placeholder — Syntaxes:** Add a compatibility matrix grouping
> syntaxes by controller/machine family, normal units, suffix, arc support, and
> companion output. Populate it from verified writer/parser behavior.

## Appendix C. Shortcut reference

| Action | Shortcut |
| --- | --- |
| Load Model for Building | Ctrl+O |
| Slice | Ctrl+G |
| Undo | Ctrl+Z or platform standard |
| Redo | Platform standard |
| Copy | Ctrl+C |
| Paste | Ctrl+V |
| Reload selected model | Ctrl+R |
| Delete selected model | Ctrl+Delete |
| Zoom in | Ctrl+= |
| Zoom out | Ctrl+- |
| Reset zoom | Ctrl+0 |
| Load from Template | Ctrl+T |
| Save as Template | Ctrl+Shift+T |
| Application Preferences | Ctrl+P |
| Load Project | Ctrl+Shift+O |
| Save Project | Ctrl+S |

Shortcuts are application-wide. When focus is in a text field, copy and paste
operate on text; otherwise they operate on selected parts where supported.

> **Diagram placeholder — Shortcuts:** Add a keyboard layout highlighting the
> most common model, template, project, view, and slicing shortcuts.

## Appendix D. Glossary

| Term | Meaning |
| --- | --- |
| Bead | Deposited material represented by a printable segment with width and height. |
| Build model | Geometry that should produce manufacturing output. |
| Clipping model | Geometry subtracted from intersecting build geometry. |
| G-code syntax | Controller-specific formatting, units, commands, comments, and suffix rules. |
| Global setting | Active value used unless a supported narrower scope overrides it. |
| Island | One connected printable area within a planar layer. |
| Layer range | Consecutive layers sharing a set of local overrides. |
| Part | A loaded scene object together with its role, transforms, local settings, and computed output. |
| Path | Ordered collection of printable, travel, or scan segments. |
| Project | `.s2p` archive that preserves geometry and slicing state. |
| Region | A pathing purpose such as perimeter, inset, skin, infill, support, or scan. |
| Settings region | Spatial geometry that applies supported local values where it overlaps a planar build part. |
| Slicing mode | Planar, Cylindrical, or Image processing workflow. |
| Template | Reusable `.s2c` global or `.s2l` layer settings file. |
| Travel | Non-print motion connecting printable or scan paths. |

> **Diagram placeholder — Glossary:** Add a labeled single-layer toolpath that
> identifies island, region, path, segment, bead, and travel terms.
