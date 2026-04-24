#include "widgets/settings/double_spin_subtypes/setting_density_spin_box.h"

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

SettingDensitySpinBox::SettingDensitySpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                             fifojson json, QGridLayout* layout, int index)
    : SettingDoubleSpinBox(parent, sb, key, json, layout, index) {
    Density cur;
    m_warn = false;
    if (sb->contains(key))
        cur = sb->setting<Density>(key);
    else {
        cur = json[Constants::Settings::Master::kDefault].get<Density>();
        m_sb->setSetting(key, cur);
    }

    Density unit = PreferencesManager::getInstance()->getDensityUnit();

    this->setMaximum(9999.9999);
    this->setDecimals(m_precision);
    this->setValue(cur.to(unit));

    // Set setting units
    m_unit_label->setText(PreferencesManager::getInstance()->getDensityUnitText());
}

SettingRowBase* SettingDensitySpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                      fifojson json, QGridLayout* layout, int index) {
    return new SettingDensitySpinBox(parent, sb, key, json, layout, index);
}

void SettingDensitySpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    Density base_value;
    base_value.from(val.toDouble(), PreferencesManager::getInstance()->getDensityUnit());
    SettingDoubleSpinBox::valueChanged(base_value());
}

void SettingDensitySpinBox::reloadValue() {
    this->blockSignals(true);
    Density unit = PreferencesManager::getInstance()->getDensityUnit();
    this->setMaximum(9999.9999);
    m_unit_label->setText(PreferencesManager::getInstance()->getDensityUnitText());

    bool consistent = true;
    Density cur(reloadValueHelper<double>(consistent));
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
