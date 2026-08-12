#include "gcode/gcode_settings_importer.h"

#include <QFile>
#include <QHash>
#include <QIODevice>
#include <cmath>
#include <limits>

#include <qchar.h>
#include <qcontainerfwd.h>

#include "managers/settings/settings_manager.h"
#include "managers/settings/settings_version_control.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
constexpr double kIntegerTolerance              = 1e-9;
constexpr double kModifyFeedrateLayerTimeMethod = 1.0;

const QString kSettingsFooter          = "Settings Footer";
const QString kInfillMaximumPathLength = "infill_maximum_path_length";

QString jsonString(const fifojson& object, const std::string& key) {
    if (!object.contains(key) || !object.at(key).is_string()) return QString();

    return QString::fromStdString(object.at(key).get<std::string>());
}

QString settingType(const fifojson& master_entry) {
    return jsonString(master_entry, Constants::Settings::Master::kType);
}

QString settingDisplay(const QString& key, const fifojson& master) {
    const auto entry = master.find(key.toStdString());
    if (entry == master.end()) return key;

    const QString display = GcodeSettingsImporter::displayName(entry.value());
    return display.isEmpty() ? key : display;
}

bool numberValue(const fifojson& value, double& result) {
    if (!value.is_number()) return false;

    result = value.get<double>();
    return std::isfinite(result);
}

bool integerValue(const fifojson& value, int& result) {
    double number = 0.0;
    if (!numberValue(value, number)) return false;

    const double rounded = std::round(number);
    if (std::abs(number - rounded) > kIntegerTolerance || rounded < std::numeric_limits<int>::min() ||
        rounded > std::numeric_limits<int>::max()) {
        return false;
    }

    result = static_cast<int>(rounded);
    return true;
}

bool boolValue(const fifojson& value, bool& result) {
    if (value.is_boolean()) {
        result = value.get<bool>();
        return true;
    }

    if (value.is_number()) {
        double number = 0.0;
        if (!numberValue(value, number)) return false;

        if (std::abs(number) <= kIntegerTolerance) {
            result = false;
            return true;
        }

        if (std::abs(number - 1.0) <= kIntegerTolerance) {
            result = true;
            return true;
        }

        return false;
    }

    if (value.is_string()) {
        const QString text = QString::fromStdString(value.get<std::string>()).trimmed().toLower();
        if (text == "true" || text == "1") {
            result = true;
            return true;
        }

        if (text == "false" || text == "0") {
            result = false;
            return true;
        }
    }

    return false;
}

QString commentText(const QString& line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) return QString();

    if (trimmed.startsWith(';') || trimmed.startsWith('\'')) { return trimmed.mid(1).trimmed(); }

    if (trimmed.startsWith('(') && trimmed.endsWith(')') && trimmed.size() >= 2) {
        return trimmed.mid(1, trimmed.size() - 2).trimmed();
    }

    return QString();
}

int firstSpaceIndex(const QString& value) {
    for (int i = 0, end = value.size(); i < end; ++i) {
        if (value.at(i).isSpace()) return i;
    }

    return -1;
}

bool parseFooterSettings(QIODevice& contents, QHash<QString, QString>& raw_values, QStringList& warnings) {
    bool in_footer = false;

    while (!contents.atEnd()) {
        const QString text = commentText(QString::fromUtf8(contents.readLine()));
        if (text.isEmpty()) continue;

        if (!in_footer) {
            if (text.compare(kSettingsFooter, Qt::CaseInsensitive) == 0) in_footer = true;
            continue;
        }

        const int separator = firstSpaceIndex(text);
        if (separator < 0) {
            warnings.append("Ignoring footer line without a setting value: " + text);
            continue;
        }

        const QString key   = text.left(separator).trimmed();
        const QString value = text.mid(separator + 1).trimmed();

        if (key.isEmpty() || value.isEmpty()) {
            warnings.append("Ignoring malformed footer line: " + text);
            continue;
        }

        if (raw_values.contains(key)) warnings.append("Duplicate footer value for " + key + "; using the last value.");

        raw_values[key] = value;
    }

    return in_footer;
}

void migrateLegacyRawSettingKeys(QHash<QString, QString>& raw_values) {
    fifojson raw_settings = fifojson::object();
    for (auto raw = raw_values.constBegin(); raw != raw_values.constEnd(); ++raw) {
        raw_settings[raw.key().toStdString()] = raw.value().toStdString();
    }

    SettingsVersionControl::migrateLegacySettingKeys(raw_settings);

    raw_values.clear();
    for (const auto& item : raw_settings.items()) {
        if (item.value().is_string())
            raw_values[QString::fromStdString(item.key())] = QString::fromStdString(item.value().get<std::string>());
    }
}

bool parseRawValue(const QString& key, const fifojson& master_entry, const QString& raw_value, fifojson& parsed_value,
                   QString& error) {
    try {
        parsed_value = fifojson::parse(raw_value.toStdString());
        return true;
    } catch (const std::exception& e) {
        const QString type = settingType(master_entry);
        if (type == "string" || type == "multiline_text") {
            parsed_value = raw_value.toStdString();
            return true;
        }

        error = "Could not parse " + key + " value `" + raw_value + "` as JSON: " + e.what();
        return false;
    }
}

bool validateDoubleRange(const QString& key, const QString& type, double value, QString& error) {
    double minimum = 0.0;
    double maximum = static_cast<double>(std::numeric_limits<float>::max());

    if (type == "location") { minimum = static_cast<double>(std::numeric_limits<float>::lowest()); }
    else if (type == "unitless_float") {
        minimum = Constants::Limits::Minimums::kMinUnitlessFloat;
        maximum = Constants::Limits::Maximums::kMaxUnitlessFloat;
    }
    else if (type == "percentage") {
        maximum = 500.0;
    }
    else if (type == "percentage100") {
        maximum = 100.0;
    }
    else if (type == "rpm") {
        maximum = 9999.99;
    }
    else if (type == "deposition_rate") {
        maximum = 10000.0;
    }
    else if (type == "density") {
        maximum = 9999.9999;
    }
    else if (type == "angle") {
        minimum = Constants::Limits::Minimums::kMinAngle();
        maximum = Constants::Limits::Maximums::kMaxAngle();
    }
    else if (type == "temperature") { minimum = 0.0; }

    if (value < minimum || value > maximum) {
        error = key + " is outside the allowed " + type + " range.";
        return false;
    }

    return true;
}

bool isDoubleType(const QString& type) {
    return type == "location" || type == "distance" || type == "unitless_float" || type == "voltage" ||
           type == "speed" || type == "rpm" || type == "deposition_rate" || type == "accel" || type == "density" ||
           type == "ang_vel" || type == "time" || type == "percentage" || type == "percentage100" ||
           type == "temperature" || type == "angle" || type == "area";
}

double settingDouble(const fifojson& settings, const QString& key) {
    const auto value = settings.find(key.toStdString());
    if (value == settings.end()) return 0.0;

    double result = 0.0;
    if (numberValue(value.value(), result)) return result;

    bool bool_result = false;
    if (boolValue(value.value(), bool_result)) return bool_result ? 1.0 : 0.0;

    return 0.0;
}

bool settingBool(const fifojson& settings, const QString& key) {
    const auto value = settings.find(key.toStdString());
    if (value == settings.end()) return false;

    bool result = false;
    return boolValue(value.value(), result) && result;
}

int settingInt(const fifojson& settings, const QString& key) {
    const auto value = settings.find(key.toStdString());
    if (value == settings.end()) return 0;

    int result = 0;
    if (integerValue(value.value(), result)) return result;

    return static_cast<int>(settingDouble(settings, key));
}

bool jsonValuesMatch(const fifojson& actual, const fifojson& expected) {
    if (expected.is_boolean()) {
        bool actual_bool = false;
        return boolValue(actual, actual_bool) && actual_bool == expected.get<bool>();
    }

    if (expected.is_number()) {
        double actual_number   = 0.0;
        double expected_number = 0.0;
        return numberValue(actual, actual_number) && numberValue(expected, expected_number) &&
               std::abs(actual_number - expected_number) <= kIntegerTolerance;
    }

    if (expected.is_string() && actual.is_string()) return actual.get<std::string>() == expected.get<std::string>();

    return actual == expected;
}

bool dependencyIsActive(const fifojson& dependency, const fifojson& settings);

bool dependencyArrayIsActive(const fifojson& dependencies, bool require_all, const fifojson& settings) {
    if (!dependencies.is_array()) return true;

    if (require_all) {
        for (const auto& dependency : dependencies) {
            if (!dependencyIsActive(dependency, settings)) return false;
        }
        return true;
    }

    for (const auto& dependency : dependencies) {
        if (dependencyIsActive(dependency, settings)) return true;
    }
    return false;
}

bool dependencyIsActive(const fifojson& dependency, const fifojson& settings) {
    if (dependency.is_null()) return true;

    if (dependency.is_string()) return dependency.get<std::string>().empty();

    if (dependency.is_array()) return dependencyArrayIsActive(dependency, true, settings);

    if (!dependency.is_object()) return true;

    for (const auto& item : dependency.items()) {
        const QString key = QString::fromStdString(item.key());
        if (key == "AND") {
            if (!dependencyArrayIsActive(item.value(), true, settings)) return false;
        }
        else if (key == "OR") {
            if (!dependencyArrayIsActive(item.value(), false, settings)) return false;
        }
        else {
            const auto actual = settings.find(item.key());
            if (actual == settings.end() || !jsonValuesMatch(actual.value(), item.value())) return false;
        }
    }

    return true;
}

bool settingIsActive(const QString& key, const fifojson& master, const fifojson& settings) {
    const auto entry = master.find(key.toStdString());
    if (entry == master.end()) return false;

    const auto dependency = entry.value().find(Constants::Settings::Master::kDepends);
    return dependency == entry.value().end() || dependencyIsActive(dependency.value(), settings);
}

bool configuredRange(double minimum, double maximum) {
    return minimum != 0.0 || maximum != 0.0;
}

bool validRange(double minimum, double maximum) {
    return configuredRange(minimum, maximum) && minimum < maximum;
}

QString numberText(double value) {
    return QString::number(value, 'g', 6);
}

void addDimensionRangeError(const QString& min_key, const QString& max_key, const QString& min_label,
                            const QString& max_label, const fifojson& settings, const fifojson& master,
                            QStringList& errors) {
    if (!settingIsActive(min_key, master, settings) && !settingIsActive(max_key, master, settings)) return;

    const double minimum = settingDouble(settings, min_key);
    const double maximum = settingDouble(settings, max_key);
    if (configuredRange(minimum, maximum) && minimum >= maximum) {
        errors.append(min_label + " (" + numberText(minimum) + ") must be less than " + max_label + " (" +
                      numberText(maximum) + ").");
    }
}

void addBoundsError(const QString& key, const QString& min_key, const QString& max_key, const QString& axis,
                    const fifojson& settings, const fifojson& master, QStringList& errors) {
    if (!settingIsActive(key, master, settings)) return;

    const double minimum = settingDouble(settings, min_key);
    const double maximum = settingDouble(settings, max_key);
    const double value   = settingDouble(settings, key);
    if (validRange(minimum, maximum) && (value < minimum || value > maximum)) {
        errors.append(settingDisplay(key, master) + " (" + numberText(value) + ") is outside the " + axis +
                      " build volume range (" + numberText(minimum) + " to " + numberText(maximum) + ").");
    }
}

void validateActiveStaticSettings(const fifojson& settings, const fifojson& master, QStringList& errors) {
    for (const auto& item : master.items()) {
        const QString key = QString::fromStdString(item.key());
        if (!settingIsActive(key, master, settings)) continue;

        const auto value = settings.find(item.key());
        if (value == settings.end()) {
            errors.append("Missing active setting " + key + ".");
            continue;
        }

        fifojson normalized;
        QString error;
        if (!GcodeSettingsImporter::validateValue(key, item.value(), value.value(), normalized, error, true))
            errors.append(error);
    }
}

void validateDynamicSettings(const fifojson& settings, const fifojson& master, QStringList& errors) {
    addDimensionRangeError(PRS::Dimensions::kXMin, PRS::Dimensions::kXMax, "Minimum X", "Maximum X", settings, master,
                           errors);
    addDimensionRangeError(PRS::Dimensions::kYMin, PRS::Dimensions::kYMax, "Minimum Y", "Maximum Y", settings, master,
                           errors);
    addDimensionRangeError(PRS::Dimensions::kZMin, PRS::Dimensions::kZMax, "Minimum Z", "Maximum Z", settings, master,
                           errors);
    addDimensionRangeError(PRS::Dimensions::kWMin, PRS::Dimensions::kWMax, "Minimum W", "Maximum W", settings, master,
                           errors);

    const double min_extruder_speed   = settingDouble(settings, PRS::MachineSpeed::kMinExtruderSpeed);
    const double max_extruder_speed   = settingDouble(settings, PRS::MachineSpeed::kMaxExtruderSpeed);
    const bool has_min_extruder_speed = min_extruder_speed > 0.0;
    const bool has_max_extruder_speed = max_extruder_speed > 0.0;
    const bool invalid_extruder_range =
        has_min_extruder_speed && has_max_extruder_speed && min_extruder_speed > max_extruder_speed;
    const bool integer_deposition =
        settingInt(settings, PRS::MachineSetup::kMachineType) == static_cast<int>(MachineType::kFrictionStir);

    if (!integer_deposition && invalid_extruder_range) {
        errors.append("Minimum Extruder Speed (" + numberText(min_extruder_speed) +
                      ") is greater than Maximum Extruder Speed (" + numberText(max_extruder_speed) + ").");
    }

    const double min_xy_speed = settingDouble(settings, PRS::MachineSpeed::kMinXYSpeed);
    const double max_xy_speed = settingDouble(settings, PRS::MachineSpeed::kMaxXYSpeed);
    if (min_xy_speed > 0.0 && max_xy_speed > 0.0 && min_xy_speed > max_xy_speed) {
        errors.append("Minimum XY Speed (" + numberText(min_xy_speed) + ") is greater than Maximum XY Speed (" +
                      numberText(max_xy_speed) + ").");
    }

    for (const auto& item : master.items()) {
        const QString key = QString::fromStdString(item.key());
        if (!settingIsActive(key, master, settings)) continue;

        const QString type    = settingType(item.value());
        const double value    = settingDouble(settings, key);
        const QString display = settingDisplay(key, master);

        if (type == "deposition_rate" && integer_deposition &&
            std::abs(value - std::round(value)) > kIntegerTolerance) {
            errors.append(display + " must be a whole-number deposition value for Friction Stir.");
        }

        const bool rpm_based_deposition = type == "rpm" || (type == "deposition_rate" && !integer_deposition);
        if (rpm_based_deposition && key != PRS::MachineSpeed::kMinExtruderSpeed &&
            key != PRS::MachineSpeed::kMaxExtruderSpeed && !invalid_extruder_range) {
            if (has_min_extruder_speed && value < min_extruder_speed) {
                errors.append(display + " (" + numberText(value) + ") is below Minimum Extruder Speed (" +
                              numberText(min_extruder_speed) + ").");
            }
            if (has_max_extruder_speed && value > max_extruder_speed) {
                errors.append(display + " (" + numberText(value) + ") exceeds Maximum Extruder Speed (" +
                              numberText(max_extruder_speed) + ").");
            }
        }
    }

    addBoundsError(PRS::Dimensions::kPurgeX, PRS::Dimensions::kXMin, PRS::Dimensions::kXMax, "X", settings, master,
                   errors);
    addBoundsError(PRS::Dimensions::kPurgeY, PRS::Dimensions::kYMin, PRS::Dimensions::kYMax, "Y", settings, master,
                   errors);
    addBoundsError(PRS::Dimensions::kPurgeZ, PRS::Dimensions::kZMin, PRS::Dimensions::kZMax, "Z", settings, master,
                   errors);
    addBoundsError(PRS::Dimensions::kDoffingHeight, PRS::Dimensions::kWMin, PRS::Dimensions::kWMax, "W", settings,
                   master, errors);
    addBoundsError(PS::Perimeter::kEnableLeadInX, PRS::Dimensions::kXMin, PRS::Dimensions::kXMax, "X", settings, master,
                   errors);
    addBoundsError(PS::Perimeter::kEnableLeadInY, PRS::Dimensions::kYMin, PRS::Dimensions::kYMax, "Y", settings, master,
                   errors);

    if (settingIsActive(PS::Layer::kLayerHeight, master, settings) &&
        settingDouble(settings, PS::Layer::kLayerHeight) <= 0.0) {
        errors.append("Layer Height must be greater than zero.");
    }

    const QStringList bead_width_keys {PS::Layer::kBeadWidth,
                                       PS::Perimeter::kBeadWidth,
                                       PS::Inset::kBeadWidth,
                                       PS::Skeleton::kBeadWidth,
                                       PS::Skin::kBeadWidth,
                                       PS::Infill::kBeadWidth,
                                       MS::PlatformAdhesion::kRaftBeadWidth,
                                       MS::PlatformAdhesion::kBrimBeadWidth,
                                       MS::PlatformAdhesion::kSkirtBeadWidth};

    const double layer_height = settingDouble(settings, PS::Layer::kLayerHeight);
    for (const QString& key : bead_width_keys) {
        if (!settingIsActive(key, master, settings)) continue;

        const double value    = settingDouble(settings, key);
        const QString display = settingDisplay(key, master);
        if (value <= 0.0) { errors.append(display + " must be greater than zero."); }
        else if (layer_height > 0.0 && value < layer_height) {
            errors.append(display + " (" + numberText(value) + ") is smaller than Layer Height (" +
                          numberText(layer_height) + ").");
        }
    }

    if ((settingIsActive(PS::Infill::kMinPathLength, master, settings) ||
         settingIsActive(kInfillMaximumPathLength, master, settings)) &&
        settingDouble(settings, PS::Infill::kMinPathLength) > 0.0 &&
        settingDouble(settings, kInfillMaximumPathLength) > 0.0 &&
        settingDouble(settings, PS::Infill::kMinPathLength) > settingDouble(settings, kInfillMaximumPathLength)) {
        errors.append("Minimum Infill Path Length is greater than Maximum Infill Path Length.");
    }

    const QStringList lift_distance_keys {PS::Travel::kLiftHeight,           PS::Travel::kFinalLiftDistance,
                                          MS::SpiralLift::kLiftHeight,       MS::Slowdown::kPerimeterLiftDistance,
                                          MS::Slowdown::kInsetLiftDistance,  MS::Slowdown::kSkinLiftDistance,
                                          MS::Slowdown::kInfillLiftDistance, MS::Slowdown::kSkeletonLiftDistance,
                                          MS::TipWipe::kPerimeterLiftHeight, MS::TipWipe::kInsetLiftHeight,
                                          MS::TipWipe::kSkinLiftHeight,      MS::TipWipe::kInfillLiftHeight,
                                          MS::TipWipe::kSkeletonLiftHeight};

    const double z_min = settingDouble(settings, PRS::Dimensions::kZMin);
    const double z_max = settingDouble(settings, PRS::Dimensions::kZMax);
    for (const QString& key : lift_distance_keys) {
        if (!settingIsActive(key, master, settings)) continue;

        const double value = settingDouble(settings, key);
        if (value <= 0.0) continue;

        if (settingDouble(settings, PRS::MachineSpeed::kZSpeed) <= 0.0)
            errors.append(settingDisplay(key, master) + " requires Z Speed to be greater than zero.");

        if (validRange(z_min, z_max) && value > (z_max - z_min)) {
            errors.append(settingDisplay(key, master) + " exceeds the Z build volume range.");
        }
    }

    if (settingIsActive(PS::Travel::kMinTravelForLift, master, settings) &&
        settingDouble(settings, PS::Travel::kMinTravelForLift) > 0.0 &&
        settingDouble(settings, PS::Travel::kLiftHeight) <= 0.0) {
        errors.append("Minimum Travel For Lift is set, but Travel Lift Height is zero.");
    }

    const double fan_min_speed = settingDouble(settings, MS::Cooling::kMinSpeed);
    const double fan_max_speed = settingDouble(settings, MS::Cooling::kMaxSpeed);
    if ((settingIsActive(MS::Cooling::kMinSpeed, master, settings) ||
         settingIsActive(MS::Cooling::kMaxSpeed, master, settings)) &&
        fan_min_speed > fan_max_speed) {
        errors.append("Min Fan Speed (" + numberText(fan_min_speed) + ") is greater than Max Fan Speed (" +
                      numberText(fan_max_speed) + ").");
    }

    const bool force_layer_time = settingBool(settings, MS::Cooling::kForceMinLayerTime);
    const bool use_feedrate_layer_time =
        settingInt(settings, MS::Cooling::kForceMinLayerTimeMethod) == static_cast<int>(kModifyFeedrateLayerTimeMethod);
    const double min_layer_time = settingDouble(settings, MS::Cooling::kMinLayerTime);
    const double max_layer_time = settingDouble(settings, MS::Cooling::kMaxLayerTime);

    if (force_layer_time && min_layer_time <= 0.0)
        errors.append("Minimum Layer Time must be greater than zero when Force Min / Max Layer Time is enabled.");

    if (force_layer_time && use_feedrate_layer_time) {
        if (max_layer_time <= 0.0)
            errors.append(
                "Maximum Layer Time must be greater than zero when Modify Feedrate layer-time control is "
                "selected.");
        else if (min_layer_time > 0.0 && min_layer_time > max_layer_time)
            errors.append("Minimum Layer Time is greater than Maximum Layer Time.");

        if (settingDouble(settings, MS::Cooling::kExtruderScaleFactor) <= 0.0) {
            errors.append(
                "Extruder Scale Factor must be greater than zero when Modify Feedrate layer-time control is "
                "selected.");
        }
    }
}

double currentMasterVersion() {
    QFile versions(":/configs/versions.conf");
    if (!versions.open(QIODevice::ReadOnly)) return 0.0;

    try {
        const fifojson version_data = fifojson::parse(QString(versions.readAll()).toStdString());
        return version_data.at("master_version").get<double>();
    } catch (...) { return 0.0; }
}

QString limitedList(const QStringList& values, int maximum = 12) {
    if (values.size() <= maximum) return values.join("\n");

    QStringList limited = values.mid(0, maximum);
    limited.append(QString("...and %1 more.").arg(values.size() - maximum));
    return limited.join("\n");
}
}  // namespace

GcodeSettingsImporter::ImportResult GcodeSettingsImporter::importFile(
    const QString& gcode_path, bool use_defaults_for_missing, const MissingValueCallback& missing_value_callback) {
    ImportResult result;

    QFile gcode_file(gcode_path);
    if (!gcode_file.exists() || !gcode_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errors.append("Could not open G-Code file: " + gcode_path);
        return result;
    }

    QHash<QString, QString> raw_values;
    const bool footer_found = parseFooterSettings(gcode_file, raw_values, result.warnings);
    if (!footer_found) {
        result.errors.append("The selected G-Code file does not contain a Settings Footer.");
        return result;
    }

    if (raw_values.isEmpty()) {
        result.errors.append("The Settings Footer did not contain any setting values.");
        return result;
    }
    migrateLegacyRawSettingKeys(raw_values);

    const fifojson master = GSM->getMaster()->json();
    fifojson settings     = fifojson::object();

    for (auto raw = raw_values.constBegin(); raw != raw_values.constEnd(); ++raw) {
        if (master.find(raw.key().toStdString()) == master.end()) result.unknown_keys.append(raw.key());
    }

    for (const auto& item : master.items()) {
        const QString key            = QString::fromStdString(item.key());
        const fifojson& master_entry = item.value();
        fifojson candidate;
        bool value_was_prompted = false;

        const auto raw_value = raw_values.constFind(key);
        if (raw_value != raw_values.constEnd()) {
            QString parse_error;
            if (!parseRawValue(key, master_entry, raw_value.value(), candidate, parse_error)) {
                result.errors.append(parse_error);
                continue;
            }

            result.imported_keys.append(key);
        }
        else if (use_defaults_for_missing) {
            candidate = master_entry.at(Constants::Settings::Master::kDefault);
            result.defaulted_keys.append(key);
            result.missing_keys.append(key);
        }
        else {
            result.missing_keys.append(key);
            if (!missing_value_callback) {
                result.errors.append("Missing setting value for " + key + ".");
                continue;
            }

            const std::optional<fifojson> prompted_value = missing_value_callback(key, master_entry);
            if (!prompted_value.has_value()) {
                result.errors.append("Import canceled while prompting for missing setting " + key + ".");
                return result;
            }

            candidate          = prompted_value.value();
            value_was_prompted = true;
            result.prompted_keys.append(key);
        }

        fifojson normalized;
        QString validation_error;
        if (!validateValue(key, master_entry, candidate, normalized, validation_error, false)) {
            result.errors.append(validation_error);
            continue;
        }

        settings[item.key()] = normalized;
        if (value_was_prompted && !result.prompted_keys.contains(key)) result.prompted_keys.append(key);
    }

    if (!result.unknown_keys.isEmpty()) {
        result.warnings.append("Unknown footer settings were ignored:\n" + limitedList(result.unknown_keys));
    }

    if (!result.errors.isEmpty()) return result;

    validateActiveStaticSettings(settings, master, result.errors);
    validateDynamicSettings(settings, master, result.errors);
    if (!result.errors.isEmpty()) return result;

    fifojson settings_array = fifojson::array();
    settings_array.push_back(settings);
    SettingsVersionControl::formatSettings(currentMasterVersion(), settings_array);
    result.settings_file = settings_array;

    return result;
}

bool GcodeSettingsImporter::validateValue(const QString& key, const fifojson& master_entry, const fifojson& value,
                                          fifojson& normalized_value, QString& error, bool enforce_ranges) {
    const QString type = settingType(master_entry);
    if (type.isEmpty()) {
        error = "Missing type metadata for " + key + ".";
        return false;
    }

    if (type == "boolean") {
        bool result = false;
        if (!boolValue(value, result)) {
            error = key + " must be a boolean value.";
            return false;
        }

        normalized_value = result;
        return true;
    }

    if (type == "enumeration") {
        int index = -1;
        if (value.is_string()) {
            const QString option      = QString::fromStdString(value.get<std::string>()).trimmed();
            const QStringList options = settingOptions(master_entry);
            for (int i = 0, end = options.size(); i < end; ++i) {
                if (options[i].compare(option, Qt::CaseInsensitive) == 0) {
                    index = i;
                    break;
                }
            }
        }
        else if (!integerValue(value, index)) {
            error = key + " must be an enumeration index.";
            return false;
        }

        const QStringList options = settingOptions(master_entry);
        if (index < 0 || index >= options.size()) {
            error = key + " enumeration index " + QString::number(index) + " is outside the available options.";
            return false;
        }

        normalized_value = index;
        return true;
    }

    if (type == "number" || type == "positive_int" || type == "power") {
        int result = 0;
        if (!integerValue(value, result)) {
            error = key + " must be an integer value.";
            return false;
        }

        if (enforce_ranges && (type == "positive_int" || type == "power") && result < 1) {
            error = key + " must be greater than zero.";
            return false;
        }

        if (enforce_ranges && type == "number" && result < 0) {
            error = key + " must be zero or greater.";
            return false;
        }

        normalized_value = result;
        return true;
    }

    if (isDoubleType(type)) {
        double result = 0.0;
        if (!numberValue(value, result)) {
            error = key + " must be a numeric value.";
            return false;
        }

        if (enforce_ranges && !validateDoubleRange(key, type, result, error)) return false;

        normalized_value = result;
        return true;
    }

    if (type == "string" || type == "multiline_text") {
        if (!value.is_string()) {
            error = key + " must be a string value.";
            return false;
        }

        normalized_value = value;
        return true;
    }

    if (type == "numbered_list") {
        if (!value.is_array()) {
            error = key + " must be a list of strings.";
            return false;
        }

        for (const auto& entry : value) {
            if (!entry.is_string()) {
                error = key + " must be a list of strings.";
                return false;
            }
        }

        normalized_value = value;
        return true;
    }

    error = "Unsupported setting type `" + type + "` for " + key + ".";
    return false;
}

QString GcodeSettingsImporter::displayName(const fifojson& master_entry) {
    return jsonString(master_entry, Constants::Settings::Master::kDisplay);
}

QStringList GcodeSettingsImporter::settingOptions(const fifojson& master_entry) {
    QStringList options =
        jsonString(master_entry, Constants::Settings::Master::kOptions).split(',', Qt::SkipEmptyParts);
    for (QString& option : options) option = option.trimmed();

    return options;
}

}  // namespace ORNL
