#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QWidget>
#include <qgridlayout.h>
#include <qobject.h>
#include <qtmetamacros.h>

#include "units/unit.h"

namespace ORNL {
/*!
 * \class XtrudeCalcWindow
 * \brief Window that allows the user to calculate flow rate for given inputs
 */
class XtrudeCalcWindow : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    explicit XtrudeCalcWindow(QWidget* parent);

    //! \brief Destructor
    ~XtrudeCalcWindow();

  protected:
    //! \brief Close event override to reset form values
    void closeEvent(QCloseEvent* event);

  private slots:
    //! \brief Check to see if preferences were updated in order to update units
    void checkUnitPref();

    //! \brief Check that all inputs are valid and perform necessary calculations
    //! Inputs are checked automatically as a user alters them
    void checkInputAndCalculate();

    //! \brief Enable/Disable density input based on combobox index
    //! \param index Current combo index
    void enableDensity(int index);

  private:
    //! \brief Setup events for widget components
    void setupEvents();

    //! \brief Layout for widget
    QGridLayout* m_layout;

    //! \brief Preferred units for use in conversion
    Time m_time_pref;
    Distance m_dist_pref;
    Mass m_mass_pref;
    Velocity m_velo_pref;
    Density m_dens_pref;

    //! \brief Conversion factors
    double m_time_conv;
    double m_dist_conv;
    double m_mass_conv;
    double m_velo_conv;
    double m_dens_conv;

    //! \brief Concatenated unit text for use in syntax
    QString m_time_text;
    QString m_dist_text;
    QString m_mass_text;
    QString m_density_text;
    QString m_fpr_text;
    QString m_spindle_text;
    QString m_feed_text;

    //! \brief Manipulatable labels for use in syntax
    QLabel* m_bead_width_label;
    QLabel* m_layer_height_label;
    QLabel* m_2mtm_label;
    QLabel* m_min_layer_time_label;
    QLabel* m_d_spindle_speed_label;
    QLabel* m_d_feed_rate_label;
    QLabel* m_toolpath_length_label;
    QLabel* m_density_label;
    QLabel* m_spindle_speed_label;
    QLabel* m_fpr_label;
    QLabel* m_o1_feed_rate_label;
    QLabel* m_o1_spindle_speed_label;
    QLabel* m_o2_feed_rate_label;
    QLabel* m_o3_spindle_speed_label;

    //! \brief Individual input components
    QLineEdit* m_bead_width;
    QLineEdit* m_layer_height;
    QLineEdit* m_2mtm;
    QLineEdit* m_min_layer_time;
    QLineEdit* m_d_spindle_speed;
    QLineEdit* m_d_feed_rate;
    QLineEdit* m_toolpath_length;
    QComboBox* m_material_type;
    QLineEdit* m_density;

    //! \brief Individual output components that show results
    QLineEdit* m_spindle_speed;
    QLineEdit* m_fpr;
    QLineEdit* m_o1_feed_rate;
    QLineEdit* m_o1_spindle_speed;
    QLineEdit* m_o2_feed_rate;
    QLineEdit* m_o3_spindle_speed;

    //! \brief Status label to provide feedback for input validation
    QLabel* m_status_label;

    //! \brief General Usage Directions
    QLabel* m_directions;

    //! \brief parent widget of this Xtrude calculator window
    QWidget* m_parent;
}; // class XtrudeCalcWindow
} // namespace ORNL
