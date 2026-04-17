#include "managers/preferences_manager.h"

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <QDir>
#include <QStandardPaths>
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
QSharedPointer<PreferencesManager> PreferencesManager::m_singleton = QSharedPointer<PreferencesManager>();

QSharedPointer<PreferencesManager> PreferencesManager::getInstance() {
    if (m_singleton.isNull()) {
        m_singleton.reset(new PreferencesManager());
    }
    return m_singleton;
}

PreferencesManager::PreferencesManager()
    : m_import_unit(mm), m_distance_unit(in), m_velocity_unit(in / s), m_acceleration_unit(in / s / s),
      m_density_unit(g / cm / cm / cm), m_angle_unit(deg), m_time_unit(s), m_temperature_unit(K), m_voltage_unit(V),
      m_mass_unit(kg), m_project_shift_preference(PreferenceChoice::kAsk),
      m_file_shift_preference(PreferenceChoice::kPerformAutomatically), m_align_preference(PreferenceChoice::kAsk),
      m_hide_travel_preference(false), m_hide_support_preference(false), m_use_true_widths_preference(true),
      m_themeName(ThemeName::kLightMode), m_theme(static_cast<int>(m_themeName)),
      m_rotation_unit(RotationUnit::kPitchRollYaw), m_dirty(false), m_is_maximized(false), m_window_size(-1, -1),
      m_window_pos(-1, -1), m_use_implicit_transforms(false), m_always_drop_parts(false), m_layer_lag(100),
      m_segment_lag(10) {
    m_hidden_settings["Printer"] = std::list<std::string>();
    m_hidden_settings["Material"] = std::list<std::string>();
    m_hidden_settings["Profile"] = std::list<std::string>();
    m_hidden_settings["Experimental"] = std::list<std::string>();
    m_tcp_port = 12345;
    m_tcp_server_autostart = false;
    m_step_connectivity = QVector<bool>(5, false);
    m_katana_tcp_ip = "127.0.0.1";
    m_katana_tcp_port = 12345;
}

QColor PreferencesManager::getVisualizationColor(VisualizationColors color) {
    return m_visualization_qcolors[VisualizationColorsName(color).toStdString()];
}

void PreferencesManager::setVisualizationColor(QString name, QColor value) {
    m_visualization_qcolors[name.toStdString()] = value;
}

QColor PreferencesManager::revertVisualizationColor(QString name) {
    int visualizationColorsLength = (int)VisualizationColors::Length;
    for (int i = 0; i < visualizationColorsLength; ++i) {
        VisualizationColors colorEnum = (VisualizationColors)i;
        if (VisualizationColorsName(colorEnum) == name) {
            m_visualization_qcolors[name.toStdString()] = VisualizationColorsDefaults(colorEnum);
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
    std::unordered_map<std::string, std::string> visualizationColorsHex) {
    m_visualization_qcolors.clear();
    int visualizationColorsLength = (int)VisualizationColors::Length;
    for (int i = 0; i < visualizationColorsLength; ++i) {
        VisualizationColors colorEnum = (VisualizationColors)i;
        m_visualization_qcolors[VisualizationColorsName(colorEnum).toStdString()] =
            VisualizationColorsDefaults(colorEnum);
    }

    for (const auto& color : visualizationColorsHex) {
        if (!visualizationColorsHex[color.first].empty()) {
            bool validColorHexStr;
            int val = QString::fromStdString(color.second).remove('#').toInt(&validColorHexStr, 16);
            if (validColorHexStr)
                m_visualization_qcolors[color.first] = QColor(val);
        }
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
        fifojson j = json::parse(preferences.toStdString());
        if (j.find("import_unit") != j.end())
            setImportUnit(j.value("import_unit", m_import_unit));
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
        m_file_shift_preference = j.value("file_shift", m_file_shift_preference);
        m_align_preference = j.value("align", m_align_preference);
        m_hide_travel_preference = j.value("hide_travel", m_hide_travel_preference);
        m_hide_support_preference = j.value("hide_support", m_hide_support_preference);
        m_is_maximized = j.value("is_window_maximized", m_is_maximized);
        if (j.find("window_size") != j.end())
            m_window_size = QSize(j["window_size"][0], j["window_size"][1]);
        if (j.find("window_pos") != j.end())
            m_window_pos = QPoint(j["window_pos"][0], j["window_pos"][1]);

        if (j.find("hidden_settings") != j.end())
            m_hidden_settings = j.at("hidden_settings").get<std::unordered_map<std::string, std::list<std::string>>>();

        m_rotation_unit = j.value("rotation", m_rotation_unit);

        if (j.find("invert_camera") != j.end())
            setInvertCamera(j["invert_camera"]);

        if (j.contains("always_drop_parts"))
            setShouldAlwaysDrop(j["always_drop_parts"]);

        if (j.contains("use_implicit_transforms"))
            setUseImplicitTransforms(j["use_implicit_transforms"]);

        if (j.contains("use_true_widths"))
            setUseTrueWidthsPreference(j["use_true_widths"]);

        std::unordered_map<std::string, std::string> visualizationColorsHex;
        if (j.find("visualization_colors") != j.end())
            visualizationColorsHex = j.at("visualization_colors").get<std::unordered_map<std::string, std::string>>();
        setDefaultVisualizationColors(visualizationColorsHex);

        if (j.find("tcp_server_settings") != j.end()) {
            m_tcp_port = j["tcp_server_settings"]["port"];
            m_tcp_server_autostart = j["tcp_server_settings"]["auto_start"];
            std::vector<bool> connectivities = j["tcp_server_settings"]["step_connectivity"];
            m_step_connectivity = QVector<bool>(connectivities.begin(), connectivities.end());
        }

        if (j.find("katana_server_settings") != j.end()) {
            setKatanaSendOutput(j["katana_server_settings"]["send_output"]);
            setKatanaTCPIp(j["katana_server_settings"]["ip"]);
            setKatanaTCPPort(j["katana_server_settings"]["port"]);
        }

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

    j["import_unit"] = m_import_unit;
    j["distance"] = m_distance_unit;
    j["velocity"] = m_velocity_unit;
    j["acceleration"] = m_acceleration_unit;
    j["density"] = m_density_unit;
    j["angle"] = m_angle_unit;
    j["time"] = m_time_unit;
    j["temperature"] = m_temperature_unit;
    j["voltage"] = m_voltage_unit;
    j["mass"] = m_mass_unit;
    j["shift"] = m_project_shift_preference;
    j["file_shift"] = m_file_shift_preference;
    j["align"] = m_align_preference;
    j["hide_travel"] = m_hide_travel_preference;
    j["hide_support"] = m_hide_support_preference;
    j["use_true_widths"] = m_use_true_widths_preference;
    j["hidden_settings"] = m_hidden_settings;
    j["rotation"] = m_rotation_unit;
    j["invert_camera"] = m_invert_camera;
    j["theme"] = m_themeName;
    j["is_window_maximized"] = m_is_maximized;
    j["window_size"] = {m_window_size.width(), m_window_size.height()};
    j["window_pos"] = {m_window_pos.x(), m_window_pos.y()};
    j["use_implicit_transforms"] = m_use_implicit_transforms;
    j["always_drop_parts"] = m_always_drop_parts;
    j["visualization_colors"] = getVisualizationHexColors();
    j["tcp_server_settings"]["port"] = m_tcp_port;
    j["tcp_server_settings"]["auto_start"] = m_tcp_server_autostart;
    j["tcp_server_settings"]["step_connectivity"] = m_step_connectivity;
    j["katana_server_settings"]["send_output"] = m_katana_send_output;
    j["katana_server_settings"]["ip"] = m_katana_tcp_ip;
    j["katana_server_settings"]["port"] = m_katana_tcp_port;
    j["layer_lag"] = m_layer_lag;
    j["segment_lag"] = m_segment_lag;

    return j;
}

Distance PreferencesManager::getImportUnit() { return m_import_unit; }

Distance PreferencesManager::getDistanceUnit() { return m_distance_unit; }

Velocity PreferencesManager::getVelocityUnit() { return m_velocity_unit; }

Acceleration PreferencesManager::getAccelerationUnit() { return m_acceleration_unit; }

Density PreferencesManager::getDensityUnit() { return m_density_unit; }

Angle PreferencesManager::getAngleUnit() { return m_angle_unit; }

Theme PreferencesManager::getTheme() { return m_theme; }

Time PreferencesManager::getTimeUnit() { return m_time_unit; }

Temperature PreferencesManager::getTemperatureUnit() { return m_temperature_unit; }

Voltage PreferencesManager::getVoltageUnit() { return m_voltage_unit; }

Mass PreferencesManager::getMassUnit() { return m_mass_unit; }

QString PreferencesManager::getDistanceUnitText() { return m_distance_unit.toString(); }

QString PreferencesManager::getVelocityUnitText() { return m_velocity_unit.toString(); }

QString PreferencesManager::getAccelerationUnitText() { return m_acceleration_unit.toString(); }

QString PreferencesManager::getDensityUnitText() { return m_density_unit.toString(); }

QString PreferencesManager::getAngleUnitText() { return m_angle_unit.toString(); }

QString PreferencesManager::getThemeText() { return toString(m_themeName); }

QString PreferencesManager::getTimeUnitText() { return m_time_unit.toString(); }

QString PreferencesManager::getTemperatureUnitText() { return m_temperature_unit.toString(); }

QString PreferencesManager::getVoltageUnitText() { return m_voltage_unit.toString(); }

QString PreferencesManager::getMassUnitText() { return m_mass_unit.toString(); }

PreferenceChoice PreferencesManager::getProjectShiftPreference() { return m_project_shift_preference; }

PreferenceChoice PreferencesManager::getFileShiftPreference() { return m_file_shift_preference; }

PreferenceChoice PreferencesManager::getAlignPreference() { return m_align_preference; }

bool PreferencesManager::getHideTravelPreference() { return m_hide_travel_preference; }

bool PreferencesManager::getHideSupportPreference() { return m_hide_support_preference; }

bool PreferencesManager::getUseTrueWidthsPreference() { return m_use_true_widths_preference; }

bool PreferencesManager::getWindowMaximizedPreference() { return m_is_maximized; }

QSize PreferencesManager::getWindowSizePreference() { return m_window_size; }

QPoint PreferencesManager::getWindowPosPreference() { return m_window_pos; }

RotationUnit PreferencesManager::getRotationUnit() { return m_rotation_unit; }

QString PreferencesManager::getRotationUnitText() {
    if (m_rotation_unit == RotationUnit::kPitchRollYaw)
        return Constants::Units::kPitchRollYaw;
    else // m_rotation_unit == RotationUnit::kXYZ
        return Constants::Units::kXYZ;
}

bool PreferencesManager::invertCamera() { return m_invert_camera; }

bool PreferencesManager::getUseImplicitTransforms() { return m_use_implicit_transforms; }

bool PreferencesManager::getAlwaysDropParts() { return m_always_drop_parts; }

int PreferencesManager::getLayerLag() { return m_layer_lag; }

int PreferencesManager::getSegmentLag() { return m_segment_lag; }

void PreferencesManager::setImportUnit(QString du) {
    try {
        setImportUnit(Distance::fromString(du));
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setImportUnit(Distance du) {
    Distance old = m_import_unit;
    m_import_unit = du;
    m_dirty = true;
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
    Distance old = m_distance_unit;
    m_distance_unit = d;
    m_dirty = true;
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
    Velocity old = m_velocity_unit;
    m_velocity_unit = v;
    m_dirty = true;
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
    Acceleration old = m_acceleration_unit;
    m_acceleration_unit = a;
    m_dirty = true;
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
    Density old = m_density_unit;
    m_density_unit = d;
    m_dirty = true;
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
    Angle old = m_angle_unit;
    m_angle_unit = a;
    m_dirty = true;
    emit angleUnitChanged(m_angle_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setTheme(QString theme) {
    ThemeName name = themeFromString(theme);
    int themeNum = static_cast<int>(name);
    m_theme.chooseTheme(themeNum);
    m_themeName = name;
    m_dirty = true;
    emit themeChanged();
}

void PreferencesManager::setTheme(ThemeName theme) {
    int themeNum = static_cast<int>(theme);
    m_theme.chooseTheme(themeNum);
    m_themeName = theme;
    m_dirty = true;
    emit themeChanged();
}

void PreferencesManager::setTimeUnit(QString t) {
    try {
        setTimeUnit(Time::fromString(t));
        m_dirty = true;
    } catch (UnknownUnitException e) { qWarning() << e.what(); }
}

void PreferencesManager::setTimeUnit(Time t) {
    Time old = m_time_unit;
    m_time_unit = t;
    m_dirty = true;
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
    Temperature old = m_temperature_unit;
    m_temperature_unit = t;
    m_dirty = true;
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
    Voltage old = m_voltage_unit;
    m_voltage_unit = v;
    m_dirty = true;
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
    Mass old = m_mass_unit;
    m_mass_unit = m;
    m_dirty = true;
    emit massUnitChanged(m_mass_unit, old);
    emit anyUnitChanged();
}

void PreferencesManager::setProjectShiftPreference(PreferenceChoice shift) {
    m_project_shift_preference = shift;
    m_dirty = true;
}

void PreferencesManager::setFileShiftPreference(PreferenceChoice shift) {
    m_file_shift_preference = shift;
    m_dirty = true;
}

void PreferencesManager::setAlignPreference(PreferenceChoice align) {
    m_align_preference = align;
    m_dirty = true;
}

void PreferencesManager::setHideTravelPreference(bool hide) {
    m_hide_travel_preference = hide;
    m_dirty = true;
}

void PreferencesManager::setHideSupportPreference(bool hide) {
    m_hide_support_preference = hide;
    m_dirty = true;
}

void PreferencesManager::setUseTrueWidthsPreference(bool use) {
    m_use_true_widths_preference = use;
    m_dirty = true;
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
    m_dirty = true;
}

void PreferencesManager::setUseImplicitTransforms(bool use) { m_use_implicit_transforms = use; }

void PreferencesManager::setShouldAlwaysDrop(bool should) { m_always_drop_parts = should; }

void PreferencesManager::setWindowMaximizedPreference(bool isMaximized) {
    m_is_maximized = isMaximized;
    m_dirty = true;
}

void PreferencesManager::setWindowSizePreference(QSize window_size) {
    m_window_size = window_size;
    m_dirty = true;
}

void PreferencesManager::setWindowPosPreference(QPoint window_pos) {
    m_window_pos = window_pos;
    m_dirty = true;
}

void PreferencesManager::setLayerLag(int lag) { m_layer_lag = lag; }

void PreferencesManager::setSegmentLag(int lag) { m_segment_lag = lag; }

bool PreferencesManager::isDirty() { return m_dirty; }

QList<QString> PreferencesManager::getHiddenSettings(QString panel) {
    std::list<std::string> tempList(m_hidden_settings[panel.toStdString()]);
    QList<QString> result;
    result.reserve(tempList.size());
    for (std::string str : tempList)
        result.append(QString::fromStdString(str));

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

void PreferencesManager::setStepConnectivity(StatusUpdateStepType type, bool toggle) {
    m_step_connectivity[(int)type] = toggle;
}

void PreferencesManager::setTCPServerPort(int port) { m_tcp_port = port; }

void PreferencesManager::setTcpServerAutoStart(bool start) { m_tcp_server_autostart = start; }

bool PreferencesManager::getStepConnectivity(StatusUpdateStepType type) { return m_step_connectivity[(int)type]; }

int PreferencesManager::getTCPServerPort() { return m_tcp_port; }

bool PreferencesManager::getTcpServerAutoStart() { return m_tcp_server_autostart; }

bool PreferencesManager::getKatanaSendOutput() { return m_katana_send_output; }

void PreferencesManager::setKatanaSendOutput(bool send) { m_katana_send_output = send; }

QString PreferencesManager::getKatanaTCPIp() { return m_katana_tcp_ip; }

void PreferencesManager::setKatanaTCPIp(QString ipAddress) { m_katana_tcp_ip = ipAddress; }

int PreferencesManager::getKatanaTCPPort() { return m_katana_tcp_port; }

void PreferencesManager::setKatanaTCPPort(int port) { m_katana_tcp_port = port; }
} // namespace ORNL
