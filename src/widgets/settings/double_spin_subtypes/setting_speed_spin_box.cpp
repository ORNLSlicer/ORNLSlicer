#include "widgets/settings/double_spin_subtypes/setting_speed_spin_box.h"

#include <qgridlayout.h>
#include <qhashfunctions.h>
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

SettingSpeedSpinBox::SettingSpeedSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                         fifojson json, QGridLayout* layout, int index)
    : SettingDoubleSpinBox(parent, sb, key, json, layout, index) {
    Velocity cur;
    m_warn = false;
    if (sb->contains(key))
        cur = sb->setting<Velocity>(key);
    else {
        cur = json[Constants::Settings::Master::kDefault].get<Velocity>();
        sb->setSetting(key, cur);
    }

    Velocity unit = PreferencesManager::getInstance()->getVelocityUnit();
    this->setMaximum(Constants::Limits::Maximums::kMaxSpeed.to(unit));
    this->setDecimals(m_precision);
    this->setValue(cur.to(unit));

    // Set setting units
    m_unit_label->setText(PreferencesManager::getInstance()->getVelocityUnitText());
}

SettingRowBase* SettingSpeedSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                    fifojson json, QGridLayout* layout, int index) {
    return new SettingSpeedSpinBox(parent, sb, key, json, layout, index);
}

void SettingSpeedSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    Velocity base_value;
    base_value.from(val.toDouble(), PreferencesManager::getInstance()->getVelocityUnit());
    SettingDoubleSpinBox::valueChanged(base_value());
}

void SettingSpeedSpinBox::reloadValue() {
    this->blockSignals(true);
    Velocity unit = PreferencesManager::getInstance()->getVelocityUnit();
    this->setMaximum(Constants::Limits::Maximums::kMaxSpeed.to(PreferencesManager::getInstance()->getVelocityUnit()));
    m_unit_label->setText(PreferencesManager::getInstance()->getVelocityUnitText());

    bool consistent = true;
    Velocity cur(reloadValueHelper<double>(consistent));
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
