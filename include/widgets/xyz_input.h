#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <qhashfunctions.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include "utilities/enums.h"

namespace ORNL {
/*!
 * \brief Generic XYZ input class for use around UI.
 */
class XYZInputWidget : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent: A widget.
    XYZInputWidget(QWidget* parent = nullptr);

    //! \brief Value of all 3 spinboxes.
    QVector3D value();

  public slots:
    //! \brief Shows front label.
    void showLabel(bool show);
    //! \brief Shows unit text.
    void showUnit(bool show);
    //! \brief Shows lock toggle.
    void showLock(bool show);

    //! \brief Sets label text.
    void setLabelText(QString str);
    //! \brief Sets unit text.
    void setUnitText(QString str);
    //! \brief Sets lock status.
    void setLock(bool lock);

    //! \brief Sets x spinbox text.
    void setXText(QString x);
    //! \brief Sets y spinbox text.
    void setYText(QString y);
    //! \brief Sets z spinbox text.
    void setZText(QString z);

    //! \brief Sets x spinbox value.
    void setXValue(double val);
    //! \brief Sets y spinbox value.
    void setYValue(double val);
    //! \brief Sets z spinbox value.
    void setZValue(double val);

    //! \brief Sets all 3 spinbox values.
    void setValue(QVector3D xyz);

    //! \brief Sets spinbox increments.
    void setIncrement(double inc);

  signals:
    //! \brief Signal that a spinbox has changed.
    void valueChanged(Axis ax, double val);

    //! \brief Signal that the x spinbox changed.
    void valueXChanged(double x);
    //! \brief Signal that the y spinbox changed.
    void valueYChanged(double y);
    //! \brief Signal that the z spinbox changed.
    void valueZChanged(double z);

    //! \brief Signal that any XYZ value has changed.
    void valueChanged(QVector3D xyz);

  private slots:
    //! \brief Relays info from spinboxes.
    void readValue(double val);
    //! \brief Copies x spinbox to y & z before relaying.
    void lockReadValue(double val);

  private:
    // Widget Setup
    void setupWidget();
    void setupEvents();

    // Labels
    QLabel* m_front_label;
    QLabel* m_x_label;
    QLabel* m_y_label;
    QLabel* m_z_label;
    QLabel* m_unit_label;

    // Input
    QDoubleSpinBox* m_x_dsb;
    QDoubleSpinBox* m_y_dsb;
    QDoubleSpinBox* m_z_dsb;
    QCheckBox* m_lock_check;

    // Layout
    QHBoxLayout* m_layout;
};
} // namespace ORNL
