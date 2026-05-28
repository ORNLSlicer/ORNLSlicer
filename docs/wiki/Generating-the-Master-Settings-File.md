## Creating or Updating a Setting

The canonical settings metadata lives in `resources/settings/*.yaml`. These files define every available setting and
the metadata used by the UI, including display name, type, tooltip, dependencies, enum options, default value, category,
and local-setting support.

`resources/configs/master.conf` and `resources/configs/setting_inputs.conf` are generated from those YAML files and
embedded in the application through Qt resources. ORNL Slicer still reads the generated config files at runtime, but
they should not be edited by hand.

To create or edit a setting:

1. Make the necessary change in the relevant file under `resources/settings`.
2. Run CMake/build, or run the generator directly:

   ```bash
   python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf resources/configs/setting_inputs.conf
   ```

3. Verify that the generated files changed as expected.

The generator uses only the Python standard library. It validates duplicate setting names, required fields, setting
types, enum defaults, enum options, dependency references, and grouped input component references.

Use a top-level `inputs:` section when scalar settings should be shown as one composite UI row, such as `vector2` or
`vector3` inputs. Keep each component as a normal setting under `settings:` so saved settings remain flat, and list the
component setting names under the grouped input.

**DO NOT edit `resources/configs/master.conf` or `resources/configs/setting_inputs.conf` directly.**
