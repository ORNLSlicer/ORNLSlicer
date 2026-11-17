"""Exercise perimeter avoidance through CLI slicing and Cincinnati export."""

import argparse
import copy
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys


def write_fixture(path):
    """Two separate closed prisms with diagonal walls at machine-scale coordinates."""
    triangles = []
    for cx in (3048, 3498):
        ring = [(cx + 203.2 * math.cos(i * math.pi / 3),
                 1079.5 + 203.2 * math.sin(i * math.pi / 3)) for i in range(6)]
        bottom = [(x, y, 0) for x, y in ring]
        top = [(x, y, 7.62) for x, y in ring]
        for i in range(1, 5):
            triangles.extend(((bottom[0], bottom[i + 1], bottom[i]), (top[0], top[i], top[i + 1])))
        for i in range(6):
            j = (i + 1) % 6
            triangles.extend(((bottom[i], bottom[j], top[j]), (bottom[i], top[j], top[i])))
    with path.open("w") as output:
        output.write("solid hexagons\n")
        for triangle in triangles:
            output.write("facet normal 0 0 0\nouter loop\n")
            for point in triangle:
                output.write("vertex " + " ".join(format(value, ".12g") for value in point) + "\n")
            output.write("endloop\nendfacet\n")
        output.write("endsolid hexagons\n")


def fixture_settings(template):
    document = json.loads(template.read_text())
    document["settings"][0].update({
        "syntax": 1, "slicing_mode": 0, "layer_height": 3810, "default_width": 8636,
        "minimum_x": 0, "maximum_x": 4000000, "minimum_y": 0, "maximum_y": 2000000,
        "minimum_z": 0, "maximum_z": 500000, "x_offset": 0, "y_offset": 0, "z_offset": 0,
        "perimeter": True, "perimeter_count": 3, "perimeter_width": 8636,
        "perimeter_speed": 60000, "perimeter_extruder_speed": 400,
        "perimeter_minimum_path_length": 0, "perimeter_minimum_segment_length": 0,
        "connect_perimeters": False, "perimeter_path_order_optimization": 0,
        "path_order_optimization": 4, "point_order_optimization": 4,
        "custom_point_order_x_location": 0, "custom_point_order_y_location": 1079500,
        "custom_point_order_z_location": 0, "inset": False, "infill": False,
        "skin": False, "skeleton": False, "support": False, "skirt": False,
        "brim": False, "raft": False, "perimeter_wipe": True,
        "perimeter_wipe_direction": 0, "perimeter_wipe_distance": 76200,
        "perimeter_wipe_speed": 100000, "perimeter_wipe_extruder_speed": 0,
        "perimeter_wipe_lift_height": 0, "perimeter_wipe_cutoff_distance": 0,
        "travel_speed": 120000, "travel_lift_height": 0, "retraction": False,
        "enable_travel_pause": False, "travel_centroid_move": False,
        "force_minimum_layer_time": False, "enable_arc_fitting": False,
    })
    return document


def cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def point_distance(point, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = dx * dx + dy * dy
    t = min(1, max(0, ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / length)) if length else 0
    return math.hypot(point[0] - a[0] - t * dx, point[1] - a[1] - t * dy)


def rounded_contact(point, a, b):
    # Cincinnati emits X to 0.001 inch and Y to 0.0001 inch. Bound the signed-line
    # uncertainty rather than using an intersection-distance cutoff at shallow angles.
    dx, dy = b[0] - a[0], b[1] - a[1]
    px, py = point[0] - a[0], point[1] - a[1]
    ex, ey = 0.0005, 0.00005
    uncertainty = 2 * ey * (abs(dx) + abs(px)) + 2 * ex * (abs(dy) + abs(py)) + 8 * ex * ey
    return (abs(dx * py - dy * px) <= uncertainty and
            min(a[0], b[0]) - 2 * ex <= point[0] <= max(a[0], b[0]) + 2 * ex and
            min(a[1], b[1]) - 2 * ey <= point[1] <= max(a[1], b[1]) + 2 * ey)


def crossing(a, b, c, d):
    u, v, w = (b[0] - a[0], b[1] - a[1]), (d[0] - c[0], d[1] - c[1]), (c[0] - a[0], c[1] - a[1])
    determinant = cross(u, v)
    if abs(determinant) < 1e-12:
        return False
    t, s = cross(w, v) / determinant, cross(w, u) / determinant
    return (0 < t < 1 and 0 < s < 1 and
            not any(rounded_contact(point, c, d) for point in (a, b)) and
            not any(rounded_contact(point, a, b) for point in (c, d)))


def inspect(path):
    assert path.is_file() and path.stat().st_size, f"Expected nonempty regular G-code: {path}"
    position, layer = (0, 0, 0), 0
    material, runs, active, perimeters = [], [], [], []
    crossings, clearances, depositing, wipes = 0, [], 0, 0

    def flush():
        if active:
            runs.append((layer, active[0][0], active[-1][1]))
            # The first/last legs must transition from/to the immutable endpoints.
            for a, b in active[1:-1]:
                distance = min(min(point_distance(p, c, d), point_distance(q, a, b))
                               for c, d in perimeters for p in (a, b) for q in (c, d))
                clearances.append(distance * 25.4)
            active.clear()

    for line in path.read_text().splitlines():
        command = line.split("(", 1)[0].strip()
        comments = re.findall(r"\(([^)]*)\)", line)
        comment = comments[-1].strip() if comments else ""
        if comment.startswith("BEGINNING LAYER:"):
            flush()
            layer = int(comment.split(":", 1)[1])
            perimeters.clear()
        if not re.match(r"^G0?[01](?:\s|$)", command):
            continue
        axes = {axis: float(value) for axis, value in re.findall(r"([XYZ])([-+]?(?:\d+(?:\.\d*)?|\.\d+))", command)}
        if not axes:
            continue
        target = tuple(axes.get(axis, position[i]) for i, axis in enumerate("XYZ"))
        start, position = position, target
        if start[:2] == target[:2]:
            continue
        if comment == "TRAVEL":
            active.append((start, target))
            crossings += sum(crossing(start, target, a, b) for a, b in perimeters)
        elif comment.startswith("PERIMETER"):
            flush()
            material.append((layer, comment, start, target))
            if "TIP WIPE" in comment:
                wipes += 1
            else:
                depositing += 1
                perimeters.append((start, target))
    flush()
    return {"layers": layer, "printing": depositing, "wipes": wipes, "crossings": crossings,
            "material": material, "endpoints": runs, "clearance": min(clearances) if clearances else None}


def check_diagnostics(stderr_path, enabled):
    lines = [line for line in stderr_path.read_text().splitlines() if "Perimeter avoidance layer" in line]
    if not enabled:
        assert not lines, "Disabled avoidance must not emit routing summaries"
        return
    pattern = (r"\[ornlslicer\] Perimeter avoidance layer (\d+): clear=(\d+) detours=(\d+) "
               r"reduced_clearance=(\d+) unroutable=(\d+) non_planar=(\d+) travel_limit=(\d+) obstacles=(\d+)")
    summaries = [re.fullmatch(pattern, line) for line in lines]
    assert len(summaries) == 2 and all(summaries), "Enabled avoidance must summarize each layer exactly once"
    values = [tuple(map(int, summary.groups())) for summary in summaries]
    assert [row[0] for row in values] == [1, 2], "Routing summaries must identify the two fixture layers"
    for _, clear, detours, reduced, unroutable, non_planar, limit, obstacles in values:
        assert clear + detours + reduced + unroutable + non_planar + limit == 6, "Each travel needs one routing outcome"
        assert reduced == unroutable == non_planar == limit == 0, "Fixture has room for full-clearance planar routing"
        assert obstacles == 36, "Only deposited perimeter segments should be recorded as obstacles"
    assert sum(row[2] for row in values) > 0, "Routing summaries must record the fixture's required detour"


def run(args):
    assert args.slicer.is_file(), f"Build the slicer executable before running this test: {args.slicer}"
    args.work_dir.mkdir(parents=True, exist_ok=True)
    model = args.work_dir / "hexagons.stl"
    write_fixture(model)
    template = fixture_settings(args.template)
    results = []
    for name, enabled in (("direct", False), ("avoided", True)):
        settings = copy.deepcopy(template)
        settings["settings"][0]["avoid_crossing_perimeters"] = enabled
        settings_path = args.work_dir / f"{name}.s2c"
        settings_path.write_text(json.dumps(settings))
        prefix = args.work_dir / name
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                           XDG_DATA_HOME=str(args.work_dir / name / "data"),
                           XDG_CONFIG_HOME=str(args.work_dir / name / "config"))
        command = [str(args.slicer), "--input_stl_files", str(model), "--input_global_settings", str(settings_path),
                   "--output_location", str(prefix), "--shift_parts_on_load", "false", "--align_parts", "false",
                   "--use_implicit_transforms", "true"]
        stdout_path, stderr_path = args.work_dir / f"{name}.stdout.log", args.work_dir / f"{name}.stderr.log"
        prefix.with_suffix(".nc").unlink(missing_ok=True)
        try:
            with stdout_path.open("w") as stdout, stderr_path.open("w") as stderr:
                subprocess.run(command, env=environment, stdout=stdout, stderr=stderr, timeout=90, check=True)
            results.append(inspect(prefix.with_suffix(".nc")))
            check_diagnostics(stderr_path, enabled)
        except (subprocess.SubprocessError, AssertionError):
            for log in (stdout_path, stderr_path):
                print(f"{log}:\n{log.read_text()}", file=sys.stderr)
            raise
    direct, avoided = results
    assert direct["layers"] == avoided["layers"] == 2, "Fixture must exercise two layers"
    assert direct["printing"] == avoided["printing"] == 72, "Fixture must print three hexagon walls per island/layer"
    assert direct["wipes"] == avoided["wipes"] == 12, "Fixture must exercise diagonal forward wipes"
    assert direct["crossings"] > 0, "Fixture did not reproduce crossing travel"
    assert avoided["crossings"] == 0, "Exported travel crosses a completed perimeter"
    assert direct["material"] == avoided["material"], "Printing or wipe motions changed"
    assert direct["endpoints"] == avoided["endpoints"], "Original travel endpoint pairs changed"
    assert avoided["clearance"] is not None and avoided["clearance"] >= 12.954 - 0.04, "Detour lost bead-edge standoff"
    print(f"CLI regression passed: {direct['crossings']} -> 0 crossings; printing/wipes/endpoints unchanged; "
          f"interior clearance {avoided['clearance']:.3f} mm")
    run_supported_travel(args)


def write_box_hole(path):
    """A narrow frame around a rectangular hole, joined to a solid area for insets."""
    outer = [(2734.564, 927.1), (3648.964, 927.1), (3648.964, 1231.9), (2734.564, 1231.9)]
    inner = [(2751.836, 944.372), (3056.636, 944.372), (3056.636, 1214.628), (2751.836, 1214.628)]
    triangles = []

    def quad(points):
        triangles.extend(((points[0], points[1], points[2]), (points[0], points[2], points[3])))

    for i in range(4):
        j = (i + 1) % 4
        for z in (0, 7.62):
            points = [(*outer[i], z), (*outer[j], z), (*inner[j], z), (*inner[i], z)]
            quad(points if z else list(reversed(points)))
        quad([(*outer[i], 0), (*outer[j], 0), (*outer[j], 7.62), (*outer[i], 7.62)])
        quad([(*inner[j], 0), (*inner[i], 0), (*inner[i], 7.62), (*inner[j], 7.62)])
    with path.open("w") as output:
        output.write("solid box_hole\n")
        for triangle in triangles:
            output.write("facet normal 0 0 0\nouter loop\n")
            for point in triangle:
                output.write("vertex " + " ".join(format(value, ".12g") for value in point) + "\n")
            output.write("endloop\nendfacet\n")
        output.write("endsolid box_hole\n")


def supported_fraction(a, b, perimeters):
    # Independently clip the travel to deposited bead rectangles, then union the
    # covered intervals. Fixture walls are axis aligned; 0.001in covers export rounding.
    intervals = []
    for c, d in perimeters:
        assert c[0] == d[0] or c[1] == d[1], "Box fixture must have axis-aligned walls"
        lower, upper = 0.0, 1.0
        for i in (0, 1):
            low, high = min(c[i], d[i]) - 0.171, max(c[i], d[i]) + 0.171
            delta = b[i] - a[i]
            if abs(delta) < 1e-12:
                if not low <= a[i] <= high:
                    upper = -1
                    break
            else:
                enter, leave = sorted(((low - a[i]) / delta, (high - a[i]) / delta))
                lower, upper = max(lower, enter), min(upper, leave)
        if lower <= upper:
            intervals.append((lower, upper))
    covered, end = 0.0, 0.0
    for low, high in sorted(intervals):
        if high > end:
            covered += high - max(end, low)
            end = high
    return covered


def inspect_supported_travel(path):
    assert path.is_file() and path.stat().st_size, f"Expected nonempty regular G-code: {path}"
    position, layer, previous_region = (0, 0, 0), 0, None
    perimeters, active, material, endpoints, transitions = [], [], [], [], []
    for line in path.read_text().splitlines():
        command = line.split("(", 1)[0].strip()
        comments = re.findall(r"\(([^)]*)\)", line)
        comment = comments[-1].strip() if comments else ""
        if comment.startswith("BEGINNING LAYER:"):
            layer = int(comment.split(":", 1)[1])
            perimeters, active, previous_region = [], [], None
        if not re.match(r"^G0?[01](?:\s|$)", command):
            continue
        axes = {axis: float(value) for axis, value in re.findall(r"([XYZ])([-+]?(?:\d+(?:\.\d*)?|\.\d+))", command)}
        target = tuple(axes.get(axis, position[i]) for i, axis in enumerate("XYZ"))
        start, position = position, target
        if start[:2] == target[:2]:
            continue
        if comment == "TRAVEL":
            active.append((start, target))
        elif comment.startswith(("PERIMETER", "INSET")):
            region = comment.split()[0]
            if active:
                endpoints.append((layer, active[0][0], active[-1][1]))
                if previous_region == "PERIMETER" and region == "INSET":
                    fractions = [supported_fraction(a, b, perimeters) for a, b in active]
                    unsupported = sum(math.dist(a[:2], b[:2]) * (1 - fraction) * 25.4
                                      for (a, b), fraction in zip(active, fractions))
                    transitions.append((layer, unsupported, all(f >= 1 - 1e-8 for f in fractions[:-1])))
                active.clear()
            material.append((layer, comment, start, target))
            previous_region = region
            if region == "PERIMETER" and "TIP WIPE" not in comment:
                perimeters.append((start, target))
    return material, endpoints, transitions


def run_supported_travel(args):
    model = args.work_dir / "box_hole.stl"
    write_box_hole(model)
    template = fixture_settings(args.template)
    template["settings"][0].update({
        "perimeter_count": 1, "path_order_optimization": 0, "custom_point_order_y_location": 0,
        "inset": True, "inset_count": 2, "inset_width": 8636, "inset_speed": 60000,
        "inset_extruder_speed": 400, "inset_path_order_optimization": 0,
        "inset_minimum_path_length": 0, "inset_minimum_segment_length": 0, "connect_insets": False,
        "inset_wipe": True, "inset_wipe_direction": 0, "inset_wipe_distance": 76200,
        "inset_wipe_speed": 100000, "inset_wipe_extruder_speed": 0, "inset_wipe_lift_height": 0,
    })
    results = []
    for name, enabled in (("box_direct", False), ("box_avoided", True)):
        settings = copy.deepcopy(template)
        settings["settings"][0]["avoid_crossing_perimeters"] = enabled
        settings_path = args.work_dir / f"{name}.s2c"
        settings_path.write_text(json.dumps(settings))
        prefix = args.work_dir / name
        environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                           XDG_DATA_HOME=str(args.work_dir / name / "data"),
                           XDG_CONFIG_HOME=str(args.work_dir / name / "config"))
        command = [str(args.slicer), "--input_stl_files", str(model), "--input_global_settings", str(settings_path),
                   "--output_location", str(prefix), "--shift_parts_on_load", "false", "--align_parts", "false",
                   "--use_implicit_transforms", "true"]
        stdout_path, stderr_path = args.work_dir / f"{name}.stdout.log", args.work_dir / f"{name}.stderr.log"
        prefix.with_suffix(".nc").unlink(missing_ok=True)
        try:
            with stdout_path.open("w") as stdout, stderr_path.open("w") as stderr:
                subprocess.run(command, env=environment, stdout=stdout, stderr=stderr, timeout=90, check=True)
            results.append(inspect_supported_travel(prefix.with_suffix(".nc")))
        except (subprocess.SubprocessError, AssertionError):
            for log in (stdout_path, stderr_path):
                print(f"{log}:\n{log.read_text()}", file=sys.stderr)
            raise
    direct, avoided = results
    assert direct[0] == avoided[0], "Supported travel changed printing or wipe motions"
    assert direct[1] == avoided[1], "Supported travel changed original travel endpoint pairs"
    assert len(direct[0]) == 40 and sum("TIP WIPE" in row[1] for row in direct[0]) == 8, "Box fixture lost walls/wipes"
    assert [row[0] for row in direct[2]] == [1, 2], "Fixture must reproduce both perimeter-to-inset transitions"
    assert max(row[1] for row in direct[2]) > 300, "Fixture did not reproduce the unsupported gap"
    assert [row[0] for row in avoided[2]] == [1, 2], "Supported transitions must retain their layers"
    assert all(row[1] <= 8.636 + 0.04 and row[2] for row in avoided[2]), f"Travel left the perimeter across the open gap: {avoided[2]}"
    print(f"Supported-travel regression passed: maximum unsupported span "
          f"{max(row[1] for row in direct[2]):.3f} -> {max(row[1] for row in avoided[2]):.3f} mm")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slicer", type=Path, required=True)
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    run(parser.parse_args())
