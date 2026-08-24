#include "managers/preferences_manager.h"

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qfiledevice.h>
#include <qlist.h>
#include <qlogging.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qsize.h>
#include <qtmetamacros.h>

#include "exceptions/exceptions.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"
#include "utilities/theme_tool.h"

namespace ORNL {
namespace {
constexpr int kVisualizationColorMigrationVersion            = 1;
constexpr const char* kVisualizationColorMigrationVersionKey = "visualization_color_migration_version";

/*!
 * \brief Resolve a persisted visualization color name to its enum value.
 * \param name Persisted visualization color name.
 * \param color Output enum value when the name is recognized.
 * \return True if \p name maps to a known VisualizationColors entry.
 */
bool visualizationColorFromName(const std::string& name, VisualizationColors& color) {
    int visualizationColorsLength = (int)VisualizationColors::Length;
    for (int i = 0; i < visualizationColorsLength; ++i) {
        VisualizationColors colorEnum = (VisualizationColors)i;
        if (VisualizationColorsName(colorEnum).toStdString() == name) {
            color = colorEnum;
            return true;
        }
    }

    return false;
}

/*!
 * \brief Parse a persisted visualization color string.
 * \param colorText Hex color text, with or without a leading '#'.
 * \param valid Output validity flag from Qt's color parser.
 * \return Parsed opaque color when valid, otherwise an invalid QColor.
 */
QColor parseVisualizationColor(const std::string& colorText, bool& valid) {
    QString text = QString::fromStdString(colorText).trimmed();
    QColor color(text);

    if (!color.isValid() && !text.startsWith("#")) { color = QColor(QString("#") + text); }

    valid = color.isValid();
    if (valid) { color.setAlpha(255); }

    return color;
}

bool visualizationColorMatches(const std::string& colorText, const QColor& expectedColor) {
    bool validColor;
    QColor parsedColor = parseVisualizationColor(colorText, validColor);
    return validColor && parsedColor == expectedColor;
}

bool replaceStaleVisualizationColor(std::unordered_map<std::string, std::string>& visualizationColorsHex,
                                    VisualizationColors color, const std::vector<QColor>& staleColors) {
    const std::string name = VisualizationColorsName(color).toStdString();
    auto colorIt           = visualizationColorsHex.find(name);
    if (colorIt == visualizationColorsHex.end()) { return false; }

    for (const QColor& staleColor : staleColors) {
        if (visualizationColorMatches(colorIt->second, staleColor)) {
            colorIt->second = VisualizationColorsDefaults(color).name().toStdString();
            return true;
        }
    }

    return false;
}

bool migrateVisualizationColorDefaults(std::unordered_map<std::string, std::string>& visualizationColorsHex) {
    bool migrated = false;

    // Carry forward revised arc defaults for users who ran pre-release builds with these stale values persisted.
    migrated |= replaceStaleVisualizationColor(visualizationColorsHex, VisualizationColors::kInsetArc,
                                               {QColor(255, 179, 0, 255), QColor(102, 224, 255, 255)});
    migrated |= replaceStaleVisualizationColor(visualizationColorsHex, VisualizationColors::kPerimeterArc,
                                               {QColor(255, 0, 204, 255), QColor(0, 85, 255, 255)});

    return migrated;
}
}  // namespace

QSharedPointer<PreferencesManager> PreferencesManager::m_singleton = QSharedPointer<PreferencesManager>();

QSharedPointer<PreferencesManager> PreferencesManager::getInstance() {
    if (m_singleton.isNull()) { m_singleton.reset(new PreferencesManager()); }
    return m_singleton;
}

PreferencesManager::PreferencesManager()
    : m_import_unit(mm),
      m_distance_unit(in),
      m_velocity_unit(in / s),
      m_acceleration_unit(in / s / s),
      m_density_unit(g / cm / cm / cm),
      m_angle_unit(deg),
      m_time_unit(s),
      m_temperature_unit(K),
      m_voltage_unit(V),
      m_mass_unit(kg),
      m_project_shift_preference(PreferenceChoice::kAsk),
      m_file_shift_preference(PreferenceChoice::kPerformAutomatically),
      m_align_preference(PreferenceChoice::kAsk),
      m_hide_travel_preference(false),
      m_hide_support_preference(false),
      m_use_true_widths_preference(true),
      m_gcode_preview_mode_preference(GCodePreviewMode::kAuto),
      m_gcode_preview_vertex_threshold_preference(5000000),
      m_disabled_setting_visibility_preference(DisabledSettingVisibility::kGrey),
      m_warn_unsaved_project_on_close_preference(true),
      m_themeName(ThemeName::kLightMode),
      m_theme(static_cast<int>(m_themeName)),
      m_rotation_unit(RotationUnit::kPitchRollYaw),
      m_dirty(false),
      m_is_maximized(false),
      m_window_size(-1, -1),
      m_window_pos(-1, -1),
      m_use_implicit_transforms(false),
      m_always_drop_parts(false),
      m_layer_lag(100),
      m_segment_lag(10),
      m_visualization_color_migration_version(kVisualizationColorMigrationVersion) {
    m_hidden_settings["Printer"]      = std::list<std::string>();
    m_hidden_settings["Material"]     = std::list<std::string>();
    m_hidden_settings["Profile"]      = std::list<std::string>();
    m_hidden_settings["Experimental"] = std::list<std::string>();
    setDefaultVisualizationColors({});
}

QColor PreferencesManager::getVisualizationColor(VisualizationColors color) {
    std::string name = VisualizationColorsName(color).toStdString();
    return m_visualization_qcolors.try_emplace(name, VisualizationColorsDefaults(color)).first->second;
}

void PreferencesManager::setVisualizationColor(QString name, QColor value) {
    m_visualization_qcolors[name.toStdString()] = value;
    m_dirty                                     = true;
}

QColor PreferencesManager::revertVisualizationColor(QString name) {
    int visualizationColorsLength = (int)VisualizationColors::Length;
    for (int i = 0; i < visualizationColorsLength; ++i) {
        VisualizationColors colorEnum = (VisualizationColors)i;
        if (VisualizationColorsName(colorEnum) == name) {
            m_visualization_qcolors[name.toStdString()] = VisualizationColorsDefaults(colorEnum);
            m_dirty                                     = true;
            break;
        }
    }

    return m_visualization_qcolors[name.toStdString()];
}

bool PreferencesManager::isDefaultVisualizationColor(QString name) {
    int visualizationColorsLength = (int)VisualizationColors::Length;
    for (int i = 0; i < visualizationColorsLength; ++i) {
        VisualizationColors colorEnum = (VisualizationColors)i;
        if (VisualizationColorsName(colorEnum) == name) {
            return m_visualization_qcolors[name.toStdString()] == VisualizationColorsDefaults(colorEnum);
        }
    }

    return false;
}

std::map<std::string, QColor> PreferencesManager::getVisualizationColors() {
    std::map<std::string, QColor> visualizationColors(m_visualization_qcolors.begin(), m_visualization_qcolors.end());
    return visualizationColors;
}

std::map<std::string, std::string> PreferencesManager::getVisualizationHexColors() {
    std::unordered_map<std::string, std::string> visualizationColorsHex;
    for (const auto& color : m_visualization_qcolors)
        visualizationColorsHex[color.first] = color.second.name().toStdString();

    std::map<std::string, std::string> visualizationColors(visualizationColorsHex.begin(),
                                                           visualizationColorsHex.end());
    return visualizationColors;
}

void PreferencesManager::setDefaultVisualizationColors(
    const std::unordered_map<std::string, std::string>& visualizationColorsHex) {
    std::unordered_map<std::string, std::string> migratedVisualizationColorsHex = visualizationColorsHex;
    if (m_visualization_color_migration_version < kVisualizationColorMigrationVersion) {
        migrateVisualizationColorDefaults(migratedVisualizationColorsHex);
        m_visualization_color_migration_version = kVisualizationColorMigrationVersion;
        m_dirty                                 = true;
    }

    m_visualization_qcolors.clear();
    int visualizationColorsLength = (int)VisualizationColors::Length;
    for (int i = 0; i < visualizationColorsLength; ++i) {
        VisualizationColors colorEnum = (VisualizationColors)i;
        m_visualization_qcolors[VisualizationColorsName(colorEnum).toStdString()] =
            VisualizationColorsDefaults(colorEnum);
    }

    for (const auto& color : migratedVisualizationColorsHex) {
        VisualizationColors colorEnum;
        if (!visualizationColorFromName(color.first, colorEnum)) { continue; }

        if (color.second.empty()) {
            m_dirty = true;
            continue;
        }

        bool validColor;
        QColor parsedColor = parseVisualizationColor(color.second, validColor);
        if (!validColor) {
            m_dirty = true;
            continue;
        }

        m_visualization_qcolors[color.first] = parsedColor;
    }
}

void PreferencesManager::importPreferences(QString filepath) {
    if (filepath.isEmpty()) {
        filepath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/app.preferences";
    }

    QFile file(filepath);
    if (file.exists()) {
        file.open(QIODevice::ReadOnly);
        QString preferences = file.readAll();
        fifojson j          = json::parse(preferences.toStdString());
        if (j.find("import_unit") != j.end()) setImportUnit(j.value("import_unit", m_import_unit));
        setDistanceUnit(j.value("distance", m_distance_unit));
        setVelocityUnit(j.value("velocity", m_velocity_unit));
        setAccelerationUnit(j.value("acceleration", m_acceleration_unit));
        setDensityUnit(j.value("density", m_density_unit));
        setAngleUnit(j.value("angle", m_angle_unit));
        setTimeUnit(j.value("time", m_time_unit));
        setTemperatureUnit(j.value("temperature", m_temperature_unit));
        setVoltageUnit(j.value("voltage", m_voltage_unit));
        setMassUnit(j.value("mass", m_mass_unit));
        setTheme(j.value("theme", m_themeName));
        setLayerLag(j.value("layer_lag", m_layer_lag));
        setSegmentLag(j.value("segment_lag", m_segment_lag));
        m_project_shift_preference = j.value("shift", m_project_shift_preference);
        m_file_shift_preference    = j.value("file_shift", m_file_shift_preference);
        m_align_preference         = j.value("align", m_align_preference);
        m_hide_travel_preference   = j.value("hide_travel", m_hide_travel_preference);
        m_hide_support_preference  = j.value("hide_support", m_hide_support_preference);
        m_is_maximized             = j.value("is_window_maximized", m_is_maximized);
        if (j.find("window_size") != j.end()) m_window_size = QSize(j["window_size"][0], j["window_size"][1]);
        if (j.find("window_pos") != j.end()) m_window_pos = QPoint(j["window_pos"][0], j["window_pos"][1]);

        if (j.find("hidden_settings") != j.end())
            m_hidden_settings = j.at("hidden_settings").get<std::unordered_map<std::string, std::list<std::string>>>();

        m_rotation_unit = j.value("rotation", m_rotation_unit);

        if (j.find("invert_camera") != j.end()) setInvertCamera(j["invert_camera"]);

        if (j.contains("always_drop_parts")) setShouldAlwaysDrop(j["always_drop_parts"]);

        if (j.contains("use_implicit_transforms")) setUseImplicitTransforms(j["use_implicit_transforms"]);

        if (j.contains("use_true_widths")) setUseTrueWidthsPreference(j["use_true_widths"]);

        if (j.contains("gcode_preview_mode")) setGCodePreviewModePreference(j["gcode_preview_mode"].get<int>());

        if (j.contains("gcode_preview_vertex_threshold"))
            setGCodePreviewVertexThresholdPreference(j["gcode_preview_vertex_threshold"].get<int>());

        if (j.contains("disabled_setting_visibility"))
            setDisabledSettingVisibilityPreference(j["disabled_setting_visibility"].get<int>());

        if (j.contains("warn_unsaved_project_on_close"))
            setWarnUnsavedProjectOnClosePreference(j["warn_unsaved_project_on_close"]);

        m_visualization_color_migration_version = j.value(kVisualizationColorMigrationVersionKey, 0);

        std::unordered_map<std::string, std::string> visualizationColorsHex;
        if (j.find("visualization_colors") != j.end())
            visualizationColorsHex = j.at("visualization_colors").get<std::unordered_map<std::string, std::string>>();
        setDefaultVisualizationColors(visualizationColorsHex);

        file.close();
    }
    else
        m_dirty = true;
}

void PreferencesManager::exportPreferences(QString filepath) {
    if (filepath.isEmpty()) {
        filepath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/app.preferences";
    }

    fifojson j = this->json();

    QFile file(filepath);
    file.open(QIODevice::WriteOnly);
    file.write(j.dump(4).c_str());
    file.close();
    m_dirty = false;
}

fifojson PreferencesManager::json() {
    fifojson j;

    j["import_unit"]                          = m_import_unit;
    j["distance"]                             = m_distance_unit;
    j["velocity"]                             = m_velocity_unit;
    j["acceleration"]                         = m_acceleration_unit;
    j["density"]                              = m_density_unit;
    j["angle"]                                = m_angle_unit;
    j["time"]                                 = m_time_unit;
    j["temperature"]                          = m_temperature_unit;
    j["voltage"]                              = m_voltage_unit;
    j["mass"]                                 = m_mass_unit;
    j["shift"]                                = m_project_shift_preference;
    j["file_shift"]                           = m_file_shift_preference;
    j["align"]                                = m_align_preference;
    j["hide_travel"]                          = m_hide_travel_preference;
    j["hide_support"]                         = m_hide_support_preference;
    j["use_true_widths"]                      = m_use_true_widths_preference;
    j["gcode_preview_mode"]                   = static_cast<int>(m_gcode_preview_mode_preference);
    j["gcode_preview_vertex_threshold"]       = m_gcode_preview_vertex_threshold_preference;
    j["disabled_setting_visibility"]          = static_cast<int>(m_disabled_setting_visibility_preference);
    j["warn_unsaved_project_on_close"]        = m_warn_unsaved_project_on_close_preference;
    j["hidden_settings"]                      = m_hidden_settings;
    j["rotation"]                             = m_rotation_unit;
    j["invert_camera"]                        = m_invert_camera;
    j["theme"]                                = m_themeName;
    j["is_window_maximized"]                  = m_is_maximized;
    j["window_size"]                          = {m_window_size.width(), m_window_size.height()};
    j["window_pos"]                           = {m_window_pos.x(), m_window_pos.y()};
    j["use_implicit_transforms"]              = m_use_implicit_transforms;
    j["always_drop_parts"]                    = m_always_drop_parts;
    j["visualization_colors"]                 = getVisualizationHexColors();
    j[kVisualizationColorMigrationVersionKey] = m_visualization_color_migration_version;
    j["layer_lag"]                            = m_layer_lag;
    j["segment_lag"]                          = m_segment_lag;

    return j;
}

Distance PreferencesManager::getImportUnit() {
    return m_import_unit;
}

Distance PreferencesManager::getDistanceUnit() {
    return m_distance_unit;
}

Velocity PreferencesManager::getVelocityUnit() {
    return m_velocity_unit;
}

Acceleration PreferencesManager::getAccelerationUnit() {
    return m_acceleration_unit;
}

Density PreferencesManager::getDensityUnit() {
    return m_density_unit;
}

Angle PreferencesManager::getAngleUnit() {
    return m_angle_unit;
}

Theme PreferencesManager::getTheme() {
    return m_theme;
}

Time PreferencesManager::getTimeUnit() {
    return m_time_unit;
}

Temperature PreferencesManager::getTemperatureUnit() {
    return m_temperature_unit;
}

Voltage PreferencesManager::getVoltageUnit() {
    return m_voltage_unit;
}

Mass PreferencesManager::getMassUnit() {
    return m_mass_unit;
}

QString PreferencesManager::getDistanceUnitText() {
    return m_distance_unit.toString();
}

QString PreferencesManager::getVelocityUnitText() {
    return m_velocity_unit.toString();
}

QString PreferencesManager::getAccelerationUnitText() {
    return m_acceleration_unit.toString();
}

QString PreferencesManager::getDensityUnitText() {
    return m_density_unit.toString();
}

QString PreferencesManager::getAngleUnitText() {
    return m_angle_unit.toString();
}

QString PreferencesManager::getThemeText() {
    return toString(m_themeName);
}

QString PreferencesManager::getTimeUnitText() {
    return m_time_unit.toString();
}

QString PreferencesManager::getTemperatureUnitText() {
    return m_temperature_unit.toString();
}

QString PreferencesManager::getVoltageUnitText() {
    return m_voltage_unit.toString();
}

QString PreferencesManager::getMassUnitText() {
    return m_mass_unit.toString();
}

PreferenceChoice PreferencesManager::getProjectShiftPreference() {
    return m_project_shift_preference;
}

PreferenceChoice PreferencesManager::getFileShiftPreference() {
    return m_file_shift_preference;
}

PreferenceChoice PreferencesManager::getAlignPreference() {
    return m_align_preference;
}

bool PreferencesManager::getHideTravelPreference() {
    return m_hide_travel_preference;
}

bool PreferencesManager::getHideSupportPreference() {
    return m_hide_support_preference;
}

bool PreferencesManager::getUseTrueWidthsPreference() {
    return m_use_true_widths_preference;
}

GCodePreviewMode PreferencesManager::getGCodePreviewModePreference() {
    return m_gcode_preview_mode_preference;
}

int PreferencesManager::getGCodePreviewVertexThresholdPreference() {
    return m_gcode_preview_vertex_threshold_preference;
}

DisabledSettingVisibility PreferencesManager::getDisabledSettingVisibilityPreference() {
    return m_disabled_setting_visibility_preference;
}

bool PreferencesManager::getWarnUnsavedProjectOnClosePreference() {
    return m_warn_unsaved_project_on_close_preference;
}

bool PreferencesManager::getWindowMaximizedPreference() {
    return m_is_maximized;
}

QSize PreferencesManager::getWindowSizePreference() {
    return m_window_size;
}

QPoint PreferencesManager::getWindowPosPreference() {
    return m_window_pos;
}

RotationUnit PreferencesManager::getRotationUnit() {
    return m_rotation_unit;
}

QString PreferencesManager::getRotationUnitText() {
    if (m_rotation_unit == RotationUnit::kPitchRollYaw)
        return Constants::Units::kPitchRollYaw;
    else  // m_rotation_unit == RotationUnit::kXYZ
        return Constants::Units::kXYZ;
}

bool PreferencesManager::invertCamera() {
    return m_invert_camera;
}

bool PreferencesManager::getUseImplicitTransforms() {
    return m_use_implicit_transforms;
}

bool PreferencesManager::getAlwaysDropParts() {
    return m_always_drop_parts;
}

int PreferencesManager::getLayerLag() {
    return m_layer_lag;
}

int PreferencesManager::getSegmentLag() {
    return m_segment_lag;
}

void PreferencesManager::setImportUnit(QString du) {
    try {
        setImportUnit(Distance::fromString(du));
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setImportUnit(Distance du) {
    Distance old  = m_import_unit;
    m_import_unit = du;
    m_dirty       = true;
    emit importUnitChanged(m_import_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setDistanceUnit(QString du) {
    try {
        setDistanceUnit(Distance::fromString(du));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setDistanceUnit(Distance d) {
    Distance old    = m_distance_unit;
    m_distance_unit = d;
    m_dirty         = true;
    emit distanceUnitChanged(m_distance_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setVelocityUnit(QString vu) {
    try {
        setVelocityUnit(Velocity::fromString(vu));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setVelocityUnit(Velocity v) {
    Velocity old    = m_velocity_unit;
    m_velocity_unit = v;
    m_dirty         = true;
    emit velocityUnitChanged(m_velocity_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setAccelerationUnit(QString au) {
    try {
        setAccelerationUnit(Acceleration::fromString(au));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setAccelerationUnit(Acceleration a) {
    Acceleration old    = m_acceleration_unit;
    m_acceleration_unit = a;
    m_dirty             = true;
    emit accelerationUnitChanged(m_acceleration_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setDensityUnit(QString du) {
    try {
        setDensityUnit(Density::fromString(du));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setDensityUnit(Density d) {
    Density old    = m_density_unit;
    m_density_unit = d;
    m_dirty        = true;
    emit densityUnitChanged(m_density_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setAngleUnit(QString au) {
    try {
        setAngleUnit(Angle::fromString(au));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setAngleUnit(Angle a) {
    Angle old    = m_angle_unit;
    m_angle_unit = a;
    m_dirty      = true;
    emit angleUnitChanged(m_angle_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setTheme(QString theme) {
    ThemeName name = themeFromString(theme);
    int themeNum   = static_cast<int>(name);
    m_theme.chooseTheme(themeNum);
    m_themeName = name;
    m_dirty     = true;
    emit themeChanged();
}

void PreferencesManager::setTheme(ThemeName theme) {
    int themeNum = static_cast<int>(theme);
    m_theme.chooseTheme(themeNum);
    m_themeName = theme;
    m_dirty     = true;
    emit themeChanged();
}

void PreferencesManager::setTimeUnit(QString t) {
    try {
        setTimeUnit(Time::fromString(t));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setTimeUnit(Time t) {
    Time old    = m_time_unit;
    m_time_unit = t;
    m_dirty     = true;
    emit timeUnitChanged(m_time_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setTemperatureUnit(QString t) {
    try {
        setTemperatureUnit(Temperature::fromString(t));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setTemperatureUnit(Temperature t) {
    Temperature old    = m_temperature_unit;
    m_temperature_unit = t;
    m_dirty            = true;
    emit temperatureUnitChanged(m_temperature_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setVoltageUnit(QString v) {
    try {
        setVoltageUnit(Voltage::fromString(v));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setVoltageUnit(Voltage v) {
    Voltage old    = m_voltage_unit;
    m_voltage_unit = v;
    m_dirty        = true;
    emit voltageUnitChanged(m_voltage_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setMassUnit(QString m) {
    try {
        setMassUnit(Mass::fromString(m));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setMassUnit(Mass m) {
    Mass old    = m_mass_unit;
    m_mass_unit = m;
    m_dirty     = true;
    emit massUnitChanged(m_mass_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setProjectShiftPreference(PreferenceChoice shift) {
    m_project_shift_preference = shift;
    m_dirty                    = true;
}

void PreferencesManager::setFileShiftPreference(PreferenceChoice shift) {
    m_file_shift_preference = shift;
    m_dirty                 = true;
}

void PreferencesManager::setAlignPreference(PreferenceChoice align) {
    m_align_preference = align;
    m_dirty            = true;
}

void PreferencesManager::setHideTravelPreference(bool hide) {
    m_hide_travel_preference = hide;
    m_dirty                  = true;
}

void PreferencesManager::setHideSupportPreference(bool hide) {
    m_hide_support_preference = hide;
    m_dirty                   = true;
}

void PreferencesManager::setUseTrueWidthsPreference(bool use) {
    m_use_true_widths_preference = use;
    m_dirty                      = true;
}

void PreferencesManager::setGCodePreviewModePreference(GCodePreviewMode mode) {
    switch (mode) {
        case GCodePreviewMode::kAuto:
        case GCodePreviewMode::kTrueWidths:
        case GCodePreviewMode::kThinLines:
            m_gcode_preview_mode_preference = mode;
            break;
        default:
            m_gcode_preview_mode_preference = GCodePreviewMode::kAuto;
            break;
    }

    m_dirty = true;
}

void PreferencesManager::setGCodePreviewModePreference(int mode) {
    setGCodePreviewModePreference(static_cast<GCodePreviewMode>(mode));
}

void PreferencesManager::setGCodePreviewVertexThresholdPreference(int threshold) {
    m_gcode_preview_vertex_threshold_preference = std::max(0, threshold);
    m_dirty                                     = true;
}

void PreferencesManager::setDisabledSettingVisibilityPreference(DisabledSettingVisibility visibility) {
    switch (visibility) {
        case DisabledSettingVisibility::kGrey:
        case DisabledSettingVisibility::kHide:
            m_disabled_setting_visibility_preference = visibility;
            break;
        default:
            m_disabled_setting_visibility_preference = DisabledSettingVisibility::kGrey;
            break;
    }

    m_dirty = true;
    emit disabledSettingVisibilityChanged();
}

void PreferencesManager::setDisabledSettingVisibilityPreference(int visibility) {
    setDisabledSettingVisibilityPreference(static_cast<DisabledSettingVisibility>(visibility));
}

void PreferencesManager::setWarnUnsavedProjectOnClosePreference(bool warn) {
    m_warn_unsaved_project_on_close_preference = warn;
    m_dirty                                    = true;
}

void PreferencesManager::setRotationUnit(QString unit) {
    if (unit == Constants::Units::kPitchRollYaw)
        m_rotation_unit = RotationUnit::kPitchRollYaw;
    else if (unit == Constants::Units::kXYZ)
        m_rotation_unit = RotationUnit::kXYZ;

    m_dirty = true;
    emit rotationUnitChanged(m_rotation_unit);
}

void PreferencesManager::setInvertCamera(bool invert) {
    m_invert_camera = invert;
    m_dirty         = true;
}

void PreferencesManager::setUseImplicitTransforms(bool use) {
    m_use_implicit_transforms = use;
}

void PreferencesManager::setShouldAlwaysDrop(bool should) {
    m_always_drop_parts = should;
}

void PreferencesManager::setWindowMaximizedPreference(bool isMaximized) {
    m_is_maximized = isMaximized;
    m_dirty        = true;
}

void PreferencesManager::setWindowSizePreference(QSize window_size) {
    m_window_size = window_size;
    m_dirty       = true;
}

void PreferencesManager::setWindowPosPreference(QPoint window_pos) {
    m_window_pos = window_pos;
    m_dirty      = true;
}

void PreferencesManager::setLayerLag(int lag) {
    m_layer_lag = lag;
}

void PreferencesManager::setSegmentLag(int lag) {
    m_segment_lag = lag;
}

bool PreferencesManager::isDirty() {
    return m_dirty;
}

QList<QString> PreferencesManager::getHiddenSettings(QString panel) {
    std::list<std::string> tempList(m_hidden_settings[panel.toStdString()]);
    QList<QString> result;
    result.reserve(tempList.size());
    for (std::string str : tempList) result.append(QString::fromStdString(str));

    return result;
}

void PreferencesManager::addHiddenSetting(QString panel, QString setting) {
    m_hidden_settings[panel.toStdString()].push_back(setting.toStdString());
    m_dirty = true;
}

void PreferencesManager::removeHiddenSetting(QString panel, QString setting) {
    std::string stdSetting = setting.toStdString();
    m_hidden_settings[panel.toStdString()].remove_if([stdSetting](std::string str) { return str == stdSetting; });
    m_dirty = true;
}

bool PreferencesManager::isSettingHidden(QString panel, QString setting) {
    std::list<std::string> settingList = m_hidden_settings[panel.toStdString()];
    return std::find(settingList.begin(), settingList.end(), setting.toStdString()) != settingList.end();
}
}  // namespace ORNL
