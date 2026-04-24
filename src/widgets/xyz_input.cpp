#include "widgets/xyz_input.h"

#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qhashfunctions.h>
#include <qicon.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qoverload.h>
#include <qsizepolicy.h>
#include <qspinbox.h>
#include <qtmetamacros.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
XYZInputWidget::XYZInputWidget(QWidget* parent) : QWidget(parent) {
    this->setupWidget();
    this->setupEvents();
}

QVector3D XYZInputWidget::value() { return QVector3D(m_x_dsb->value(), m_y_dsb->value(), m_z_dsb->value()); }

void XYZInputWidget::showLabel(bool show) { m_front_label->setHidden(!show); }

void XYZInputWidget::showUnit(bool show) { m_unit_label->setHidden(!show); }

void XYZInputWidget::showLock(bool show) { m_lock_check->setHidden(!show); }

void XYZInputWidget::setLabelText(QString str) { m_front_label->setText(str); }

void XYZInputWidget::setUnitText(QString str) { m_unit_label->setText(str); }

void XYZInputWidget::setLock(bool lock) { m_lock_check->setChecked(lock); }

void XYZInputWidget::setXText(QString x) { m_x_label->setText(x + " "); }

void XYZInputWidget::setYText(QString y) { m_y_label->setText(y + " "); }

void XYZInputWidget::setZText(QString z) { m_z_label->setText(z + " "); }

void XYZInputWidget::setXValue(double val) { m_x_dsb->setValue(val); }

void XYZInputWidget::setYValue(double val) { m_y_dsb->setValue(val); }

void XYZInputWidget::setZValue(double val) { m_z_dsb->setValue(val); }

void XYZInputWidget::setValue(QVector3D xyz) {
    m_x_dsb->setValue(xyz.x());
    m_y_dsb->setValue(xyz.y());
    m_z_dsb->setValue(xyz.z());
}

void XYZInputWidget::setIncrement(double inc) {
    m_x_dsb->setSingleStep(inc);
    m_y_dsb->setSingleStep(inc);
    m_z_dsb->setSingleStep(inc);
}

void XYZInputWidget::readValue(double val) {
    QObject* dsb = QObject::sender();
    Axis ax = Axis::kX;

    if (dsb == m_y_dsb)
        ax = Axis::kY;
    else if (dsb == m_z_dsb)
        ax = Axis::kZ;

    switch (ax) {
        case Axis::kX:
            emit valueXChanged(val);
            break;
        case Axis::kY:
            emit valueYChanged(val);
            break;
        case Axis::kZ:
            emit valueZChanged(val);
            break;
    }

    emit valueChanged(ax, val);
    emit valueChanged(QVector3D(m_x_dsb->value(), m_y_dsb->value(), m_z_dsb->value()));
}

void XYZInputWidget::lockReadValue(double val) {
    m_y_dsb->setValue(val);
    m_z_dsb->setValue(val);

    this->readValue(val);
}

void XYZInputWidget::setupWidget() {
    m_front_label = new QLabel("", this);
    m_front_label->setHidden(true);
    m_x_label = new QLabel("X: ", this);
    m_x_label->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    m_y_label = new QLabel("Y: ", this);
    m_y_label->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    m_z_label = new QLabel("Z: ", this);
    m_z_label->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    m_unit_label = new QLabel("", this);
    m_unit_label->setMinimumWidth(25);
    m_unit_label->setHidden(true);

    m_x_dsb = new QDoubleSpinBox(this);
    m_x_dsb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_x_dsb->setDecimals(Constants::UI::PartToolbar::Input::kPrecision);
    m_x_dsb->setMinimum(0.0001);
    m_y_dsb = new QDoubleSpinBox(this);
    m_y_dsb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_y_dsb->setDecimals(Constants::UI::PartToolbar::Input::kPrecision);
    m_y_dsb->setMinimum(0.0001);
    m_z_dsb = new QDoubleSpinBox(this);
    m_z_dsb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_z_dsb->setDecimals(Constants::UI::PartToolbar::Input::kPrecision);
    m_z_dsb->setMinimum(0.0001);

    m_lock_check = new QCheckBox(this);
    m_lock_check->setIcon(QIcon(":/icons/lock.png"));
    m_lock_check->setToolTip("Locks the X/Y/Z scales together.");
    m_lock_check->setHidden(true);

    m_layout = new QHBoxLayout(this);

    m_layout->addWidget(m_front_label);
    m_layout->addWidget(m_x_label);
    m_layout->addWidget(m_x_dsb);
    m_layout->addWidget(m_y_label);
    m_layout->addWidget(m_y_dsb);
    m_layout->addWidget(m_z_label);
    m_layout->addWidget(m_z_dsb);

    m_layout->addWidget(m_unit_label);
    m_layout->addWidget(m_lock_check);
}

void XYZInputWidget::setupEvents() {
    QWidget::connect(m_x_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &XYZInputWidget::readValue);
    QWidget::connect(m_y_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &XYZInputWidget::readValue);
    QWidget::connect(m_z_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &XYZInputWidget::readValue);

    QWidget::connect(m_lock_check, &QCheckBox::toggled, this, [this](bool toggle) {
        if (toggle) {
            disconnect(m_x_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &XYZInputWidget::readValue);
            disconnect(m_y_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &XYZInputWidget::readValue);
            disconnect(m_z_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &XYZInputWidget::readValue);

            m_y_dsb->setDisabled(true);
            m_z_dsb->setDisabled(true);

            double val = m_x_dsb->value();
            m_y_dsb->setValue(val);
            m_z_dsb->setValue(val);

            connect(m_x_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    &XYZInputWidget::lockReadValue);

            this->lockReadValue(val);
        }
        else {
            QWidget::connect(m_x_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                             &XYZInputWidget::readValue);
            QWidget::connect(m_y_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                             &XYZInputWidget::readValue);
            QWidget::connect(m_z_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                             &XYZInputWidget::readValue);

            m_y_dsb->setDisabled(false);
            m_z_dsb->setDisabled(false);

            disconnect(m_x_dsb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                       &XYZInputWidget::lockReadValue);
        }
    });
}
} // namespace ORNL
