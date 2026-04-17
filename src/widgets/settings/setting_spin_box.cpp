#include "widgets/settings/setting_spin_box.h"

#include <climits>

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

SettingSpinBox::SettingSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                               QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QSpinBox() {
    this->setFocusPolicy(Qt::StrongFocus);

    float cur;
    m_warn = false;
    if (m_sb->contains(key))
        cur = m_sb->setting<float>(key);
    else {
        cur = json.operator[](Constants::Settings::Master::kDefault).get<float>();
        m_sb->setSetting(key, cur);
    }

    QString type = json[Constants::Settings::Master::kType];
    if (type == "positive_int" || type == "power")
        this->setMinimum(1);

    this->setMaximum(INT_MAX);
    this->setAlignment(Qt::AlignRight);
    this->setValue(cur);

    connect(this, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingSpinBox::valueChanged);
    connect(this, &SettingSpinBox::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingSpinBox::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

SettingRowBase* SettingSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                               fifojson json, QGridLayout* layout, int index) {
    return new SettingSpinBox(parent, sb, key, json, layout, index);
}

void SettingSpinBox::setEnabled(bool enabled) {
    dynamic_cast<QSpinBox*>(this)->setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void SettingSpinBox::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingSpinBox::clearNotification() {
    // clear notification information
    // emit signal to next level
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingSpinBox::hide() {
    dynamic_cast<QSpinBox*>(this)->hide();
    SettingRowBase::hide();
}

void SettingSpinBox::show() {
    dynamic_cast<QSpinBox*>(this)->show();
    SettingRowBase::show();
}

void SettingSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<int>(val.toInt());
    emit modified(m_key);
}

void SettingSpinBox::reloadValue() {
    this->blockSignals(true);
    bool consistent = true;
    int cur = reloadValueHelper<int>(consistent);
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

void SettingSpinBox::wheelEvent(QWheelEvent* event) {
    if (!hasFocus())
        event->ignore();
    else
        QSpinBox::wheelEvent(event);
}
} // namespace ORNL
