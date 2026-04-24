#pragma once

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QGroupBox>
#include <QMainWindow>
#include <QRadioButton>
#include <QSpinBox>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class PreferencesWindow
 * \brief Window that allows the user to change his/her preferences
 */
class PreferencesWindow : public QMainWindow {
    Q_OBJECT
  public:
    //! \brief Constructor
    explicit PreferencesWindow(QWidget* parent = 0);

    //! \brief Destructor
    ~PreferencesWindow();

  signals:
    void updateTheme();

  private slots:
    //! \brief Tell the preferences manager to import preferences from file
    void importPreferences();

    //! \brief Tell the preferences manager to export preferences to file
    void exportPreferences();

    //! \brief Update appropriate unit
    void updateImportUnitVisual(Distance oldUnit, Distance newUnit);
    void updateDistanceUnitVisual(Distance oldUnit, Distance newUnit);
    void updateVelocityUnitVisual(Velocity oldUnit, Velocity newUnit);
    void updateAccelerationUnitVisual(Acceleration oldUnit, Acceleration newUnit);
    void updateDensityUnitVisual(Density oldUnit, Density newUnit);
    void updateAngleUnitVisual(Angle oldUnit, Angle newUnit);
    void updateTimeUnitVisual(Time oldUnit, Time newUnit);
    void updateTemperatureUnitVisual(Temperature oldUnit, Temperature newUnit);
    void updateVoltageUnitVisual(Voltage oldUnit, Voltage newUnit);
    void updateMassUnitVisual(Mass oldUnit, Mass newUnit);
    void updateThemeVisual();

  private:
    //! \brief Setup all components
    void setupLayout();
    //! \brief Setup the events for the various widgets.
    void setupEvents();

    //! \brief Setup the events for the various widgets.
    //! \param choice Preference choice for current preference set
    //! \param displayStrings set of strings that make up the radio buttons in the group
    //! \param func function pointer for callback used to tie radio buttons to slots
    QGroupBox* createContainer(PreferenceChoice choice, QList<QString> displayStrings,
                               void (PreferencesWindow::*func)(PreferenceChoice));

    //! \brief Handles closing of the window and clearing the pointer in the
    //! window manager
    void closeEvent(QCloseEvent* event);

    //! \brief Functions to forward preference update to manager
    void shiftProjectPreferenceChanged(PreferenceChoice shift);
    void shiftFilePreferenceChanged(PreferenceChoice shift);
    void alignPreferenceChanged(PreferenceChoice align);

    //! \brief Set value for preference group from manager
    void setPreferenceValue(QGroupBox* box, PreferenceChoice choice);

    //! \brief Individual comboboxes for units
    QComboBox* m_import_unit_combobox;
    QComboBox* m_distance_unit_combobox;
    QComboBox* m_velocity_unit_combobox;
    QComboBox* m_acceleration_unit_combobox;
    QComboBox* m_density_unit_combobox;
    QComboBox* m_angle_unit_combobox;
    QComboBox* m_time_unit_combobox;
    QComboBox* m_temperature_unit_combobox;
    QComboBox* m_voltage_unit_combobox;
    QComboBox* m_mass_unit_combobox;
    QComboBox* m_rotation_unit_combobox;
    QComboBox* m_theme_combobox;

    //! \brief List of groupboxes that hold radio buttons for various preferences
    QList<QGroupBox*> m_boxes;

    //! \brief Copy of pointer to manager that holds actual preference information
    QSharedPointer<PreferencesManager> m_preferences_manager;
}; // class PreferencesWindow
} // namespace ORNL
