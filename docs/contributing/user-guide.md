# Maintaining the User Guide

The Markdown user guide ([`docs/ornlslicer-user-guide.md`](../ornlslicer-user-guide.md)) is the canonical manual source for ORNLSlicer. Its exhaustive settings appendix is generated from `resources/settings/*.yaml`; choice-level and implementation notes that are not part of the UI schema live in `scripts/generate_settings_reference.py`.

## Contents

- [Updating the Settings Appendix](#updating-the-settings-appendix)
- [Building the User Guide PDF](#building-the-user-guide-pdf)
- [Managing Screenshots and Figures](#managing-screenshots-and-figures)
- [Diagram Placeholders](#diagram-placeholders)

---

## Updating the Settings Appendix

After changing `resources/settings/*.yaml` or the notes in `scripts/generate_settings_reference.py`, refresh the master configs and regenerate the settings reference from the repository root:

```bash
python3 scripts/generate_master_config.py \
  resources/settings \
  resources/configs/master.conf \
  resources/configs/setting_inputs.conf
python3 -B scripts/generate_settings_reference.py
python3 -B scripts/generate_settings_reference.py --check
```

The `--check` flag ensures that all settings have valid documentation metadata, no orphaned anchors exist, and that generated reference sections are up to date.

---

## Building the User Guide PDF

Regenerate the same-stem `ornlslicer-user-guide.pdf` for a release rather than editing the PDF directly. With Pandoc and a LaTeX engine installed, run from the repository root:

```bash
FORCE_SOURCE_DATE=1 SOURCE_DATE_EPOCH=1735689600 TZ=UTC \
pandoc --from=gfm docs/ornlslicer-user-guide.md \
  --resource-path=docs \
  --lua-filter=docs/pandoc/red-diagram-placeholders.lua \
  --include-in-header=docs/pandoc/user-guide-header.tex \
  --metadata title-meta="ORNLSlicer User Guide" \
  --output=docs/ornlslicer-user-guide.pdf
```

CI uses the same stable timestamp (`SOURCE_DATE_EPOCH=1735689600`) and timezone (`TZ=UTC`) when building generated PDF artifacts to reduce build-time metadata churn.

---

## Managing Screenshots and Figures

Documentation screenshots live under `docs/user-guide-images/` using semantic names matching the GUI components (e.g. `main_window_overview.png`, `settings_sidebar.png`).

Screenshots can be generated automatically via the `ornlslicer_gui_capture` tool:

```bash
# Build the headless capture executable
cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_gui_capture -j 6

# Run full capture or targeted sections
./build/generic-llvm-ninja/Debug/ornlslicer_gui_capture --section all
```

The user guide embeds these images directly from `user-guide-images/`.

---

## Diagram Placeholders

The Lua filter (`docs/pandoc/red-diagram-placeholders.lua`) colors diagram-placeholder callouts red in generated PDF/HTML outputs so unreplaced manual figures are easy to spot before release:

```markdown
> **Diagram placeholder — Topic:** Description of the diagram to create.
```

When creating figures, prefer generating reproducible screenshots via `ornlslicer_gui_capture` whenever the figure depicts application UI or sliced toolpath geometry.
