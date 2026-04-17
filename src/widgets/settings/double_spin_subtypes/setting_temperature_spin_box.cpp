#include "widgets/settings/double_spin_subtypes/setting_temperature_spin_box.h"

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

SettingTemperatureSpinBox::SettingTemperatureSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                     fifojson json, QGridLayout* layout, int index)
    : SettingDoubleSpinBox(parent, sb, key, json, layout, index) {
    Temperature cur;
    m_warn = false;
    if (sb->contains(key))
        cur = sb->setting<Temperature>(key);
    else {
        cur = json[Constants::Settings::Master::kDefault].get<Temperature>();
        sb->setSetting(key, cur);
    }

    Temperature unit = PreferencesManager::getInstance()->getTemperatureUnit();

    this->setMaximum(Constants::Limits::Maximums::kMaxTemperature.to(unit));
    this->setMinimum(-459.67); // absolute zero
    this->setDecimals(m_precision);
    this->setValue(cur.to(unit));

    // Set setting units
    m_unit_label->setText(PreferencesManager::getInstance()->getTemperatureUnitText());
}

SettingRowBase* SettingTemperatureSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb,
                                                          QString key, fifojson json, QGridLayout* layout, int index) {
    return new SettingTemperatureSpinBox(parent, sb, key, json, layout, index);
}

void SettingTemperatureSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    Temperature base_value;
    base_value.from(val.toDouble(), PreferencesManager::getInstance()->getTemperatureUnit());
    SettingDoubleSpinBox::valueChanged(base_value());
}

void SettingTemperatureSpinBox::reloadValue() {
    this->blockSignals(true);
    Temperature unit = PreferencesManager::getInstance()->getTemperatureUnit();
    this->setMaximum(Constants::Limits::Maximums::kMaxTemperature.to(unit));

    if (unit == K)
        this->setMinimum(0.0);
    else if (unit == degC)
        this->setMinimum(-273.15);
    else
        this->setMinimum(-459.67);

    m_unit_label->setText(PreferencesManager::getInstance()->getTemperatureUnitText());

    bool consistent = true;
    Temperature cur(reloadValueHelper<double>(consistent));
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
