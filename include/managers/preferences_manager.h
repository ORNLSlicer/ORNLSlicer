#pragma once

#include "QApplication"
#include "QObject"
#include "QPair"
#include "QStyle"
#include "units/unit.h"
#include "utilities/enums.h"
#include "utilities/theme_tool.h"

namespace ORNL {
/*!
 * \class PreferencesManager
 * \brief Manager for preferences
 */
class PreferencesManager : public QObject {
    Q_OBJECT

  public:
    //! \brief Returns the singleton
    static QSharedPointer<PreferencesManager> getInstance();

    //! \brief import preferences from file
    void importPreferences(QString filepath = "");

    //! \brief export preferences to file
    void exportPreferences(QString filepath = "");

    //! \brief Get the json from the preferences
    fifojson json();

    //! \brief gets the unit that is used to scale models when imported
    //! \return a unit
    Distance getImportUnit();

    //! \brief Returns the unit used for distance/length
    Distance getDistanceUnit();

    //! \brief Returns the unit used for velocity
    Velocity getVelocityUnit();

    //! \brief Returns the unit used for acceleration
    Acceleration getAccelerationUnit();

    //! \brief Returns the unit used for density
    Density getDensityUnit();

    //! \brief Returns the unit used for angle
    Angle getAngleUnit();

    //! \brief Returns the current theme
    Theme getTheme();

    //! \brief Returns the unit used for time
    Time getTimeUnit();

    //! \brief Returns the unit used for temperature
    Temperature getTemperatureUnit();

    //! \brief Returns the unit used for voltage
    Voltage getVoltageUnit();

    //! \brief Returns the unit used for mass
    Mass getMassUnit();

    //! \brief Returns the text for the unit used for distance/length
    QString getDistanceUnitText();

    //! \brief Returns the text for the unit used for velocity
    QString getVelocityUnitText();

    //! \brief Returns the text for the unit used for acceleration
    QString getAccelerationUnitText();

    //! \brief Returns the text for the unit used for density
    QString getDensityUnitText();

    //! \brief Returns the text for the unit used for angle
    QString getAngleUnitText();

    //! \brief Returns the text for the name of the current theme
    QString getThemeText();

    //! \brief Returns the text for the unit used for time
    QString getTimeUnitText();

    //! \brief Return the text used for the temperature
    QString getTemperatureUnitText();

    //! \brief Return the text used for the voltage
    QString getVoltageUnitText();

    //! \brief Return the text used for the unit used for mass
    QString getMassUnitText();

    //! \brief Return the user preference for shifting when loading projects
    PreferenceChoice getProjectShiftPreference();

    //! \brief Return the user preference for shifting when loading objects
    PreferenceChoice getFileShiftPreference();

    //! \brief Return the user preference for aligning when slice starts
    PreferenceChoice getAlignPreference();

    //! \brief Return the user preference for hiding travels during gcode visualization
    bool getHideTravelPreference();

    //! \brief Return the user preference for hiding travels during gcode visualization
    bool getHideSupportPreference();

    //! \brief gets if true bead widths should be use when visualising g-code
    //! \return if true widths should be used
    bool getUseTrueWidthsPreference();

    //! \brief Return the window preference for maximization
    bool getWindowMaximizedPreference();

    //! \brief Return the window preference for window size
    QSize getWindowSizePreference();

    //! \brief Return the window preference for window position
    QPoint getWindowPosPreference();

    //! \brief Return the user preference for rotation units
    RotationUnit getRotationUnit();

    //! \brief Return the user preference for rotation unit text
    QString getRotationUnitText();

    //! \brief should the camera rotation be inverted
    //! \return if camera should be inverted
    bool invertCamera();

    //! \brief gets if implicit part translation should be used when a model is imported (ie where the part is located
    //! about (0,0,0) in CAD space) \return is implicit transforms should be used
    bool getUseImplicitTransforms();

    //! \brief gets if a part should always be forced (dropped) to the floor after moving it
    //! \return if parts should always be dropped
    bool getAlwaysDropParts();

    //! \brief Get the lag between layers
    //! \return lag in milliseconds
    int getLayerLag();

    //! \brief Get the lag between segments
    //! \return lag in milliseconds
    int getSegmentLag();

    //! \brief Checks if any preferences have changed
    bool isDirty();

    //! \brief Returns all hidden settings for a given panel
    //! \param panel Panel of interest (Printer, Material, Profile)
    //! \return List of setting categories (headers)
    QList<QString> getHiddenSettings(QString panel);

    //! \brief Add a hidden setting
    //! \param panel Panel setting header is located on
    //! \param setting Actual setting category
    void addHiddenSetting(QString panel, QString setting);

    //! \brief Remove a hidden setting
    //! \param panel Panel setting header is located on
    //! \param setting Actual setting category
    void removeHiddenSetting(QString panel, QString setting);

    //! \brief Check whether or not a particular setting is hidden
    //! \param panel Panel setting header is located on
    //! \param setting Actual setting category
    //! \return bool indicating hidden status
    bool isSettingHidden(QString panel, QString setting);

    //! \brief Get Visualization Color
    //! \param color enum
    //! \return QColor
    QColor getVisualizationColor(VisualizationColors color);

    //! \brief Set Visualization Color
    //! \param color name as string
    //! \param color
    //! \param QColor
    void setVisualizationColor(QString name, QColor value);

    //! \brief Revert Visualization Color, sets it to the system default
    //! \param color name as string
    //! \return QColor
    QColor revertVisualizationColor(QString name);

    //! \brief Revert Visualization Color, sets it to the system default
    //! \param color name as string
    //! \return boolean
    bool isDefaultVisualizationColor(QString name);

    //! \brief Get visualization color map
    //! \return visualization color map of name and color
    std::map<std::string, QColor> getVisualizationColors();

    //! \brief Get Visualization Colors as Hex strings
    //! \return visualization color map of name and color as hex string
    std::map<std::string, std::string> getVisualizationHexColors();

    //! \brief Get processing step connectivity
    //! \param type: processing step to get
    //! \return whether or not to transmit results over TCP
    bool getStepConnectivity(StatusUpdateStepType type);

    //! \brief Get TCP server port
    //! \return TCP port
    int getTCPServerPort();

    //! \brief Get TCP server autostart
    //! \return whether or not to autostart
    bool getTcpServerAutoStart();

    //! \brief Get Send Output To Katana
    //! \return whether or not to send gcode / model to Katana
    bool getKatanaSendOutput();

    //! \brief Get Katana TCP IP Address
    //! \return Katana TCP IP Address as string
    QString getKatanaTCPIp();

    //! \brief Get Katana TCP Port
    //! \return Katana TCP Port as integer
    int getKatanaTCPPort();

  signals:
    //! \brief Signal emitted when the import unit is changed
    void importUnitChanged(Distance new_value, Distance old_value);

    //! \brief Signal emitted when the distance unit is changed
    void distanceUnitChanged(Distance new_value, Distance old_value);

    //! \brief Signal emitted when the velocity unit is changed
    void velocityUnitChanged(Velocity new_value, Velocity old_value);

    //! \brief Signal emitted when the acceleration unit is changed
    void accelerationUnitChanged(Acceleration new_value, Acceleration old_value);

    //! \brief Signal emitted when the density unit is changed
    void densityUnitChanged(Density new_value, Density old_value);

    //! \brief Signal emitted when the angle unit is changed
    void angleUnitChanged(Angle new_value, Angle old_value);

    //! \brief Signal emitted when the time unit is changed
    void timeUnitChanged(Time new_value, Time old_value);

    //! \brief Signal emitted when the temp unit is changed
    void temperatureUnitChanged(Temperature new_value, Temperature old_value);

    //! \brief Signal emitted when the voltage unit is changed
    void voltageUnitChanged(Voltage new_value, Voltage old_value);

    //! \brief Signal emitted when the rotation unit is changed
    void rotationUnitChanged(RotationUnit new_value);

    //! \brief Signal emitted when the mass unit is changed
    void massUnitChanged(Mass new_value, Mass old_value);

    //! \brief Signal that any of the above units have changed
    void anyUnitChanged();

    //! \brief Signal that any of the above units have changed
    void themeChanged();

  public slots:
    //! \brief sets the unit used to scale when importing models
    //! \param du the unit text
    void setImportUnit(QString du);

    //! \brief sets the unit used to scale when importing models
    //! \param du the unit
    void setImportUnit(Distance d);

    //! \brief Sets the unit used for distance/length
    void setDistanceUnit(QString du);

    //! \brief Sets the unit used for distance/length
    void setDistanceUnit(Distance d);

    //! \brief Sets the unit used for velocity
    void setVelocityUnit(QString vu);

    //! \brief Sets the unit used for velocity
    void setVelocityUnit(Velocity v);

    //! \brief Sets the unit used for acceleration
    void setAccelerationUnit(QString au);

    //! \brief Sets the unit used for acceleration
    void setAccelerationUnit(Acceleration a);

    //! \brief Sets the unit used for density
    void setDensityUnit(QString du);

    //! \brief Sets the unit used for density
    void setDensityUnit(Density d);

    //! \brief Sets the unit used for angle
    void setAngleUnit(QString au);

    //! \brief Sets the unit used for angle
    void setAngleUnit(Angle a);

    //! \brief Sets the theme of ORNLSlicer
    void setTheme(QString theme);

    //! \brief Sets the theme of ORNLSlicer
    void setTheme(ThemeName theme);

    //! \brief Sets the unit used for time
    void setTimeUnit(QString t);

    //! \brief Sets the unit used for time
    void setTimeUnit(Time t);

    //! \brief Sets the unit used for temperature
    void setTemperatureUnit(QString t);

    //! \brief Sets the unit used for temperature
    void setTemperatureUnit(Temperature t);

    //! \brief Sets the unit used for voltage
    void setVoltageUnit(QString v);
    void setVoltageUnit(Voltage v);

    //! \brief Sets the unit used for mass
    void setMassUnit(QString m);
    void setMassUnit(Mass m);

    //! \brief Sets the preference for part shift when loading a project
    void setProjectShiftPreference(PreferenceChoice shift);

    //! \brief Sets the preference for part shift when loading individual files
    void setFileShiftPreference(PreferenceChoice shift);

    //! \brief Sets the preference for part alignment with table when slicing
    void setAlignPreference(PreferenceChoice align);

    //! \brief Sets the preference for hiding travels during gcode visualization
    void setHideTravelPreference(bool hide);

    //! \brief Sets the preference for hiding supports during gcode visualization
    void setHideSupportPreference(bool hide);

    //! \brief Sets the preference for using true bead widths in gcode view
    //! \param use if true widths should be used
    void setUseTrueWidthsPreference(bool use);

    //! \brief Sets the unit used for rotation
    void setRotationUnit(QString unit);

    //! \brief Sets if the camera rotation should be inverted
    //! \param invert if it should be inverted
    void setInvertCamera(bool invert);

    //! \brief Sets if part should inherit its implicit transform when imported
    //! \param use if to use
    void setUseImplicitTransforms(bool use);

    //! \brief Sets if part should always be dropped to floor
    //! \param should if the part should be dropped
    void setShouldAlwaysDrop(bool should);

    //! \brief Sets if the window should be maximized
    //! \param isMaximized: whether or not the window is maximized
    void setWindowMaximizedPreference(bool isMaximized);

    //! \brief Sets window size when not maximized
    //! \param window_size: size of window
    void setWindowSizePreference(QSize window_size);

    //! \brief Sets window position when not maximized
    //! \param window_pos: position of window
    void setWindowPosPreference(QPoint window_pos);

    //! \brief Sets the lag between layers when using the play button
    //! \param lag: time to lag in milliseconds
    void setLayerLag(int lag);

    //! \brief Sets the lag between segments when using the play button
    //! \param lag: time to lag in milliseconds
    void setSegmentLag(int lag);

    //! \brief Sets processing stage connectivity (currently only gcode)
    //! \param type: processing stage to set
    //! \param toggle: whether or not to transmit over TCP
    void setStepConnectivity(StatusUpdateStepType type, bool toggle);

    //! \brief Sets TCP server's port
    //! \param port: port to set
    void setTCPServerPort(int port);

    //! \brief Sets whether TCP server should autostart or not
    //! \param start: whether or not to autostart
    void setTcpServerAutoStart(bool start);

    //! \brief Sets whether Send Output To Katana or not
    //! \param send: whether or not to send
    void setKatanaSendOutput(bool send);

    //! \brief Set Katana TCP IP Address
    //! \param Katana TCP IP Address as string
    void setKatanaTCPIp(QString ipAddress);

    //! \brief Set Katana TCP Port
    //! \param Katana TCP Port as integer
    void setKatanaTCPPort(int port);

  private:
    //! \brief Constructor
    PreferencesManager();

    //! \brief Set Visualization Colors to Default
    void setDefaultVisualizationColors(std::unordered_map<std::string, std::string> visualizationColorsHex);

    //! \brief Singleton pointer
    static QSharedPointer<PreferencesManager> m_singleton;

    //! \brief Unit preference values
    Distance m_import_unit;
    Distance m_distance_unit;
    Velocity m_velocity_unit;
    Acceleration m_acceleration_unit;
    Density m_density_unit;
    Angle m_angle_unit;
    Time m_time_unit;
    Temperature m_temperature_unit;
    Voltage m_voltage_unit;
    Mass m_mass_unit;

    //! \brief Preference choices for other values
    PreferenceChoice m_project_shift_preference;
    PreferenceChoice m_file_shift_preference;
    PreferenceChoice m_align_preference;
    bool m_hide_travel_preference;
    bool m_hide_support_preference;
    bool m_use_true_widths_preference;

    //! \brief Preferences for window size and position
    bool m_is_maximized;
    QSize m_window_size;
    QPoint m_window_pos;

    //! \brief preference choice for theme
    ThemeName m_themeName;

    //! \brief the theme
    Theme m_theme;

    //! \brief Prefernce choice for rotation unit
    RotationUnit m_rotation_unit;

    //! \brief Camera Preference
    bool m_invert_camera = false;

    //! \brief Part preferences
    bool m_use_implicit_transforms;
    bool m_always_drop_parts;

    //! \brief Layer preferences
    int m_layer_lag;
    int m_segment_lag;

    //! \brief bool to determine if a save is necessary
    int m_dirty;

    //! Std construct to make it easier to write out to json for hidden settings
    //! Construct limited to this class.  All input/output converted to Qt structures.
    std::unordered_map<std::string, std::list<std::string>> m_hidden_settings;

    //! \brief visualization color's preferences QColors
    std::unordered_map<std::string, QColor> m_visualization_qcolors;

    //! \brief whether or not to transmit information for each major processing stage (currently only gcode)
    QVector<bool> m_step_connectivity;

    //! \brief port to run tcp server on
    int m_tcp_port;

    //! \brief whether to automatically start tcp server on startup
    bool m_tcp_server_autostart;

    //! \brief whether to Send Output To Katana
    bool m_katana_send_output;

    //! \brief Katana TCP IP Address
    QString m_katana_tcp_ip;

    //! \brief Katana TCP Port
    int m_katana_tcp_port;

}; // class PreferencesManager
} // namespace ORNL
