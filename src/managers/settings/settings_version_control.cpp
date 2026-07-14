#include "managers/settings/settings_version_control.h"

#include <array>
#include <list>
#include <string>
#include <utility>

#include <QDateTime>
#include <QRegularExpression>
#include <qhashfunctions.h>

#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"

namespace {
constexpr int kCincinnatiSyntax = 1;
constexpr int kMarlinSyntax = 10;
constexpr int kThermwoodSyntax = 16;
constexpr int kRemovedGcodeSyntax = 28;
constexpr int kRemovedRadialSyntax = 31;
constexpr int kArcSpecialtiesSyntax = 31;
constexpr int kLegacyArcSpecialtiesSyntax = 32;
constexpr int kPlanarSlicer = 0;
constexpr int kV4ImageSlicer = 1;
constexpr int kLegacyRadialSlicer = 2;
constexpr int kLegacyHelicalSlicer = 3;
constexpr int kV8CylindricalSlicer = 2;
constexpr int kV9CylindricalSlicer = 1;
constexpr int kV9ImageSlicer = 2;
constexpr int kRadialPathType = 0;
constexpr int kHelicalPathType = 1;
constexpr int kClipBoundaryHandling = 0;
constexpr int kV3LegacySlicerType2 = 2;
constexpr int kV3ImageSlicer = 3;
constexpr int kAllPerimeterBoundaries = 0;
const std::string kLegacyHelicalClippingMethod = "helical_clipping_method";

constexpr std::array<int, 35> kSyntaxV2ToV3 = {
    0,                 // Beam
    1,                 // Cincinnati
    2,                 // Common
    3,                 // Dmg Dmu
    kCincinnatiSyntax, // GKN removed
    4,                 // Gudel
    5,                 // Haas Inch
    6,                 // Haas Metric
    7,                 // Haas Metric No Comments
    8,                 // Hurco
    9,                 // Ingersoll
    10,                // Marlin
    11,                // JuggerBot
    12,                // Mazak
    13,                // MVP
    14,                // RomiFanuc
    kCincinnatiSyntax, // RPBF removed
    15,                // Siemens
    kThermwoodSyntax,  // SkyBaam removed; concrete templates now use Thermwood
    16,                // Thermwood
    17,                // Wolf
    18,                // RepRap
    19,                // Mach4
    20,                // AeroBasic
    21,                // Meld
    22,                // ORNL
    23,                // Okuma
    24,                // Tormach
    25,                // AML3D
    26,                // KraussMaffei
    27,                // Sandia
    28,                // Removed syntax
    29,                // Meltio
    30,                // Adamantine
    31                 // ORNL Metric
};

constexpr std::array<int, 7> kSlicerTypeV2ToV3 = {
    0,                    // Polymer
    1,                    // Legacy slicer type 1
    2,                    // Legacy slicer type 2
    kV3LegacySlicerType2, // RPBF removed
    kPlanarSlicer,        // Real Time Polymer removed
    kV3LegacySlicerType2, // Real Time RPBF removed
    kV3ImageSlicer        // Image
};

constexpr std::array<int, 4> kSlicerTypeV3ToV4 = {
    kPlanarSlicer, // Polymer
    kPlanarSlicer, // Legacy slicer type 1 removed
    kPlanarSlicer, // Legacy slicer type 2 removed
    kV4ImageSlicer // Image
};

constexpr std::array<int, 3> kSlicerTypeV8ToV9 = {
    kPlanarSlicer,       // Planar
    kV9ImageSlicer,      // Image moved after Cylindrical
    kV9CylindricalSlicer // Cylindrical moved before Image
};

constexpr std::array<int, 7> kSkinPatternV4ToV5 = {
    0, // Lines
    1, // Grid
    2, // Concentric
    2, // Removed option; use Concentric
    3, // Triangles
    4, // Hexagons and Triangles
    5  // Honeycomb
};

constexpr std::array<int, 8> kInfillPatternV4ToV5 = {
    0, // Lines
    1, // Grid
    2, // Concentric
    2, // Removed option; use Concentric
    3, // Triangles
    4, // Hexagons and Triangles
    5, // Honeycomb
    6  // Radial Hatch
};

constexpr std::array<const char*, 47> kRemovedV3Settings = {
    // GKN syntax settings
    "base_coordinate", "gkn_laser_power", "gkn_melt_pool", "gkn_print_speed", "gkn_wire_speed", "supports_E1",
    "supports_E2", "tool_coordinate",

    // RPBF and metal sector settings
    "clocking_angle", "enable_partition_scheme", "enable_radial_split", "infill_focus", "infill_partition_scheme",
    "infill_power", "infill_sector_count", "infill_spot_size", "perimeter_focus", "perimeter_power",
    "perimeter_spot_size", "sector_offsetting_enable", "sector_overlap", "sector_size", "sector_stagger_angle",
    "sector_stagger_enable",

    // Single path and real-time settings
    "corner_exclusion_distance", "enable_bridge_exclusion", "enable_single_path", "enable_zippering",
    "max_bridge_length", "min_bridge_separation", "previous_layer_exclusion_distance",

    // Removed writer/path modifier settings
    "laser_power_multiplier", "pyrometer_move", "rotation_origin_offset_x", "rotation_origin_offset_y",
    "wire_feed_multiplier",

    // Wire-feed and anchor settings
    "anchor_enable", "anchor_height", "anchor_object_distance_left", "anchor_object_distance_right", "anchor_width",
    "setting_region_mesh_split", "wire_feed_cutoff_distance", "wire_feed_enable", "wire_feed_initial_travel",
    "wire_feed_prestart_distance", "wire_feed_stickout_distance"};

template <std::size_t N>
void migrateIndexedSetting(fifojson& settings_group, const QString& setting_key, const std::array<int, N>& mapping) {
    if (!settings_group.is_object())
        return;

    auto setting = settings_group.find(setting_key.toStdString());
    if (setting == settings_group.end() || !setting.value().is_number_integer())
        return;

    int setting_value = setting.value().get<int>();
    if (setting_value >= 0 && static_cast<std::size_t>(setting_value) < mapping.size())
        setting.value() = mapping[setting_value];
}

void removeV2Settings(fifojson& settings_group) {
    if (!settings_group.is_object())
        return;

    for (const char* removed_setting : kRemovedV3Settings)
        settings_group.erase(std::string(removed_setting));
}

void addV3Settings(fifojson& settings_group) {
    if (!settings_group.is_object())
        return;

    const std::string perimeter_boundary_selection =
        ORNL::Constants::ProfileSettings::Perimeter::kBoundarySelection.toStdString();
    if (settings_group.find(perimeter_boundary_selection) == settings_group.end())
        settings_group[perimeter_boundary_selection] = kAllPerimeterBoundaries;
}

void migrateRemovedGcodeSyntax(fifojson& settings_group) {
    if (!settings_group.is_object())
        return;

    const std::string syntax_key = ORNL::Constants::PrinterSettings::MachineSetup::kSyntax.toStdString();
    auto setting = settings_group.find(syntax_key);
    if (setting == settings_group.end() || !setting.value().is_number_integer())
        return;

    const int setting_value = setting.value().get<int>();
    if (setting_value == kRemovedGcodeSyntax)
        setting.value() = kMarlinSyntax;
    else if (setting_value > kRemovedGcodeSyntax)
        setting.value() = setting_value - 1;
}

void migrateRemovedRadialSyntax(fifojson& settings_group) {
    if (!settings_group.is_object())
        return;

    const std::string syntax_key = ORNL::Constants::PrinterSettings::MachineSetup::kSyntax.toStdString();
    auto setting = settings_group.find(syntax_key);
    if (setting == settings_group.end() || !setting.value().is_number_integer())
        return;

    const int setting_value = setting.value().get<int>();
    if (setting_value == kRemovedRadialSyntax || setting_value == kLegacyArcSpecialtiesSyntax)
        setting.value() = kArcSpecialtiesSyntax;
}

void migrateCylindricalSlicingSettings(fifojson& settings_group) {
    if (!settings_group.is_object())
        return;

    const std::string slicer_type_key = ORNL::Constants::ProfileSettings::Slicing::kSlicerType.toStdString();
    const std::string path_type_key = ORNL::Constants::ProfileSettings::Slicing::kCylindricalPathType.toStdString();
    const std::string radial_boundary_key =
        ORNL::Constants::ProfileSettings::Slicing::kRadialBoundaryHandling.toStdString();
    const std::string helical_boundary_key =
        ORNL::Constants::ProfileSettings::Slicing::kHelicalBoundaryHandling.toStdString();

    auto slicer_type = settings_group.find(slicer_type_key);
    const bool has_slicer_type = slicer_type != settings_group.end() && slicer_type.value().is_number_integer();
    const int old_slicer_type = has_slicer_type ? slicer_type.value().get<int>() : kPlanarSlicer;

    if (old_slicer_type == kLegacyRadialSlicer) {
        slicer_type.value() = kV8CylindricalSlicer;
        settings_group[path_type_key] = kRadialPathType;
    }
    else if (old_slicer_type == kLegacyHelicalSlicer) {
        slicer_type.value() = kV8CylindricalSlicer;
        settings_group[path_type_key] = kHelicalPathType;

        int helical_boundary = kClipBoundaryHandling;
        auto radial_boundary = settings_group.find(radial_boundary_key);
        const bool old_boundary_was_clip = radial_boundary == settings_group.end() ||
                                           !radial_boundary.value().is_number_integer() ||
                                           radial_boundary.value().get<int>() == kClipBoundaryHandling;
        auto legacy_helical_boundary = settings_group.find(kLegacyHelicalClippingMethod);
        if (old_boundary_was_clip && legacy_helical_boundary != settings_group.end() &&
            legacy_helical_boundary.value().is_number_integer()) {
            helical_boundary = legacy_helical_boundary.value().get<int>();
        }
        settings_group[helical_boundary_key] = helical_boundary;
    }
    else if (settings_group.find(path_type_key) == settings_group.end()) {
        auto legacy_helical_boundary = settings_group.find(kLegacyHelicalClippingMethod);
        if (legacy_helical_boundary != settings_group.end() && legacy_helical_boundary.value().is_number_integer())
            settings_group[helical_boundary_key] = legacy_helical_boundary.value();
    }

    settings_group.erase(kLegacyHelicalClippingMethod);
}
} // namespace

namespace ORNL {
void SettingsVersionControl::rollSettingsForward(double& version, fifojson& settings) {
    if (version < 1)
        pre_1_0To1_0(version, settings);
    if (version < 2) // all versions converted to Version 2.0
        pre_2_0To2_0(version, settings);
    if (version < 3)
        pre_3_0To3_0(version, settings);
    if (version < 4)
        pre_4_0To4_0(version, settings);
    if (version < 5)
        pre_5_0To5_0(version, settings);
    if (version < 6)
        pre_6_0To6_0(version, settings);
    if (version < 7)
        pre_7_0To7_0(version, settings);
    if (version < 8)
        pre_8_0To8_0(version, settings);
    if (version < 9)
        pre_9_0To9_0(version, settings);
}

void SettingsVersionControl::formatSettings(double version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedBy] = "ORNLSlicer";
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedOn] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = version;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLock] = "false";

    new_format[Constants::SettingFileStrings::kSettings] = settings;
    settings = new_format;
}

void SettingsVersionControl::pre_1_0To1_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedBy] = "";
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedOn] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 1.0;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLock] = "false";

    new_format[Constants::SettingFileStrings::kSettings] = settings;

    std::list<std::string> keys;
    for (auto& el : settings.items())
        keys.push_back(el.key());

    for (std::string key : keys) {
        // get iterator to old key; TODO: error handling if key is not present
        fifojson::iterator it = new_format[Constants::SettingFileStrings::kSettings].find(key);
        // create null value for new key and swap value from old key
        std::swap(new_format[Constants::SettingFileStrings::kSettings][key + "_0"], it.value());
        // delete value at old key (cheap, because the value is null after swap)
        new_format[Constants::SettingFileStrings::kSettings].erase(it);
    }

    settings = new_format;
}

void SettingsVersionControl::pre_2_0To2_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 2.0;

    fifojson settings_array = fifojson::array({});
    fifojson current_index_settings;
    int index = 0; // Last index denotes suffix
    for (auto& el : new_format[Constants::SettingFileStrings::kSettings].items()) {
        QString key_root = QString::fromStdString(el.key());
        int last_index = key_root.lastIndexOf(QRegularExpression("_\\d+")); // index of suffix
        if (last_index >= 0) {
            key_root.chop(key_root.size() - last_index);                               // remove suffix
            int key_suffix = key_root.right(key_root.size() - last_index - 1).toInt(); // get suffix
            // If suffix matches index, add to json
            if (key_suffix == index)
                current_index_settings[key_root.toStdString()] = el.value();
            // otherwise increment index and add current json to json array
            else {
                index++;
                settings_array.push_back(current_index_settings);
                // Empty current json to be filled again with next index items
                for (auto it : current_index_settings.items()) {
                    current_index_settings.erase(current_index_settings.begin(), current_index_settings.end());
                }
                current_index_settings[key_root.toStdString()] = el.value();
            }
        }
        else {
            current_index_settings[key_root.toStdString()] = el.value();
        }
    }
    settings_array.push_back(current_index_settings);

    new_format[Constants::SettingFileStrings::kSettings] = settings_array;
    version = 2.0;
    settings = new_format;
}

void SettingsVersionControl::pre_3_0To3_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 3.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value()) {
            removeV2Settings(settings_group);
            migrateIndexedSetting(settings_group, Constants::PrinterSettings::MachineSetup::kSyntax, kSyntaxV2ToV3);
            migrateIndexedSetting(settings_group, Constants::ProfileSettings::Slicing::kSlicerType, kSlicerTypeV2ToV3);
            addV3Settings(settings_group);
        }
    }

    version = 3.0;
    settings = new_format;
}

void SettingsVersionControl::pre_4_0To4_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 4.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value()) {
            migrateIndexedSetting(settings_group, Constants::ProfileSettings::Slicing::kSlicerType, kSlicerTypeV3ToV4);
        }
    }

    version = 4.0;
    settings = new_format;
}

void SettingsVersionControl::pre_5_0To5_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 5.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value()) {
            migrateIndexedSetting(settings_group, Constants::ProfileSettings::Skin::kPattern, kSkinPatternV4ToV5);
            migrateIndexedSetting(settings_group, Constants::ProfileSettings::Infill::kPattern, kInfillPatternV4ToV5);
        }
    }

    version = 5.0;
    settings = new_format;
}

void SettingsVersionControl::pre_6_0To6_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 6.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value())
            migrateRemovedGcodeSyntax(settings_group);
    }

    version = 6.0;
    settings = new_format;
}

void SettingsVersionControl::pre_7_0To7_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 7.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value())
            migrateRemovedRadialSyntax(settings_group);
    }

    version = 7.0;
    settings = new_format;
}

void SettingsVersionControl::pre_8_0To8_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 8.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value())
            migrateCylindricalSlicingSettings(settings_group);
    }

    version = 8.0;
    settings = new_format;
}

void SettingsVersionControl::pre_9_0To9_0(double& version, fifojson& settings) {
    QString dt = QDateTime::currentDateTime().toString();
    fifojson new_format = settings;
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kLastModified] = dt.toStdString();
    new_format[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kVersion] = 9.0;

    auto settings_array = new_format.find(Constants::SettingFileStrings::kSettings);
    if (settings_array != new_format.end() && settings_array.value().is_array()) {
        for (auto& settings_group : settings_array.value())
            migrateIndexedSetting(settings_group, Constants::ProfileSettings::Slicing::kSlicerType, kSlicerTypeV8ToV9);
    }

    version = 9.0;
    settings = new_format;
}
} // namespace ORNL
