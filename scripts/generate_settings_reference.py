#!/usr/bin/env python3
"""Generate the exhaustive settings appendix in the ORNLSlicer user guide."""

from __future__ import annotations

import argparse
import html
import json
import math
import re
import textwrap
from collections import OrderedDict
from pathlib import Path
from typing import Any

try:
    from generate_master_config import load_settings, parse_settings_file
except ModuleNotFoundError:  # Support imports from repository-root checks.
    from scripts.generate_master_config import load_settings, parse_settings_file


BEGIN_MARKER = "<!-- BEGIN GENERATED SETTINGS REFERENCE -->"
END_MARKER = "<!-- END GENERATED SETTINGS REFERENCE -->"

MAJOR_DESCRIPTIONS = {
    "Printer": (
        "Printer settings describe the controller, coordinate system, build envelope, machine limits, and "
        "machine-level G-code. Treat them as machine configuration and verify them against the physical system."
    ),
    "Material": (
        "Material settings control process behavior tied to the feedstock and deposition system, including "
        "startup, extrusion, retraction, temperature, cooling, and first-layer adhesion."
    ),
    "Profile": (
        "Profile settings define how geometry becomes layers and ordered toolpaths. Many are local-capable so "
        "different parts, layers, ranges, or spatial regions can use different values in supported modes."
    ),
    "Experimental": (
        "Experimental settings expose specialized path and file-generation controls. Confirm writer and machine "
        "support before depending on them in a production workflow."
    ),
}

CATEGORY_DESCRIPTIONS = {
    ("Printer", "Machine Setup"): "Selects controller syntax, machine process, motion-command behavior, coordinates, tools, and rotary-axis values.",
    ("Printer", "Dimensions"): "Defines the build-volume shape, limits, offsets, auxiliary locations, and displayed floor grid.",
    ("Printer", "Auxiliary"): "Configures optional equipment that is separate from the primary deposition system.",
    ("Printer", "Machine Speeds"): "Sets physical motion and extrusion-rate limits used by writers, validation, and time estimation.",
    ("Printer", "Acceleration"): "Sets default and region-specific acceleration values for syntaxes that emit dynamic acceleration commands.",
    ("Printer", "G-Code"): "Controls machine-level startup, material loading, waits, boundary demonstrations, settings output, and custom command blocks.",
    ("Material", "Density"): "Chooses a known feedstock density or supplies a custom density for mass and flow calculations.",
    ("Material", "Start-Up"): "Controls prestart and ramp-up motion at the beginning of printable region paths.",
    ("Material", "Slow Down"): "Controls reduced speed, extrusion, and lift behavior near the end of printable region paths.",
    ("Material", "Tip Wipe"): "Controls wipe motion, direction, cutoff, lift, and voltage after selected printable regions.",
    ("Material", "Spiral Lift"): "Controls spiral motion used to lift away from a completed region or layer.",
    ("Material", "Purge"): "Controls purge timing, screw speed, dwell behavior, and optional purge/wipe motion.",
    ("Material", "Extruder"): "Configures initial extrusion, priming, region delays, servo behavior, and spindle-command conventions.",
    ("Material", "Filament"): "Configures filament diameter, relative extrusion, position-reset behavior, and alternate extrusion axes.",
    ("Material", "Retraction"): "Controls when filament retracts and primes around qualifying travel and layer changes.",
    ("Material", "Temperatures"): "Sets bed, standby, and multi-zone extrusion temperature targets.",
    ("Material", "Cooling"): "Controls fan output and minimum-layer-time behavior, including pauses and extrusion/feed adjustments.",
    ("Material", "Platform Adhesion"): "Adds and configures raft, brim, or skirt geometry around the first layers.",
    ("Material", "Multi-Material"): "Assigns materials to regions and controls material transitions and controller selection commands.",
    ("Profile", "Slicing"): "Selects planar, cylindrical, or image slicing and configures slice orientation and mode-specific geometry.",
    ("Profile", "Layer"): "Defines layer thickness and baseline bead, nozzle, speed, and extrusion values.",
    ("Profile", "Perimeter"): "Controls exterior contours, their process values, and perimeter-specific start and spiral behavior.",
    ("Profile", "Inset"): "Controls additional inward contours, including count, overlap, process values, and spiral behavior.",
    ("Profile", "Skeleton"): "Controls centerline input, cleanup, adaptive bead width, process values, and skeleton prestart behavior.",
    ("Profile", "Skin"): "Controls solid top/bottom coverage, pattern orientation, overlap, process values, and gradual infill.",
    ("Profile", "Infill"): "Controls interior fill density, spacing, pattern, orientation, ordering, combining, and process values.",
    ("Profile", "Support"): "Controls generated grid or organic support, interfaces, bases, spacing, tapering, and connectivity.",
    ("Profile", "Travel"): "Controls non-print motion, minimum travel thresholds, lift behavior, pauses, and centroid moves.",
    ("Profile", "G-Code"): "Adds region-specific command blocks before and after generated paths.",
    ("Profile", "Special Modes"): "Enables geometry repair and transformations such as smoothing, spiralize, and oversizing, plus bead-geometry output for a compatible HMI.",
    ("Profile", "Optimizations"): "Controls ordering at layer, island, path, and point levels, including custom points and randomness.",
    ("Profile", "Ordering"): "Sets region order and direction-reversal policies for perimeters and insets.",
    ("Profile", "Laser Scanner"): "Configures laser-scan paths, offsets, resolution, orientation, buffering, and height-map behavior.",
    ("Profile", "Thermal Scanner"): "Configures thermal scan enablement, offsets, and temperature cutoff.",
    ("Experimental", "Auto Speed Ramping"): "Adjusts speed and extrusion around path-angle changes using configurable ramp distances.",
    ("Experimental", "File Output"): "Enables syntax-specific companion, simulation, or auxiliary output files.",
    ("Experimental", "Cross-Sectioning"): "Controls gap detection and stitching tolerances used while forming cross-sections.",
}

TYPE_DESCRIPTIONS = {
    "accel": "Acceleration; displayed in the preferred acceleration unit.",
    "angle": "Angle; displayed in the preferred angle unit.",
    "ang_vel": "Angular velocity; displayed in the preferred rotation/time unit.",
    "area": "Area; displayed as the square of the preferred distance unit.",
    "boolean": "On/off checkbox.",
    "density": "Density; displayed in the preferred density unit.",
    "distance": "Nonnegative physical distance in the preferred distance unit.",
    "enumeration": "Choice from the listed values.",
    "location": "Signed position or offset in the preferred distance unit.",
    "multiline_text": "Multi-line G-code or text block.",
    "number": "Integer value.",
    "numbered_list": "Ordered list whose entries can be rearranged.",
    "percentage": "Percentage input with an allowed range of 0–500%.",
    "percentage100": "Percentage input with an allowed range of 0–100%.",
    "positive_int": "Positive integer value; the input control has a minimum of 1.",
    "power": "Power in watts.",
    "rpm": "Rotational speed in revolutions per minute.",
    "speed": "Linear velocity; displayed in the preferred velocity unit.",
    "string": "Single-line text.",
    "temperature": "Temperature; displayed in the preferred temperature unit.",
    "time": "Duration; displayed in the preferred time unit.",
    "unitless_float": "Decimal value without a physical unit.",
    "voltage": "Electrical potential; displayed in the preferred voltage unit.",
}

MAJOR_NUMBERS = {
    "Printer": "E.2",
    "Material": "E.3",
    "Profile": "E.4",
    "Experimental": "E.5",
}

GENERATED_FIGURE_NUMBERS = {
    "Setting anatomy": 60,
    "Printer settings": 61,
    "Material settings": 62,
    "Profile settings": 63,
    "Experimental settings": 64,
}

PATTERN_CHOICE_DESCRIPTIONS = {
    "Lines": "One family of parallel hatch lines at the configured angle and spacing.",
    "Grid": "Two perpendicular families of parallel lines at the configured angle and 90 degrees from it.",
    "Concentric": "Successive closed offsets that follow the boundary of the filled area.",
    "Triangles": "Three line families, rotated 60 degrees apart, that form an equilateral triangular lattice.",
    "Hexagons and Triangles": (
        "Three 60-degree line families with an alternate offset that forms mixed hexagonal and triangular cells."
    ),
    "Honeycomb": "Connected zig-zag rows that form hexagonal cells using bead width and line spacing.",
}

CHOICE_DESCRIPTIONS = {
    "syntax": {
        "Beam": (
            "Currently falls back to the Cincinnati writer: inch coordinates, inch/min feeds, parenthesized "
            "comments, and an .nc suffix."
        ),
        "Cincinnati": (
            "Cincinnati writer: inch coordinates, inch/min feeds, parenthesized comments, .nc suffix; enables "
            "laser-scanner and simulation-output settings."
        ),
        "Common": (
            "Currently falls back to the Cincinnati writer: inch coordinates, inch/min feeds, parenthesized "
            "comments, and an .nc suffix."
        ),
        "Dmg Dmu": "DMG/DMU writer: mm coordinates, mm/min feeds, semicolon comments, .mpf suffix.",
        "Gudel": "Gudel writer: mm coordinates, mm/min feeds, semicolon comments, .mpf suffix.",
        "Haas Inch": "Haas writer: inch coordinates, inch/min feeds, parenthesized comments, .nc suffix.",
        "Haas Metric": "Haas writer: mm coordinates, mm/min feeds, parenthesized comments, .nc suffix.",
        "Haas Metric No Comments": "Haas metric writer: mm coordinates, mm/min feeds, comments omitted, .nc suffix.",
        "Hurco": "Hurco writer: mm coordinates, mm/min feeds, parenthesized comments, .nc suffix.",
        "Ingersoll": "Ingersoll writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.",
        "Marlin": (
            "Marlin writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix; enables Marlin "
            "companion-output settings."
        ),
        "JuggerBot3D": (
            "JuggerBot writer using Marlin units: mm coordinates, mm/min feeds, .gcode suffix; enables "
            "simulation-output settings."
        ),
        "Mazak": "Mazak writer: mm coordinates, mm/min feeds, parenthesized comments, .eia suffix.",
        "MVP": "MVP writer: mm coordinates, mm/min feeds, semicolon comments, .mpf suffix.",
        "RomiFanuc": "Romi Fanuc writer: mm coordinates, mm/min feeds, parenthesized comments, .txt suffix.",
        "Siemens": "Siemens writer: inch coordinates, inch/min feeds, semicolon comments, .mpf suffix.",
        "Thermwood": "Thermwood writer using Cincinnati units: inch coordinates, inch/min feeds, .nc suffix.",
        "Wolf": "Wolf writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.",
        "RepRap": "RepRap writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.",
        "Mach4": "Mach4 writer using Marlin units: mm coordinates, mm/min feeds, .gcode suffix.",
        "AeroBasic": "AeroBasic writer: mm coordinates, mm/s feeds, apostrophe comments, .gcode suffix.",
        "Meld": (
            "Meld writer: inch coordinates, inch/min feeds, parenthesized comments, .nc suffix; enables Meld "
            "companion and discrete-feed settings."
        ),
        "ORNL": (
            "ORNL writer: inch coordinates, inch/min feeds, parenthesized comments, .gcode suffix; enables AMCM "
            "output and data-logging settings."
        ),
        "Okuma": "Okuma writer using Haas metric units: mm coordinates, mm/min feeds, .nc suffix.",
        "Tormach": (
            "Tormach writer: mm coordinates, mm/min feeds, semicolon comments, .nc suffix; enables Tormach "
            "companion-output settings."
        ),
        "AML3D": (
            "AML3D writer: mm coordinates, mm/s feeds, parenthesized comments, .gcode suffix; enables AML3D "
            "companion and weave settings."
        ),
        "KraussMaffei": "KraussMaffei writer: mm coordinates, mm/min feeds, semicolon comments, .gcode suffix.",
        "Sandia": (
            "Sandia writer: mm coordinates, m/s feeds, semicolon comments, .gcode suffix; enables Sandia "
            "auxiliary-output settings."
        ),
        "Meltio": "Meltio writer: mm coordinates, mm/min feeds, parenthesized comments, .nc suffix.",
        "Adamantine": "Adamantine writer: metre coordinates, m/s feeds, parenthesized comments, .txt suffix.",
        "ORNL Metric": (
            "ORNL writer: mm coordinates, mm/min feeds, parenthesized comments, .gcode suffix; enables AMCM "
            "output and data-logging settings."
        ),
        "Arc Specialties": "Arc Specialties writer: mm coordinates, mm/min feeds, semicolon comments, .nc suffix.",
    },
    "printing_material": {
        "ABS20CF": "Built-in density 1.139997 g/cm³ (20% carbon-fiber ABS).",
        "ABS": "Built-in density 1.069828 g/cm³.",
        "PPS": "Built-in density 1.349949 g/cm³.",
        "PPS50CF": "Built-in density 1.527931 g/cm³ (50% carbon-fiber PPS).",
        "PPSU": "Built-in density 1.289884 g/cm³.",
        "PPSU25CF": "Built-in density 1.381227 g/cm³ (25% carbon-fiber PPSU).",
        "PESU": "Built-in density 1.367387 g/cm³.",
        "PESU25CF": "Built-in density 1.472571 g/cm³ (25% carbon-fiber PESU).",
        "PLA": "Built-in density 1.251132 g/cm³.",
        "Concrete": "Built-in density 2.604679 g/cm³.",
        "Other": "Uses the value entered under Other Density.",
    },
    "skin_pattern": PATTERN_CHOICE_DESCRIPTIONS,
    "skin_gradual_infill_pattern": PATTERN_CHOICE_DESCRIPTIONS,
    "infill_pattern": {
        **PATTERN_CHOICE_DESCRIPTIONS,
        "Radial Hatch": (
            "Reserved compatibility choice; the current Infill region does not generate paths for this value."
        ),
    },
    "smoothing_type": {
        "Douglas Peucker": (
            "Recursively retains points with the greatest perpendicular deviation until all remaining deviations "
            "are within the tolerance."
        ),
        "Radial Distance": "Keeps a point when it is farther than the tolerance from the last retained point.",
        "Perpendicular Distance": (
            "Removes intermediate points whose perpendicular distance from a neighboring segment is within the "
            "tolerance."
        ),
        "Reumann-Witkam": (
            "Advances a tolerance-width corridor along the contour and retains a point when the contour exits it."
        ),
    },
    "layer_ordering": {
        "By Height": (
            "Merges compatible part layers by projected physical height; Layer Grouping Tolerance controls when "
            "nearby planes share a global layer."
        ),
        "By Layer Number": "Groups layer 0 from every part, then layer 1 from every part, and so on.",
        "By Part (Sequential)": "Emits every layer of one part before moving to the next part.",
    },
    "island_order_optimization": {
        "Next Closest": "Greedily selects the island with the boundary point nearest the current position.",
        "Next Farthest": "Uses the brute-force route optimizer to favor the greatest inter-island distance.",
        "Shortest Distance (approximate)": (
            "Uses a faster approximate traveling-salesperson route to reduce total island travel."
        ),
        "Shortest Distance (brute force)": (
            "Searches island permutations for the shortest route; computation grows quickly with island count."
        ),
        "Least Recently Visited": "Rotates the starting island using the island visited last on the prior layer.",
        "Random": "Chooses the next remaining island randomly on each layer.",
        "Custom Location": "Uses the custom X/Y location as the reference, then applies nearest-island selection.",
    },
    "path_order_optimization": {
        "Next Closest": "Selects the path whose available start is nearest the current position.",
        "Next Farthest": "Selects the path whose available start is farthest from the current position.",
        "Random": "Selects a remaining path and, for open paths, its direction randomly.",
        "Outside In": "Traverses contour hierarchy from exterior paths toward interior paths.",
        "Inside Out": "Traverses contour hierarchy from interior paths toward exterior paths.",
        "Custom Location": "Uses the custom X/Y location as the reference for nearest-path selection.",
    },
    "point_order_optimization": {
        "Next Closest": "Starts at the path point nearest the current tool position.",
        "Next Farthest": "Starts at the path point farthest from the current tool position.",
        "Random": "Selects a path start point randomly.",
        "Consecutive": (
            "Advances the start point with layer number until Consecutive Distance Threshold is reached."
        ),
        "Custom Location": "Starts at the path point nearest the custom X/Y reference.",
        "Custom Farthest Location": "Starts at the path point farthest from the custom X/Y reference.",
    },
    "tormach_mode": {
        "Mode_21": "Reserved mode ID; disabled saver code associates it with wire-feed speed and voltage.",
        "Mode_40": "Reserved mode ID; disabled saver code associates it with wire-feed speed and power.",
        "Mode_102": "Reserved mode ID; disabled saver code associates it with wire-feed speed, trim, and frequency.",
        "Mode_274": "Reserved mode ID; disabled saver code associates it with wire-feed speed, trim, and Ultimarc.",
        "Mode_509": "Reserved mode ID; disabled saver code associates it with wire-feed speed, trim, and Ultimarc.",
    },
}

SETTING_NOTES = {
    "syntax": (
        "Each choice selects the corresponding installed writer dialect. Units, comments, commands, suffixes, "
        "parsing, and companion outputs are dialect-specific. Arc emission is controlled separately by Supports "
        "G2/G3. Always inspect and validate generated output against the target controller before running it."
    ),
}


def markdown_text(value: Any) -> str:
    """Return one safe, normalized Markdown paragraph fragment."""
    text = " ".join(str(value).split())
    return html.escape(text, quote=False).replace("|", "\\|")


def sentence(value: Any) -> str:
    text = markdown_text(value)
    if text and text[-1] not in ".?!:;)":
        text += "."
    return text


def slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.casefold()).strip("-")


def code_span(value: Any) -> str:
    text = str(value).replace("\n", "\\n")
    delimiter = "``" if "`" in text else "`"
    return f"{delimiter}{text}{delimiter}"


def readable_number(value: float) -> str:
    if abs(value) < 5e-12:
        value = 0.0
    if float(value).is_integer():
        return f"{int(value):,}"
    return f"{value:,.6g}"


def setting_options(setting: OrderedDict[str, Any]) -> list[str]:
    options = setting["options"]
    if isinstance(options, list):
        return [str(option) for option in options]
    if not options:
        return []
    return str(options).split(", ")


def format_default(setting: OrderedDict[str, Any]) -> str:
    setting_type = setting["type"]
    value = setting["default"]

    if setting_type == "enumeration":
        options = setting_options(setting)
        if isinstance(value, int) and 0 <= value < len(options):
            return code_span(options[value])
        return code_span(value)
    if setting_type == "boolean":
        return f"{code_span('Enabled' if value else 'Disabled')} ({code_span(str(value).lower())})"
    if setting_type == "numbered_list":
        return ", ".join(code_span(item) for item in value) if value else code_span("empty list")
    if setting_type in {"string", "multiline_text"}:
        return code_span(json.dumps(value, ensure_ascii=False)) if value else code_span("empty")

    numeric = float(value)
    if setting_type in {"distance", "location"}:
        return code_span(f"{readable_number(numeric / 1_000)} mm")
    if setting_type == "area":
        return code_span(f"{readable_number(numeric / 1_000_000)} mm²")
    if setting_type == "speed":
        return code_span(f"{readable_number(numeric / 1_000)} mm/s")
    if setting_type == "accel":
        return code_span(f"{readable_number(numeric / 1_000)} mm/s²")
    if setting_type == "angle":
        return code_span(f"{readable_number(math.degrees(numeric))}°")
    if setting_type == "ang_vel":
        return code_span(f"{readable_number(math.degrees(numeric))}°/s")
    if setting_type == "temperature":
        return code_span(f"{readable_number(numeric - 273.15)} °C")
    if setting_type == "density":
        return code_span(f"{readable_number(numeric * 1e15)} g/cm³")
    if setting_type == "time":
        return code_span(f"{readable_number(numeric)} s")
    if setting_type in {"percentage", "percentage100"}:
        return code_span(f"{readable_number(numeric)}%")
    if setting_type == "rpm":
        return code_span(f"{readable_number(numeric)} rpm")
    if setting_type == "voltage":
        return code_span(f"{readable_number(numeric)} V")
    if setting_type == "power":
        return code_span(f"{readable_number(numeric)} W")
    if setting_type == "positive_int" and numeric < 1:
        return f"{code_span(readable_number(numeric))} (below the current input minimum of {code_span(1)})"
    return code_span(readable_number(numeric))


def condition_label(name: str, settings: OrderedDict[str, OrderedDict[str, Any]],
                    component_labels: dict[str, str]) -> str:
    if name in component_labels:
        return component_labels[name]
    return str(settings[name]["display"])


def condition_value(setting: OrderedDict[str, Any], value: Any) -> str:
    if setting["type"] == "enumeration" and isinstance(value, int):
        options = setting_options(setting)
        if 0 <= value < len(options):
            return code_span(options[value])
    return code_span(str(value).lower() if isinstance(value, bool) else value)


def format_dependency(depends: Any, settings: OrderedDict[str, OrderedDict[str, Any]],
                      component_labels: dict[str, str]) -> str:
    if depends in ("", None, {}, []):
        return "Always available."

    key, value = next(iter(depends.items()))
    if key in {"AND", "OR"}:
        joiner = " and " if key == "AND" else " or "
        children = [format_dependency(child, settings, component_labels).rstrip(".") for child in value]
        return f"({joiner.join(children)})."
    if key == "NOT":
        child = format_dependency(value[0], settings, component_labels).rstrip(".")
        return f"Not ({child})."

    setting = settings[key]
    label = markdown_text(condition_label(key, settings, component_labels))
    if setting["type"] == "boolean" and isinstance(value, bool):
        return f"{label} is {'enabled' if value else 'disabled'}."
    return f"{label} is {condition_value(setting, value)}."


def add_bullet(lines: list[str], label: str, value: str) -> None:
    prefix = f"- **{label}:** "
    lines.extend(
        textwrap.wrap(
            value,
            width=100,
            initial_indent=prefix,
            subsequent_indent="  ",
            break_long_words=False,
            break_on_hyphens=False,
        )
    )


def add_choice(lines: list[str], option: str, description: str) -> None:
    prefix = f"  - {code_span(option)} — "
    lines.extend(
        textwrap.wrap(
            description,
            width=100,
            initial_indent=prefix,
            subsequent_indent="    ",
            break_long_words=False,
            break_on_hyphens=False,
        )
    )


def wrap_paragraph(value: str) -> list[str]:
    return textwrap.wrap(value, width=100, break_long_words=False, break_on_hyphens=False)


def figure_placeholder(number: int, title: str) -> str:
    return f"![Figure {number:02d} placeholder: {title}](user-guide-images/figure{number:02d}.png)"


def load_catalog(source_dir: Path) -> tuple[
    OrderedDict[str, OrderedDict[str, Any]], OrderedDict[str, OrderedDict[str, Any]]
]:
    validated, inputs = load_settings(source_dir)
    raw: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    for path in sorted(source_dir.rglob("*.yaml")):
        settings, _ = parse_settings_file(path)
        for setting in settings:
            raw[setting["name"]] = setting

    if list(raw) != list(validated):
        raise ValueError("raw setting order differs from the validated master catalog")
    validate_documentation_metadata(raw, inputs)
    return raw, inputs


def validate_documentation_metadata(settings: OrderedDict[str, OrderedDict[str, Any]],
                                    inputs: OrderedDict[str, OrderedDict[str, Any]]) -> None:
    """Reject metadata defects that would produce misleading or malformed documentation."""

    normalized_displays: dict[str, str] = {}
    for key, setting in settings.items():
        for field in ("display", "tooltip", "major", "minor"):
            value = setting[field]
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"{key}: {field} must be non-empty text")

        display = re.sub(r"[^a-z0-9]+", " ", setting["display"].casefold()).strip()
        if display in normalized_displays:
            raise ValueError(
                f"duplicate display names: {normalized_displays[display]} and {key} ({setting['display']})"
            )
        normalized_displays[display] = key

        tooltip = setting["tooltip"]
        if "\\n" in tooltip:
            raise ValueError(f"{key}: tooltip contains a literal escaped newline")
        if "\u00a0" in tooltip:
            raise ValueError(f"{key}: tooltip contains a non-breaking space")

        options = setting_options(setting)
        if len(options) != len(set(options)):
            raise ValueError(f"{key}: enumeration choices must be unique")

        validate_documentation_dependency(setting["depends"], settings, key)

    for input_name, setting_input in inputs.items():
        for field in ("display", "tooltip", "major", "minor"):
            value = setting_input[field]
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"{input_name}: composite {field} must be non-empty text")
        tooltip = setting_input["tooltip"]
        if "\\n" in tooltip or "\u00a0" in tooltip:
            raise ValueError(f"{input_name}: composite tooltip contains unsupported whitespace")
        validate_documentation_dependency(setting_input["depends"], settings, input_name)

    for key, descriptions in CHOICE_DESCRIPTIONS.items():
        if key not in settings:
            raise ValueError(f"choice descriptions reference unknown setting {key}")
        options = setting_options(settings[key])
        if options != list(descriptions):
            raise ValueError(f"{key}: documented choice descriptions do not match the canonical choice order")

    unknown_notes = set(SETTING_NOTES) - set(settings)
    if unknown_notes:
        raise ValueError(f"setting notes reference unknown settings: {sorted(unknown_notes)}")


def validate_documentation_dependency(depends: Any, settings: OrderedDict[str, OrderedDict[str, Any]],
                                      current_name: str) -> None:
    if depends in ("", None, {}, []):
        return
    key, value = next(iter(depends.items()))
    if key in {"AND", "OR"}:
        for child in value:
            validate_documentation_dependency(child, settings, current_name)
        return
    if key == "NOT":
        validate_documentation_dependency(value[0], settings, current_name)
        return

    parent = settings[key]
    if parent["type"] == "boolean":
        if not isinstance(value, bool):
            raise ValueError(f"{current_name}: Boolean dependency on {key} must compare true or false")
        return
    if parent["type"] == "enumeration":
        options = setting_options(parent)
        if not isinstance(value, int) or not 0 <= value < len(options):
            raise ValueError(f"{current_name}: dependency index for {key} is outside its choices")
        return
    raise ValueError(f"{current_name}: dependency parent {key} must be Boolean or enumeration")


def composite_maps(inputs: OrderedDict[str, OrderedDict[str, Any]]) -> tuple[
    dict[str, str], dict[str, str], dict[str, str]
]:
    component_owner: dict[str, str] = {}
    first_component: dict[str, str] = {}
    component_labels: dict[str, str] = {}
    for input_name, setting_input in inputs.items():
        components = setting_input["components"]
        first_component[input_name] = components[0]["setting"]
        for component in components:
            key = component["setting"]
            component_owner[key] = input_name
            component_labels[key] = f"{setting_input['display']} — {component['label']}"
    return component_owner, first_component, component_labels


def render_scalar(lines: list[str], key: str, setting: OrderedDict[str, Any],
                  settings: OrderedDict[str, OrderedDict[str, Any]], component_labels: dict[str, str]) -> None:
    lines.append(f'<a id="setting-{key}"></a>')
    lines.append("")
    lines.append(f"##### {markdown_text(setting['display'])} ({code_span(key)})")
    lines.append("")
    lines.extend(wrap_paragraph(sentence(setting["tooltip"])))
    lines.append("")
    add_bullet(lines, "Input", f"{code_span(setting['type'])} — {TYPE_DESCRIPTIONS[setting['type']]}")
    add_bullet(lines, "Master default", format_default(setting))
    scope = (
        "Local-capable. It can be overridden at supported part, layer/range, or spatial scopes; mode-specific "
        "scope limitations still apply."
        if setting["local"]
        else "Global only. Configure it in the active global template or global Settings panel."
    )
    add_bullet(lines, "Scope", scope)
    add_bullet(lines, "Available when", format_dependency(setting["depends"], settings, component_labels))
    options = setting_options(setting)
    if options:
        lines.append("- **Choices:**")
        descriptions = CHOICE_DESCRIPTIONS.get(key)
        if descriptions:
            for option in options:
                add_choice(lines, option, descriptions[option])
        else:
            lines.extend(f"  - {code_span(option)}" for option in options)
    if key in SETTING_NOTES:
        add_bullet(lines, "Implementation note", SETTING_NOTES[key])
    lines.append("")


def render_composite(lines: list[str], input_name: str, setting_input: OrderedDict[str, Any],
                     settings: OrderedDict[str, OrderedDict[str, Any]], component_labels: dict[str, str],
                     documented: set[str]) -> None:
    lines.append(f'<a id="setting-{input_name}"></a>')
    for component in setting_input["components"]:
        lines.append(f'<a id="setting-{component["setting"]}"></a>')
    lines.append("")
    lines.append(f"##### {markdown_text(setting_input['display'])} ({code_span(input_name)})")
    lines.append("")
    lines.extend(wrap_paragraph(sentence(setting_input["tooltip"])))
    lines.append("")
    widget = setting_input["widget"]
    add_bullet(lines, "Input", f"{code_span(widget)} grouped control with the components listed below.")
    scope = (
        "Local-capable. Each component can be overridden through this grouped row at supported narrower scopes."
        if setting_input["local"]
        else "Global only. Configure the grouped value in the active global settings."
    )
    add_bullet(lines, "Scope", scope)
    add_bullet(lines, "Available when", format_dependency(setting_input["depends"], settings, component_labels))
    lines.append("- **Components and master defaults:**")
    for component in setting_input["components"]:
        key = component["setting"]
        documented.add(key)
        lines.append(
            f"  - **{markdown_text(component['label'])}:** {code_span(key)} — {format_default(settings[key])}"
        )
    lines.append("")


def build_reference(settings: OrderedDict[str, OrderedDict[str, Any]],
                    inputs: OrderedDict[str, OrderedDict[str, Any]]) -> str:
    component_owner, first_component, component_labels = composite_maps(inputs)
    categories: OrderedDict[tuple[str, str], list[str]] = OrderedDict()
    major_order: list[str] = []
    for key, setting in settings.items():
        if setting["major"] not in major_order:
            major_order.append(setting["major"])
        categories.setdefault((setting["major"], setting["minor"]), []).append(key)

    if set(major_order) != set(MAJOR_NUMBERS):
        raise ValueError("manual panel numbering does not match the canonical major panels")

    visible_rows = len(settings) - len(component_owner) + len(inputs)
    lines = [
        "## Appendix E. Detailed settings reference",
        "",
        (
            f"This generated appendix documents all {len(settings)} scalar manufacturing settings exposed by "
            f"the canonical catalog. The {len(inputs)} grouped controls combine related scalar values, producing "
            f"{visible_rows} visible setting rows across {len(categories)} categories."
        ),
        "",
        (
            "Do not edit this appendix directly. Update `resources/settings/*.yaml` for setting metadata and the "
            "documented mappings in `scripts/generate_settings_reference.py` for choice-level or implementation "
            "notes, then run the generator. It validates the source catalog and replaces everything between the "
            "generated-reference markers."
        ),
        "",
        "### E.1 How to read this reference",
        "",
        (
            "Each scalar entry starts with the user-visible label and stable persisted key. Grouped-control "
            "headings instead show the input metadata name; their component lists identify the keys stored in "
            "templates and projects. **Master default** is the fallback compiled into ORNLSlicer; a loaded "
            "template or project normally supplies practical machine and process values. Physical defaults below "
            "use canonical metric units for readability, while the application displays values in the units "
            "selected under Application Preferences."
        ),
        "",
        (
            "**Available when** translates the application's dependency expression into labels and choices. A "
            "disabled setting may remain grey or be hidden, according to the Disabled Settings preference. "
            "**Local-capable** means metadata permits a narrower override; see Section 6.1 for slicing-mode limits."
        ),
        "",
        (
            "Use the browser or PDF search for either a visible label or an internal key. Every scalar key also "
            "has a stable Markdown target in the form `#setting-<key>`, such as `#setting-layer_height`."
        ),
        "",
        "| Metadata | Meaning |",
        "| --- | --- |",
        "| Purpose | The same behavior description shown as the setting's tooltip, normalized for the manual. |",
        "| Input | Widget/value type and its unit family or allowed range. |",
        "| Master default | Built-in fallback before a template, project, or user override is applied. |",
        "| Scope | Whether the setting is global-only or eligible for supported local overrides. |",
        "| Available when | The selections or toggles that enable the setting. |",
        "| Choices | Every selectable value for enumeration settings, in stored order. |",
        "",
        figure_placeholder(GENERATED_FIGURE_NUMBERS["Setting anatomy"], "Setting anatomy"),
        "",
        "> **Diagram placeholder — Setting anatomy:** Add one annotated setting row showing its label, input, unit,",
        "> tooltip, disabled state, local-override indicator, and corresponding reference entry.",
        "",
        "The catalog is organized as follows:",
        "",
        "| Panel | Category | Scalar settings |",
        "| --- | --- | ---: |",
    ]
    for (major, minor), keys in categories.items():
        major_anchor = f"#{MAJOR_NUMBERS[major].replace('.', '').casefold()}-{major.casefold()}-settings"
        category_anchor = f"#settings-{slug(major)}-{slug(minor)}"
        lines.append(f"| [{major}]({major_anchor}) | [{minor}]({category_anchor}) | {len(keys)} |")
    lines.append("")

    documented: set[str] = set()
    for major in major_order:
        lines.append(f"### {MAJOR_NUMBERS[major]} {major} settings")
        lines.append("")
        lines.extend(wrap_paragraph(MAJOR_DESCRIPTIONS[major]))
        lines.append("")
        lines.append(figure_placeholder(GENERATED_FIGURE_NUMBERS[f"{major} settings"], f"{major} settings"))
        lines.append("")
        lines.append(f"> **Diagram placeholder — {major} settings:** Add an annotated {major} panel with its")
        lines.append("> category tabs, search field, and one enabled/disabled dependency example.")
        lines.append("")

        for (category_major, minor), keys in categories.items():
            if category_major != major:
                continue
            lines.append(f'<a id="settings-{slug(major)}-{slug(minor)}"></a>')
            lines.append("")
            lines.append(f"#### {major} > {minor}")
            lines.append("")
            lines.extend(wrap_paragraph(CATEGORY_DESCRIPTIONS[(major, minor)]))
            lines.append("")
            for key in keys:
                if key in component_owner:
                    input_name = component_owner[key]
                    if key == first_component[input_name]:
                        render_composite(lines, input_name, inputs[input_name], settings, component_labels, documented)
                    continue
                documented.add(key)
                render_scalar(lines, key, settings[key], settings, component_labels)

    missing = set(settings) - documented
    extra = documented - set(settings)
    if missing or extra:
        raise ValueError(f"settings coverage mismatch; missing={sorted(missing)}, extra={sorted(extra)}")

    lines.extend(
        wrap_paragraph(
            "The reference above is generated from the same metadata that constructs the Settings UI. "
            "Template-specific values are intentionally not listed because they vary by machine, material, and site."
        )
    )
    lines.append("")
    return "\n".join(lines)


def replace_section(manual: str, reference: str) -> str:
    if manual.count(BEGIN_MARKER) != 1 or manual.count(END_MARKER) != 1:
        raise ValueError("manual must contain exactly one generated settings reference marker pair")
    before, remainder = manual.split(BEGIN_MARKER, 1)
    _, after = remainder.split(END_MARKER, 1)
    return f"{before}{BEGIN_MARKER}\n{reference}{END_MARKER}{after}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--settings-dir", type=Path, default=Path("resources/settings"))
    parser.add_argument("--manual", type=Path, default=Path("docs/ornlslicer-user-guide.md"))
    parser.add_argument("--check", action="store_true", help="fail if the generated appendix is stale")
    args = parser.parse_args()

    settings, inputs = load_catalog(args.settings_dir)
    current = args.manual.read_text(encoding="utf-8")
    expected = replace_section(current, build_reference(settings, inputs))

    if args.check:
        if current != expected:
            raise SystemExit(f"{args.manual} has a stale generated settings reference")
        print(f"{args.manual}: generated settings reference is current ({len(settings)} settings)")
        return

    args.manual.write_text(expected, encoding="utf-8", newline="\n")
    print(f"Updated {args.manual} with {len(settings)} settings and {len(inputs)} grouped inputs")


if __name__ == "__main__":
    main()
