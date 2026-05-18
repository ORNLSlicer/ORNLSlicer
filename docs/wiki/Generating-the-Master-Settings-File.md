## Creating or Updating a Setting

The canonical settings metadata lives in `resources/settings/*.yaml`. These files define every available setting and
the metadata used by the UI, including display name, type, tooltip, dependencies, enum options, default value, category,
and local-setting support.

`resources/configs/master.conf` is generated from those YAML files and embedded in the application through Qt resources.
ORNL Slicer still reads `master.conf` at runtime, but it should not be edited by hand.

To create or edit a setting:

1. Make the necessary change in the relevant file under `resources/settings`.
2. Run CMake/build, or run the generator directly:

   ```bash
   python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf
   ```

3. Verify that `resources/configs/master.conf` changed as expected.

The generator uses only the Python standard library. It validates duplicate setting names, required fields, setting
types, enum defaults, enum options, and dependency references.

**DO NOT edit `resources/configs/master.conf` directly.**
