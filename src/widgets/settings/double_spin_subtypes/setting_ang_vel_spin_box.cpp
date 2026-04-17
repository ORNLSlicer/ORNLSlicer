#include "widgets/settings/double_spin_subtypes/setting_ang_vel_spin_box.h"

#include <qgridlayout.h>
#include <qhashfunctions.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "managers/preferences_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_double_spin_box.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {

SettingAngVelSpinBox::SettingAngVelSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                           fifojson json, QGridLayout* layout, int index)
    : SettingDoubleSpinBox(parent, sb, key, json, layout, index) {
    AngularVelocity cur;
    m_warn = false;
    if (sb->contains(key))
        cur = sb->setting<AngularVelocity>(key);
    else {
        cur = json[Constants::Settings::Master::kDefault].get<AngularVelocity>();
        sb->setSetting(key, cur);
    }

    // The angle unit divided by the time unit should result in correct value.
    AngularVelocity unit =
        PreferencesManager::getInstance()->getAngleUnit() / PreferencesManager::getInstance()->getTimeUnit();

    this->setMaximum(Constants::Limits::Maximums::kMaxAngVel.to(unit));
    this->setDecimals(m_precision);
    this->setValue(cur.to(unit));
    this->setAlignment(Qt::AlignRight);

    // Set setting units
    m_unit_label->setText(PreferencesManager::getInstance()->getAngleUnitText() % "/" %
                          PreferencesManager::getInstance()->getTimeUnitText());
}

SettingRowBase* SettingAngVelSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                     fifojson json, QGridLayout* layout, int index) {
    return new SettingAngVelSpinBox(parent, sb, key, json, layout, index);
}

void SettingAngVelSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    AngularVelocity base_value;
    base_value.from(val.toDouble(), PreferencesManager::getInstance()->getAngleUnit() /
                                        PreferencesManager::getInstance()->getTimeUnit());
    SettingDoubleSpinBox::valueChanged(base_value());
}

void SettingAngVelSpinBox::reloadValue() {
    this->blockSignals(true);
    AngularVelocity unit =
        PreferencesManager::getInstance()->getAngleUnit() / PreferencesManager::getInstance()->getTimeUnit();
    this->setMaximum(Constants::Limits::Maximums::kMaxAngVel.to(unit));
    m_unit_label->setText(PreferencesManager::getInstance()->getAngleUnitText() % "/" %
                          PreferencesManager::getInstance()->getTimeUnitText());

    bool consistent = true;
    AngularVelocity cur(reloadValueHelper<double>(consistent));
    if (consistent)
        setValue(cur.to(unit));

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
} // namespace ORNL
