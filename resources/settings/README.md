# Settings Metadata

These YAML files are the canonical source for ORNL Slicer setting metadata. Edit these files, then regenerate
`resources/configs/master.conf`:

```bash
python3 scripts/generate_master_config.py resources/settings resources/configs/master.conf
```

Files are loaded in sorted path order. Settings inside each file are emitted in file order, which controls the order in
the generated master settings file and UI.

The generator intentionally supports a small YAML subset and uses only the Python standard library. Keep scalar field
values on one line, use JSON-style values for dependency objects, and use either `options: []` or a block list for enum
options.
