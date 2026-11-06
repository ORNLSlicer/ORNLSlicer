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


if __name__ == "__main__":
    unittest.main()
