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

INPUT_REQUIRED_FIELDS = (
    "display",
    "widget",
    "tooltip",
    "depends",
    "minor",
    "major",
    "components",
)

INPUT_OPTIONAL_FIELDS = ("name",)

INPUT_COMPONENT_FIELDS = ("setting", "label")

VALID_INPUT_WIDGETS = {
    "vector2",
    "vector3",
}

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


def parse_field(field_line: str, path: Path, line_number: int) -> tuple[str, str]:
    if ":" not in field_line:
        raise ValueError(f"{path}:{line_number}: expected 'key: value' field")

    key, value = field_line.split(":", 1)
    return key.strip(), value


def parse_settings_file(path: Path) -> tuple[list[OrderedDict[str, Any]], list[OrderedDict[str, Any]]]:
    settings: list[OrderedDict[str, Any]] = []
    inputs: list[OrderedDict[str, Any]] = []
    current_setting: OrderedDict[str, Any] | None = None
    current_input: OrderedDict[str, Any] | None = None
    current_component: OrderedDict[str, Any] | None = None
    section: str | None = None
    saw_settings = False
    active_list_key: str | None = None

    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not raw_line.strip() or raw_line.lstrip().startswith("#"):
            continue

        if raw_line == "settings:":
            section = "settings"
            saw_settings = True
            active_list_key = None
            continue

        if raw_line == "inputs:":
            section = "inputs"
            active_list_key = None
            continue

        if section is None:
            raise ValueError(f"{path}:{line_number}: expected top-level 'settings:' key")

        if section == "settings":
            if active_list_key == "options" and raw_line.startswith("      - "):
                current_setting[active_list_key].append(parse_scalar(raw_line[len("      - ") :], path, line_number))
                continue

            active_list_key = None

            if raw_line.startswith("  - name: "):
                current_setting = OrderedDict()
                current_setting["name"] = parse_scalar(raw_line[len("  - name: ") :], path, line_number)
                settings.append(current_setting)
                continue

            if current_setting is None:
                raise ValueError(f"{path}:{line_number}: setting fields must follow '- name:'")

            if not raw_line.startswith("    "):
                raise ValueError(f"{path}:{line_number}: expected 4-space-indented setting field")

            field_line = raw_line[4:]
            key, value = parse_field(field_line, path, line_number)
            if key in current_setting:
                raise ValueError(f"{path}:{line_number}: duplicate field '{key}'")

            if value.strip() == "" and key == "options":
                current_setting[key] = []
                active_list_key = key
            else:
                current_setting[key] = parse_scalar(value, path, line_number)
            continue

        if active_list_key == "components":
            if raw_line.startswith("      - "):
                component = OrderedDict()
                key, value = parse_field(raw_line[len("      - ") :], path, line_number)
                component[key] = parse_scalar(value, path, line_number)
                current_input[active_list_key].append(component)
                current_component = component
                continue

            if raw_line.startswith("        "):
                if current_component is None:
                    raise ValueError(f"{path}:{line_number}: component fields must follow '- setting:'")

                key, value = parse_field(raw_line[8:], path, line_number)
                if key in current_component:
                    raise ValueError(f"{path}:{line_number}: duplicate component field '{key}'")

                current_component[key] = parse_scalar(value, path, line_number)
                continue

        active_list_key = None

        if raw_line.startswith("  - name: "):
            current_input = OrderedDict()
            current_input["name"] = parse_scalar(raw_line[len("  - name: ") :], path, line_number)
            inputs.append(current_input)
            current_component = None
            continue

        if current_input is None:
            raise ValueError(f"{path}:{line_number}: input fields must follow '- name:'")

        if not raw_line.startswith("    "):
            raise ValueError(f"{path}:{line_number}: expected 4-space-indented input field")

        field_line = raw_line[4:]
        key, value = parse_field(field_line, path, line_number)
        if key in current_input:
            raise ValueError(f"{path}:{line_number}: duplicate field '{key}'")

        if value.strip() == "" and key == "components":
            current_input[key] = []
            active_list_key = key
            current_component = None
        else:
            current_input[key] = parse_scalar(value, path, line_number)

    if not saw_settings:
        raise ValueError(f"{path}: missing top-level 'settings:' key")

    return settings, inputs


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


def validate_input(setting_input: OrderedDict[str, Any], settings_by_name: OrderedDict[str, OrderedDict[str, Any]],
                   used_components: set[str]) -> None:
    name = setting_input.get("name")
    if not isinstance(name, str) or not name:
        raise ValueError("input is missing non-empty name")

    valid_fields = set(INPUT_REQUIRED_FIELDS) | set(INPUT_OPTIONAL_FIELDS)
    unknown_fields = sorted(set(setting_input) - valid_fields)
    if unknown_fields:
        raise ValueError(f"{name}: unknown input fields: {', '.join(unknown_fields)}")

    missing_fields = [field for field in INPUT_REQUIRED_FIELDS if field not in setting_input]
    if missing_fields:
        raise ValueError(f"{name}: missing input fields: {', '.join(missing_fields)}")

    widget = setting_input["widget"]
    if widget not in VALID_INPUT_WIDGETS:
        raise ValueError(f"{name}: unknown input widget '{widget}'")

    components = setting_input["components"]
    if not isinstance(components, list):
        raise ValueError(f"{name}: components must be a list")

    expected_count = 2 if widget == "vector2" else 3
    if len(components) != expected_count:
        raise ValueError(f"{name}: {widget} inputs must define exactly {expected_count} components")

    validate_dependency(setting_input["depends"], set(settings_by_name), name)

    component_settings: list[str] = []
    for component in components:
        unknown_component_fields = sorted(set(component) - set(INPUT_COMPONENT_FIELDS))
        if unknown_component_fields:
            raise ValueError(f"{name}: unknown component fields: {', '.join(unknown_component_fields)}")

        missing_component_fields = [field for field in INPUT_COMPONENT_FIELDS if field not in component]
        if missing_component_fields:
            raise ValueError(f"{name}: component missing fields: {', '.join(missing_component_fields)}")

        setting_name = component["setting"]
        if setting_name not in settings_by_name:
            raise ValueError(f"{name}: component references unknown setting '{setting_name}'")
        if setting_name in used_components:
            raise ValueError(f"{name}: setting '{setting_name}' is already used by another input")
        if not isinstance(component["label"], str) or not component["label"]:
            raise ValueError(f"{name}: component label must be a non-empty string")

        component_settings.append(setting_name)
        used_components.add(setting_name)

    majors = {settings_by_name[setting]["major"] for setting in component_settings}
    minors = {settings_by_name[setting]["minor"] for setting in component_settings}
    locals_ = {settings_by_name[setting]["local"] for setting in component_settings}
    if majors != {setting_input["major"]}:
        raise ValueError(f"{name}: input major must match all component settings")
    if minors != {setting_input["minor"]}:
        raise ValueError(f"{name}: input minor must match all component settings")
    if len(locals_) != 1:
        raise ValueError(f"{name}: all components must have the same local value")

    component_types = [settings_by_name[setting]["type"] for setting in component_settings]
    if widget == "vector2":
        if any(component_type not in {"distance", "location"} for component_type in component_types):
            raise ValueError(f"{name}: vector2 components must be distance or location settings")
        if len(set(component_types)) != 1:
            raise ValueError(f"{name}: vector2 components must share the same setting type")
    elif any(component_type != "unitless_float" for component_type in component_types):
        raise ValueError(f"{name}: vector3 components must be unitless_float settings")


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


def normalize_input_for_config(setting_input: OrderedDict[str, Any],
                               settings_by_name: OrderedDict[str, OrderedDict[str, Any]]) -> OrderedDict[str, Any]:
    entry = OrderedDict()
    entry["name"] = setting_input["name"]
    for field in INPUT_REQUIRED_FIELDS:
        if field == "components":
            continue

        value = setting_input[field]
        if field == "depends" and dependency_is_empty(value):
            value = ""
        entry[field] = value

    component_settings = [component["setting"] for component in setting_input["components"]]
    entry["local"] = settings_by_name[component_settings[0]]["local"]
    entry["components"] = []
    for component in setting_input["components"]:
        setting = settings_by_name[component["setting"]]
        entry["components"].append(
            OrderedDict(
                (
                    ("setting", component["setting"]),
                    ("label", component["label"]),
                    ("type", setting["type"]),
                    ("default", setting["default"]),
                )
            )
        )

    return entry


def load_settings(source_dir: Path) -> tuple[OrderedDict[str, OrderedDict[str, Any]], OrderedDict[str, OrderedDict[str, Any]]]:
    paths = sorted(source_dir.rglob("*.yaml"))
    if not paths:
        raise ValueError(f"{source_dir}: no .yaml settings files found")

    raw_settings: list[OrderedDict[str, Any]] = []
    raw_inputs: list[OrderedDict[str, Any]] = []
    for path in paths:
        settings, inputs = parse_settings_file(path)
        raw_settings.extend(settings)
        raw_inputs.extend(inputs)

    names = [setting["name"] for setting in raw_settings]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise ValueError(f"duplicate setting names: {', '.join(duplicates)}")

    setting_names = set(names)
    master: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    for setting in raw_settings:
        validate_setting(setting, setting_names)
        master[setting["name"]] = normalize_for_master(setting)

    input_names = [setting_input["name"] for setting_input in raw_inputs]
    duplicate_inputs = sorted({name for name in input_names if input_names.count(name) > 1})
    if duplicate_inputs:
        raise ValueError(f"duplicate input names: {', '.join(duplicate_inputs)}")

    used_components: set[str] = set()
    setting_inputs: OrderedDict[str, OrderedDict[str, Any]] = OrderedDict()
    for setting_input in raw_inputs:
        validate_input(setting_input, master, used_components)
        setting_inputs[setting_input["name"]] = normalize_input_for_config(setting_input, master)

    return master, setting_inputs


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


def write_setting_inputs(setting_inputs: OrderedDict[str, OrderedDict[str, Any]], destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("w", encoding="utf-8", newline="\n") as output:
        json.dump(setting_inputs, output, indent=2, separators=(",", ": "), ensure_ascii=True)
        output.write("\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path, help="Directory containing split settings .yaml files")
    parser.add_argument("destination", type=Path, help="Generated master.conf destination")
    parser.add_argument("inputs_destination", type=Path, help="Generated setting_inputs.conf destination")
    args = parser.parse_args()

    master, setting_inputs = load_settings(args.source_dir)
    write_master(master, args.destination)
    write_setting_inputs(setting_inputs, args.inputs_destination)


if __name__ == "__main__":
    main()
