#include "widgets/settings/setting_check_box.h"

#include <QToolTip>
#include <qcheckbox.h>
#include <qgridlayout.h>
#include <qhashfunctions.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {

SettingCheckBox::SettingCheckBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                                 QGridLayout* layout, int index)
    : SettingRowBase(parent, sb, key, json, layout, index), QCheckBox() {
    bool cur;
    m_warn = false;
    // If the settings base contains the key, use its value. Otherwise, pull from the default.
    if (m_sb->contains(key))
        cur = m_sb->setting<bool>(key);
    // Get the value as an int from the master since json throws an error reading 0 as false.
    else {
        cur = json.operator[](Constants::Settings::Master::kDefault).get<int>();
        m_sb->setSetting(key, cur);
    }

    this->setChecked(cur);
    connect(this, &QCheckBox::stateChanged, this, &SettingCheckBox::valueChanged);
    connect(this, &SettingCheckBox::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingCheckBox::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

SettingRowBase* SettingCheckBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                fifojson json, QGridLayout* layout, int index) {
    return new SettingCheckBox(parent, sb, key, json, layout, index);
}

void SettingCheckBox::setEnabled(bool enabled) {
    dynamic_cast<QCheckBox*>(this)->setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void SettingCheckBox::setNotification(QString msg) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingCheckBox::clearNotification() {
    // clear notification information
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingCheckBox::hide() {
    dynamic_cast<QCheckBox*>(this)->hide();
    SettingRowBase::hide();
}

void SettingCheckBox::show() {
    dynamic_cast<QCheckBox*>(this)->show();
    SettingRowBase::show();
}

void SettingCheckBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<bool>(val.toBool());
    emit modified(m_key);
}

void SettingCheckBox::reloadValue() {
    this->blockSignals(true);
    bool consistent = true;
    bool cur = reloadValueHelper<bool>(consistent);
    if (consistent)
        setChecked(cur);

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
} // Namespace ORNL
