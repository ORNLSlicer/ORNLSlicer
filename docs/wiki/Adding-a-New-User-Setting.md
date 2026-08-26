# To add a new user setting:

### 1. Add the setting to `resources/settings`.

Settings metadata is stored in YAML files under `resources/settings`. Files are read in sorted path order, and settings
inside each file are emitted in file order, so add the setting where it should appear in the UI.

Each setting entry has these fields:

* `name`: Stable snake-cased setting key. This is also the key used by setting profiles and C++ constants.
* `display`: Setting name shown to the user.
* `type`: UI/input type. It must be one of the supported setting types:
  - accel
  - angle
  - ang_vel
  - area
  - boolean
  - density
  - deposition_rate
  - distance
  - enumeration
  - location
  - multiline_text
  - number
  - numbered_list
  - percentage
  - percentage100
  - positive_int
  - power
  - rpm
  - speed
  - string
  - temperature
  - time
  - unitless_float
  - voltage
* `tooltip`: Short description of what the setting does. Tooltips can include a
  small Qt resource image using rich-text markup; see
  [Adding Images to Settings Tooltips](Adding-Images-to-Settings-Tooltips.md).
* `depends`: Structured dependency object used to decide when a setting is enabled or disabled. Use `{}` for no
  dependency.

  ```yaml
  depends: {"syntax":4}
  depends: {"infill_enable":true}
  depends: {"AND":[{"syntax":1},{"doffing":true}]}
  ```

  `AND` and `OR` must have exactly two child nodes. Use nested nodes for three or more conditions. `NOT` must have
  exactly one child node.
* `options`: A YAML list of enum options. It must be empty for non-enum settings.

  ```yaml
  options:
    - "Lines"
    - "Grid"
    - "Concentric"
  ```
* `default`: Default value. It must match the setting type. Unit-backed settings use the internally stored units. For
  example, distances are internally stored as microns, so a default value of 1 inch is `25400`.
* `minor`: Minor UI category.
* `major`: Major UI category. Current values are `Printer`, `Material`, `Profile`, and `Experimental`.
* `namespace` and `symbol`: Metadata retained for future generated constants. Keep these aligned with the intended C++
  constant location when adding new settings.
* `local`: Whether the setting can be applied to a specific part or layer. Mark a setting as local only when local
  handling is actually implemented.

### 2. Generate the master settings file.

Run CMake/build, or run the generator directly:

```bash
python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf resources/configs/setting_inputs.conf
```

See [Generating the Master Settings File](Generating-the-Master-Settings-File.md) for more information.

### 3. Add the setting to the `Constants` class.

`Constants` is located in `include/utilities/constants.h` and `src/utilities/constants.cpp`.

There are classes for every major category: `PrinterSettings`, `MaterialSettings`, `ProfileSettings`, and
`ExperimentalSettings`. Within those major classes, there are subclasses for each minor category. If you added a new
minor category, add a matching subclass.

The string value must match the `name` field in the YAML metadata.

### 4. Fetch the setting value from code.

Retrieve values from the relevant settings base: global settings, a part settings base, or a layer settings base.

```C++
// Get the global settings base and determine if rafts are enabled.
auto global_sb = GSM->getGlobal();
bool is_raft_enabled = global_sb->setting<bool>(Constants::MaterialSettings::PlatformAdhesion::kRaftEnable);

if (is_raft_enabled) {
    // Do something with rafts.
}
```
