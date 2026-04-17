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
 * \class FlowrateCalcWindow
 * \brief Window that allows the user to calculate flow rate for given inputs
 */
class FlowrateCalcWindow : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    explicit FlowrateCalcWindow(QWidget* parent);

    //! \brief Destructor
    ~FlowrateCalcWindow();

  protected:
    //! \brief Close event override to reset form values
    void closeEvent(QCloseEvent* event);

  private slots:
    //! \brief Check that all inputs are valid and perform necessary calculations
    //! Inputs are checked automatically as a user alters them
    void checkInputAndCalculate();

    //! \brief Save density value when a user chooses "Other"
    void saveOtherDensity();

    //! \brief Enable/Disable density input based on combobox index
    //! \param index Current combo index
    void enableDensity(int index);

  private:
    //! \brief Setup events for widget components
    void setupEvents();

    //! \brief Layout for widget
    QGridLayout* m_layout;

    //! \brief Individual input components
    QLineEdit* m_speed_rpm;
    QLineEdit* m_lbs_hour;
    QLineEdit* m_bead_width;
    QLineEdit* m_layer_height;
    QLineEdit* m_print_rate;
    QComboBox* m_material_type;
    QLineEdit* m_density;
    //! \brief Saved density value when user inputs it for "Other"
    QString m_saved_other_density;

    //! \brief Individual output components that show results
    QLineEdit* m_gantry_speed;
    QLineEdit* m_spindle_speed;

    //! \brief Status label to provide feedback for input validation
    QLabel* m_status_label;

    //! \brief Metric to imperial conversion
    double m_density_metric_to_is; // from metric to British system, g/cm^3 --> lb/in^3

    //! \brief Density in Metric
    Density m_density_metric;
    //! \brief parent widget of this flowrate calculator window
    QWidget* m_parent;
}; // class FlowrateCalcWindow
} // namespace ORNL
