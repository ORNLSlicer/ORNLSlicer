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
- [Appendix E. Detailed settings reference](#appendix-e-detailed-settings-reference)
  - [E.1 How to read this reference](#e1-how-to-read-this-reference)
  - [E.2 Printer settings](#e2-printer-settings)
  - [E.3 Material settings](#e3-material-settings)
  - [E.4 Profile settings](#e4-profile-settings)
  - [E.5 Experimental settings](#e5-experimental-settings)

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

![Figure 01 placeholder: Overview](user-guide-images/figure01.png)

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

![Figure 02 placeholder: Structure](user-guide-images/figure02.png)

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

![Figure 03 placeholder: Processing flow](user-guide-images/figure03.png)

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

![Figure 04 placeholder: Installation](user-guide-images/figure04.png)

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

![Figure 05 placeholder: First launch](user-guide-images/figure05.png)

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

![Figure 06 placeholder: Quick start](user-guide-images/figure06.png)

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

![Figure 07 placeholder: File types](user-guide-images/figure07.png)

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

![Figure 08 placeholder: Main window](user-guide-images/figure08.png)

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

![Figure 09 placeholder: Main toolbar](user-guide-images/figure09.png)

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

![Figure 10 placeholder: Part View](user-guide-images/figure10.png)

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

![Figure 11 placeholder: G-Code View](user-guide-images/figure11.png)

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

![Figure 12 placeholder: Dock panels](user-guide-images/figure12.png)

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

![Figure 13 placeholder: Menus](user-guide-images/figure13.png)

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

![Figure 14 placeholder: Workspace layout](user-guide-images/figure14.png)

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

![Figure 15 placeholder: Model import](user-guide-images/figure15.png)

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

![Figure 16 placeholder: Model roles](user-guide-images/figure16.png)

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

![Figure 17 placeholder: Primitive geometry](user-guide-images/figure17.png)

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

![Figure 18 placeholder: Part organization](user-guide-images/figure18.png)

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

![Figure 19 placeholder: Transform tools](user-guide-images/figure19.png)

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

![Figure 20 placeholder: Model inspection](user-guide-images/figure20.png)

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

![Figure 21 placeholder: Settings precedence](user-guide-images/figure21.png)

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
For every setting's purpose, input type, fallback default, scope, dependency,
and choices, see the generated
[detailed settings reference](#appendix-e-detailed-settings-reference).

![Figure 22 placeholder: Settings panels](user-guide-images/figure22.png)

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

![Figure 23 placeholder: Setting dependencies](user-guide-images/figure23.png)

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

![Figure 24 placeholder: Templates](user-guide-images/figure24.png)

> **Diagram placeholder — Templates:** Add the template load dialog, save
> dialog with panel selection, and additional-location chooser.

### 6.5 Part settings

Select one build part and edit a setting marked as local to override the global
value for that part. Select multiple compatible parts to edit their shared
scope. Part settings travel with the part inside an `.s2p` project.

Use part settings when separate build objects need different bead widths,
speeds, infill, path options, or other locally supported values. Keep machine
configuration global unless the UI explicitly permits a local override.

![Figure 25 placeholder: Part settings](user-guide-images/figure25.png)

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

![Figure 26 placeholder: Layer settings](user-guide-images/figure26.png)

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

![Figure 27 placeholder: Settings regions](user-guide-images/figure27.png)

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

![Figure 28 placeholder: Slicing modes](user-guide-images/figure28.png)

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

![Figure 29 placeholder: Planar slicing](user-guide-images/figure29.png)

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

![Figure 30 placeholder: Path regions](user-guide-images/figure30.png)

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

![Figure 31 placeholder: Fill patterns](user-guide-images/figure31.png)

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

![Figure 32 placeholder: Support](user-guide-images/figure32.png)

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

![Figure 33 placeholder: Adhesion](user-guide-images/figure33.png)

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

![Figure 34 placeholder: Travel and modifiers](user-guide-images/figure34.png)

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

![Figure 35 placeholder: Optimization](user-guide-images/figure35.png)

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

![Figure 36 placeholder: Scanning](user-guide-images/figure36.png)

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

![Figure 37 placeholder: Cylindrical slicing](user-guide-images/figure37.png)

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

![Figure 38 placeholder: Image slicing](user-guide-images/figure38.png)

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

![Figure 39 placeholder: Slice progress](user-guide-images/figure39.png)

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

![Figure 40 placeholder: Controller syntax](user-guide-images/figure40.png)

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

![Figure 41 placeholder: Preview modes](user-guide-images/figure41.png)

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

![Figure 42 placeholder: G-code editor](user-guide-images/figure42.png)

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

![Figure 43 placeholder: Timing](user-guide-images/figure43.png)

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

![Figure 44 placeholder: G-code import](user-guide-images/figure44.png)

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

![Figure 45 placeholder: Export](user-guide-images/figure45.png)

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

![Figure 46 placeholder: Project archive](user-guide-images/figure46.png)

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

![Figure 47 placeholder: New project](user-guide-images/figure47.png)

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

![Figure 48 placeholder: Recovery](user-guide-images/figure48.png)

> **Diagram placeholder — Recovery:** Add a restore-last-session sequence from
> application start to the recovered model, settings, and project title.

## 10. Preferences and utilities

### 10.1 Application preferences

Open **Settings > Application Preferences** or press **Ctrl+P**. Preferences are
workstation/user choices rather than manufacturing settings. ORNLSlicer stores
the normal preference file as `app.preferences` in its application-data
directory. Use the Preferences window's **File > Import Preferences** and
**File > Export Preferences** actions to exchange `.preferences` files; those
dialogs initially open on the desktop.

#### Theme

**Current Theme** appears above the preference tabs. Choose **System
(default)** for the packaged default light styling or **Dark Mode** for the
packaged dark theme. Restart ORNLSlicer if a theme does not update every open
window immediately.

#### Units

Unit preferences control how numeric values are displayed and entered. They do
not change the underlying physical quantity. Changing units can introduce
small rounding differences in displayed values.

| Preference | Purpose | Choices; default in bold |
| --- | --- | --- |
| Imported Model Unit | Scale applied when imported geometry does not provide usable units. Set this before loading the model. | in, ft, **mm**, cm, m, µm |
| Distance Unit | Display/input unit for distances, positions, and offsets. | **in**, ft, mm, cm, m, µm |
| Velocity | Display/input unit for linear speeds. | **in/sec**, in/min, ft/sec, mm/sec, mm/min, cm/sec, m/sec, µm/sec |
| Acceleration | Display/input unit for linear acceleration. | **in/sec²**, ft/sec², mm/sec², cm/sec², m/sec², µm/sec² |
| Angle | Display/input unit for angular values. | **deg**, rad, rev |
| Time | Display/input unit for durations. | **sec**, ms, min |
| Temperature | Display/input unit for thermal setpoints. | °C, °F, **°K** |
| Voltage | Display/input unit for voltage settings. The current selector contains two identical µV entries. | µV, µV, mV, **V** |
| Mass | Display unit for material-mass estimates and Xtrude Calculator values. | **kg**, g, mg, lbm |
| Rotation | Labels used for orientation components. | **Pitch/Roll/Yaw**, X/Y/Z |
| Density | Display/input unit for density values. | lbm/in³, **g/cm³** |

**Imported Model Unit** is separate from **Distance Unit**: the first affects
the size assigned during import, while the second changes only the unit used to
show and edit distances.

#### Notifications

Three notification groups use **Ask User**, **Don't ask - perform
automatically**, or **Don't ask - skip automatically** behavior.

| Preference | Default | Current behavior |
| --- | --- | --- |
| Load Project – Part Shift | Ask User | Stored for compatibility; the current graphical project loader does not consult this choice. |
| Align Part – Slice | Ask User | Stored for compatibility; the current graphical slicing path does not consult this choice. |
| Load Part – File Shift | Don't ask - shift automatically | For a newly loaded identity-transform part, center/drop it and optionally shift it to avoid collisions with existing parts. |
| Warn before closing unsaved projects | Enabled | Prompts when closing the application with unsaved project changes. It does not protect **Project > New Project**, which clears parts without a prompt. |

The first three rows are presented in the UI as radio-button groups. Their
complete labels use “shift automatically” or “don't shift” for shift choices,
and “align automatically” or “don't align” for alignment choices.

#### Camera, parts, and settings display

| Preference | Default | Effect |
| --- | --- | --- |
| Invert Camera | Disabled | Reverses the camera-drag direction in Part View and G-Code View. |
| Use implicit transforms | Disabled | When a model has no supplied transform, preserve its original centroid as a translation after mesh data is internally centered. |
| Always drop parts to bed | Disabled | Drops newly transformed parts to the build surface when their transform changes. |
| Disabled Settings | Grey | **Grey** keeps dependency-unavailable manufacturing settings visible but disabled; **Hide** removes them until their dependencies are met. This is separate from manually hiding entire category tabs, which **View > Hidden Settings** restores. |

#### Visualization and playback

| Preference | Default | Effect and range |
| --- | --- | --- |
| G-code preview mode | Auto | **Auto** uses the true-width vertex threshold; **True Bead Widths** permits width meshes without that automatic cutoff when requested; **Thin Lines** disables true-width meshes. |
| True-width vertex threshold | 5,000,000 | Auto-mode vertex limit; allowed range 0–50,000,000 in steps of 100,000. Lower it to reduce preview construction cost. |
| Lag between layers | 100 ms | Delay used by layer playback; allowed range 1–5,000 ms. |
| Lag between segments | 10 ms | Delay used by segment playback; allowed range 1–5,000 ms. |

**Use implicit transforms**, **Always drop parts to bed**, and the two playback
lag values are included in imported/exported preference data. In the current
GUI, changing one of these values by itself may not mark the normal preference
file for saving; export the preferences or change another preference before
closing if the value must persist to the next session.

The G-Code Editor separately persists **Hide Travel** (disabled by default),
**Hide Support** (disabled), and **True Bead Widths** (enabled). Starting
segment playback automatically reveals travel and support so the full motion
sequence can be shown.

#### Visualization colors

Each color preference opens a color picker and has a reset action. Defaults are
listed as 8-bit RGB values.

| Path or modifier | Default RGB | Path or modifier | Default RGB |
| --- | --- | --- | --- |
| Brim | 200, 113, 55 | Coasting | 211, 95, 141 |
| Infill | 0, 255, 0 | Initial Startup | 135, 222, 205 |
| Inset | 0, 204, 255 | Laser Scan | 90, 255, 90 |
| Lead In | 255, 153, 51 | Flying Start | 120, 150, 250 |
| Perimeter | 0, 0, 255 | Prestart | 204, 0, 255 |
| Raft | 102, 102, 102 | Radial | 47, 82, 102 |
| Ramping Down | 22, 99, 137 | Ramping Up | 99, 22, 137 |
| Skeleton | 160, 44, 44 | Skin | 0, 128, 0 |
| Skirt | 211, 188, 95 | Slow Down | 44, 160, 137 |
| Spiral Lift | 113, 55, 200 | Support | 255, 102, 0 |
| Support Roof | 255, 179, 128 | Thermal Scan | 240, 130, 130 |
| Tip Wipe Angled | 179, 128, 255 | Tip Wipe Forward | 179, 128, 255 |
| Tip Wipe Reverse | 179, 128, 255 | Travel | 233, 175, 198 |
| Unknown | 0, 0, 0 |  |  |

![Figure 49 placeholder: Preferences](user-guide-images/figure49.png)

> **Diagram placeholder — Preferences:** Add the tab strip and a representative
> control from every preference area.

### 10.2 Flowrate Calculator

Open **Tools > Flowrate Calculator** to relate measured extruder RPM and mass
flow to bead width, layer height, desired print rate, material density, gantry
speed, and spindle speed. Choose a known material or enter another density.

The calculator is a process-planning aid. Transfer results to the appropriate
settings deliberately and validate them with material characterization and
machine trials.

![Figure 50 placeholder: Flowrate Calculator](user-guide-images/figure50.png)

> **Diagram placeholder — Flowrate Calculator:** Add a completed example with
> input and calculated fields distinguished by color or callouts.

### 10.3 Xtrude Calculator

Open **Tools > Xtrude Calculator** to estimate feed-per-revolution and explore
minimum-layer-time-, screw-speed-, or feed-rate-driven combinations using a
material density and two-minute test mass.

The calculator uses the preferred units. Confirm units before copying a result
to a template or machine program.

![Figure 51 placeholder: Xtrude Calculator](user-guide-images/figure51.png)

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

![Figure 52 placeholder: CLI structure](user-guide-images/figure52.png)

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

![Figure 53 placeholder: CLI examples](user-guide-images/figure53.png)

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

![Figure 54 placeholder: CLI options](user-guide-images/figure54.png)

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

![Figure 55 placeholder: Troubleshooting](user-guide-images/figure55.png)

> **Diagram placeholder — Troubleshooting:** Add a decision flow for load,
> setup, slice, preview, and export failures, pointing to the Status panel and
> issue-report details.

## Appendix A. Settings feature map

Hover over a value in the Settings panel for its description and use the active
dependency state to determine applicability. This appendix summarizes the
categories; [Appendix E](#appendix-e-detailed-settings-reference) documents
every individual setting.

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
| Profile | Special Modes | Smoothing, spiralize, model repair, oversize geometry, bead width/height HMI output |
| Profile | Optimizations | Layer/island/path/point ordering, grouping, custom points, distance/randomness rules |
| Profile | Ordering | Region order and perimeter/inset direction reversal |
| Profile | Laser Scanner | Scan path, offsets, dimensions/resolution, orientation, bed/global scan, buffering |
| Profile | Thermal Scanner | Enablement, offsets, temperature cutoff |
| Experimental | Auto Speed Ramping | Angle trigger, ramp distances, speeds, extrusion speeds |
| Experimental | File Output | Syntax-specific companion and simulation outputs |
| Experimental | Cross-Sectioning | Gap and stitch tolerances |

![Figure 56 placeholder: Settings map](user-guide-images/figure56.png)

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

![Figure 57 placeholder: Syntaxes](user-guide-images/figure57.png)

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

![Figure 58 placeholder: Shortcuts](user-guide-images/figure58.png)

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

![Figure 59 placeholder: Glossary](user-guide-images/figure59.png)

> **Diagram placeholder — Glossary:** Add a labeled single-layer toolpath that
> identifies island, region, path, segment, bead, and travel terms.

<!-- BEGIN GENERATED SETTINGS REFERENCE -->
## Appendix E. Detailed settings reference

This generated appendix documents all 544 scalar manufacturing settings exposed by the canonical catalog. The 5 grouped controls combine related scalar values, producing 537 visible setting rows across 37 categories.

Do not edit this appendix directly. Update `resources/settings/*.yaml` for setting metadata and the documented mappings in `scripts/generate_settings_reference.py` for choice-level or implementation notes, then run the generator. It validates the source catalog and replaces everything between the generated-reference markers.

### E.1 How to read this reference

Each scalar entry starts with the user-visible label and stable persisted key. Grouped-control headings instead show the input metadata name; their component lists identify the keys stored in templates and projects. **Master default** is the fallback compiled into ORNLSlicer; a loaded template or project normally supplies practical machine and process values. Physical defaults below use canonical metric units for readability, while the application displays values in the units selected under Application Preferences.

**Available when** translates the application's dependency expression into labels and choices. A disabled setting may remain grey or be hidden, according to the Disabled Settings preference. **Local-capable** means metadata permits a narrower override; see Section 6.1 for slicing-mode limits.

Use the browser or PDF search for either a visible label or an internal key. Every scalar key also has a stable Markdown target in the form `#setting-<key>`, such as `#setting-layer_height`.

| Metadata | Meaning |
| --- | --- |
| Purpose | The same behavior description shown as the setting's tooltip, normalized for the manual. |
| Input | Widget/value type and its unit family or allowed range. |
| Master default | Built-in fallback before a template, project, or user override is applied. |
| Scope | Whether the setting is global-only or eligible for supported local overrides. |
| Available when | The selections or toggles that enable the setting. |
| Choices | Every selectable value for enumeration settings, in stored order. |

![Figure 60 placeholder: Setting anatomy](user-guide-images/figure60.png)

> **Diagram placeholder — Setting anatomy:** Add one annotated setting row showing its label, input, unit,
> tooltip, disabled state, local-override indicator, and corresponding reference entry.

The catalog is organized as follows:

| Panel | Category | Scalar settings |
| --- | --- | ---: |
| [Printer](#e2-printer-settings) | [Machine Setup](#settings-printer-machine-setup) | 17 |
| [Printer](#e2-printer-settings) | [Dimensions](#settings-printer-dimensions) | 28 |
| [Printer](#e2-printer-settings) | [Auxiliary](#settings-printer-auxiliary) | 2 |
| [Printer](#e2-printer-settings) | [Machine Speeds](#settings-printer-machine-speeds) | 7 |
| [Printer](#e2-printer-settings) | [Acceleration](#settings-printer-acceleration) | 8 |
| [Printer](#e2-printer-settings) | [G-Code](#settings-printer-g-code) | 10 |
| [Material](#e3-material-settings) | [Density](#settings-material-density) | 2 |
| [Material](#e3-material-settings) | [Start-Up](#settings-material-start-up) | 32 |
| [Material](#e3-material-settings) | [Slow Down](#settings-material-slow-down) | 32 |
| [Material](#e3-material-settings) | [Tip Wipe](#settings-material-tip-wipe) | 42 |
| [Material](#e3-material-settings) | [Spiral Lift](#settings-material-spiral-lift) | 10 |
| [Material](#e3-material-settings) | [Purge](#settings-material-purge) | 9 |
| [Material](#e3-material-settings) | [Extruder](#settings-material-extruder) | 11 |
| [Material](#e3-material-settings) | [Filament](#settings-material-filament) | 4 |
| [Material](#e3-material-settings) | [Retraction](#settings-material-retraction) | 8 |
| [Material](#e3-material-settings) | [Temperatures](#settings-material-temperatures) | 12 |
| [Material](#e3-material-settings) | [Cooling](#settings-material-cooling) | 10 |
| [Material](#e3-material-settings) | [Platform Adhesion](#settings-material-platform-adhesion) | 14 |
| [Material](#e3-material-settings) | [Multi-Material](#settings-material-multi-material) | 11 |
| [Profile](#e4-profile-settings) | [Slicing](#settings-profile-slicing) | 20 |
| [Profile](#e4-profile-settings) | [Layer](#settings-profile-layer) | 6 |
| [Profile](#e4-profile-settings) | [Perimeter](#settings-profile-perimeter) | 20 |
| [Profile](#e4-profile-settings) | [Inset](#settings-profile-inset) | 13 |
| [Profile](#e4-profile-settings) | [Skeleton](#settings-profile-skeleton) | 23 |
| [Profile](#e4-profile-settings) | [Skin](#settings-profile-skin) | 18 |
| [Profile](#e4-profile-settings) | [Infill](#settings-profile-infill) | 20 |
| [Profile](#e4-profile-settings) | [Support](#settings-profile-support) | 29 |
| [Profile](#e4-profile-settings) | [Travel](#settings-profile-travel) | 9 |
| [Profile](#e4-profile-settings) | [G-Code](#settings-profile-g-code) | 12 |
| [Profile](#e4-profile-settings) | [Special Modes](#settings-profile-special-modes) | 16 |
| [Profile](#e4-profile-settings) | [Optimizations](#settings-profile-optimizations) | 34 |
| [Profile](#e4-profile-settings) | [Ordering](#settings-profile-ordering) | 3 |
| [Profile](#e4-profile-settings) | [Laser Scanner](#settings-profile-laser-scanner) | 23 |
| [Profile](#e4-profile-settings) | [Thermal Scanner](#settings-profile-thermal-scanner) | 4 |
| [Experimental](#e5-experimental-settings) | [Auto Speed Ramping](#settings-experimental-auto-speed-ramping) | 8 |
| [Experimental](#e5-experimental-settings) | [File Output](#settings-experimental-file-output) | 15 |
| [Experimental](#e5-experimental-settings) | [Cross-Sectioning](#settings-experimental-cross-sectioning) | 2 |

### E.2 Printer settings

Printer settings describe the controller, coordinate system, build envelope, machine limits, and
machine-level G-code. Treat them as machine configuration and verify them against the physical
system.

![Figure 61 placeholder: Printer settings](user-guide-images/figure61.png)

> **Diagram placeholder — Printer settings:** Add an annotated Printer panel with its
> category tabs, search field, and one enabled/disabled dependency example.

<a id="settings-printer-machine-setup"></a>

#### Printer > Machine Setup

Selects controller syntax, machine process, motion-command behavior, coordinates, tools, and
rotary-axis values.

<a id="setting-syntax"></a>

##### Syntax (`syntax`)

Selects the controller-specific G-code writer conventions, including units, comments, motion
commands, file suffix, and supported auxiliary output. G-code import detects its parser separately
from file content.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Beam`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.
- **Choices:**
  - `Beam` — Currently falls back to the Cincinnati writer: inch coordinates, inch/min feeds,
    parenthesized comments, and an .nc suffix.
  - `Cincinnati` — Cincinnati writer: inch coordinates, inch/min feeds, parenthesized comments, .nc
    suffix; enables laser-scanner and simulation-output settings.
  - `Common` — Currently falls back to the Cincinnati writer: inch coordinates, inch/min feeds,
    parenthesized comments, and an .nc suffix.
  - `Dmg Dmu` — DMG/DMU writer: mm coordinates, mm/min feeds, semicolon comments, .mpf suffix.
  - `Gudel` — Gudel writer: mm coordinates, mm/min feeds, semicolon comments, .mpf suffix.
  - `Haas Inch` — Haas writer: inch coordinates, inch/min feeds, parenthesized comments, .nc suffix.
  - `Haas Metric` — Haas writer: mm coordinates, mm/min feeds, parenthesized comments, .nc suffix.
  - `Haas Metric No Comments` — Haas metric writer: mm coordinates, mm/min feeds, comments omitted,
    .nc suffix.
  - `Hurco` — Hurco writer: mm coordinates, mm/min feeds, parenthesized comments, .nc suffix.
  - `Ingersoll` — Ingersoll writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.
  - `Marlin` — Marlin writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix;
    enables Marlin companion-output settings.
  - `JuggerBot3D` — JuggerBot writer using Marlin units: mm coordinates, mm/min feeds, .gcode
    suffix; enables simulation-output settings.
  - `Mazak` — Mazak writer: mm coordinates, mm/min feeds, parenthesized comments, .eia suffix.
  - `MVP` — MVP writer: mm coordinates, mm/min feeds, semicolon comments, .mpf suffix.
  - `RomiFanuc` — Romi Fanuc writer: mm coordinates, mm/min feeds, parenthesized comments, .txt
    suffix.
  - `Siemens` — Siemens writer: inch coordinates, inch/min feeds, semicolon comments, .mpf suffix.
  - `Thermwood` — Thermwood writer using Cincinnati units: inch coordinates, inch/min feeds, .nc
    suffix.
  - `Wolf` — Wolf writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.
  - `RepRap` — RepRap writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.
  - `Mach4` — Mach4 writer using Marlin units: mm coordinates, mm/min feeds, .gcode suffix.
  - `AeroBasic` — AeroBasic writer: mm coordinates, mm/s feeds, apostrophe comments, .gcode suffix.
  - `Meld` — Meld writer: inch coordinates, inch/min feeds, parenthesized comments, .nc suffix;
    enables Meld companion and discrete-feed settings.
  - `ORNL` — ORNL writer: inch coordinates, inch/min feeds, parenthesized comments, .gcode suffix;
    enables AMCM output and data-logging settings.
  - `Okuma` — Okuma writer using Haas metric units: mm coordinates, mm/min feeds, .nc suffix.
  - `Tormach` — Tormach writer: mm coordinates, mm/min feeds, semicolon comments, .nc suffix;
    enables Tormach companion-output settings.
  - `AML3D` — AML3D writer: mm coordinates, mm/s feeds, parenthesized comments, .gcode suffix;
    enables AML3D companion and weave settings.
  - `KraussMaffei` — KraussMaffei writer: mm coordinates, mm/min feeds, semicolon comments, .gcode
    suffix.
  - `Sandia` — Sandia writer: mm coordinates, m/s feeds, semicolon comments, .gcode suffix; enables
    Sandia auxiliary-output settings.
  - `Meltio` — Meltio writer: mm coordinates, mm/min feeds, parenthesized comments, .nc suffix.
  - `Adamantine` — Adamantine writer: metre coordinates, m/s feeds, parenthesized comments, .txt
    suffix.
  - `ORNL Metric` — ORNL writer: mm coordinates, mm/min feeds, parenthesized comments, .gcode
    suffix; enables AMCM output and data-logging settings.
  - `Arc Specialties` — Arc Specialties writer: mm coordinates, mm/min feeds, semicolon comments,
    .nc suffix.
- **Implementation note:** Each choice selects the corresponding installed writer dialect. Units,
  comments, commands, suffixes, parsing, and companion outputs are dialect-specific. Arc emission is
  controlled separately by Supports G2/G3. Always inspect and validate generated output against the
  target controller before running it.

<a id="setting-machine_type"></a>

##### Machine Type (`machine_type`)

Selects the deposition process used by the machine. This choice enables process-specific material,
extrusion, temperature, and path settings.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Pellet`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.
- **Choices:**
  - `Pellet`
  - `Filament`
  - `Wire_Arc`
  - `Laser_Wire`
  - `Concrete`
  - `Thermoset`

<a id="setting-force_G1"></a>

##### Force G1 Travels (`force_G1`)

Forces travel moves to use G1 rather than G0.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Cincinnati` or Syntax is `Ingersoll`).

<a id="setting-supports_G2_3"></a>

##### Supports G2/G3 (`supports_G2_3`)

If selected, supported arc moves are emitted as G2/G3 commands. Otherwise, the same geometry is
approximated with G1 line segments.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-enable_trafo"></a>

##### Enable TRAFO (`enable_trafo`)

Controls whether generated G-code enables the machine TRAFO transformation mode during initial
setup.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Enabled` (`true`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Arc Specialties`.

<a id="setting-g2_g3_center_point_interpretation"></a>

##### G2/G3 Center Point Interpretation (`g2_g3_center_point_interpretation`)

Selects whether G2/G3 I and J arc center coordinates are absolute positions or relative distances
from the arc start point.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Absolute`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Arc Specialties` and Supports G2/G3 is enabled).
- **Choices:**
  - `Absolute`
  - `Relative`

<a id="setting-g2_g3_absolute_center"></a>
<a id="setting-g2_g3_absolute_i"></a>
<a id="setting-g2_g3_absolute_j"></a>

##### G2/G3 Absolute Center (`g2_g3_absolute_center`)

Absolute I and J coordinates used for Arc Specialties G2/G3 arc centers.

- **Input:** `vector2` grouped control with the components listed below.
- **Scope:** Global only. Configure the grouped value in the active global settings.
- **Available when:** (G2/G3 Center Point Interpretation is `Absolute` and (Syntax is `Arc
  Specialties` and Supports G2/G3 is enabled)).
- **Components and master defaults:**
  - **I:** `g2_g3_absolute_i` — `0 mm`
  - **J:** `g2_g3_absolute_j` — `0 mm`

<a id="setting-tool_number"></a>

##### Tool Number (`tool_number`)

Sets the tool number for the primary extruder.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-tool_coordinate"></a>

##### Tool Coordinate (`tool_coordinate`)

Sets the tool coordinate for the primary extruder.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `ORNL` or Syntax is `ORNL Metric`).

<a id="setting-base_coordinate"></a>

##### Base Coordinate (`base_coordinate`)

Sets the base coordinate for the primary extruder.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `ORNL` or Syntax is `ORNL Metric`).

<a id="setting-axis_a"></a>

##### Axis A (`axis_a`)

Sets the printing angle for the A axis.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Arc Specialties` or (Syntax is `ORNL Metric` or Syntax is
  `ORNL`)).

<a id="setting-axis_b"></a>

##### Axis B (`axis_b`)

Sets the printing angle for the B axis.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Arc Specialties` or (Syntax is `ORNL Metric` or Syntax is
  `ORNL`)).

<a id="setting-axis_c"></a>

##### Axis C (`axis_c`)

Sets the printing angle for the C axis.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Arc Specialties` or (Syntax is `ORNL Metric` or Syntax is
  `ORNL`)).

<a id="setting-gcode_coordinate_frame_rotation_x"></a>

##### G-Code Frame Rotation X (`gcode_coordinate_frame_rotation_x`)

Rotates generated G-Code coordinates about the X axis before output.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-gcode_coordinate_frame_rotation_y"></a>

##### G-Code Frame Rotation Y (`gcode_coordinate_frame_rotation_y`)

Rotates generated G-Code coordinates about the Y axis before output.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-gcode_coordinate_frame_rotation_z"></a>

##### G-Code Frame Rotation Z (`gcode_coordinate_frame_rotation_z`)

Rotates generated G-Code coordinates about the Z axis before output.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="settings-printer-dimensions"></a>

#### Printer > Dimensions

Defines the build-volume shape, limits, offsets, auxiliary locations, and displayed floor grid.

<a id="setting-build_volume_type"></a>

##### Build Volume Type (`build_volume_type`)

Sets the general build volume type.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Rectangular`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.
- **Choices:**
  - `Rectangular`
  - `Cylindrical`

<a id="setting-minimum_x"></a>

##### Minimum X (`minimum_x`)

Minimum X axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Build Volume Type is `Rectangular`.

<a id="setting-maximum_x"></a>

##### Maximum X (`maximum_x`)

Maximum X axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Build Volume Type is `Rectangular`.

<a id="setting-minimum_y"></a>

##### Minimum Y (`minimum_y`)

Minimum Y axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Build Volume Type is `Rectangular`.

<a id="setting-maximum_y"></a>

##### Maximum Y (`maximum_y`)

Maximum Y axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Build Volume Type is `Rectangular`.

<a id="setting-minimum_z"></a>

##### Minimum Z (`minimum_z`)

Minimum Z axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-maximum_z"></a>

##### Maximum Z (`maximum_z`)

Maximum Z axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-outer_radius"></a>

##### Outer Radius (`outer_radius`)

Sets the radius of the cylindrical build volume, measured in the XY plane from the machine Z axis at
X=0, Y=0 to the outer boundary.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `1,000 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Build Volume Type is `Cylindrical`.

<a id="setting-x_offset"></a>

##### X Offset (`x_offset`)

Offset X position of origin from the minimum X axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-y_offset"></a>

##### Y Offset (`y_offset`)

Offset Y position of origin from the minimum Y axis value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-z_offset"></a>

##### Z Offset (`z_offset`)

Height of the Z axis where the nozzle touches the build surface (table at maximum value)

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-variable_for_z"></a>

##### Use Variable for Z Position (`variable_for_z`)

If selected, a variable, #200, is issued in place of the Z offset. Z motions are output as
mathematical operations on the variable.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-enable_w_axis"></a>

##### Enable W Axis (`enable_w_axis`)

If selected, W axis will be enabled for Z motion.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-minimum_w"></a>

##### Minimum W (`minimum_w`)

Sets the lower W-axis table-travel bound used for positioning and limit checks. Disable W motion
with Enable W Axis rather than this value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable W Axis is enabled.

<a id="setting-maximum_w"></a>

##### Maximum W (`maximum_w`)

Sets the upper W-axis table-travel bound and initial writer position. Disable W motion with Enable W
Axis rather than this value.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable W Axis is enabled.

<a id="setting-initial_w"></a>

##### Initial W (`initial_w`)

Initial W position when printing in Z only mode.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Enable W Axis is enabled and Layer Change is `Z_ONLY`).

<a id="setting-layer_change"></a>

##### Layer Change (`layer_change`)

What type of Z axis movement is used to change layers.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Z_ONLY`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.
- **Choices:**
  - `Z_ONLY`
  - `W_ONLY`
  - `BOTH_Z_AND_W`

<a id="setting-doffing"></a>

##### Use Doffing Station (`doffing`)

If selected, W Table will lower to a specific height at the end of print.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-doffing_location"></a>

##### Doffing Location (`doffing_location`)

Height for the W Table when doffing.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Cincinnati` and Use Doffing Station is enabled).

<a id="setting-purge_x"></a>

##### Purge X Location (`purge_x`)

X location for the purge routine.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Marlin`.

<a id="setting-purge_y"></a>

##### Purge Y Location (`purge_y`)

Y location for the purge routine.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Marlin`.

<a id="setting-purge_z"></a>

##### Purge Z Location (`purge_z`)

Z location for the purge routine.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Marlin`.

<a id="setting-enable_grid_x"></a>

##### Enable Grid X (`enable_grid_x`)

Enable grid rendering on the floor of build volume on x axis (vertical bars)

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-grid_x_distance"></a>

##### Grid X Distance (`grid_x_distance`)

Distance between x grid ticks (vertical bars)

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Grid X is enabled.

<a id="setting-grid_x_offset"></a>

##### Grid X Offset Distance (`grid_x_offset`)

Offset distance for location of first X grid line.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Grid X is enabled.

<a id="setting-enable_grid_y"></a>

##### Enable Grid Y (`enable_grid_y`)

Enable grid rendering on the floor of build volume on y axis (horizontal bars)

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-grid_y_distance"></a>

##### Grid Y Distance (`grid_y_distance`)

Distance between y grid ticks (horizontal bars)

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Grid Y is enabled.

<a id="setting-grid_y_offset"></a>

##### Grid Y Offset Distance (`grid_y_offset`)

Offset distance for location of first Y grid line.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Grid Y is enabled.

<a id="settings-printer-auxiliary"></a>

#### Printer > Auxiliary

Configures optional equipment that is separate from the primary deposition system.

<a id="setting-enable_tamper"></a>

##### Enable Tamper (`enable_tamper`)

If selected, will turn tamper on during all extrude moves.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-tamper_voltage"></a>

##### Tamper Voltage (`tamper_voltage`)

Voltage sent to the tamper to control it's speed.

- **Input:** `voltage` — Electrical potential; displayed in the preferred voltage unit.
- **Master default:** `0 V`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Tamper is enabled.

<a id="settings-printer-machine-speeds"></a>

#### Printer > Machine Speeds

Sets physical motion and extrusion-rate limits used by writers, validation, and time estimation.

<a id="setting-min_xy_speed"></a>

##### Minimum XY Speed (`min_xy_speed`)

Minimum XY speed of the machine.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-max_xy_speed"></a>

##### Maximum XY Speed (`max_xy_speed`)

Maximum XY speed of the machine.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-min_extruder_speed"></a>

##### Minimum Extruder Speed (`min_extruder_speed`)

Minimum speed of the extruder.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-max_extruder_speed"></a>

##### Maximum Extruder Speed (`max_extruder_speed`)

Maximum speed of the extruder.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-w_table_speed"></a>

##### W Table Speed (`w_table_speed`)

Speed to move the table.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable W Axis is enabled.

<a id="setting-z_speed"></a>

##### Z Speed (`z_speed`)

Speed for Z axis moves.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-extruder_gear_ratio"></a>

##### Extruder Gear Ratio (`extruder_gear_ratio`)

Sets the gear ratio for the extruder. This number is multiplied by any output RPM i.e. a Perimeter
RPM of 100 and Gear Ratio of 7 would output 700RPM to the G-Code.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Machine Type is `Pellet`.

<a id="settings-printer-acceleration"></a>

#### Printer > Acceleration

Sets default and region-specific acceleration values for syntaxes that emit dynamic acceleration
commands.

<a id="setting-enable_dynamic_acceleration"></a>

##### Enable Dynamic Acceleration (`enable_dynamic_acceleration`)

If selected, machine acceleration will be changed throughout the G-Code.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Cincinnati` or Syntax is `Marlin`).

<a id="setting-default_acceleration"></a>

##### Default Acceleration (`default_acceleration`)

Default acceleration value.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="setting-perimeter_acceleration"></a>

##### Perimeter Acceleration (`perimeter_acceleration`)

Acceleration value for perimeters.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="setting-inset_acceleration"></a>

##### Inset Acceleration (`inset_acceleration`)

Acceleration value for inset.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="setting-skin_acceleration"></a>

##### Skin Acceleration (`skin_acceleration`)

Acceleration value for skin.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="setting-infill_acceleration"></a>

##### Infill Acceleration (`infill_acceleration`)

Acceleration value for infill.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="setting-skeleton_acceleration"></a>

##### Skeleton Acceleration (`skeleton_acceleration`)

Acceleration value for skeletons.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="setting-support_acceleration"></a>

##### Support Acceleration (`support_acceleration`)

Acceleration value for support.

- **Input:** `accel` — Acceleration; displayed in the preferred acceleration unit.
- **Master default:** `0 mm/s²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Dynamic Acceleration is enabled.

<a id="settings-printer-g-code"></a>

#### Printer > G-Code

Controls machine-level startup, material loading, waits, boundary demonstrations, settings output,
and custom command blocks.

<a id="setting-enable_default_startup_code"></a>

##### Use Default Startup G-Code (`enable_default_startup_code`)

Adds the selected syntax's built-in startup commands to the print header. The exact commands are
writer-specific; inspect and verify the exported header.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-enable_material_load"></a>

##### Material Load (`enable_material_load`)

If selected, material loading/purging commands will be added to the header of the G-Code.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-enable_wait_for_user"></a>

##### Wait For User (`enable_wait_for_user`)

If selected, M0 wait for user commands are added at the end of the header for the operator to
initiate the print.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-enable_bounding_box"></a>

##### Demo Bounding Box (`enable_bounding_box`)

If selected, will trace a perimeter bounding box around the max X/Y coordinates of the object before
starting the print.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-enable_settings_footer"></a>

##### Settings Footer (`enable_settings_footer`)

If selected, will write all settings values at the end of the g-code file as comments.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Enabled` (`true`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-enable_layer_time_comments"></a>

##### Layer Time Comments (`enable_layer_time_comments`)

If selected, each layer marker in the G-Code includes the estimated layer time in seconds.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-arc_specialties_g2_g3_optional_stop"></a>

##### Arc Specialties G2/G3 Optional Stop (`arc_specialties_g2_g3_optional_stop`)

If selected, Arc Specialties G-Code adds an inline G81 optional stop routine to each G2/G3 arc move.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Arc Specialties` and Supports G2/G3 is enabled).

<a id="setting-start_code"></a>

##### Start Code (`start_code`)

Input G-Code to be executed before the start of the print.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-layer_change_code"></a>

##### Layer Change Code (`layer_change_code`)

Input G-Code to be executed at each layer change.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-end_code"></a>

##### End Code (`end_code`)

Input G-Code to be executed after the end of the print.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

### E.3 Material settings

Material settings control process behavior tied to the feedstock and deposition system, including
startup, extrusion, retraction, temperature, cooling, and first-layer adhesion.

![Figure 62 placeholder: Material settings](user-guide-images/figure62.png)

> **Diagram placeholder — Material settings:** Add an annotated Material panel with its
> category tabs, search field, and one enabled/disabled dependency example.

<a id="settings-material-density"></a>

#### Material > Density

Chooses a known feedstock density or supplies a custom density for mass and flow calculations.

<a id="setting-printing_material"></a>

##### Printing Material (`printing_material`)

Selects the built-in material density used to convert estimated deposited volume to mass. Choose
Other to enter a custom density.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `ABS20CF`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.
- **Choices:**
  - `ABS20CF` — Built-in density 1.139997 g/cm³ (20% carbon-fiber ABS).
  - `ABS` — Built-in density 1.069828 g/cm³.
  - `PPS` — Built-in density 1.349949 g/cm³.
  - `PPS50CF` — Built-in density 1.527931 g/cm³ (50% carbon-fiber PPS).
  - `PPSU` — Built-in density 1.289884 g/cm³.
  - `PPSU25CF` — Built-in density 1.381227 g/cm³ (25% carbon-fiber PPSU).
  - `PESU` — Built-in density 1.367387 g/cm³.
  - `PESU25CF` — Built-in density 1.472571 g/cm³ (25% carbon-fiber PESU).
  - `PLA` — Built-in density 1.251132 g/cm³.
  - `Concrete` — Built-in density 2.604679 g/cm³.
  - `Other` — Uses the value entered under Other Density.

<a id="setting-other_density"></a>

##### Other Density (`other_density`)

Sets the custom material density used for deposited-mass estimation when Printing Material is Other.

- **Input:** `density` — Density; displayed in the preferred density unit.
- **Master default:** `0 g/cm³`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Printing Material is `Other`.

<a id="settings-material-start-up"></a>

#### Material > Start-Up

Controls prestart and ramp-up motion at the beginning of printable region paths.

<a id="setting-perimeter_start-up"></a>

##### Enable Perimeter Start-Up (`perimeter_start-up`)

If selected, will move at a slower speed for a certain distance at the beginning of a perimeter
path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_start-up_distance"></a>

##### Perimeter Start-Up Distance (`perimeter_start-up_distance`)

Distance for the perimeter start-up move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Start-Up is enabled)).

<a id="setting-perimeter_start-up_speed"></a>

##### Perimeter Start-Up Speed (`perimeter_start-up_speed`)

Speed for the perimeter start-up move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Start-Up is enabled)).

<a id="setting-perimeter_start-up_extruder_speed"></a>

##### Perimeter Start-Up Extruder Speed (`perimeter_start-up_extruder_speed`)

Extruder Speed for the perimeter start-up move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Enable
  Perimeter Start-Up is enabled and Use Width and Height is disabled))).

<a id="setting-perimeter_start-up_ramp-up"></a>

##### Enable Perimeter Start-Up Ramp-Up (`perimeter_start-up_ramp-up`)

If selected, will ramp-up the extruder speed from the Perimeter Start-Up Extruder Speed to the
Perimeter Speed using short segments of increasing RPM.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Start-Up is enabled)).

<a id="setting-perimeter_start-up_steps"></a>

##### Perimeter Start-Up Ramp-Up Steps (`perimeter_start-up_steps`)

Numbers of steps to take while ramping up the extruder speed during perimeter start-up paths.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Enable
  Perimeter Start-Up Ramp-Up is enabled and Enable Perimeter Start-Up is enabled))).

<a id="setting-inset_start-up"></a>

##### Enable Inset Start-Up (`inset_start-up`)

If selected, will move at a slower speed for a certain distance at the beginning of an inset path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_start-up_distance"></a>

##### Inset Start-Up Distance (`inset_start-up_distance`)

Distance for the inset start-up move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset
  Start-Up is enabled)).

<a id="setting-inset_start-up_speed"></a>

##### Inset Start-Up Speed (`inset_start-up_speed`)

Speed for the inset start-up move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset
  Start-Up is enabled)).

<a id="setting-inset_start-up_extruder_speed"></a>

##### Inset Start-Up Extruder Speed (`inset_start-up_extruder_speed`)

Extruder Speed for the inset start-up move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Enable Inset
  Start-Up is enabled and Use Width and Height is disabled))).

<a id="setting-inset_start-up_ramp-up"></a>

##### Enable Inset Start-Up Ramp-Up (`inset_start-up_ramp-up`)

If selected, will ramp-up the extruder speed from the Inset Start-Up Extruder Speed to the Inset
Speed using short segments of increasing RPM.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset
  Start-Up is enabled)).

<a id="setting-inset_start-up_steps"></a>

##### Inset Start-Up Ramp-Up Steps (`inset_start-up_steps`)

Numbers of steps to take while ramping up the extruder speed during inset start-up paths.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Enable Inset
  Start-Up Ramp-Up is enabled and Enable Inset Start-Up is enabled))).

<a id="setting-skin_start-up"></a>

##### Enable Skin Start-Up (`skin_start-up`)

If selected, will move at a slower speed for a certain distance at the beginning of a skin path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_start-up_distance"></a>

##### Skin Start-Up Distance (`skin_start-up_distance`)

Distance for the skin start-up move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Start-Up
  is enabled)).

<a id="setting-skin_start-up_speed"></a>

##### Skin Start-Up Speed (`skin_start-up_speed`)

Speed for the skin start-up move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Start-Up
  is enabled)).

<a id="setting-skin_start-up_extruder_speed"></a>

##### Skin Start-Up Extruder Speed (`skin_start-up_extruder_speed`)

Extruder Speed for the skin start-up move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Enable Skin
  Start-Up is enabled and Use Width and Height is disabled))).

<a id="setting-skin_start-up_ramp-up"></a>

##### Enable Skin Start-Up Ramp-Up (`skin_start-up_ramp-up`)

If selected, will ramp-up the extruder speed from the Skin Start-Up Extruder Speed to the Skin Speed
using short segments of increasing RPM.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Start-Up
  is enabled)).

<a id="setting-skin_start-up_steps"></a>

##### Skin Start-Up Ramp-Up Steps (`skin_start-up_steps`)

Numbers of steps to take while ramping up the extruder speed during skin start-up paths.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Enable Skin
  Start-Up Ramp-Up is enabled and Enable Skin Start-Up is enabled))).

<a id="setting-infill_start-up"></a>

##### Enable Infill Start-Up (`infill_start-up`)

If selected, will move at a slower speed for a certain distance at the beginning of an infill path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_start-up_distance"></a>

##### Infill Start-Up Distance (`infill_start-up_distance`)

Distance for the infill start-up move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill
  Start-Up is enabled)).

<a id="setting-infill_start-up_speed"></a>

##### Infill Start-Up Speed (`infill_start-up_speed`)

Speed for the infill start-up move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill
  Start-Up is enabled)).

<a id="setting-infill_start-up_extruder_speed"></a>

##### Infill Start-Up Extruder Speed (`infill_start-up_extruder_speed`)

Extruder Speed for the infill start-up move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Enable Infill
  Start-Up is enabled and Use Width and Height is disabled))).

<a id="setting-infill_start-up_ramp-up"></a>

##### Enable Infill Start-Up Ramp-Up (`infill_start-up_ramp-up`)

If selected, will ramp-up the extruder speed from the Infill Start-Up Extruder Speed to the Infill
Speed using short segments of increasing RPM.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill
  Start-Up is enabled)).

<a id="setting-infill_start-up_steps"></a>

##### Infill Start-Up Ramp-Up Steps (`infill_start-up_steps`)

Numbers of steps to take while ramping up the extruder speed during infill start-up paths.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Enable Infill
  Start-Up Ramp-Up is enabled and Enable Infill Start-Up is enabled))).

<a id="setting-skeleton_start-up"></a>

##### Enable Skeleton Start-Up (`skeleton_start-up`)

If selected, will move at a slower speed for a certain distance at the beginning of a skeleton path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_start-up_distance"></a>

##### Skeleton Start-Up Distance (`skeleton_start-up_distance`)

Distance for the skeleton start-up move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Start-Up is enabled)).

<a id="setting-skeleton_start-up_speed"></a>

##### Skeleton Start-Up Speed (`skeleton_start-up_speed`)

Speed for the skeleton start-up move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Start-Up is enabled)).

<a id="setting-skeleton_start-up_extruder_speed"></a>

##### Skeleton Start-Up Extruder Speed (`skeleton_start-up_extruder_speed`)

Extruder Speed for the skeleton start-up move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Enable
  Skeleton Start-Up is enabled and Use Width and Height is disabled))).

<a id="setting-skeleton_start-up_ramp-up"></a>

##### Enable Skeleton Start-Up Ramp-Up (`skeleton_start-up_ramp-up`)

If selected, will ramp-up the extruder speed from the Skeleton Start-Up Extruder Speed to the
Skeleton Speed using short segments of increasing RPM.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Start-Up is enabled)).

<a id="setting-skeleton_start-up_steps"></a>

##### Skeleton Start-Up Ramp-Up Steps (`skeleton_start-up_steps`)

Numbers of steps to take while ramping up the extruder speed during skeleton start-up paths.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Enable
  Skeleton Start-Up Ramp-Up is enabled and Enable Skeleton Start-Up is enabled))).

<a id="setting-start-up_area_modifier"></a>

##### Start-Up Bead Area Modifier (`start-up_area_modifier`)

Percent multiplier for bead area of start-up paths.

- **Input:** `percentage` — Percentage input with an allowed range of 0–500%.
- **Master default:** `100%`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Use Width and Height is enabled and (Enable
  Perimeter Start-Up is enabled or (Enable Inset Start-Up is enabled or (Enable Skin Start-Up is
  enabled or (Enable Infill Start-Up is enabled or Enable Skeleton Start-Up is enabled)))))).

<a id="setting-disable_start-up_feedrate_scaling"></a>

##### Disable Feedrate Scaling for Start-Up (`disable_start-up_feedrate_scaling`)

If selected, minimum layer time feedrate adjustments will not change start-up path speeds.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `MODIFY FEEDRATE`)).

<a id="settings-material-slow-down"></a>

#### Material > Slow Down

Controls reduced speed, extrusion, and lift behavior near the end of printable region paths.

<a id="setting-perimeter_slow_down"></a>

##### Enable Perimeter Slow Down (`perimeter_slow_down`)

If selected, will move at a slower speed for a certain distance at the end of a perimeter path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_slow_down_distance"></a>

##### Perimeter Slow Down Distance (`perimeter_slow_down_distance`)

Distance for the perimeter slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Slow Down is enabled)).

<a id="setting-perimeter_slow_down_lift_distance"></a>

##### Perimeter Slow Down Lift Distance (`perimeter_slow_down_lift_distance`)

Distance to lift during the perimeter slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Slow Down is enabled)).

<a id="setting-perimeter_slow_down_speed"></a>

##### Perimeter Slow Down Speed (`perimeter_slow_down_speed`)

Speed for the perimeter slow down move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Slow Down is enabled)).

<a id="setting-perimeter_slow_down_extruder_speed"></a>

##### Perimeter Slow Down Extruder Speed (`perimeter_slow_down_extruder_speed`)

Extruder Speed for the perimeter slow down move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Enable
  Perimeter Slow Down is enabled and Use Width and Height is disabled))).

<a id="setting-perimeter_slow_down_extruder_off_distance"></a>

##### Perimeter Slow Down Extruder Cutoff Distance (`perimeter_slow_down_extruder_off_distance`)

Distance from the end of the perimeter slow down move to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Slow Down is enabled)).

<a id="setting-inset_slow_down"></a>

##### Enable Inset Slow Down (`inset_slow_down`)

If selected, will move at a slower speed for a certain distance at the end of an inset path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_slow_down_distance"></a>

##### Inset Slow Down Distance (`inset_slow_down_distance`)

Distance for the inset slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Slow
  Down is enabled)).

<a id="setting-inset_slow_down_lift_distance"></a>

##### Inset Slow Down Lift Distance (`inset_slow_down_lift_distance`)

Distance to lift during the inset slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Slow
  Down is enabled)).

<a id="setting-inset_slow_down_speed"></a>

##### Inset Slow Down Speed (`inset_slow_down_speed`)

Speed for the inset slow down move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Slow
  Down is enabled)).

<a id="setting-inset_slow_down_extruder_speed"></a>

##### Inset Slow Down Extruder Speed (`inset_slow_down_extruder_speed`)

Extruder Speed for the inset slow down move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Enable Inset Slow
  Down is enabled and Use Width and Height is disabled))).

<a id="setting-inset_slow_down_extruder_off_distance"></a>

##### Inset Slow Down Extruder Cutoff Distance (`inset_slow_down_extruder_off_distance`)

Distance from the end of the inset slow down move to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Slow
  Down is enabled)).

<a id="setting-skin_slow_down"></a>

##### Enable Skin Slow Down (`skin_slow_down`)

If selected, will move at a slower speed for a certain distance at the end of a skin path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_slow_down_distance"></a>

##### Skin Slow Down Distance (`skin_slow_down_distance`)

Distance for the skin slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Slow
  Down is enabled)).

<a id="setting-skin_slow_down_lift_distance"></a>

##### Skin Slow Down Lift Distance (`skin_slow_down_lift_distance`)

Distance to lift during the skin slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Slow
  Down is enabled)).

<a id="setting-skin_slow_down_speed"></a>

##### Skin Slow Down Speed (`skin_slow_down_speed`)

Speed for the skin slow down move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Slow
  Down is enabled)).

<a id="setting-skin_slow_down_extruder_speed"></a>

##### Skin Slow Down Extruder Speed (`skin_slow_down_extruder_speed`)

Extruder Speed for the skin slow down move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Enable Skin Slow
  Down is enabled and Use Width and Height is disabled))).

<a id="setting-skin_slow_down_extruder_off_distance"></a>

##### Skin Slow Down Extruder Cutoff Distance (`skin_slow_down_extruder_off_distance`)

Distance from the end of the skin slow down move to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Slow
  Down is enabled)).

<a id="setting-infill_slow_down"></a>

##### Enable Infill Slow Down (`infill_slow_down`)

If selected, will move at a slower speed for a certain distance at the end of an infill path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_slow_down_distance"></a>

##### Infill Slow Down Distance (`infill_slow_down_distance`)

Distance for the infill slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Slow
  Down is enabled)).

<a id="setting-infill_slow_down_lift_distance"></a>

##### Infill Slow Down Lift Distance (`infill_slow_down_lift_distance`)

Distance to lift during the infill slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Slow
  Down is enabled)).

<a id="setting-infill_slow_down_speed"></a>

##### Infill Slow Down Speed (`infill_slow_down_speed`)

Speed for the infill slow down move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Slow
  Down is enabled)).

<a id="setting-infill_slow_down_extruder_speed"></a>

##### Infill Slow Down Extruder Speed (`infill_slow_down_extruder_speed`)

Extruder Speed for the infill slow down move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Enable Infill
  Slow Down is enabled and Use Width and Height is disabled))).

<a id="setting-infill_slow_down_extruder_off_distance"></a>

##### Infill Slow Down Extruder Cutoff Distance (`infill_slow_down_extruder_off_distance`)

Distance from the end of the infill slow down move to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Slow
  Down is enabled)).

<a id="setting-skeleton_slow_down"></a>

##### Enable Skeleton Slow Down (`skeleton_slow_down`)

If selected, will move at a slower speed for a certain distance at the end of a skeleton path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_slow_down_distance"></a>

##### Skeleton Slow Down Distance (`skeleton_slow_down_distance`)

Distance for the skeleton slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Slow Down is enabled)).

<a id="setting-skeleton_slow_down_lift_distance"></a>

##### Skeleton Slow Down Lift Distance (`skeleton_slow_down_lift_distance`)

Distance to lift during the skeleton slow down move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Slow Down is enabled)).

<a id="setting-skeleton_slow_down_speed"></a>

##### Skeleton Slow Down Speed (`skeleton_slow_down_speed`)

Speed for the skeleton slow down move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Slow Down is enabled)).

<a id="setting-skeleton_slow_down_extruder_speed"></a>

##### Skeleton Slow Down Extruder Speed (`skeleton_slow_down_extruder_speed`)

Extruder Speed for the skeleton slow down move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Enable
  Skeleton Slow Down is enabled and Use Width and Height is disabled))).

<a id="setting-skeleton_slow_down_extruder_off_distance"></a>

##### Skeleton Slow Down Extruder Cutoff Distance (`skeleton_slow_down_extruder_off_distance`)

Distance from the end of the skeleton slow down move to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Slow Down is enabled)).

<a id="setting-slow_down_area_modifier"></a>

##### Slow Down Bead Area Modifier (`slow_down_area_modifier`)

Percent multiplier for bead area of slow down paths.

- **Input:** `percentage` — Percentage input with an allowed range of 0–500%.
- **Master default:** `100%`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Use Width and Height is enabled and (Enable
  Perimeter Slow Down is enabled or (Enable Inset Slow Down is enabled or (Enable Skin Slow Down is
  enabled or (Enable Infill Slow Down is enabled or Enable Skeleton Slow Down is enabled)))))).

<a id="setting-disable_slow_down_feedrate_scaling"></a>

##### Disable Feedrate Scaling for Slow Down (`disable_slow_down_feedrate_scaling`)

If selected, minimum layer time feedrate adjustments will not change slow down path speeds.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `MODIFY FEEDRATE`)).

<a id="settings-material-tip-wipe"></a>

#### Material > Tip Wipe

Controls wipe motion, direction, cutoff, lift, and voltage after selected printable regions.

<a id="setting-perimeter_wipe"></a>

##### Enable Perimeter Tip Wipe (`perimeter_wipe`)

If selected, will do a tip wipe for perimeter moves to prevent material from sticking up.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_wipe_direction"></a>

##### Perimeter Wipe Direction (`perimeter_wipe_direction`)

Selects the direction for perimeter tip wipes. WIPE_OPTIMAL currently follows the forward path
direction, matching WIPE_FORWARD.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `WIPE_OPTIMAL`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Tip Wipe is enabled)).
- **Choices:**
  - `WIPE_OPTIMAL`
  - `WIPE_FORWARD`
  - `WIPE_REVERSE`
  - `WIPE_ANGLED`

<a id="setting-perimeter_wipe_distance"></a>

##### Perimeter Wipe Distance (`perimeter_wipe_distance`)

Sets the travel distance for perimeter tip wiping. If using an incrementing wipe, this is the
initial distance.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Tip Wipe is enabled)).

<a id="setting-perimeter_wipe_speed"></a>

##### Perimeter Wipe Speed (`perimeter_wipe_speed`)

Sets the speed for the perimeter tip wipe move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Tip Wipe is enabled)).

<a id="setting-perimeter_wipe_extruder_speed"></a>

##### Perimeter Wipe Extruder Speed (`perimeter_wipe_extruder_speed`)

Sets the extruder speed for the perimeter tip wipe move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Tip Wipe is enabled)).

<a id="setting-perimeter_wipe_angle"></a>

##### Perimeter Wipe Angle (`perimeter_wipe_angle`)

Sets the angle of the perimeter tip wipe.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Enable
  Perimeter Tip Wipe is enabled and Perimeter Wipe Direction is `WIPE_ANGLED`))).

<a id="setting-perimeter_wipe_cutoff_distance"></a>

##### Perimeter Wipe Cutoff Distance (`perimeter_wipe_cutoff_distance`)

Sets the distance from the end of the perimeter tip wipe to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Tip Wipe is enabled)).

<a id="setting-perimeter_wipe_lift_height"></a>

##### Perimeter Wipe Lift Height (`perimeter_wipe_lift_height`)

Sets the distance to lift during the perimeter tip wipe move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Tip Wipe is enabled)).

<a id="setting-inset_wipe"></a>

##### Enable Inset Tip Wipe (`inset_wipe`)

If selected, will do a tip wipe for inset moves to prevent material from sticking up.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_wipe_direction"></a>

##### Inset Wipe Direction (`inset_wipe_direction`)

Selects the direction for inset tip wipes. WIPE_OPTIMAL currently follows the forward path
direction, matching WIPE_FORWARD.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `WIPE_OPTIMAL`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Tip
  Wipe is enabled)).
- **Choices:**
  - `WIPE_OPTIMAL`
  - `WIPE_FORWARD`
  - `WIPE_REVERSE`
  - `WIPE_ANGLED`

<a id="setting-inset_wipe_distance"></a>

##### Inset Wipe Distance (`inset_wipe_distance`)

Sets the travel distance for inset tip wiping. If using an incrementing wipe, this is the initial
distance.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Tip
  Wipe is enabled)).

<a id="setting-inset_wipe_speed"></a>

##### Inset Wipe Speed (`inset_wipe_speed`)

Sets the speed for the inset tip wipe move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Tip
  Wipe is enabled)).

<a id="setting-inset_wipe_extruder_speed"></a>

##### Inset Wipe Extruder Speed (`inset_wipe_extruder_speed`)

Sets the extruder speed for the inset tip wipe move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Tip
  Wipe is enabled)).

<a id="setting-inset_wipe_angle"></a>

##### Inset Wipe Angle (`inset_wipe_angle`)

Sets the angle of the inset tip wipe.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Enable Inset Tip
  Wipe is enabled and Inset Wipe Direction is `WIPE_ANGLED`))).

<a id="setting-inset_wipe_cutoff_distance"></a>

##### Inset Wipe Cutoff Distance (`inset_wipe_cutoff_distance`)

Sets the distance from the end of the inset tip wipe to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Tip
  Wipe is enabled)).

<a id="setting-inset_wipe_lift_height"></a>

##### Inset Wipe Lift Height (`inset_wipe_lift_height`)

Sets the distance to lift during the inset tip wipe move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Inset Tip
  Wipe is enabled)).

<a id="setting-skeleton_wipe"></a>

##### Enable Skeleton Tip Wipe (`skeleton_wipe`)

If selected, will do a tip wipe for skeleton moves to prevent material from sticking up.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_wipe_direction"></a>

##### Skeleton Wipe Direction (`skeleton_wipe_direction`)

Selects the direction for skeleton tip wipes. WIPE_OPTIMAL currently follows the forward path
direction, matching WIPE_FORWARD.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `WIPE_OPTIMAL`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Tip Wipe is enabled)).
- **Choices:**
  - `WIPE_OPTIMAL`
  - `WIPE_FORWARD`
  - `WIPE_REVERSE`
  - `WIPE_ANGLED`

<a id="setting-skeleton_wipe_distance"></a>

##### Skeleton Wipe Distance (`skeleton_wipe_distance`)

Sets the travel distance for skeleton tip wiping. If using an incrementing wipe, this is the initial
distance.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Tip Wipe is enabled)).

<a id="setting-skeleton_wipe_speed"></a>

##### Skeleton Wipe Speed (`skeleton_wipe_speed`)

Sets the speed for the skeleton tip wipe move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Tip Wipe is enabled)).

<a id="setting-skeleton_wipe_extruder_speed"></a>

##### Skeleton Wipe Extruder Speed (`skeleton_wipe_extruder_speed`)

Sets the extruder speed for the skeleton tip wipe move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Tip Wipe is enabled)).

<a id="setting-skeleton_wipe_angle"></a>

##### Skeleton Wipe Angle (`skeleton_wipe_angle`)

Sets the angle of the skeleton tip wipe.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Enable
  Skeleton Tip Wipe is enabled and Skeleton Wipe Direction is `WIPE_ANGLED`))).

<a id="setting-skeleton_wipe_cutoff_distance"></a>

##### Skeleton Wipe Cutoff Distance (`skeleton_wipe_cutoff_distance`)

Sets the distance from the end of the skeleton tip wipe to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Tip Wipe is enabled)).

<a id="setting-skeleton_wipe_lift_height"></a>

##### Skeleton Wipe Lift Height (`skeleton_wipe_lift_height`)

Sets the distance to lift during the skeleton tip wipe move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Tip Wipe is enabled)).

<a id="setting-skin_wipe"></a>

##### Enable Skin Tip Wipe (`skin_wipe`)

If selected, will do a tip wipe for skin moves to prevent material from sticking up.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_wipe_direction"></a>

##### Skin Wipe Direction (`skin_wipe_direction`)

Selects the direction for skin tip wipes. WIPE_OPTIMAL uses a forward wipe when inset or perimeter
paths are enabled, or when the fill pattern is Concentric; otherwise it uses a reverse wipe.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `WIPE_OPTIMAL`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Tip Wipe
  is enabled)).
- **Choices:**
  - `WIPE_OPTIMAL`
  - `WIPE_FORWARD`
  - `WIPE_REVERSE`
  - `WIPE_ANGLED`

<a id="setting-skin_wipe_distance"></a>

##### Skin Wipe Distance (`skin_wipe_distance`)

Sets the travel distance for skin tip wiping. If using an incrementing wipe, this is the initial
distance.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Tip Wipe
  is enabled)).

<a id="setting-skin_wipe_speed"></a>

##### Skin Wipe Speed (`skin_wipe_speed`)

Sets the speed for the skin tip wipe move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Tip Wipe
  is enabled)).

<a id="setting-skin_wipe_extruder_speed"></a>

##### Skin Wipe Extruder Speed (`skin_wipe_extruder_speed`)

Sets the extruder speed for the skin tip wipe move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Tip Wipe
  is enabled)).

<a id="setting-skin_wipe_angle"></a>

##### Skin Wipe Angle (`skin_wipe_angle`)

Sets the angle of the skin tip wipe.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Enable Skin Tip
  Wipe is enabled and Skin Wipe Direction is `WIPE_ANGLED`))).

<a id="setting-skin_wipe_cutoff_distance"></a>

##### Skin Wipe Cutoff Distance (`skin_wipe_cutoff_distance`)

Sets the distance from the end of the skin tip wipe to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Tip Wipe
  is enabled)).

<a id="setting-skin_wipe_lift_height"></a>

##### Skin Wipe Lift Height (`skin_wipe_lift_height`)

Sets the distance to lift during the skin tip wipe move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Skin Tip Wipe
  is enabled)).

<a id="setting-infill_wipe"></a>

##### Enable Infill Tip Wipe (`infill_wipe`)

If selected, will do a tip wipe for infill moves to prevent material from sticking up.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_wipe_direction"></a>

##### Infill Wipe Direction (`infill_wipe_direction`)

Selects the direction for infill tip wipes. WIPE_OPTIMAL uses a forward wipe when inset or perimeter
paths are enabled, or when the fill pattern is Concentric; otherwise it uses a reverse wipe.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `WIPE_OPTIMAL`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Tip
  Wipe is enabled)).
- **Choices:**
  - `WIPE_OPTIMAL`
  - `WIPE_FORWARD`
  - `WIPE_REVERSE`
  - `WIPE_ANGLED`

<a id="setting-infill_wipe_distance"></a>

##### Infill Wipe Distance (`infill_wipe_distance`)

Sets the travel distance for infill tip wiping. If using an incrementing wipe, this is the initial
distance.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Tip
  Wipe is enabled)).

<a id="setting-infill_wipe_speed"></a>

##### Infill Wipe Speed (`infill_wipe_speed`)

Sets the speed for the infill tip wipe move.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Tip
  Wipe is enabled)).

<a id="setting-infill_wipe_extruder_speed"></a>

##### Infill Wipe Extruder Speed (`infill_wipe_extruder_speed`)

Sets the extruder speed for the infill tip wipe move.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Tip
  Wipe is enabled)).

<a id="setting-infill_wipe_angle"></a>

##### Infill Wipe Angle (`infill_wipe_angle`)

Sets the angle of the infill tip wipe.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Enable Infill Tip
  Wipe is enabled and Infill Wipe Direction is `WIPE_ANGLED`))).

<a id="setting-infill_wipe_cutoff_distance"></a>

##### Infill Wipe Cutoff Distance (`infill_wipe_cutoff_distance`)

Sets the distance from the end of the infill tip wipe move to turn the extruder off.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Tip
  Wipe is enabled)).

<a id="setting-infill_wipe_lift_height"></a>

##### Infill Wipe Lift Height (`infill_wipe_lift_height`)

Sets the distance to lift during the infill tip wipe move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Infill Tip
  Wipe is enabled)).

<a id="setting-tip_wipe_voltage"></a>

##### Tip Wipe Voltage (`tip_wipe_voltage`)

Sets welder voltage during tip wipe motions.

- **Input:** `voltage` — Electrical potential; displayed in the preferred voltage unit.
- **Master default:** `0 V`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Syntax is `Tormach`).

<a id="setting-disable_tip_wipe_feedrate_scaling"></a>

##### Disable Feedrate Scaling for Tip Wipes (`disable_tip_wipe_feedrate_scaling`)

If selected, minimum layer time feedrate adjustments will not change tip wipe path speeds.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `MODIFY FEEDRATE`)).

<a id="settings-material-spiral-lift"></a>

#### Material > Spiral Lift

Controls spiral motion used to lift away from a completed region or layer.

<a id="setting-enable_spiral_perimeter"></a>

##### Spiral Perimeters (`enable_spiral_perimeter`)

If selected, will perform a spiral lift on perimeters.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-enable_spiral_inset"></a>

##### Spiral Insets (`enable_spiral_inset`)

If selected, will perform a spiral lift on insets.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-enable_spiral_skin"></a>

##### Spiral Skins (`enable_spiral_skin`)

If selected, will perform a spiral lift on skin.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-enable_spiral_infill"></a>

##### Spiral Infill (`enable_spiral_infill`)

If selected, will perform a spiral lift on infill.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-spiral_end_of_layer"></a>

##### End of Layer (`spiral_end_of_layer`)

Reserved catalog setting for requesting a spiral lift on the last path in a layer. The current
slicing and writer paths do not read this value, so it has no effect.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-spiral_lift_height"></a>

##### Spiral Lift Height (`spiral_lift_height`)

Sets the distance the extruder lifts before starting the spiral.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Spiral Perimeters is enabled or (Spiral Insets
  is enabled or (Spiral Skins is enabled or (Spiral Infill is enabled or End of Layer is
  enabled))))).

<a id="setting-spiral_lift_points"></a>

##### Number of Points (`spiral_lift_points`)

Number of small line segments in the spiral move.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Supports G2/G3 is disabled and (Spiral
  Perimeters is enabled or (Spiral Insets is enabled or (Spiral Skins is enabled or (Spiral Infill
  is enabled or End of Layer is enabled)))))).

<a id="setting-spiral_lift_radius"></a>

##### Spiral Lift Radius (`spiral_lift_radius`)

Sets maximum distance from starting point that the extruder will spiral outward to.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Spiral Perimeters is enabled or (Spiral Insets
  is enabled or (Spiral Skins is enabled or (Spiral Infill is enabled or End of Layer is
  enabled))))).

<a id="setting-spiral_lift_speed"></a>

##### Spiral Lift Speed (`spiral_lift_speed`)

Sets the speed for spiral lift moves.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Spiral Perimeters is enabled or (Spiral Insets
  is enabled or (Spiral Skins is enabled or (Spiral Infill is enabled or End of Layer is
  enabled))))).

<a id="setting-disable_spiral_lift_feedrate_scaling"></a>

##### Disable Feedrate Scaling for Spiral Lift (`disable_spiral_lift_feedrate_scaling`)

If selected, minimum layer time feedrate adjustments will not change spiral lift path speeds.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `MODIFY FEEDRATE`)).

<a id="settings-material-purge"></a>

#### Material > Purge

Controls purge timing, screw speed, dwell behavior, and optional purge/wipe motion.

<a id="setting-initial_purge_duration"></a>

##### Initial Purge Duration (`initial_purge_duration`)

Reserved catalog duration for an initial purge before normal deposition. The current slicing and
writer paths do not read this value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Material Load is enabled and Syntax is
  `Cincinnati`)).

<a id="setting-initial_purge_dwell_screw_rpm"></a>

##### Initial Purge Screw RPM (`initial_purge_dwell_screw_rpm`)

Speed for the screw during the initial purge.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Material Load is enabled and Syntax is
  `Cincinnati`)).

<a id="setting-initial_purge_tip_wipe_delay"></a>

##### Initial Purge Tip Wipe Delay (`initial_purge_tip_wipe_delay`)

Amount of time the extruder will pause before moving to wipe the tip during the initial purge.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Material Load is enabled and Syntax is
  `Cincinnati`)).

<a id="setting-purge_during_dwell"></a>

##### Purge During Dwell (`purge_during_dwell`)

If selected, a purge command is issued during dwells forced by the minimum layer time cooling
settings.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Min / Max Layer Time Method is `ADD DWELL
  TIME`).

<a id="setting-purge_dwell_duration"></a>

##### Purge Dwell Duration (`purge_dwell_duration`)

Duration to run the extruder during a purge dwell.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Purge During Dwell is enabled and Machine Type
  is `Pellet`)).

<a id="setting-purge_dwell_screw_rpm"></a>

##### Purge Dwell Screw RPM (`purge_dwell_screw_rpm`)

Speed for the screw during purge dwells.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Purge During Dwell is enabled and Machine Type
  is `Pellet`)).

<a id="setting-purge_tip_wipe_delay"></a>

##### Purge Dwell Tip Wipe Delay (`purge_tip_wipe_delay`)

Amount of time the extruder will pause before moving to wipe the tip during a purge dwell.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Purge During Dwell is enabled and Machine Type
  is `Pellet`)).

<a id="setting-purge_length"></a>

##### Purge Length (`purge_length`)

Length of filament, or screw rotation distance, for purges issued between layers.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Purge During Dwell is enabled and (Syntax is
  `Marlin` or Machine Type is `Filament`))).

<a id="setting-purge_feedrate"></a>

##### Purge Feedrate (`purge_feedrate`)

Feedrate for extrusion used for the purge between layers.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Purge During Dwell is enabled and (Syntax is
  `Marlin` or Machine Type is `Filament`))).

<a id="settings-material-extruder"></a>

#### Material > Extruder

Configures initial extrusion, priming, region delays, servo behavior, and spindle-command
conventions.

<a id="setting-initial_extruder_speed"></a>

##### Initial Extruder Speed (`initial_extruder_speed`)

Sets the initial extruder speed that is used during the specific extruder on delay set below.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Use Width and Height is disabled and (Machine
  Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-extruder_prime_volume"></a>

##### Extruder Prime Volume (`extruder_prime_volume`)

Sets the volume of material to purge during priming. Used for pellet extruder without RPM control.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Syntax is `KraussMaffei`).

<a id="setting-extruder_prime_speed"></a>

##### Extruder Prime Speed (`extruder_prime_speed`)

Sets the speed of the extruder during priming. Used for pellet extruder without RPM control.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Syntax is `KraussMaffei`).

<a id="setting-extruder_on_delay_perimeter"></a>

##### Perimeter Extruder On Delay (`extruder_on_delay_perimeter`)

Reserved catalog delay before perimeter motion. The current slicing and writer paths do not read
this value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Machine Type
  is `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-extruder_on_delay_inset"></a>

##### Inset Extruder On Delay (`extruder_on_delay_inset`)

Reserved catalog delay before inset motion. The current slicing and writer paths do not read this
value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Machine Type is
  `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-extruder_on_delay_skin"></a>

##### Skin Extruder On Delay (`extruder_on_delay_skin`)

Reserved catalog delay before skin motion. The current slicing and writer paths do not read this
value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Machine Type is
  `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-extruder_on_delay_infill"></a>

##### Infill Extruder On Delay (`extruder_on_delay_infill`)

Reserved catalog delay before infill motion. The current slicing and writer paths do not read this
value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Machine Type is
  `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-extruder_on_delay_skeleton"></a>

##### Skeleton Extruder On Delay (`extruder_on_delay_skeleton`)

Reserved catalog delay before skeleton motion. The current slicing and writer paths do not read this
value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Machine Type
  is `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-extruder_off_delay"></a>

##### Extruder Off Delay (`extruder_off_delay`)

Reserved catalog delay after print-head motion stops. The current slicing and writer paths do not
read this value, so it has no effect.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Machine Type is `Pellet` or (Machine Type is
  `Concrete` or Machine Type is `Thermoset`))).

<a id="setting-servo_extruder_to_travel_speed"></a>

##### Servo Extruder to Travel Speed (`servo_extruder_to_travel_speed`)

If selected, the speed of the extruder will servo with the speed of the gantry.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Syntax is `Cincinnati`).

<a id="setting-enable_m3s"></a>

##### Enable M3 S (`enable_m3s`)

If selected, CI BAAM extruder commands will always use M3 S rather than G1 S. This is helpful for
using arc welder, but prevents feedrate scaling from working.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Syntax is `Cincinnati`).

<a id="settings-material-filament"></a>

#### Material > Filament

Configures filament diameter, relative extrusion, position-reset behavior, and alternate extrusion
axes.

<a id="setting-filament_diameter"></a>

##### Filament Diameter (`filament_diameter`)

Average diameter of the filament.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Machine Type is `Filament`.

<a id="setting-filament_relative_distance"></a>

##### Enable Relative Distance (`filament_relative_distance`)

If selected, sets extrusion distances to relative mode. This issues a G91 command in the startup
sequence.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Enabled` (`true`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Machine Type is `Filament`.

<a id="setting-disable_g92"></a>

##### Remove G92 Commands (`disable_g92`)

If selected, the G92 command to reset the filament axis to 0 will not be issued.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Machine Type is `Filament`.

<a id="setting-filament_b_axis"></a>

##### Use B for Filament Axis (`filament_b_axis`)

If selected, uses B for filament distance output in the g-code rather than the standard E.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Machine Type is `Filament`.

<a id="settings-material-retraction"></a>

#### Material > Retraction

Controls when filament retracts and primes around qualifying travel and layer changes.

<a id="setting-retraction"></a>

##### Enable Retraction (`retraction`)

If selected, filament is retracted back into the extruder to end the path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Machine Type is `Filament`.

<a id="setting-retract_min_travel_length"></a>

##### Minimum Travel for Retraction (`retract_min_travel_length`)

Travel moves longer than this distance will force a retraction.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="setting-retraction_length"></a>

##### Retraction Length (`retraction_length`)

The length of the filament that will be retracted into the extruder.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="setting-retraction_speed"></a>

##### Retraction Speed (`retraction_speed`)

The speed that the filament that will be retracted into the extruder.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="setting-retraction_open_spaces_only"></a>

##### Retract on Open Spaces Only (`retraction_open_spaces_only`)

If selected, retracts only for travel moves that cross open space.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="setting-retraction_layer_change"></a>

##### Retract on Layer Change (`retraction_layer_change`)

If selected, retracts during every layer change.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="setting-filament_prime_speed"></a>

##### Filament Prime Speed (`filament_prime_speed`)

Speed of the extruder during priming.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="setting-filament_prime_length"></a>

##### Additional Prime Length (`filament_prime_length`)

Length of extra filament to extrude during priming after a retraction.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` and Enable Retraction is enabled).

<a id="settings-material-temperatures"></a>

#### Material > Temperatures

Sets bed, standby, and multi-zone extrusion temperature targets.

<a id="setting-bed_temperature"></a>

##### Bed Temperature (`bed_temperature`)

Temperature of bed while printing.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `125 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Machine Type is `Filament` or Syntax is `JuggerBot3D`).

<a id="setting-two_zone_extruder"></a>

##### Two Zone Extruder (`two_zone_extruder`)

Enables two zones for extruder temperature control.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Three Zone Extruder is disabled and (Four Zone Extruder is disabled and (Five
  Zone Extruder is disabled and (Machine Type is `Filament` or Syntax is `JuggerBot3D`)))).

<a id="setting-three_zone_extruder"></a>

##### Three Zone Extruder (`three_zone_extruder`)

Enables three zones for extruder temperature control.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Two Zone Extruder is disabled and (Four Zone Extruder is disabled and (Five
  Zone Extruder is disabled and (Machine Type is `Filament` or Syntax is `JuggerBot3D`)))).

<a id="setting-four_zone_extruder"></a>

##### Four Zone Extruder (`four_zone_extruder`)

Enables four zones for extruder temperature control.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Three Zone Extruder is disabled and (Two Zone Extruder is disabled and (Five
  Zone Extruder is disabled and (Machine Type is `Filament` or Syntax is `JuggerBot3D`)))).

<a id="setting-five_zone_extruder"></a>

##### Five Zone Extruder (`five_zone_extruder`)

Enables five zones for extruder temperature control.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Two Zone Extruder is disabled and (Three Zone Extruder is disabled and (Four
  Zone Extruder is disabled and (Machine Type is `Filament` or Syntax is `JuggerBot3D`)))).

<a id="setting-extruder_temperature"></a>

##### Extruder Temperature (`extruder_temperature`)

Temperature of extruder while printing.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `200 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Two Zone Extruder is disabled and (Three Zone Extruder is disabled and (Four
  Zone Extruder is disabled and (Five Zone Extruder is disabled and (Machine Type is `Filament` or
  Syntax is `JuggerBot3D`))))).

<a id="setting-standby_temperature"></a>

##### Extruder Standby Temperature (`standby_temperature`)

Temperature to hold extruder at when it's not in use.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `150 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Two Zone Extruder is disabled and (Three Zone Extruder is disabled and (Four
  Zone Extruder is disabled and (Five Zone Extruder is disabled and (Machine Type is `Filament` or
  Syntax is `JuggerBot3D`))))).

<a id="setting-extruder_zone1"></a>

##### Extruder Zone 1 Temperature (`extruder_zone1`)

Temperature for extruder zone 1.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `-273.15 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Two Zone Extruder is enabled or (Three Zone Extruder is enabled or (Four Zone
  Extruder is enabled or Five Zone Extruder is enabled))).

<a id="setting-extruder_zone2"></a>

##### Extruder Zone 2 Temperature (`extruder_zone2`)

Temperature for extruder zone 2.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `-273.15 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Two Zone Extruder is enabled or (Three Zone Extruder is enabled or (Four Zone
  Extruder is enabled or Five Zone Extruder is enabled))).

<a id="setting-extruder_zone3"></a>

##### Extruder Zone 3 Temperature (`extruder_zone3`)

Temperature for extruder zone 3.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `-273.15 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Three Zone Extruder is enabled or (Four Zone Extruder is enabled or Five Zone
  Extruder is enabled)).

<a id="setting-extruder_zone4"></a>

##### Extruder Zone 4 Temperature (`extruder_zone4`)

Temperature for extruder zone 4.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `-273.15 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Four Zone Extruder is enabled or Five Zone Extruder is enabled).

<a id="setting-extruder_zone5"></a>

##### Extruder Zone 5 Temperature (`extruder_zone5`)

Temperature for extruder zone 5.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `-273.15 °C`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Five Zone Extruder is enabled.

<a id="settings-material-cooling"></a>

#### Material > Cooling

Controls fan output and minimum-layer-time behavior, including pauses and extrusion/feed
adjustments.

<a id="setting-fan"></a>

##### Fan Control (`fan`)

Enables cooling-fan commands for filament printing. Supported writers turn the fan on using Max Fan
Speed and turn it off at shutdown.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Machine Type is `Filament`).

<a id="setting-fan_min_speed"></a>

##### Min Fan Speed (`fan_min_speed`)

Reserved minimum cooling-fan percentage. The current slicing and writer paths do not read this
value, so it has no effect.

- **Input:** `percentage100` — Percentage input with an allowed range of 0–100%.
- **Master default:** `0%`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Machine Type is `Filament` and Fan Control is
  enabled)).

<a id="setting-fan_max_speed"></a>

##### Max Fan Speed (`fan_max_speed`)

Sets the cooling-fan percentage emitted by supported writers when Fan Control is enabled.

- **Input:** `percentage100` — Percentage input with an allowed range of 0–100%.
- **Master default:** `0%`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Machine Type is `Filament` and Fan Control is
  enabled)).

<a id="setting-force_minimum_layer_time"></a>

##### Force Min / Max Layer Time (`force_minimum_layer_time`)

If selected, a min / max layer time is reinforced for each layer so that there is adequate time for
cooling.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-minimum_layer_time_method"></a>

##### Min / Max Layer Time Method (`minimum_layer_time_method`)

Choose one of two ways to meet the min / max layer time requirement: either add dwell time or adjust
feed rate in G0 G1 lines.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `ADD DWELL TIME`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Force Min / Max Layer Time is enabled).
- **Choices:**
  - `ADD DWELL TIME`
  - `MODIFY FEEDRATE`

<a id="setting-minimum_layer_time"></a>

##### Minimum Layer Time (`minimum_layer_time`)

The minimum layer time required to meet the cooling need.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Force Min / Max Layer Time is enabled).

<a id="setting-maximum_layer_time"></a>

##### Maximum Layer Time (`maximum_layer_time`)

The maximum layer time required to meet the cooling need.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `MODIFY FEEDRATE`)).

<a id="setting-extruder_scale_factor"></a>

##### Extruder Scale Factor (`extruder_scale_factor`)

Scale factor to be applied to extruder commands when adjusting feedrate. Value of 1 has no effect,
greater than 1 increases extruder speed, and less than 1 decreases speed.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `MODIFY FEEDRATE`)).

<a id="setting-pre_pause_code"></a>

##### Pre Pause G-Code (`pre_pause_code`)

G-Code to be executed before the minimum layer time pause command, such as moving to a location to
purge.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `ADD DWELL TIME`)).

<a id="setting-post_pause_code"></a>

##### Post Pause G-Code (`post_pause_code`)

G-Code to be executed after the minimum layer time pause command, such as turning an extruder off.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Force Min / Max Layer Time is enabled and Min /
  Max Layer Time Method is `ADD DWELL TIME`)).

<a id="settings-material-platform-adhesion"></a>

#### Material > Platform Adhesion

Adds and configures raft, brim, or skirt geometry around the first layers.

<a id="setting-raft"></a>

##### Add Raft (`raft`)

If selected, a raft will be printed below the part to help account for an unlevel build surface.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-raft_offset"></a>

##### Raft XY Offset (`raft_offset`)

Determines how far beyond the base of the part the raft extends.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Raft is enabled).

<a id="setting-raft_layer_count"></a>

##### Raft Layer Count (`raft_layer_count`)

Determines how many layers of raft are created.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Raft is enabled).

<a id="setting-raft_bead_width"></a>

##### Raft Bead Width (`raft_bead_width`)

Bead width for raft paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Raft is enabled).

<a id="setting-brim"></a>

##### Add Brim (`brim`)

If selected, a brim will be attached to the part at the base to help with platform adhesion.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-brim_width"></a>

##### Brim Width (`brim_width`)

Width of the brim.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Brim is enabled).

<a id="setting-brim_layer_count"></a>

##### Brim Layer Count (`brim_layer_count`)

Number of layers to print the brim starting with layer 1.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Brim is enabled).

<a id="setting-brim_bead_width"></a>

##### Brim Bead Width (`brim_bead_width`)

Bead width for brim paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Brim is enabled).

<a id="setting-skirt"></a>

##### Add Skirt (`skirt`)

If selected, a skirt will be printed around the object to prime the extruder.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-skirt_loops"></a>

##### Skirt Loops (`skirt_loops`)

Sets the number of closed skirt loops printed around the part to prime the extruder.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Skirt is enabled).

<a id="setting-skirt_distance_from_object"></a>

##### Skirt Distance from Object (`skirt_distance_from_object`)

Sets the horizontal distance between the part and the innermost skirt loop.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Skirt is enabled).

<a id="setting-skirt_layer_count"></a>

##### Skirt Layer Count (`skirt_layer_count`)

Determines how many layers the skirt contains.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Skirt is enabled).

<a id="setting-skirt_minimum_length"></a>

##### Skirt Minimum Length (`skirt_minimum_length`)

The minimum extrusion length for the skirt.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Skirt is enabled).

<a id="setting-skirt_bead_width"></a>

##### Skirt Bead Width (`skirt_bead_width`)

Bead width for skirt paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Add Skirt is enabled).

<a id="settings-material-multi-material"></a>

#### Material > Multi-Material

Assigns materials to regions and controls material transitions and controller selection commands.

<a id="setting-enable_multi_material"></a>

##### Enable Multi-material (`enable_multi_material`)

If selected, enables the use of multiple printing materials.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Syntax is `Cincinnati` or (Syntax is `Marlin`
  or Syntax is `JuggerBot3D`))).

<a id="setting-perimeter_material_num"></a>

##### Perimeter Material Number (`perimeter_material_num`)

Sets the material number to be used during perimeter, brim, skeleton, and skirt paths.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Multi-material is enabled and (Enable
  Perimeter is enabled or (Add Brim is enabled or (Enable Skeletons is enabled or Add Skirt is
  enabled))))).

<a id="setting-inset_material_num"></a>

##### Inset Material Number (`inset_material_num`)

Sets the material number to be used during inset paths.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable
  Multi-material is enabled)).

<a id="setting-skeleton_material_num"></a>

##### Skeleton Material Number (`skeleton_material_num`)

Sets the material number to be used for skeleton paths.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable
  Multi-material is enabled)).

<a id="setting-skin_material_num"></a>

##### Skin Material Number (`skin_material_num`)

Sets the material number to be used during skin paths.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable
  Multi-material is enabled)).

<a id="setting-infill_material_num"></a>

##### Infill Material Number (`infill_material_num`)

Sets the material number to be used during infill paths.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable
  Multi-material is enabled)).

<a id="setting-support_material_num"></a>

##### Support Material Number (`support_material_num`)

Sets the material number to be used during support and raft paths.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Multi-material is enabled and (Enable
  Support is enabled or Add Raft is enabled))).

<a id="setting-material_transition_distance"></a>

##### Transition Distance (`material_transition_distance`)

The distance needed to transition between materials.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Multi-material is enabled).

<a id="setting-enable_second_transition_distance"></a>

##### Enable Second Transition Distance (`enable_second_transition_distance`)

If selected, Transition Distance will be used to transition from 1 to 2 and Second Transition
Distance will be used for 2 to 1.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Multi-material is enabled).

<a id="setting-second_transition_distance"></a>

##### Second Transition Distance (`second_transition_distance`)

Distance used for transition from material 2 to material 1.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Multi-material is enabled and Enable
  Second Transition Distance is enabled)).

<a id="setting-enable_m222"></a>

##### Use M222 Code (`enable_m222`)

Uses the M222 code for material transitions rather than the standard M237.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Multi-material is enabled).

### E.4 Profile settings

Profile settings define how geometry becomes layers and ordered toolpaths. Many are local-capable so
different parts, layers, ranges, or spatial regions can use different values in supported modes.

![Figure 63 placeholder: Profile settings](user-guide-images/figure63.png)

> **Diagram placeholder — Profile settings:** Add an annotated Profile panel with its
> category tabs, search field, and one enabled/disabled dependency example.

<a id="settings-profile-slicing"></a>

#### Profile > Slicing

Selects planar, cylindrical, or image slicing and configures slice orientation and mode-specific
geometry.

<a id="setting-slicing_mode"></a>

##### Slicing Mode (`slicing_mode`)

Selects the slicing workflow to use. Planar creates flat layer slices along the slice plane normal.
Cylindrical creates radial or helical paths around a cylinder axis. Image creates image slices from
the model cross sections.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Planar`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.
- **Choices:**
  - `Planar`
  - `Cylindrical`
  - `Image`

<a id="setting-slice_plane_normal"></a>
<a id="setting-slice_plane_normal_x"></a>
<a id="setting-slice_plane_normal_y"></a>
<a id="setting-slice_plane_normal_z"></a>

##### Slice Plane Normal (`slice_plane_normal`)

Defines the normal direction for planar slice planes. Values are normalized before slicing; {0, 0,
1} creates horizontal XY layers.

- **Input:** `vector3` grouped control with the components listed below.
- **Scope:** Global only. Configure the grouped value in the active global settings.
- **Available when:** Slicing Mode is `Planar`.
- **Components and master defaults:**
  - **X:** `slice_plane_normal_x` — `0`
  - **Y:** `slice_plane_normal_y` — `0`
  - **Z:** `slice_plane_normal_z` — `1`

<a id="setting-cylinder_axis_source"></a>

##### Cylinder Axis Source (`cylinder_axis_source`)

Selects the XY centerline used for cylindrical slicing. Part Centroid uses each part's XY centroid.
Custom XY uses the configured cylinder axis coordinates.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Part Centroid`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Cylindrical`.
- **Choices:**
  - `Part Centroid`
  - `Custom XY`

<a id="setting-cylinder_axis"></a>
<a id="setting-cylinder_axis_x"></a>
<a id="setting-cylinder_axis_y"></a>

##### Cylinder Axis (`cylinder_axis`)

Defines the custom XY coordinate used as the cylinder axis.

- **Input:** `vector2` grouped control with the components listed below.
- **Scope:** Local-capable. Each component can be overridden through this grouped row at supported
  narrower scopes.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylinder Axis Source is `Custom XY`).
- **Components and master defaults:**
  - **X:** `cylinder_axis_x` — `0 mm`
  - **Y:** `cylinder_axis_y` — `0 mm`

<a id="setting-cylinder_inner_radius"></a>

##### Cylinder Inner Radius (`cylinder_inner_radius`)

Sets the starting inner radius for cylindrical paths. The first path is generated one half layer
height beyond this radius.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Cylindrical`.

<a id="setting-cylinder_height"></a>

##### Cylinder Height (`cylinder_height`)

Limits cylindrical path generation to this height above the part base. Values less than or equal to
zero use the part height.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Cylindrical`.

<a id="setting-cylindrical_path_pattern"></a>

##### Cylindrical Path Pattern (`cylindrical_path_pattern`)

Selects the path pattern generated by cylindrical slicing. Radial creates concentric paths at each Z
section. Helical creates rising spiral paths around the cylinder axis.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Radial`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Cylindrical`.
- **Choices:**
  - `Radial`
  - `Helical`

<a id="setting-radial_path_boundary_policy"></a>

##### Radial Path Boundary Policy (`radial_path_boundary_policy`)

Controls radial paths that intersect the model boundary. Clip keeps only the portions inside the
model. Keep outputs the original path if any portion is inside. Discard omits paths that are cut by
the boundary.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Clip`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Radial`).
- **Choices:**
  - `Clip`
  - `Keep`
  - `Discard`

<a id="setting-radial_path_start_angle"></a>

##### Radial Path Start Angle (`radial_path_start_angle`)

Sets the angular start position for generated radial paths around the cylinder axis. 0 degrees
starts on +X and 90 degrees starts on +Y.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Radial`).

<a id="setting-helical_path_boundary_policy"></a>

##### Helical Path Boundary Policy (`helical_path_boundary_policy`)

Controls helical paths that intersect the model boundary. Clip keeps every retained section inside
the model. Clip Z keeps the continuous helix through the highest Z intersection.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Clip`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Helical`).
- **Choices:**
  - `Clip`
  - `Clip Z`

<a id="setting-helical_path_z_clip_rounding"></a>

##### Helical Z Clip Rounding (`helical_path_z_clip_rounding`)

Controls how Clip Z rounds the helical path endpoint at the highest model intersection. Exact
Intersection stops at the intersection. Complete Revolution continues to the next full revolution.
Last Full Revolution stops at the previous full revolution.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Exact Intersection`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** ((Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Helical`) and
  Helical Path Boundary Policy is `Clip Z`).
- **Choices:**
  - `Exact Intersection`
  - `Complete Revolution`
  - `Last Full Revolution`

<a id="setting-helical_path_handedness"></a>

##### Helical Path Handedness (`helical_path_handedness`)

Selects the handedness for generated helical paths. Right Handed uses a counter-clockwise XY sweep
as Z rises. Left Handed mirrors the sweep clockwise while preserving positive Z rise.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Right Handed`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Helical`).
- **Choices:**
  - `Right Handed`
  - `Left Handed`

<a id="setting-helical_path_start_angle"></a>

##### Helical Path Start Angle (`helical_path_start_angle`)

Sets the angular start position for generated helical paths around the cylinder axis. 0 degrees
starts on +X and 90 degrees starts on +Y.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `5,156.62°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Helical`).

<a id="setting-max_helical_path_length"></a>

##### Max Helical Path Length (`max_helical_path_length`)

Maximum length of each generated helical path segment. Set to 0 to leave helical paths unsplit.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Cylindrical` and Cylindrical Path Pattern is `Helical`).

<a id="setting-arcs_per_revolution"></a>

##### Arcs per Revolution (`arcs_per_revolution`)

Sets the maximum number of circular arc spans used for each full revolution of cylindrical motion.
Minimum is 1.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `1`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Cylindrical`.

<a id="setting-image_pixel_size"></a>
<a id="setting-image_pixel_size_x"></a>
<a id="setting-image_pixel_size_y"></a>

##### Image Pixel Size (`image_pixel_size`)

Defines the physical X and Y size of pixels in generated image slices.

- **Input:** `vector2` grouped control with the components listed below.
- **Scope:** Global only. Configure the grouped value in the active global settings.
- **Available when:** Slicing Mode is `Image`.
- **Components and master defaults:**
  - **X:** `image_pixel_size_x` — `0 mm`
  - **Y:** `image_pixel_size_y` — `0 mm`

<a id="settings-profile-layer"></a>

#### Profile > Layer

Defines layer thickness and baseline bead, nozzle, speed, and extrusion values.

<a id="setting-layer_height"></a>

##### Layer Height (`layer_height`)

Thickness of each layer.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Always available.

<a id="setting-nozzle_diameter"></a>

##### Nozzle Diameter (`nozzle_diameter`)

The diameter of the main extruder nozzle.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Machine Type is `Pellet` or (Machine Type is
  `Filament` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-default_width"></a>

##### Default Bead Width (`default_width`)

Default bead width for paths that don't have a defined width.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Always available.

<a id="setting-default_speed"></a>

##### Default Print Speed (`default_speed`)

Default printing speed for paths that don't have a defined speed.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Always available.

<a id="setting-default_extruder_speed"></a>

##### Default Extruder Speed (`default_extruder_speed`)

Default extruder speed for paths that don't have a defined extruder speed.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Use Width and Height is disabled and (Machine
  Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is `Thermoset`)))).

<a id="setting-minimum_extrude_length"></a>

##### Minimum Extrude Length (`minimum_extrude_length`)

Default minimum extrusion length. Paths less than this are eliminated. Currently used for brim,
raft, and support paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="settings-profile-perimeter"></a>

#### Profile > Perimeter

Controls exterior contours, their process values, and perimeter-specific start and spiral behavior.

<a id="setting-perimeter"></a>

##### Enable Perimeter (`perimeter`)

If selected, perimeters will be generated.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-perimeter_count"></a>

##### Number of Perimeters (`perimeter_count`)

Sets the number of exterior contour paths generated per layer. The editor accepts 1 or more; the
built-in catalog fallback is 0, so set an explicit value before enabling perimeters.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `0` (below the current input minimum of `1`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_boundary_selection"></a>

##### Perimeter Boundaries (`perimeter_boundary_selection`)

Selects which polygon boundaries are used to generate perimeter paths.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `All Boundaries`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).
- **Choices:**
  - `All Boundaries`
  - `Internal Boundaries`
  - `External Boundaries`

<a id="setting-perimeter_width"></a>

##### Perimeter Bead Width (`perimeter_width`)

Bead width for perimeter paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_adapt"></a>

##### Enable Adaptive Perimeter Bead Widths (`perimeter_adapt`)

If selected, perimeter bead widths are adjusted within the configured limits to better fill narrow
regions whose remaining contour space is not an exact multiple of the nominal bead width.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_adapt_min_width"></a>

##### Minimum Adaptive Perimeter Bead Width (`perimeter_adapt_min_width`)

Minimum bead width allowed when adaptive perimeter bead widths are enabled.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable Adaptive
  Perimeter Bead Widths is enabled)).

<a id="setting-perimeter_adapt_max_width"></a>

##### Maximum Adaptive Perimeter Bead Width (`perimeter_adapt_max_width`)

Maximum bead width allowed when adaptive perimeter bead widths are enabled.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `10 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable Adaptive
  Perimeter Bead Widths is enabled)).

<a id="setting-perimeter_speed"></a>

##### Perimeter Speed (`perimeter_speed`)

Speed for perimeter paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_extruder_speed"></a>

##### Perimeter Extruder Speed (`perimeter_extruder_speed`)

Extruder speed for perimeter paths.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Use Width and
  Height is disabled and (Machine Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is
  `Thermoset`))))).

<a id="setting-perimeter_extrusion_multiplier"></a>

##### Perimeter Extrusion Multiplier (`perimeter_extrusion_multiplier`)

Extrusion multiplier to increase/decrease flowrate for perimeter paths for systems without RPM
control.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and (Machine Type
  is `Filament` or Syntax is `KraussMaffei`))).

<a id="setting-perimeter_minimum_path_length"></a>

##### Minimum Perimeter Path Length (`perimeter_minimum_path_length`)

Perimeter extrusion paths less than this value are deleted.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_minimum_segment_length"></a>

##### Minimum Perimeter Segment Length (`perimeter_minimum_segment_length`)

Perimeter path segments shorter than this value are collapsed before extrusion paths are created.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_lead_in"></a>

##### Enable Perimeter Lead-In (`perimeter_lead_in`)

If selected, a lead-in path segment is added to the first perimeter path that can be used to prime
the extruder.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_lead_in_first_layer"></a>

##### Lead-In on First Layer Only (`perimeter_lead_in_first_layer`)

If selected, lead-in is only applied to layer 1.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Enabled` (`true`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Lead-In is enabled)).

<a id="setting-perimeter_lead_in_x"></a>

##### Lead-In Point X Position (`perimeter_lead_in_x`)

X Position for the start of the perimeter lead-in segment.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Lead-In is enabled)).

<a id="setting-perimeter_lead_in_y"></a>

##### Lead-In Point Y Position (`perimeter_lead_in_y`)

Y Position for the start of the perimeter lead-in segment.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Lead-In is enabled)).

<a id="setting-perimeter_flying_start"></a>

##### Enable Perimeter Flying Start (`perimeter_flying_start`)

If selected, motions are created prior to the first extrusion move such that the end effector is in
motion at the start of the path.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_flying_start_distance"></a>

##### Perimeter Flying Start Distance (`perimeter_flying_start_distance`)

Distance away from the start of the extrusion path to begin the flying start motion.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Flying Start is enabled)).

<a id="setting-perimeter_flying_start_speed"></a>

##### Perimeter Flying Start Speed (`perimeter_flying_start_speed`)

Speed for the flying start motion.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Perimeter is enabled and Enable
  Perimeter Flying Start is enabled)).

<a id="setting-spiral_perimeter"></a>

##### Enable Spiral Perimeter (`spiral_perimeter`)

If selected, spiral perimeters will be generated.&lt;br&gt;&lt;br&gt;&lt;img
src=':/tooltips/profile/spiral_perimeter.png' width='260' height='146'&gt;

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="settings-profile-inset"></a>

#### Profile > Inset

Controls additional inward contours, including count, overlap, process values, and spiral behavior.

<a id="setting-inset"></a>

##### Enable Inset (`inset`)

If selected, insets will be generated.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-inset_count"></a>

##### Number of Insets (`inset_count`)

Sets the number of inward contour paths generated after the perimeter. The editor accepts 1 or more;
the built-in catalog fallback is 0, so set an explicit value before enabling insets.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `0` (below the current input minimum of `1`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_width"></a>

##### Inset Bead Width (`inset_width`)

Bead width for inset paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_adapt"></a>

##### Enable Adaptive Inset Bead Widths (`inset_adapt`)

If selected, inset bead widths are adjusted within the configured limits to better fill narrow
regions whose remaining contour space is not an exact multiple of the nominal bead width.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_adapt_min_width"></a>

##### Minimum Adaptive Inset Bead Width (`inset_adapt_min_width`)

Minimum bead width allowed when adaptive inset bead widths are enabled.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Adaptive
  Inset Bead Widths is enabled)).

<a id="setting-inset_adapt_max_width"></a>

##### Maximum Adaptive Inset Bead Width (`inset_adapt_max_width`)

Maximum bead width allowed when adaptive inset bead widths are enabled.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `10 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and Enable Adaptive
  Inset Bead Widths is enabled)).

<a id="setting-inset_speed"></a>

##### Inset Speed (`inset_speed`)

Speed for inset paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_extruder_speed"></a>

##### Inset Extruder Speed (`inset_extruder_speed`)

Extruder speed for inset paths.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Use Width and
  Height is disabled and (Machine Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is
  `Thermoset`))))).

<a id="setting-inset_extrusion_multiplier"></a>

##### Inset Extrusion Multiplier (`inset_extrusion_multiplier`)

Extrusion multiplier to increase/decrease flowrate for inset paths for systems without RPM control.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Inset is enabled and (Machine Type is
  `Filament` or Syntax is `KraussMaffei`))).

<a id="setting-inset_minimum_path_length"></a>

##### Minimum Inset Path Length (`inset_minimum_path_length`)

Inset extrusion paths less than this value are deleted.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_minimum_segment_length"></a>

##### Minimum Inset Segment Length (`inset_minimum_segment_length`)

Inset path segments shorter than this value are collapsed before extrusion paths are created.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_overlap_distance"></a>

##### Inset Overlap Distance (`inset_overlap_distance`)

Width of the inset overlaps with exterior.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-spiral_inset"></a>

##### Enable Spiral Inset (`spiral_inset`)

If selected, spiral insets will be generated.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="settings-profile-skeleton"></a>

#### Profile > Skeleton

Controls centerline input, cleanup, adaptive bead width, process values, and skeleton prestart
behavior.

<a id="setting-skeleton"></a>

##### Enable Skeletons (`skeleton`)

If selected, skeletons will allow for an open loop path to fill a space that is too thin to use
concentric paths.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-skeleton_input"></a>

##### Input Geometry (`skeleton_input`)

Specifies how the bounding geometry is provided to the Skeleton Voronoi Diagram generator. The
geometry is fed into the generator as either segments or points. This input method influences how
the skeleton paths are constructed.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `SEGMENT`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).
- **Choices:**
  - `SEGMENT`
  - `POINT`

<a id="setting-skeleton_input_cleaning_distance"></a>

##### Input Cleaning Distance (`skeleton_input_cleaning_distance`)

Specifies the distance threshold used to clean and simplify the input geometry before generating the
skeleton. The cleaning process involves: Removing vertices that connect co-linear or nearly
co-linear edges: If moving a vertex by no more than this distance would make the connected edges
co-linear, the vertex is removed to simplify the geometry. Eliminating vertices that are too close
to adjacent vertices: Vertices within this distance of an adjacent vertex are removed to prevent
redundant points and overlaps. Removing vertices near semi-adjacent vertices along with their
outlying vertices: If a vertex is within this distance of a semi-adjacent vertex, both the vertex
and its connected outlying vertices are removed to simplify complex connections. By adjusting this
cleaning distance, you can optimize the input geometry by merging or removing unnecessary vertices.
This leads to improved accuracy and performance of the skeleton generation process, resulting in
cleaner and more efficient skeleton paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_input_chamfering_angle"></a>

##### Input Chamfering Angle (`skeleton_input_chamfering_angle`)

Specifies the angle threshold used to chamfer (flatten) sharp corners in the input geometry during
skeleton generation. Corners with internal angles less than this value will be automatically
chamfered to smooth out acute angles. This chamfering process helps to simplify the geometry,
improving the accuracy of the skeleton pruning process.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_output_cleaning_distance"></a>

##### Output Cleaning Distance (`skeleton_output_cleaning_distance`)

Sets the distance tolerance used to simplify the generated skeleton by removing nearly collinear,
adjacent, or semi-adjacent redundant vertices.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_width"></a>

##### Bead Width (`skeleton_width`)

Bead width for skeleton paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_speed"></a>

##### Speed (`skeleton_speed`)

Speed of skeleton printing paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_extruder_speed"></a>

##### Extruder Speed (`skeleton_extruder_speed`)

Extruder speed for skeletons.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Use Width and
  Height is disabled and (Machine Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is
  `Thermoset`))))).

<a id="setting-skeleton_extrusion_multiplier"></a>

##### Extrusion Multiplier (`skeleton_extrusion_multiplier`)

Extrusion multiplier to increase/decrease flowrate for skeleton paths for systems without RPM
control.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and (Machine Type
  is `Filament` or Syntax is `KraussMaffei`))).

<a id="setting-skeleton_adapt"></a>

##### Enable Adaptive Bead Widths (`skeleton_adapt`)

If selected, skeleton bead widths will dynamically adjust to better fill their surrounding regions,
minimizing both under- and over-filling. This adaptation is achieved by inversely adjusting the
speed relative to the desired bead width using the formula: Adjusted Speed = (Reference Speed ×
Reference Bead Width) / Desired Bead Width. When wider bead widths are needed to fill larger areas,
the traversal speed decreases, allowing more material to be deposited. Conversely, in tighter spaces
requiring narrower bead widths, the speed increases, depositing less material.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_adapt_step_size"></a>

##### Adaptivity Step Size (`skeleton_adapt_step_size`)

Specifies the distance used to divide skeleton segments into smaller subsegments for adaptive bead
width adjustment. A smaller step size allows for finer adaptation to geometric variations.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0.001 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Adaptive
  Bead Widths is enabled)).

<a id="setting-skeleton_adapt_min_width"></a>

##### Minimum Adaptive Bead Width (`skeleton_adapt_min_width`)

Specifies the minimum allowable bead width for adaptive skeletons. Skeletons with adapted bead
widths less than this value will be removed.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Adaptive
  Bead Widths is enabled)).

<a id="setting-skeleton_adapt_min_width_filter"></a>

##### Minimum Adaptive Bead Width Filter (`skeleton_adapt_min_width_filter`)

Specifies how to handle adaptive skeleton segments with bead widths below the minimum threshold.
Clamp: Assigns the minimum bead width to segments with adapted widths below the threshold. Prune:
Removes segments with adapted widths below the threshold.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `CLAMP`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Adaptive
  Bead Widths is enabled)).
- **Choices:**
  - `CLAMP`
  - `PRUNE`

<a id="setting-skeleton_adapt_max_width"></a>

##### Maximum Adaptive Bead Width (`skeleton_adapt_max_width`)

Specifies the maximum allowable bead width for adaptive skeletons. Skeletons with adapted bead
widths greater than this value will be assigned this value as their bead width.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `10 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Adaptive
  Bead Widths is enabled)).

<a id="setting-skeleton_adapt_max_width_filter"></a>

##### Maximum Adaptive Bead Width Filter (`skeleton_adapt_max_width_filter`)

Specifies how to handle adaptive skeleton segments with bead widths above the maximum threshold.
Clamp: Assigns the maximum bead width to segments with adapted widths above the threshold. Prune:
Removes segments with adapted widths above the threshold.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `CLAMP`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Adaptive
  Bead Widths is enabled)).
- **Choices:**
  - `CLAMP`
  - `PRUNE`

<a id="setting-skeleton_minimum_path_length"></a>

##### Minimum Path Length (`skeleton_minimum_path_length`)

Skeleton extrusion paths whose length is less than this value are removed.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_minimum_segment_length"></a>

##### Minimum Skeleton Segment Length (`skeleton_minimum_segment_length`)

Skeleton path segments shorter than this value are collapsed before extrusion paths are created.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_prestart"></a>

##### Enable Skeleton Prestart (`skeleton_prestart`)

If selected, a prestart motion is added to the front of the skeleton to give it more time to deposit
material. This prestart is along the same vector as the first segment of the skeleton.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_prestart_distance"></a>

##### Skeleton Prestart Distance (`skeleton_prestart_distance`)

Length of the skeleton prestart move.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Prestart is enabled)).

<a id="setting-skeleton_prestart_speed"></a>

##### Skeleton Prestart Speed (`skeleton_prestart_speed`)

Speed of skeleton prestart paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Prestart is enabled)).

<a id="setting-skeleton_prestart_extruder_speed"></a>

##### Skeleton Prestart Extruder Speed (`skeleton_prestart_extruder_speed`)

Extruder speed for skeleton prestart paths.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Prestart is enabled)).

<a id="setting-skeleton_prestart_area_modifier"></a>

##### Skeleton Prestart Bead Area Modifier (`skeleton_prestart_area_modifier`)

Percent multiplier for bead area of skeleton prestart paths.

- **Input:** `percentage` — Percentage input with an allowed range of 0–500%.
- **Master default:** `100%`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skeletons is enabled and Enable Skeleton
  Prestart is enabled)).

<a id="setting-skeleton_skin_mcode"></a>

##### Use Skin M-Code for Skeletons (`skeleton_skin_mcode`)

If selected, the skin m-code override (M15) will be issued rather than the default inset m-code
override (M13) on BAAM.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="settings-profile-skin"></a>

#### Profile > Skin

Controls solid top/bottom coverage, pattern orientation, overlap, process values, and gradual
infill.

<a id="setting-skin"></a>

##### Enable Skin (`skin`)

If selected, skins will be generated.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-skin_top_count"></a>

##### Top Skin Count (`skin_top_count`)

Number of skin layers at the top of a print.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_bottom_count"></a>

##### Bottom Skin Count (`skin_bottom_count`)

Number of skin layers at the bottom of a print.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_pattern"></a>

##### Skin Pattern (`skin_pattern`)

Selects the geometry used to fill solid top and bottom skin areas. Pattern angle and layer-to-layer
rotation orient non-concentric patterns.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Lines`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).
- **Choices:**
  - `Lines` — One family of parallel hatch lines at the configured angle and spacing.
  - `Grid` — Two perpendicular families of parallel lines at the configured angle and 90 degrees
    from it.
  - `Concentric` — Successive closed offsets that follow the boundary of the filled area.
  - `Triangles` — Three line families, rotated 60 degrees apart, that form an equilateral triangular
    lattice.
  - `Hexagons and Triangles` — Three 60-degree line families with an alternate offset that forms
    mixed hexagonal and triangular cells.
  - `Honeycomb` — Connected zig-zag rows that form hexagonal cells using bead width and line
    spacing.

<a id="setting-skin_angle"></a>

##### Skin Fill Angle (`skin_angle`)

Sets the angle for the skin infill.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_angle_rotation"></a>

##### Skin Fill Angle Rotation (`skin_angle_rotation`)

Sets the amount the skin fill rotates layer to layer.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_exterior_overlap"></a>

##### Skin Exterior Overlap (`skin_exterior_overlap`)

Width of the skin overlaps with the exterior.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_width"></a>

##### Skin Bead Width (`skin_width`)

Bead width for skin paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_speed"></a>

##### Skin Speed (`skin_speed`)

Speed for skin paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_extruder_speed"></a>

##### Skin Extruder Speed (`skin_extruder_speed`)

Extruder speed for skin paths.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Use Width and
  Height is disabled and (Machine Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is
  `Thermoset`))))).

<a id="setting-skin_extrusion_multiplier"></a>

##### Skin Extrusion Multiplier (`skin_extrusion_multiplier`)

Extrusion multiplier to increase/decrease flowrate for skin paths for systems without RPM control.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and (Machine Type is
  `Filament` or Syntax is `KraussMaffei`))).

<a id="setting-skin_minimum_path_length"></a>

##### Minimum Skin Path Length (`skin_minimum_path_length`)

Skin extrusion paths less than this value are deleted.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_minimum_segment_length"></a>

##### Minimum Skin Segment Length (`skin_minimum_segment_length`)

Skin path segments shorter than this value are collapsed before extrusion paths are created.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_gradual_infill"></a>

##### Enable Gradual Infill Steps (`skin_gradual_infill`)

If selected, gradual infill steps will be generated.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_gradual_infill_steps"></a>

##### Number of Gradual Infill Steps (`skin_gradual_infill_steps`)

Number of gradual infill steps between top skin and infill.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Gradual
  Infill Steps is enabled)).

<a id="setting-skin_gradual_infill_pattern"></a>

##### Gradual Infill Pattern (`skin_gradual_infill_pattern`)

Selects the fill geometry used in the transition steps between solid skin and sparse infill. Each
step changes spacing while retaining this pattern.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Lines`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Gradual
  Infill Steps is enabled)).
- **Choices:**
  - `Lines` — One family of parallel hatch lines at the configured angle and spacing.
  - `Grid` — Two perpendicular families of parallel lines at the configured angle and 90 degrees
    from it.
  - `Concentric` — Successive closed offsets that follow the boundary of the filled area.
  - `Triangles` — Three line families, rotated 60 degrees apart, that form an equilateral triangular
    lattice.
  - `Hexagons and Triangles` — Three 60-degree line families with an alternate offset that forms
    mixed hexagonal and triangular cells.
  - `Honeycomb` — Connected zig-zag rows that form hexagonal cells using bead width and line
    spacing.

<a id="setting-skin_gradual_infill_angle"></a>

##### Gradual Infill Angle (`skin_gradual_infill_angle`)

Sets the angle for the gradual infill.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Gradual
  Infill Steps is enabled)).

<a id="setting-skin_gradual_infill_angle_rotation"></a>

##### Gradual Infill Angle Rotation (`skin_gradual_infill_angle_rotation`)

Sets the angle for the gradual infill rotation layer to layer.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Skin is enabled and Enable Gradual
  Infill Steps is enabled)).

<a id="settings-profile-infill"></a>

#### Profile > Infill

Controls interior fill density, spacing, pattern, orientation, ordering, combining, and process
values.

<a id="setting-infill"></a>

##### Enable Infill (`infill`)

If selected, infill will be generated.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-infill_density"></a>

##### Infill Density (`infill_density`)

Percent of area to be covered by infill.

- **Input:** `percentage100` — Percentage input with an allowed range of 0–100%.
- **Master default:** `0%`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Manual
  Infill Line Spacing is disabled)).

<a id="setting-infill_manual_spacing"></a>

##### Enable Manual Infill Line Spacing (`infill_manual_spacing`)

Override infill density and manually define distance between beads on infill.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_line_spacing"></a>

##### Infill Line Spacing (`infill_line_spacing`)

Distance between beads on sparse infill.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Enable Manual
  Infill Line Spacing is enabled)).

<a id="setting-infill_pattern"></a>

##### Infill Pattern (`infill_pattern`)

Selects the geometry used to fill interior regions at the configured density or line spacing. Radial
Hatch is listed for compatibility but currently generates no infill paths in this region.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Lines`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).
- **Choices:**
  - `Lines` — One family of parallel hatch lines at the configured angle and spacing.
  - `Grid` — Two perpendicular families of parallel lines at the configured angle and 90 degrees
    from it.
  - `Concentric` — Successive closed offsets that follow the boundary of the filled area.
  - `Triangles` — Three line families, rotated 60 degrees apart, that form an equilateral triangular
    lattice.
  - `Hexagons and Triangles` — Three 60-degree line families with an alternate offset that forms
    mixed hexagonal and triangular cells.
  - `Honeycomb` — Connected zig-zag rows that form hexagonal cells using bead width and line
    spacing.
  - `Radial Hatch` — Reserved compatibility choice; the current Infill region does not generate
    paths for this value.

<a id="setting-infill_lines_partitioned_linking"></a>

##### Apply Path and Point Ordering to Infill Lines (`infill_lines_partitioned_linking`)

If enabled and the Infill Pattern is Lines, travel-separated infill segments are selected using Path
Order Optimization and oriented using Point Order Optimization. If disabled, they retain the
existing monotonic ordering.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and Infill Pattern is
  `Lines`)).

<a id="setting-infill_avoid_link_overlap"></a>

##### Avoid Infill Link Contour Overlap (`infill_avoid_link_overlap`)

If enabled, infill linking segments whose bead core would exceed the configured infill overlap are
replaced by travel moves.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_based_on_printer"></a>

##### Infill Based on Printer Area (`infill_based_on_printer`)

If selected, infill pattern is generated based on the printer area rather than the object. This
means the infill pattern is specific to where the object is within the printer.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_angle"></a>

##### Infill Angle (`infill_angle`)

Sets the angle for the infill.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_angle_rotation"></a>

##### Infill Angle Rotation (`infill_angle_rotation`)

Determines how much the infill angle rotates on every layer, default 90 degree.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `90°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_overlap_distance"></a>

##### Infill Overlap Distance (`infill_overlap_distance`)

Width of the infill overlaps with exterior.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_width"></a>

##### Infill Bead Width (`infill_width`)

Width of the bead for the infill print moves.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_speed"></a>

##### Infill Speed (`infill_speed`)

Speed for the infill paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_extruder_speed"></a>

##### Infill Extruder Speed (`infill_extruder_speed`)

Extruder speed for the infill.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `0 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Use Width and
  Height is disabled and (Machine Type is `Pellet` or (Machine Type is `Concrete` or Machine Type is
  `Thermoset`))))).

<a id="setting-infill_extrusion_multiplier"></a>

##### Infill Extrusion Multiplier (`infill_extrusion_multiplier`)

Extrusion multiplier to increase/decrease flowrate for infill paths for systems without RPM control.

- **Input:** `unitless_float` — Decimal value without a physical unit.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Enable Infill is enabled and (Machine Type is
  `Filament` or Syntax is `KraussMaffei`))).

<a id="setting-infill_minimum_path_length"></a>

##### Minimum Infill Path Length (`infill_minimum_path_length`)

Infill extrusion paths less than this value are deleted.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_minimum_segment_length"></a>

##### Minimum Infill Segment Length (`infill_minimum_segment_length`)

Infill path segments shorter than this value are collapsed before extrusion paths are created.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_maximum_path_length"></a>

##### Maximum Infill Path Length (`infill_maximum_path_length`)

Infill extrusion paths greater than this are split.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_combine_every_x_layers"></a>

##### Combine Infill Every X Layers (`infill_combine_every_x_layers`)

Prints perimeter for X amount of layers then goes back and prints all of the infill for those layers
as one thicker layer, saves time.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_combine_layer_shift"></a>

##### Shift Combine Infill Start Layer (`infill_combine_layer_shift`)

Shift the first starting layer of infill when using combine infill every X layers. A shift value of
1 paired with a combine value of 4 would mean infill on layers 5, 9, 13 and so on.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="settings-profile-support"></a>

#### Profile > Support

Controls generated grid or organic support, interfaces, bases, spacing, tapering, and connectivity.

<a id="setting-support"></a>

##### Enable Support (`support`)

If selected, support structures will be generated for overhangs.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-support_print_first"></a>

##### Print Support First (`support_print_first`)

If selected, support material will be printed first.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_structure"></a>

##### Support Structure (`support_structure`)

Generate conventional grid columns or branching organic supports.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Grid`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).
- **Choices:**
  - `Grid`
  - `Organic / Tree`

<a id="setting-support_placement"></a>

##### Support Placement (`support_placement`)

Allow support to land on the model or retain only support connected to the build plate.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Everywhere`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).
- **Choices:**
  - `Everywhere`
  - `Build Plate Only`

<a id="setting-support_tapering"></a>

##### Taper Support (`support_tapering`)

Grow support inward toward the interface while retaining a stable vertical outer tube wall.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_taper_angle"></a>

##### Support Taper Angle (`support_taper_angle`)

Angle of inward support growth below the interface; larger angles require less taper height.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `45°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Taper Support is
  enabled)).

<a id="setting-support_taper_wall_contours"></a>

##### Minimum Tube Wall Contours (`support_taper_wall_contours`)

Minimum number of bead-width contours retained in the vertical support tube wall.

- **Input:** `number` — Integer value.
- **Master default:** `2`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Taper Support is
  enabled)).

<a id="setting-support_threshold_angle"></a>

##### Support Threshold Angle (`support_threshold_angle`)

Overhangs exceeding this angle will generate support.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_xy_distance"></a>

##### Support XY Distance (`support_xy_distance`)

XY distance from the part to generate support.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_layer_offset"></a>

##### Support Layer Offset (`support_layer_offset`)

Number of vertical layers to offset support from model surface.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_minimum_infill_area"></a>

##### Minimum Support Infill Area (`support_minimum_infill_area`)

Support structures smaller than this will not generate infill.

- **Input:** `area` — Area; displayed as the square of the preferred distance unit.
- **Master default:** `0 mm²`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_minimum_area"></a>

##### Minimum Support Area (`support_minimum_area`)

Areas of the part smaller than this will not have supports generated.

- **Input:** `area` — Area; displayed as the square of the preferred distance unit.
- **Master default:** `0 mm²`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_pattern"></a>

##### Support Pattern (`support_pattern`)

Infill pattern used inside support structures.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Grid`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).
- **Choices:**
  - `Lines`
  - `Grid`

<a id="setting-support_line_spacing"></a>

##### Support Line Spacing (`support_line_spacing`)

Distance between beads on sparse support fill.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `10 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_minimum_segment_length"></a>

##### Minimum Support Segment Length (`support_minimum_segment_length`)

Support path segments shorter than this value are collapsed before extrusion paths are created.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_wall_contours"></a>

##### Support Wall Contours (`support_wall_contours`)

Number of perimeter contours around conventional sparse support.

- **Input:** `number` — Integer value.
- **Master default:** `1`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_interface_layers"></a>

##### Support Interface Layers (`support_interface_layers`)

Number of dense support layers immediately below supported surfaces; zero disables the interface.

- **Input:** `number` — Integer value.
- **Master default:** `3`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_interface_line_spacing"></a>

##### Support Interface Line Spacing (`support_interface_line_spacing`)

Line spacing in dense support interfaces; zero automatically uses one bead width.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_interface_expansion"></a>

##### Support Interface Expansion (`support_interface_expansion`)

Horizontal expansion of dense interface layers beyond the detected overhang.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_base_layers"></a>

##### Solid Support Base Layers (`support_base_layers`)

Number of dense stabilization layers at support structures that reach the build plate.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_base_expansion"></a>

##### Support Base Expansion (`support_base_expansion`)

Horizontal expansion of build-plate support bases.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_bridge_suppression"></a>

##### Suppress Support Under Bridges (`support_bridge_suppression`)

Skip support where an overhang is anchored on opposing sides within the configured bridge length.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_bridge_max_length"></a>

##### Maximum Unsupported Bridge Length (`support_bridge_max_length`)

Longest opposing-edge span that may be printed as a bridge without support.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Suppress Support
  Under Bridges is enabled)).

<a id="setting-support_validation"></a>

##### Validate Support Connectivity (`support_validation`)

Remove support components that do not connect to the build plate or an allowed model landing.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Enabled` (`true`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_validation_minimum_overlap"></a>

##### Minimum Support Layer Overlap (`support_validation_minimum_overlap`)

Minimum percentage of each support component that must overlap valid support or model below.

- **Input:** `percentage100` — Percentage input with an allowed range of 0–100%.
- **Master default:** `5%`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Validate Support
  Connectivity is enabled)).

<a id="setting-support_validation_minimum_base_area"></a>

##### Minimum Support Base Area (`support_validation_minimum_base_area`)

Minimum build-plate contact area for a support component; zero disables this check.

- **Input:** `area` — Area; displayed as the square of the preferred distance unit.
- **Master default:** `0 mm²`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Validate Support
  Connectivity is enabled)).

<a id="setting-support_organic_branch_diameter"></a>

##### Organic Branch Diameter (`support_organic_branch_diameter`)

Minimum branch diameter; zero automatically uses three bead widths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Support Structure
  is `Organic / Tree`)).

<a id="setting-support_organic_branch_spacing"></a>

##### Organic Contact Spacing (`support_organic_branch_spacing`)

Spacing between organic contact branches; zero chooses an automatic spacing.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Support Structure
  is `Organic / Tree`)).

<a id="setting-support_organic_branch_angle"></a>

##### Organic Branch Angle (`support_organic_branch_angle`)

Maximum branch convergence angle measured from vertical.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `25°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and (Enable Support is enabled and Support Structure
  is `Organic / Tree`)).

<a id="settings-profile-travel"></a>

#### Profile > Travel

Controls non-print motion, minimum travel thresholds, lift behavior, pauses, and centroid moves.

<a id="setting-travel_speed"></a>

##### Travel Speed (`travel_speed`)

Speed for travel moves.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Always available.

<a id="setting-minimum_infill_travel_length"></a>

##### Minimum Infill Travel Length (`minimum_infill_travel_length`)

Travel moves less than this value in infill, skin, and support fill are replaced with extrusion
moves.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `25.4 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and ((Enable Infill is enabled or Enable Skin is
  enabled) or Enable Support is enabled)).

<a id="setting-min_travel_length"></a>

##### Minimum Travel Length (`min_travel_length`)

Travel moves less than this value will leave the extruder enabled.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Syntax is `Cincinnati` or (Syntax is `JuggerBot3D` or Syntax is `Arc
  Specialties`)).

<a id="setting-minimum_travel_for_lift"></a>

##### Minimum Travel Length for Lifting (`minimum_travel_for_lift`)

Travel moves less than this value will not create a travel lift.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Always available.

<a id="setting-travel_lift_height"></a>

##### Travel Lift Height (`travel_lift_height`)

Height the Z axis lifts by to clear the part during travel moves.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-final_lift_distance"></a>

##### Final Lift Distance (`final_lift_distance`)

Distance to lift the tool at the end of the print along the slicing plane normal before shutdown.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-enable_travel_pause"></a>

##### Pause During Travel (`enable_travel_pause`)

Allows for a pause command to be issued after the travel lift but before the travel XY motion. The
centroid move can be added and issued before the pause.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-travel_centroid_move"></a>

##### Move to Centroid During Pause (`travel_centroid_move`)

Moves to the XY centroid of the object before the travel pause.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Pause During Travel is enabled).

<a id="setting-travel_pause_duration"></a>

##### Travel Pause Duration (`travel_pause_duration`)

Duration of the pause during travel motion.

- **Input:** `time` — Duration; displayed in the preferred time unit.
- **Master default:** `0 s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Pause During Travel is enabled).

<a id="settings-profile-g-code"></a>

#### Profile > G-Code

Adds region-specific command blocks before and after generated paths.

<a id="setting-perimeter_start_code"></a>

##### Perimeter Start G-Code (`perimeter_start_code`)

Code to be executed at the start of perimeter paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-perimeter_end_code"></a>

##### Perimeter End G-Code (`perimeter_end_code`)

Code to be executed at the end of perimeter paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).

<a id="setting-inset_start_code"></a>

##### Inset Start G-Code (`inset_start_code`)

Code to be executed at the start of inset paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-inset_end_code"></a>

##### Inset End G-Code (`inset_end_code`)

Code to be executed at the end of inset paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).

<a id="setting-skeleton_start_code"></a>

##### Skeleton Start G-Code (`skeleton_start_code`)

Code to be executed at the start of skeleton paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skeleton_end_code"></a>

##### Skeleton End G-Code (`skeleton_end_code`)

Code to be executed at the end of skeleton paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skeletons is enabled).

<a id="setting-skin_start_code"></a>

##### Skin Start G-Code (`skin_start_code`)

Code to be executed at the start of skin paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-skin_end_code"></a>

##### Skin End G-Code (`skin_end_code`)

Code to be executed at the end of skin paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).

<a id="setting-infill_start_code"></a>

##### Infill Start G-Code (`infill_start_code`)

Code to be executed at the start of infill paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-infill_end_code"></a>

##### Infill End G-Code (`infill_end_code`)

Code to be executed at the end of infill paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Infill is enabled).

<a id="setting-support_start_code"></a>

##### Support Start G-Code (`support_start_code`)

Code to be executed at the start of support paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="setting-support_end_code"></a>

##### Support End G-Code (`support_end_code`)

Code to be executed at the end of support paths.

- **Input:** `multiline_text` — Multi-line G-code or text block.
- **Master default:** `empty`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Support is enabled).

<a id="settings-profile-special-modes"></a>

#### Profile > Special Modes

Enables geometry repair and transformations such as smoothing, spiralize, and oversizing, plus
bead-geometry output for a compatible HMI.

<a id="setting-smoothing"></a>

##### Enable Smoothing (`smoothing`)

If selected, a smoothing process will be applied on all contours of the STL.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-smoothing_type"></a>

##### Smoothing Type (`smoothing_type`)

Selects the polyline simplification algorithm applied to sliced contours. The algorithms remove
points differently, so compare geometry at the chosen tolerance before production use.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Douglas Peucker`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Smoothing is enabled.
- **Choices:**
  - `Douglas Peucker` — Recursively retains points with the greatest perpendicular deviation until
    all remaining deviations are within the tolerance.
  - `Radial Distance` — Keeps a point when it is farther than the tolerance from the last retained
    point.
  - `Perpendicular Distance` — Removes intermediate points whose perpendicular distance from a
    neighboring segment is within the tolerance.
  - `Reumann-Witkam` — Advances a tolerance-width corridor along the contour and retains a point
    when the contour exits it.

<a id="setting-smoothing_tolerance"></a>

##### Smoothing Tolerance (`smoothing_tolerance`)

Sets the distance threshold supplied to the selected contour-smoothing algorithm. Larger values
generally remove more vertices and can deviate farther from the original cross-section.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Smoothing is enabled.

<a id="setting-arc_fitting"></a>

##### Enable Arc Fitting (`arc_fitting`)

Fits eligible planar print moves to circular G2/G3 arcs when the selected machine and syntax support
G2/G3.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Supports G2/G3 is enabled and (Slicing Mode is `Planar` and (Not (Syntax is
  `MVP`) and Not (Syntax is `Adamantine`)))).

<a id="setting-arc_fitting_tolerance"></a>

##### Arc Fitting Tolerance (`arc_fitting_tolerance`)

Maximum radial deviation allowed when replacing a run of line segments with a circular arc.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0.05 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Enable Arc Fitting is enabled.

<a id="setting-arc_fitting_minimum_segment_count"></a>

##### Minimum Arc Fitting Segments (`arc_fitting_minimum_segment_count`)

Minimum number of consecutive line segments required before arc fitting can replace them with one
G2/G3 move.

- **Input:** `positive_int` — Positive integer value; the input control has a minimum of 1.
- **Master default:** `3`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Enable Arc Fitting is enabled.

<a id="setting-sharp_corner_extension"></a>

##### Enable Sharp Corner Extension (`sharp_corner_extension`)

If selected, sharp toolpath junctions are extended outward to compensate for corner rounding during
printing.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Always available.

<a id="setting-sharp_corner_extension_angle"></a>

##### Sharp Corner Angle Threshold (`sharp_corner_extension_angle`)

Maximum corner angle that can be sharpened.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `90°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Enable Sharp Corner Extension is enabled.

<a id="setting-sharp_corner_extension_distance"></a>

##### Sharp Corner Extension Length (`sharp_corner_extension_distance`)

Distance to extend the corner merge point along the corner bisector.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Enable Sharp Corner Extension is enabled.

<a id="setting-sharp_corner_close_points_threshold"></a>

##### Sharp Corner Close Points Threshold (`sharp_corner_close_points_threshold`)

Maximum length of a bead segment connecting two corner legs that can be removed before sharpening.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Enable Sharp Corner Extension is enabled.

<a id="setting-sharp_corner_sharpening_leg_length"></a>

##### Sharp Corner Sharpening Leg Length (`sharp_corner_sharpening_leg_length`)

Distance along each original corner leg to replace with sharpened geometry. A value of 0 uses the
extension length.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Enable Sharp Corner Extension is enabled.

<a id="setting-enable_spiralize_mode"></a>

##### Enable Spiralize Mode (`enable_spiralize_mode`)

If selected, extruder will never lift and part will be made in one continuous path without infill.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-enable_fix_model"></a>

##### Enable Fix Model (`enable_fix_model`)

If selected, manifold issues with the model will be repaired.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-oversize"></a>

##### Oversize Part (`oversize`)

If selected, the part can be oversized in the X and Y dimensions to allow for a machining tolerance.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-oversize_distance"></a>

##### Oversize Distance (`oversize_distance`)

Distance to oversize the part in the X and Y directions.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Oversize Part is enabled).

<a id="setting-enable_width_height"></a>

##### Use Width and Height (`enable_width_height`)

If selected, G-Code will not command feeds and speed and instead send desired bead geometry for the
HMI to interpret.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Syntax is `JuggerBot3D`).

<a id="settings-profile-optimizations"></a>

#### Profile > Optimizations

Controls ordering at layer, island, path, and point levels, including custom points and randomness.

<a id="setting-layer_ordering"></a>

##### Layer Ordering (`layer_ordering`)

Selects how layers from multiple parts are grouped and emitted: by physical height, by each part's
layer index, or one complete part at a time.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `By Height`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Slicing Mode is `Planar`.
- **Choices:**
  - `By Height` — Merges compatible part layers by projected physical height; Layer Grouping
    Tolerance controls when nearby planes share a global layer.
  - `By Layer Number` — Groups layer 0 from every part, then layer 1 from every part, and so on.
  - `By Part (Sequential)` — Emits every layer of one part before moving to the next part.

<a id="setting-layer_grouping_tolerance"></a>

##### Layer Grouping Tolerance (`layer_grouping_tolerance`)

Layers with the same plane normal that are separated by a distance less than this value will be
assigned to the same global layer.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0.001 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Slicing Mode is `Planar` and Layer Ordering is `By Height`).

<a id="setting-island_order_optimization"></a>

##### Island Order Optimization (`island_order_optimization`)

Selects which printable island is visited next, trading travel distance, computation time, thermal
distribution, or a user-defined reference point.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Next Closest`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.
- **Choices:**
  - `Next Closest` — Greedily selects the island with the boundary point nearest the current
    position.
  - `Next Farthest` — Uses the brute-force route optimizer to favor the greatest inter-island
    distance.
  - `Shortest Distance (approximate)` — Uses a faster approximate traveling-salesperson route to
    reduce total island travel.
  - `Shortest Distance (brute force)` — Searches island permutations for the shortest route;
    computation grows quickly with island count.
  - `Least Recently Visited` — Rotates the starting island using the island visited last on the
    prior layer.
  - `Random` — Chooses the next remaining island randomly on each layer.
  - `Custom Location` — Uses the custom X/Y location as the reference, then applies nearest-island
    selection.

<a id="setting-custom_island_order_x_location"></a>

##### Custom Island Point X Location (`custom_island_order_x_location`)

X Coordinate for Custom Point Optimization Location.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Island Order Optimization is `Custom Location`).

<a id="setting-custom_island_order_y_location"></a>

##### Custom Island Point Y Location (`custom_island_order_y_location`)

Y Coordinate for Custom Point Optimization Location.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Island Order Optimization is `Custom Location`).

<a id="setting-custom_island_order_z_location"></a>

##### Custom Island Point Z Location (`custom_island_order_z_location`)

Z Coordinate for Custom Point Optimization Location.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Island Order Optimization is `Custom Location`).

<a id="setting-path_order_optimization"></a>

##### Path Order Optimization (`path_order_optimization`)

Selects which path within an island is printed next, using distance, randomness, contour hierarchy,
or a user-defined reference point.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Next Closest`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.
- **Choices:**
  - `Next Closest` — Selects the path whose available start is nearest the current position.
  - `Next Farthest` — Selects the path whose available start is farthest from the current position.
  - `Random` — Selects a remaining path and, for open paths, its direction randomly.
  - `Outside In` — Traverses contour hierarchy from exterior paths toward interior paths.
  - `Inside Out` — Traverses contour hierarchy from interior paths toward exterior paths.
  - `Custom Location` — Uses the custom X/Y location as the reference for nearest-path selection.

<a id="setting-cylindrical_path_order_optimization"></a>

##### Cylindrical Path Order Optimization (`cylindrical_path_order_optimization`)

Type of path order optimizer to use for radial and helical cylindrical slicing paths.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Next Closest`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Cylindrical`.
- **Choices:**
  - `Next Closest`
  - `Next Farthest`

<a id="setting-perimeter_path_order_optimization"></a>

##### Perimeter Path Order Optimization (`perimeter_path_order_optimization`)

Type of order optimizer to use on perimeter paths. Use Global follows Path Order Optimization.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Use Global`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).
- **Choices:**
  - `Use Global`
  - `Next Closest`
  - `Next Farthest`
  - `Random`
  - `Outside In`
  - `Inside Out`
  - `Custom Location`

<a id="setting-inset_path_order_optimization"></a>

##### Inset Path Order Optimization (`inset_path_order_optimization`)

Type of order optimizer to use on inset paths. Use Global follows Path Order Optimization.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Use Global`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).
- **Choices:**
  - `Use Global`
  - `Next Closest`
  - `Next Farthest`
  - `Random`
  - `Outside In`
  - `Inside Out`
  - `Custom Location`

<a id="setting-skin_path_order_optimization"></a>

##### Skin Path Order Optimization (`skin_path_order_optimization`)

Type of order optimizer to use on skin paths. Use Global follows Path Order Optimization.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Use Global`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Skin is enabled).
- **Choices:**
  - `Use Global`
  - `Next Closest`
  - `Next Farthest`
  - `Random`
  - `Outside In`
  - `Inside Out`
  - `Custom Location`

<a id="setting-custom_path_order_x_location"></a>

##### Custom Path Point X Location (`custom_path_order_x_location`)

X Coordinate for Custom Point Optimization Location.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Path Order Optimization is `Custom Location` or
  (Perimeter Path Order Optimization is `Custom Location` or (Inset Path Order Optimization is
  `Custom Location` or Skin Path Order Optimization is `Custom Location`)))).

<a id="setting-custom_path_order_y_location"></a>

##### Custom Path Point Y Location (`custom_path_order_y_location`)

Y Coordinate for Custom Point Optimization Location.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Path Order Optimization is `Custom Location` or
  (Perimeter Path Order Optimization is `Custom Location` or (Inset Path Order Optimization is
  `Custom Location` or Skin Path Order Optimization is `Custom Location`)))).

<a id="setting-custom_path_order_z_location"></a>

##### Custom Path Point Z Location (`custom_path_order_z_location`)

Z Coordinate for Custom Point Optimization Location.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Path Order Optimization is `Custom Location` or
  (Perimeter Path Order Optimization is `Custom Location` or (Inset Path Order Optimization is
  `Custom Location` or Skin Path Order Optimization is `Custom Location`)))).

<a id="setting-point_order_optimization"></a>

##### Point Order Optimization (`point_order_optimization`)

Selects the start point or direction after a path is chosen, using proximity, randomness,
layer-to-layer progression, or a user-defined reference point.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Next Closest`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.
- **Choices:**
  - `Next Closest` — Starts at the path point nearest the current tool position.
  - `Next Farthest` — Starts at the path point farthest from the current tool position.
  - `Random` — Selects a path start point randomly.
  - `Consecutive` — Advances the start point with layer number until Consecutive Distance Threshold
    is reached.
  - `Custom Location` — Starts at the path point nearest the custom X/Y reference.
  - `Custom Farthest Location` — Starts at the path point farthest from the custom X/Y reference.

<a id="setting-enable_point_order_segment_breaking"></a>

##### Enable Point Order Segment Breaking (`enable_point_order_segment_breaking`)

Allows point order optimization to split a closed contour segment so the seam can start at the
closest point along an edge.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Next Closest` or
  Point Order Optimization is `Custom Location`)).

<a id="setting-local_randomness_enable"></a>

##### Enable Local Randomness (`local_randomness_enable`)

Enables local randomness within a specified radius after the selected point optimization scheme.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-local_randomness_radius"></a>

##### Local Randomness Radius (`local_randomness_radius`)

Radius within which the travel connection will be randomized after having computed connectivity via
point optimization scheme.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Local Randomness is enabled).

<a id="setting-enable_min_distance"></a>

##### Enable Minimum Distance (`enable_min_distance`)

Enables a minimum distance threshold when using next closest point optimization.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Point Order Optimization is `Next Closest`).

<a id="setting-min_distance_threshold"></a>

##### Minimum Distance Threshold (`min_distance_threshold`)

Minimum distance that a point must be when selected via next closest point optimization. If none are
found, furthest point is selected.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Next Closest` and
  Enable Minimum Distance is enabled)).

<a id="setting-consecutive_distance_threshold"></a>

##### Consecutive Distance Threshold (`consecutive_distance_threshold`)

Sets minimum distance to rotate consecutive point layer to layer.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Point Order Optimization is `Consecutive`).

<a id="setting-custom_point_order_x_location"></a>

##### Custom Point X Location (`custom_point_order_x_location`)

X Coordinate for Custom Point for Point Optimization Scheme.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Custom Location`
  or Point Order Optimization is `Custom Farthest Location`)).

<a id="setting-custom_point_order_y_location"></a>

##### Custom Point Y Location (`custom_point_order_y_location`)

Y Coordinate for Custom Point for Point Optimization Scheme.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Custom Location`
  or Point Order Optimization is `Custom Farthest Location`)).

<a id="setting-custom_point_order_z_location"></a>

##### Custom Point Z Location (`custom_point_order_z_location`)

Z Coordinate for Custom Point for Point Optimization Scheme.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Custom Location`
  or Point Order Optimization is `Custom Farthest Location`)).

<a id="setting-enable_second_custom_point_location"></a>

##### Add Second Custom Location (`enable_second_custom_point_location`)

Enables a second custom point location so that points alternate layer to layer. The second point is
used for even numbered layers.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Custom Location`
  or Point Order Optimization is `Custom Farthest Location`)).

<a id="setting-enable_second_point_every_two"></a>

##### Enable Second Custom Location Every Two Layers (`enable_second_point_every_two`)

Enables the second custom point, but custom points are alternated every two layers. I.e. layers 1
and 2 use the original point and layers 3 and 4 use the second point.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Add Second Custom Location is enabled and
  (Point Order Optimization is `Custom Location` or Point Order Optimization is `Custom Farthest
  Location`))).

<a id="setting-custom_second_point_order_x_location"></a>

##### Second Custom Point X Location (`custom_second_point_order_x_location`)

X Coordinate for Second Custom Point for Point Optimization Scheme.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Add Second Custom Location is enabled and
  (Point Order Optimization is `Custom Location` or Point Order Optimization is `Custom Farthest
  Location`))).

<a id="setting-custom_second_point_order_y_location"></a>

##### Second Custom Point Y Location (`custom_second_point_order_y_location`)

Y Coordinate for Second Custom Point for Point Optimization Scheme.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Add Second Custom Location is enabled and
  (Point Order Optimization is `Custom Location` or Point Order Optimization is `Custom Farthest
  Location`))).

<a id="setting-custom_second_point_order_z_location"></a>

##### Second Custom Point Z Location (`custom_second_point_order_z_location`)

Z Coordinate for Second Custom Point for Point Optimization Scheme.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Add Second Custom Location is enabled and
  (Point Order Optimization is `Custom Location` or Point Order Optimization is `Custom Farthest
  Location`))).

<a id="setting-seam_attractor_vector"></a>
<a id="setting-seam_attractor_vector_x"></a>
<a id="setting-seam_attractor_vector_y"></a>
<a id="setting-seam_attractor_vector_z"></a>

##### Seam Attractor Vector (`seam_attractor_vector`)

Vector used to project seam attractor points onto each slicing plane. Leave all components at 0 to
use the slicing vector automatically.

- **Input:** `vector3` grouped control with the components listed below.
- **Scope:** Local-capable. Each component can be overridden through this grouped row at supported
  narrower scopes.
- **Available when:** (Slicing Mode is `Planar` and (Island Order Optimization is `Custom Location`
  or ((Path Order Optimization is `Custom Location` or (Perimeter Path Order Optimization is `Custom
  Location` or (Inset Path Order Optimization is `Custom Location` or Skin Path Order Optimization
  is `Custom Location`))) or (Point Order Optimization is `Custom Location` or Point Order
  Optimization is `Custom Farthest Location`)))).
- **Components and master defaults:**
  - **X:** `seam_attractor_vector_x` — `0`
  - **Y:** `seam_attractor_vector_y` — `0`
  - **Z:** `seam_attractor_vector_z` — `0`

<a id="setting-custom_point_order_x_increment"></a>

##### Custom Point X Increment (`custom_point_order_x_increment`)

Distance to increment the custom point X location from layer to layer. This is used for angled
printing to move the seam along with the slicing plane.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Custom Location`
  or Point Order Optimization is `Custom Farthest Location`)).

<a id="setting-custom_point_order_y_increment"></a>

##### Custom Point Y Increment (`custom_point_order_y_increment`)

Distance to increment the custom point Y location from layer to layer. This is used for angled
printing to move the seam along with the slicing plane.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and (Point Order Optimization is `Custom Location`
  or Point Order Optimization is `Custom Farthest Location`)).

<a id="settings-profile-ordering"></a>

#### Profile > Ordering

Sets region order and direction-reversal policies for perimeters and insets.

<a id="setting-region_order"></a>

##### Region Order (`region_order`)

Order that region paths will be connected.

- **Input:** `numbered_list` — Ordered list whose entries can be rearranged.
- **Master default:** `Perimeter`, `Inset`, `Skin`, `Infill`, `Skeleton`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-perimeter_reverse_direction"></a>

##### Reverse Perimeter Direction (`perimeter_reverse_direction`)

Reverse the printing direction of the perimeters (CW vs CCW)

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `REVERSE_OFF`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Perimeter is enabled).
- **Choices:**
  - `REVERSE_OFF`
  - `REVERSE_ALL_LAYERS`
  - `REVERSE_ALTERNATING_LAYERS`

<a id="setting-inset_reverse_direction"></a>

##### Reverse Inset Direction (`inset_reverse_direction`)

Reverse the printing direction of the insets (CW vs CCW)

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `REVERSE_OFF`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Inset is enabled).
- **Choices:**
  - `REVERSE_OFF`
  - `REVERSE_ALL_LAYERS`
  - `REVERSE_ALTERNATING_LAYERS`

<a id="settings-profile-laser-scanner"></a>

#### Profile > Laser Scanner

Configures laser-scan paths, offsets, resolution, orientation, buffering, and height-map behavior.

<a id="setting-laser_scanner"></a>

##### Enable Laser Scanner (`laser_scanner`)

If selected, will generate G/M codes for laser scanner path between layers.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-laser_speed"></a>

##### Laser Scan Speed (`laser_speed`)

Speed for scan paths.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `0 mm/s`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_height_offset"></a>

##### Laser Scanner Height Offset (`laser_scanner_height_offset`)

Height of scanner above table in home position.

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_x_offset"></a>

##### Laser Scanner X Offset (`laser_scanner_x_offset`)

X offset of scanner from command position (typically extruder head)

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_y_offset"></a>

##### Laser Scanner Y Offset (`laser_scanner_y_offset`)

Y offset of scanner from command position (typically extruder head)

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_height"></a>

##### Laser Scanner Height (`laser_scanner_height`)

Target height of scanner above layers.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_width"></a>

##### Laser Scanner Width (`laser_scanner_width`)

Sets the center-to-center spacing between adjacent serpentine scan passes. Supported scanner output
also transmits this value as the scanner width.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_step_distance"></a>

##### Laser Step Distance (`laser_scanner_step_distance`)

Sets the scanner step-axis sample spacing sent to height-map post-processing. It affects reported
patch resolution, not the spacing of generated scan paths.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scan_line_resolution"></a>

##### Laser Scan Line Resolution (`laser_scan_line_resolution`)

Sets the sample spacing along each scan line sent to height-map post-processing. It affects reported
patch resolution, not G-code path subdivision.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-laser_scanner_axis"></a>

##### Scanner Axis (`laser_scanner_axis`)

Selects whether each serpentine scan pass travels along machine X or machine Y; adjacent passes are
offset on the other axis.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `X`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.
- **Choices:**
  - `X`
  - `Y`

<a id="setting-invert_laser_scanner_head"></a>

##### Invert Laser Head (`invert_laser_scanner_head`)

Direction of mounted laser head with respect to machine axis.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-enable_bed_scan"></a>

##### Enable Bed Scan (`enable_bed_scan`)

Scan print bed before first layer deposition.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-scan_layer_skip"></a>

##### Scan Layer Skip (`scan_layer_skip`)

Sets the inter-layer scan interval: 0 disables inter-layer scans, 1 scans every layer, 2 scans every
other layer, and so on.

- **Input:** `number` — Integer value.
- **Master default:** `0`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-enable_scanner_buffer"></a>

##### Enable Scanner Buffer (`enable_scanner_buffer`)

If selected, will allow scan lines to be offset by buffer distance to allow acceleration.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-buffer_distance"></a>

##### Buffer Distance (`buffer_distance`)

Distance to offset scan lines to allow acceleration.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Enable Laser Scanner is enabled and Enable Scanner Buffer is enabled).

<a id="setting-transmit_height_map"></a>

##### Transmit Height Map (`transmit_height_map`)

Transmit height map via socket to controlling software.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-global_scan"></a>

##### Global Scan (`global_scan`)

Scan all objects as part of global bounding box.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-orientation_axis"></a>

##### Orientation Axis (`orientation_axis`)

Selects the machine X, Y, or Z axis identifier transmitted with scan metadata to describe the
scan-plane orientation. It does not rotate generated scan paths.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Z`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.
- **Choices:**
  - `X`
  - `Y`
  - `Z`

<a id="setting-orientation_angle"></a>

##### Orientation Angle (`orientation_angle`)

Sets the angle transmitted with Orientation Axis to describe the scan-plane orientation. The default
vertical orientation is Z at 0 degrees; this value does not rotate generated scan paths.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-enable_orientation_definition"></a>

##### Custom Orientation Definition (`enable_orientation_definition`)

Define angles that represent orientation (robotic arm systems)

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Laser Scanner is enabled.

<a id="setting-orientation_a"></a>

##### Angle A (`orientation_a`)

Angle A (X axis rotation - pitch)

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Enable Laser Scanner is enabled and Custom Orientation Definition is
  enabled).

<a id="setting-orientation_b"></a>

##### Angle B (`orientation_b`)

Angle B (Y axis rotation - yaw)

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Enable Laser Scanner is enabled and Custom Orientation Definition is
  enabled).

<a id="setting-orientation_c"></a>

##### Angle C (`orientation_c`)

Angle C (Z axis rotation - roll)

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `0°`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Enable Laser Scanner is enabled and Custom Orientation Definition is
  enabled).

<a id="settings-profile-thermal-scanner"></a>

#### Profile > Thermal Scanner

Configures thermal scan enablement, offsets, and temperature cutoff.

<a id="setting-thermal_scanner"></a>

##### Enable Thermal Scanner (`thermal_scanner`)

If selected, will generate g/m codes for IR camera measurements between layers.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Cincinnati`.

<a id="setting-thermal_scanner_x_offset"></a>

##### Thermal Scanner X Offset (`thermal_scanner_x_offset`)

X offset of scanner from command position (typically extruder head)

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Thermal Scanner is enabled.

<a id="setting-thermal_scanner_y_offset"></a>

##### Thermal Scanner Y Offset (`thermal_scanner_y_offset`)

Y offset of scanner from command position (typically extruder head)

- **Input:** `location` — Signed position or offset in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Thermal Scanner is enabled.

<a id="setting-thermal_scanner_temperature_cutoff"></a>

##### Thermal Scanner Temperature Cutoff (`thermal_scanner_temperature_cutoff`)

Reserved IR-camera temperature cutoff. The current thermal-scan slicing and writer paths do not read
this value, so it has no effect.

- **Input:** `temperature` — Temperature; displayed in the preferred temperature unit.
- **Master default:** `-273.15 °C`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Thermal Scanner is enabled.

### E.5 Experimental settings

Experimental settings expose specialized path and file-generation controls. Confirm writer and
machine support before depending on them in a production workflow.

![Figure 64 placeholder: Experimental settings](user-guide-images/figure64.png)

> **Diagram placeholder — Experimental settings:** Add an annotated Experimental panel with its
> category tabs, search field, and one enabled/disabled dependency example.

<a id="settings-experimental-auto-speed-ramping"></a>

#### Experimental > Auto Speed Ramping

Adjusts speed and extrusion around path-angle changes using configurable ramp distances.

<a id="setting-trajectory_angle_slow_down"></a>

##### Enable Trajectory Auto Speed Ramping (`trajectory_angle_slow_down`)

Enables speed and extruder ramping around corners whose computed trajectory angle is below Angle
Threshold.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** Slicing Mode is `Planar`.

<a id="setting-trajectory_angle_threshold_slow_down"></a>

##### Angle Threshold (`trajectory_angle_threshold_slow_down`)

Sets the trajectory-angle cutoff for auto speed ramping. A corner qualifies when its computed angle
is less than this threshold.

- **Input:** `angle` — Angle; displayed in the preferred angle unit.
- **Master default:** `135°`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="setting-trajectory_angle_distance_slow_down"></a>

##### Min Ramp Down Distance (`trajectory_angle_distance_slow_down`)

Sets the minimum path distance before a qualifying corner over which the ramp-down values are
applied.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `25.4 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="setting-trajectory_angle_distance_speed_up"></a>

##### Min Ramp Up Distance (`trajectory_angle_distance_speed_up`)

Sets the minimum path distance after a qualifying corner over which the ramp-up values are applied.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `25.4 mm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="setting-trajectory_angle_speed_slow_down"></a>

##### Ramping Down Speed (`trajectory_angle_speed_slow_down`)

Sets the reduced gantry speed used while ramping down before a corner whose trajectory angle is
below the threshold.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `25.4 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="setting-trajectory_angle_extruder_speed_slow_down"></a>

##### Ramping Down Extruder Speed (`trajectory_angle_extruder_speed_slow_down`)

Sets the extruder speed used while ramping down before a corner whose trajectory angle is below the
threshold.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `14 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="setting-trajectory_angle_speed_up"></a>

##### Ramping Up Speed (`trajectory_angle_speed_up`)

Sets the gantry speed used while ramping up after a corner whose trajectory angle is below the
threshold.

- **Input:** `speed` — Linear velocity; displayed in the preferred velocity unit.
- **Master default:** `50.8 mm/s`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="setting-trajectory_angle_extruder_speed_up"></a>

##### Ramping Up Extruder Speed (`trajectory_angle_extruder_speed_up`)

Sets the extruder speed used while ramping up after a corner whose trajectory angle is below the
threshold.

- **Input:** `rpm` — Rotational speed in revolutions per minute.
- **Master default:** `28 rpm`
- **Scope:** Local-capable. It can be overridden at supported part, layer/range, or spatial scopes;
  mode-specific scope limitations still apply.
- **Available when:** (Slicing Mode is `Planar` and Enable Trajectory Auto Speed Ramping is
  enabled).

<a id="settings-experimental-file-output"></a>

#### Experimental > File Output

Enables syntax-specific companion, simulation, or auxiliary output files.

<a id="setting-additional_meld_output"></a>

##### Enable Meld Companion File Output (`additional_meld_output`)

Additional companion file csv output for Meld syntax.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Meld`.

<a id="setting-meld_discrete_feed_commands"></a>

##### Enable Meld Discrete Feed Commands (`meld_discrete_feed_commands`)

Uses discrete feed commands for g-code output.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Meld`.

<a id="setting-tormach_file_output"></a>

##### Enable Tormach File Output (`tormach_file_output`)

Modified output file format for Tormach will be created.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Tormach`.

<a id="setting-tormach_mode"></a>

##### Tormach Print Mode (`tormach_mode`)

Reserved Tormach welder-mode identifier. The mode-specific saver code is currently disabled, so
Mode_21, Mode_40, Mode_102, Mode_274, and Mode_509 produce no different output.

- **Input:** `enumeration` — Choice from the listed values.
- **Master default:** `Mode_21`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Tormach`.
- **Choices:**
  - `Mode_21` — Reserved mode ID; disabled saver code associates it with wire-feed speed and
    voltage.
  - `Mode_40` — Reserved mode ID; disabled saver code associates it with wire-feed speed and power.
  - `Mode_102` — Reserved mode ID; disabled saver code associates it with wire-feed speed, trim, and
    frequency.
  - `Mode_274` — Reserved mode ID; disabled saver code associates it with wire-feed speed, trim, and
    Ultimarc.
  - `Mode_509` — Reserved mode ID; disabled saver code associates it with wire-feed speed, trim, and
    Ultimarc.

<a id="setting-aml3d_file_output"></a>

##### Enable AML3D Companion File Output (`aml3d_file_output`)

Modified output file format for AML3D will be created.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `AML3D`.

<a id="setting-aml3d_weave_length"></a>

##### AML3D Weave Length (`aml3d_weave_length`)

Sets the wavelength of the weave pattern in the AML3D companion output.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `AML3D`.

<a id="setting-aml3d_weave_width"></a>

##### AML3D Weave Width (`aml3d_weave_width`)

Sets the amplitude of the weave pattern in the AML3D companion output.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `0 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `AML3D`.

<a id="setting-sandia_file_output"></a>

##### Enable Sandia Companion File Output (`sandia_file_output`)

Modified output file format for Sandia will be created.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Sandia`.

<a id="setting-sandia_metal_file"></a>

##### Use Sandia Metal Printing Mode (`sandia_metal_file`)

If enabled, the Sandia companion file will use metal deposition commands. If disabled, companion
file will use polymer commands.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Sandia` and Enable Sandia Companion File Output is enabled).

<a id="setting-sandia_cvel"></a>

##### Use Sandia C_VEL (`sandia_cvel`)

If enabled, the Sandia companion file will use C_VEL rather than C_DIS.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Sandia` and Enable Sandia Companion File Output is enabled).

<a id="setting-marlin_file_output"></a>

##### Enable Marlin Companion File Output (`marlin_file_output`)

Additional output file format to be used for simulation.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Syntax is `Marlin`.

<a id="setting-marlin_include_travels"></a>

##### Include Travel Flag for Marlin Output (`marlin_include_travels`)

If enabled, a 9th column will be added to the Marlin companion file that signifies if the move is a
travel. If disabled, travels are included but without the ninth column in the output.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Enable Marlin Companion File Output is enabled.

<a id="setting-simulation_file_output"></a>

##### Enable Simulation Companion File Output (`simulation_file_output`)

If enabled, a text file listing timestamp, coordinates, and extrusion on/off will be generated for
use with ABAQUS. Works with Cincinnati and JuggerBot 3D.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `Cincinnati` or Syntax is `JuggerBot3D`).

<a id="setting-amcm_file_output"></a>

##### Enable AMCM SRC File Output (`amcm_file_output`)

If enabled, a src file for AMCM will be saved when exporting the g-code file.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `ORNL` or Syntax is `ORNL Metric`).

<a id="setting-amcm_data_logging"></a>

##### Enable AMCM Data Logging (`amcm_data_logging`)

If enabled, AMCM data logging commands will be added to the g-code.

- **Input:** `boolean` — On/off checkbox.
- **Master default:** `Disabled` (`false`)
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** (Syntax is `ORNL` or Syntax is `ORNL Metric`).

<a id="settings-experimental-cross-sectioning"></a>

#### Experimental > Cross-Sectioning

Controls gap detection and stitching tolerances used while forming cross-sections.

<a id="setting-cross_section_largest_gap"></a>

##### Largest Gap Distance (`cross_section_largest_gap`)

Sets the primary endpoint-gap tolerance used to connect adjacent cross-section segments. Endpoints
closer than this distance are treated as neighbors and joined.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `2.54 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

<a id="setting-max_stitch_distance"></a>

##### Max Stitch Distance (`max_stitch_distance`)

Sets the larger fallback stitching tolerance used when primary gap connection still leaves open
cross-section polylines. Remaining endpoints closer than this distance are joined during healing.

- **Input:** `distance` — Nonnegative physical distance in the preferred distance unit.
- **Master default:** `25.4 mm`
- **Scope:** Global only. Configure it in the active global template or global Settings panel.
- **Available when:** Always available.

The reference above is generated from the same metadata that constructs the Settings UI.
Template-specific values are intentionally not listed because they vary by machine, material, and
site.
<!-- END GENERATED SETTINGS REFERENCE -->
