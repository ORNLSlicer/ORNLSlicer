# Settings Metadata

These YAML files are the canonical source for ORNL Slicer setting metadata. Edit these files, then regenerate
`resources/configs/master.conf` and `resources/configs/setting_inputs.conf`:

```bash
python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf resources/configs/setting_inputs.conf
```

Files are loaded in sorted path order. Settings inside each file are emitted in file order, which controls the order in
the generated master settings file and UI.

The generator intentionally supports a small YAML subset and uses only the Python standard library. Keep scalar field
values on one line, use JSON-style values for dependency objects, and use either `options: []` or a block list for enum
options.

Tooltips come from the `tooltip` field and may include compact Qt rich-text image tags that reference bundled resources.
See [Adding Images to Settings Tooltips](../../docs/wiki/Adding-Images-to-Settings-Tooltips.md) for the project
procedure.

Grouped UI controls live in an optional top-level `inputs:` section. Use those entries to group existing scalar settings
into composite rows such as `vector2` and `vector3` without making one component setting carry the display name and
tooltip for the whole group.
