#include "widgets/settings/setting_double_spin_box.h"

#include <QToolTip>
#include <QWheelEvent>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qoverload.h>
#include <qsharedpointer.h>
#include <qspinbox.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {
SettingDoubleSpinBox::SettingDoubleSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                           fifojson json, QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QDoubleSpinBox() {
    this->setFocusPolicy(Qt::StrongFocus);
    this->setAlignment(Qt::AlignRight);

    double cur;
    m_warn = false;
    if (sb->contains(key))
        cur = sb->setting<double>(key);
    else {
        cur = json[Constants::Settings::Master::kDefault].get<double>();
        sb->setSetting(key, cur);
    }

    QString unitText;
    QString type = json[Constants::Settings::Master::kType];
    if (type == "rpm") {
        this->setMaximum(9999.99);
        unitText = "rpm";
    }
    else if (type == "percentage100") // Percentage values with a maximum value of 100
    {
        this->setMinimum(0);
        this->setMaximum(100);
        unitText = "%";
    }
    else if (type == "percentage") {
        this->setMinimum(0);
        this->setMaximum(500);
        unitText = "%";
    }
    else if (type == "unitless_float") {
        this->setMinimum(-9999.99);
        this->setMaximum(9999.99);
        this->setDecimals(m_precision);
    }

    this->setValue(cur);

    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SettingDoubleSpinBox::valueChanged);
    connect(this, &SettingDoubleSpinBox::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingDoubleSpinBox::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(unitText));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

SettingRowBase* SettingDoubleSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                     fifojson json, QGridLayout* layout, int index) {
    return new SettingDoubleSpinBox(parent, sb, key, json, layout, index);
}

void SettingDoubleSpinBox::setEnabled(bool enabled) {
    dynamic_cast<QDoubleSpinBox*>(this)->setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void SettingDoubleSpinBox::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingDoubleSpinBox::clearNotification() {
    // clear notification information
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingDoubleSpinBox::hide() {
    dynamic_cast<QDoubleSpinBox*>(this)->hide();
    SettingRowBase::hide();
}

void SettingDoubleSpinBox::show() {
    dynamic_cast<QDoubleSpinBox*>(this)->show();
    SettingRowBase::show();
}

void SettingDoubleSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<double>(val.toDouble());
    emit modified(m_key);
}

void SettingDoubleSpinBox::reloadValue() {
    this->blockSignals(true);
    bool consistent = true;
    double cur = reloadValueHelper<double>(consistent);
    if (consistent)
        setValue(cur);

    this->blockSignals(false);
    emit modified(m_key);
    // give (1) warning if there is a mismatch, otherwise give no (0) warning
    if (!consistent) {
        emit warnParent(1);
        m_warn = true;
    }
    else
        emit warnParent(0);
}

void SettingDoubleSpinBox::wheelEvent(QWheelEvent* event) {
    if (!hasFocus())
        event->ignore();
    else
        QDoubleSpinBox::wheelEvent(event);
}
} // namespace ORNL
