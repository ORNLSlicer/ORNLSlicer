"""Validate grouped setting metadata and its generated configuration."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
import generate_master_config as generator


class SettingGroupTests(unittest.TestCase):
    def setUp(self):
        settings, _ = generator.parse_settings_file(ROOT / "resources/settings/040_profile_arc_specialties.yaml")
        self.setting = next(setting for setting in settings if setting["name"] == "arc_specialties_perimeter_preflow")
        self.setting = copy.deepcopy(self.setting)
        self.setting["group"] = {"region": "Perimeter", "section": "Gas Timing", "label": "Preflow", "columns": 2}
        self.names = {"syntax", "slicing_mode", "cylindrical_path_pattern", "perimeter", "helical_perimeter_revolutions"}

    def test_valid_group_survives_generation(self):
        generator.validate_setting(self.setting, self.names)
        normalized = generator.normalize_for_master(self.setting)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "master.conf"
            generator.write_master({self.setting["name"]: normalized}, output)
            generated = json.loads(output.read_text())[self.setting["name"]]
        self.assertEqual(generated["group"], self.setting["group"])
        self.assertEqual(generated["display"], "Perimeter Preflow")

    def test_reject_invalid_groups(self):
        group = self.setting["group"]
        invalid = [None, "Perimeter", [], {}, {**group, "unknown": 1}]
        for field in ("region", "section", "label"):
            for value in ("", " ", 1, None):
                invalid.append({**group, field: value})
        for value in (0, 3, True, "2", 1.5):
            invalid.append({**group, "columns": value})
        for value in invalid:
            with self.subTest(group=value):
                self.setting["group"] = value
                with self.assertRaises(ValueError):
                    generator.validate_setting(self.setting, self.names)

    def test_catalog_matches_generated_settings(self):
        master, inputs = generator.load_settings(ROOT / "resources/settings")
        self.assertEqual(master, json.loads((ROOT / "resources/configs/master.conf").read_text()))
        self.assertEqual(inputs, json.loads((ROOT / "resources/configs/setting_inputs.conf").read_text()))
        grouped = [setting for setting in master.values() if "group" in setting]
        self.assertEqual(len(grouped), 76)
        for region in ("Perimeter", "Inset", "Infill", "Skeleton"):
            regional = [setting["group"] for setting in grouped if setting["group"]["region"] == region]
            self.assertEqual(len(regional), 19)
            self.assertEqual([entry["section"] for entry in regional].count("Gas Timing"), 4)
            self.assertEqual([entry["section"] for entry in regional].count("Startup"), 6)
            self.assertEqual([entry["section"] for entry in regional].count("Main Weld"), 4)
            self.assertEqual([entry["section"] for entry in regional].count("Crater Fill"), 5)
            for entry in regional:
                if entry["section"] == "Gas Timing":
                    self.assertEqual(entry.get("columns", 1), 1)


class DlpSettingTests(unittest.TestCase):
    def test_dlp_pixel_width_is_fixed_without_changing_saved_value_type(self):
        master, _ = generator.load_settings(ROOT / "resources/settings")
        setting = master["dlp_pixel_width"]
        self.assertEqual(setting["type"], "positive_int")
        self.assertEqual(setting["fixed"], 4096)
        self.assertNotIn("fixed", master["dlp_projection_width"])

    def test_fixed_metadata_requires_a_matching_integer_default(self):
        settings, _ = generator.parse_settings_file(ROOT / "resources/settings/041_experimental_dlp_slicer.yaml")
        setting = next(setting for setting in settings if setting["name"] == "dlp_pixel_width")
        names = {"slicing_mode"}
        generator.validate_setting(setting, names)
        for fixed in (1024, 4096.0, "4096", True, None, 0):
            with self.subTest(fixed=fixed):
                invalid = {**setting, "fixed": fixed}
                with self.assertRaises(ValueError):
                    generator.validate_setting(invalid, names)
        for setting_type in ("distance", "boolean", "string"):
            with self.subTest(setting_type=setting_type):
                with self.assertRaises(ValueError):
                    generator.validate_setting({**setting, "type": setting_type}, names)

    def test_dlp_mode_preserves_existing_mode_indices(self):
        master, _ = generator.load_settings(ROOT / "resources/settings")
        self.assertEqual(master["slicing_mode"]["options"].split(", "), ["Planar", "Cylindrical", "Image", "DLP"])
        self.assertEqual(master["slicing_mode"]["default"], 0)

    def test_dlp_stitching_is_optional_with_conditional_physical_overlap(self):
        master, _ = generator.load_settings(ROOT / "resources/settings")
        stitching = master["dlp_interlaced_stitching"]
        self.assertEqual(stitching["type"], "boolean")
        self.assertIs(stitching["default"], False)
        self.assertEqual(stitching["depends"], {"slicing_mode": 3})
        self.assertEqual(stitching["visible_if"], {"slicing_mode": 3})
        self.assertFalse(stitching["local"])
        overlap = master["dlp_stitch_overlap"]
        self.assertEqual(overlap["type"], "distance")
        self.assertEqual(overlap["default"], 1000)
        enabled = {"AND": [{"slicing_mode": 3}, {"dlp_interlaced_stitching": True}]}
        self.assertEqual(overlap["depends"], enabled)
        self.assertEqual(overlap["visible_if"], enabled)
        self.assertFalse(overlap["local"])
        for setting in (stitching, overlap):
            self.assertEqual(setting["major"], "Experimental")
            self.assertEqual(setting["minor"], "DLP Slicer")

    def test_dlp_settings_are_global_and_only_visible_in_dlp_mode(self):
        master, _ = generator.load_settings(ROOT / "resources/settings")
        for name, default in (
            ("dlp_output_mode", 0),
            ("dlp_projection_width", 204800),
            ("dlp_pixel_width", 4096),
            ("dlp_bit_depth", 0),
        ):
            with self.subTest(setting=name):
                setting = master[name]
                self.assertEqual(setting["major"], "Experimental")
                self.assertEqual(setting["minor"], "DLP Slicer")
                self.assertEqual(setting["depends"], {"slicing_mode": 3})
                visibility = (
                    {"AND": [{"slicing_mode": 3}, {"dlp_output_mode": 0}]}
                    if name == "dlp_bit_depth"
                    else {"slicing_mode": 3}
                )
                self.assertEqual(setting["visible_if"], visibility)
                self.assertFalse(setting["local"])
                self.assertEqual(setting["default"], default)
        self.assertEqual(master["dlp_bit_depth"]["options"].split(", "), ["1-bit", "8-bit"])
        self.assertEqual(master["dlp_output_mode"]["options"].split(", "), ["Static", "Scrolling"])


if __name__ == "__main__":
    unittest.main()
