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
See [Adding Images to Settings Tooltips](../../docs/contributing/settings-tooltips.md) for the project
procedure.

Grouped UI controls live in an optional top-level `inputs:` section. Use those entries to group existing scalar settings
into composite rows such as `vector2` and `vector3` without making one component setting carry the display name and
tooltip for the whole group.

Settings and grouped inputs may define `visible_if` using the same expression syntax as `depends`. A false
`visible_if` always hides the row, including when disabled settings are normally greyed out. Use it to select
the controls for the active slicing mode; keep region enablement and revolution-count checks in `depends`.

Scalar settings may define a `group` object to arrange existing editors into regional forms:

```yaml
    group: {"region":"Perimeter","section":"Gas Timing","label":"Preflow"}
```

`region`, `section`, and `label` are nonempty strings. Regions are collapsible; sections are titled forms inside them.
`columns` is optional and defaults to 1; use 2 for a two-column grid of setting rows. Keep it consistent within a section.
Groups and rows retain their YAML order. The shorter `label` is shown in the group, while the full setting `display`
and group titles remain searchable. Empty groups hide after dependency/search filtering; search temporarily opens
matching regions and restores their collapse state when cleared. Grouping does not change stored setting keys or values.
