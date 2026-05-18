#!/usr/bin/env python3
"""Generate resources/configs/master.conf from split settings YAML files.

The parser intentionally supports only the small YAML subset used by
resources/settings/*.yaml.  This keeps the build dependency-free while still
making the settings catalog reviewable as text.
"""

from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from pathlib import Path
from typing import Any


REQUIRED_FIELDS = (
    "display",
    "type",
    "tooltip",
    "depends",
    "options",
    "default",
    "minor",
    "major",
    "namespace",
    "symbol",
    "local",
)

OPTIONAL_FIELDS = ("name",)

VALID_TYPES = {
    "accel",
    "angle",
    "ang_vel",
    "area",
    "boolean",
    "density",
    "distance",
    "enumeration",
    "location",
    "multiline_text",
    "number",
    "numbered_list",
    "percentage",
    "percentage100",
    "positive_int",
    "power",
    "rpm",
    "speed",
    "string",
    "temperature",
    "time",
    "unitless_float",
    "voltage",
}


def parse_scalar(value: str, path: Path, line_number: int) -> Any:
    value = value.strip()

    if value == "":
        return ""

    if value in {"true", "false"}:
        return value == "true"

    if value == "null":
        return None

    if value[0] in {'"', "[", "{"}:
        try:
            return json.loads(value)
        except json.JSONDecodeError as exc:
            raise ValueError(f"{path}:{line_number}: invalid JSON-style YAML value: {value}") from exc

    try:
        return int(value)
    except ValueError:
        pass

    try:
        return float(value)
    except ValueError:
        return value


def parse_settings_file(path: Path) -> list[OrderedDict[str, Any]]:
    settings: list[OrderedDict[str, Any]] = []
    current: OrderedDict[str, Any] | None = None
    saw_header = False
    active_list_key: str | None = None

    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not raw_line.strip() or raw_line.lstrip().startswith("#"):
            continue

        if active_list_key is not None and raw_line.startswith("      - "):
            current[active_list_key].append(parse_scalar(raw_line[len("      - ") :], path, line_number))
            continue

        active_list_key = None

        if raw_line == "settings:":
            saw_header = True
            continue

        if raw_line.startswith("  - name: "):
            current = OrderedDict()
            current["name"] = parse_scalar(raw_line[len("  - name: ") :], path, line_number)
            settings.append(current)
            continue

        if not saw_header:
            raise ValueError(f"{path}:{line_number}: expected top-level 'settings:' key")

        if current is None:
            raise ValueError(f"{path}:{line_number}: setting fields must follow '- name:'")

        if not raw_line.startswith("    "):
            raise ValueError(f"{path}:{line_number}: expected 4-space-indented setting field")

        field_line = raw_line[4:]
        if ":" not in field_line:
            raise ValueError(f"{path}:{line_number}: expected 'key: value' field")

        key, value = field_line.split(":", 1)
        key = key.strip()
        if key in current:
            raise ValueError(f"{path}:{line_number}: duplicate field '{key}'")

        if value.strip() == "" and key == "options":
            current[key] = []
            active_list_key = key
        else:
            current[key] = parse_scalar(value, path, line_number)

    if not saw_header:
        raise ValueError(f"{path}: missing top-level 'settings:' key")

    return settings


def dependency_is_empty(depends: Any) -> bool:
    return depends in ("", None, {}, [])


def validate_dependency(depends: Any, setting_names: set[str], current_name: str) -> None:
    if dependency_is_empty(depends):
        return

    if not isinstance(depends, dict):
        raise ValueError(f"{current_name}: depends must be an object or empty object")

    if len(depends) != 1:
        raise ValueError(f"{current_name}: each dependency node must contain exactly one key")

    key, value = next(iter(depends.items()))
    if key in {"AND", "OR"}:
        if not isinstance(value, list) or len(value) != 2:
            raise ValueError(f"{current_name}: {key} dependency must contain exactly two child nodes")
        for child in value:
            validate_dependency(child, setting_names, current_name)
        return

    if key == "NOT":
        if not isinstance(value, list) or len(value) != 1:
            raise ValueError(f"{current_name}: NOT dependency must contain exactly one child node")
        validate_dependency(value[0], setting_names, current_name)
        return

    if key not in setting_names:
        raise ValueError(f"{current_name}: dependency references unknown setting '{key}'")

    if isinstance(value, (dict, list)):
        raise ValueError(f"{current_name}: dependency value for '{key}' must be scalar")


def validate_setting(setting: OrderedDict[str, Any], setting_names: set[str]) -> None:
    name = setting.get("name")
    if not isinstance(name, str) or not name:
        raise ValueError("setting is missing non-empty name")

    valid_fields = set(REQUIRED_FIELDS) | set(OPTIONAL_FIELDS)
    unknown_fields = sorted(set(setting) - valid_fields)
    if unknown_fields:
        raise ValueError(f"{name}: unknown fields: {', '.join(unknown_fields)}")

    missing_fields = [field for field in REQUIRED_FIELDS if field not in setting]
    if missing_fields:
        raise ValueError(f"{name}: missing fields: {', '.join(missing_fields)}")

    if setting["type"] not in VALID_TYPES:
        raise ValueError(f"{name}: unknown setting type '{setting['type']}'")

    if not isinstance(setting["local"], bool):
        raise ValueError(f"{name}: local must be true or false")

    if not isinstance(setting["options"], list):
        raise ValueError(f"{name}: options must be a list")

    if setting["type"] == "enumeration":
        if not setting["options"]:
            raise ValueError(f"{name}: enumeration settings must define at least one option")
        if not isinstance(setting["default"], int):
            raise ValueError(f"{name}: enumeration default must be an integer index")
        if setting["default"] < 0 or setting["default"] >= len(setting["options"]):
            raise ValueError(f"{name}: enumeration default index is outside the options list")
    elif setting["options"]:
        raise ValueError(f"{name}: only enumeration settings may define options")

    if setting["type"] == "boolean" and not isinstance(setting["default"], bool):
        raise ValueError(f"{name}: boolean default must be true or false")

    validate_dependency(setting["depends"], setting_names, name)


def normalize_for_master(setting: OrderedDict[str, Any]) -> OrderedDict[str, Any]:
    entry = OrderedDict()
    for field in REQUIRED_FIELDS:
        value = setting[field]
        if field == "depends" and dependency_is_empty(value):
            value = ""
        elif field == "options":
            value = ", ".join(value)
        entry[field] = value
    return entry


def load_settings(source_dir: Path) -> OrderedDict[str, OrderedDict[str, Any]]:
    paths = sorted(source_dir.rglob("*.yaml"))
    if not paths:
        raise ValueError(f"{source_dir}: no .yaml settings files found")

    raw_settings: list[OrderedDict[str, Any]] = []
    for path in paths:
        raw_settings.extend(parse_settings_file(path))

    names = [setting["name"] for setting in raw_settings]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise ValueError(f"duplicate setting names: {', '.join(duplicates)}")

    setting_names = set(names)
    master: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    for setting in raw_settings:
        validate_setting(setting, setting_names)
        master[setting["name"]] = normalize_for_master(setting)

    return master


def write_master(master: OrderedDict[str, OrderedDict[str, Any]], destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="utf-8", newline="\n") as output:
        output.write("{\n")
        last_key = next(reversed(master))
        for key, value in master.items():
            output.write(f'  "{key}": {{\n')
            last_field = REQUIRED_FIELDS[-1]
            for field in REQUIRED_FIELDS:
                field_value = json.dumps(value[field], separators=(",", ":"), ensure_ascii=True)
                field_value = field_value.replace("\\/", "/")
                suffix = "," if field != last_field else ""
                output.write(f'      "{field}":{field_value}{suffix}\n')
            output.write("  }")
            if key != last_key:
                output.write(",\n")
        output.write("\n}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path, help="Directory containing split settings .yaml files")
    parser.add_argument("destination", type=Path, help="Generated master.conf destination")
    args = parser.parse_args()

    master = load_settings(args.source_dir)
    write_master(master, args.destination)


if __name__ == "__main__":
    main()
