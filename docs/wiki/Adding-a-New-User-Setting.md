# To add a new user setting:

### 1. Add the setting to mss_for_json.ods file.
  * `mss_for_json.ods` is located in the root of the project directory.
  * The `name` column should be snake cased and is used again in Step #3
  * The `display` column contains the setting name as shown to the user
  * The `type` column determines what UI widget is used. It must be one of the following supported types: 
     - accel          (acceleration)
     - angle
     - ang_vel        (angular velocity)
     - area
     - boolean
     - density
     - distance
     - enumeration    (must be defined in the `enums` class)
     - location       (functionally the same as distance)
     - multiline_text
     - number         (a positive integer including zero)
     - numbered_list
     - percentage
     - positive_int   (Does not include zero)
     - power
     - rpm
     - speed
     - string         (Single line text input)
     - temperature
     - time
     - unitless_float
     - voltage
  * The `tooltip` column should be a short description of what the setting does
  * The `depends` column is used to decide when a setting is enabled or disabled. The column must contain valid json, and supports boolean combinations of dependencies. See examples below.

      ```C++
      {"syntax": 4}                              // depends on a syntax; the number is determined by the GcodeSyntax enum
      {"infill_enable": true }                   // depends on a boolean-type setting
      {"AND": [{"syntax":1}, {"doffing":true}]}  // depends on two settings both being enabled
      ```
      > Note: "AND" and "OR" combinations of dependency support only two parameters. If you need a combination of three or more settings, you must nest the AND/OR blocks
  * The `options` column contains the list of options for enumeration settings. It should be left blank for settings that are not enums.
  * The `default` column contains the default value of the setting. It must match the type defined in the `type` column. If the setting type has units, then the default value must be in the internally stored units. For example, distances are internally stored as microns, so a default value of 1 inch would be 25400. 
  * The `dependency_group` column is for groups of settings with dynamic dependencies that require extra computation. Static dependencies should use the `depends` column.
  * The `local` column denotes whether a setting can be applied to a specific part or layer. A setting should be marked as local **only** when it is actually implemented to respect part/ layer specific settings, not if it *could* be a local setting but is currently unimplemented.

### 2. Generate the Master Settings File
  * Run the Cmake. See [Generating the Master Settings File](Generating-the-Master-Settings-File) for more information

### 3. Add the setting to the `constants` class. 
  * `Constants` class is located in the utilities folder 
  * There are classes for every major category -- `PrinterSettings`, `MaterialSettings`, `ProfileSettings`, and `ExperimentalSettings`. 
  * Within the 4 major classes, there are subclasses for every minor category. **If you added a new minor category, you must add a new sub class.**
  * Add your new constants to the appropriate class & subclass
  * The string value must match the `name` column in the `mss_for_json.ods`

### 4. Fetch the setting value from your code
You can retrieve values from a settings base with the following code. Be sure to reference the correct settings base for your context, whether that is the global settings base, a part's settings base, or a layer's settings base.

```C++
  // get the global settings base and determine if rafts were enabled
  auto global_sb = GSM->getGlobal();
  bool is_raft_enabled = global_sb ->setting<bool>(Constants::MaterialSettings::PlatformAdhesion::kRaftEnable)

  if (is_raft_enabled)
  {
      // do something with rafts
  }
```