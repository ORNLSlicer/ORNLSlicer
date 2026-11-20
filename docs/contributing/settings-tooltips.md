# Adding Images to Settings Tooltips

Settings tooltips come from the `tooltip` field in
`resources/settings/*.yaml`. The same metadata path is used for `Printer`,
`Material`, `Profile`, and `Experimental` settings, so this procedure applies
to all four major settings categories.

The settings generator does not support a separate `tooltip_image` field. Add
images by embedding a small Qt rich-text image tag in the existing `tooltip`
string.

## Procedure

1. Add the image asset.

   Put tooltip-specific images under a stable Qt resource path, grouped by
   major settings category:

   | Major | Recommended asset path |
   | --- | --- |
   | `Printer` | `resources/tooltips/printer/<setting_name>.png` |
   | `Material` | `resources/tooltips/material/<setting_name>.png` |
   | `Profile` | `resources/tooltips/profile/<setting_name>.png` |
   | `Experimental` | `resources/tooltips/experimental/<setting_name>.png` |

   Prefer PNG images around 240-320 px wide. Name each file after the setting
   key so the tooltip stays easy to audit. Static GIF files can be used when
   the deployed Qt build includes GIF image support, but animated GIFs should be
   avoided because rich-text tooltips display them as still images.

2. Register the image in a Qt resource file.

   If this is the first tooltip image, create `resources/tooltips/tooltips.qrc`:

   ```xml
   <RCC>
       <qresource prefix="/tooltips">
           <file>profile/layer_height.png</file>
       </qresource>
   </RCC>
   ```

   For later images, add another `<file>` entry under the same
   `/tooltips` prefix. The build already collects `resources/**.qrc`, so no
   CMake source-list edit is needed.

3. Edit the setting tooltip.

   Find the setting in the matching YAML file:

   | Major | YAML files |
   | --- | --- |
   | `Printer` | `resources/settings/*_printer_*.yaml` |
   | `Material` | `resources/settings/*_material_*.yaml` |
   | `Profile` | `resources/settings/*_profile_*.yaml` |
   | `Experimental` | `resources/settings/*_experimental_*.yaml` |

   Add the image to the existing `tooltip` value:

   ```yaml
   tooltip: "Thickness of each layer.<br><br><img src=':/tooltips/profile/layer_height.png' width='260' height='146'>"
   ```

   Keep the tooltip on one physical line. Use single quotes inside HTML
   attributes so the outer JSON-style YAML string can stay double-quoted.

4. Regenerate the generated settings files.

   ```bash
   python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf resources/configs/setting_inputs.conf
   ```

5. Validate in the UI.

   Build and run ORNLSlicer, then hover the setting label. Confirm the text and
   image render for the intended `Printer`, `Material`, `Profile`, or
   `Experimental` setting. If the setting can be local, also check a locally
   overridden row because the tooltip is wrapped with an extra local-override
   message.

## Tooltip Markup Rules

- Use Qt resource paths such as `:/tooltips/profile/layer_height.png`; do not
  use local filesystem paths or remote URLs.
- Prefer PNG. GIF can work as a static image, but do not rely on GIF animation
  in settings tooltips.
- Do not include outer `<html>`, `<body>`, or `<p>` tags. Settings rows add
  those wrappers automatically.
- Use `<br><br>` before the image for spacing.
- Specify `width` and `height` on the `<img>` tag so large source images do not
  create oversized tooltips.
- For grouped settings in an `inputs:` section, add the image to the input
  `tooltip`; the grouped row uses that tooltip instead of an individual
  component tooltip.
- Do not add new YAML fields unless `scripts/generate_master_config.py`, the
  generated config format, and the settings UI are updated together.
