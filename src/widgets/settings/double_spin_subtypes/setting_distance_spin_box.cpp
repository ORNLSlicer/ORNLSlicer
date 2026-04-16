#include "widgets/settings/double_spin_subtypes/setting_distance_spin_box.h"

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

SettingDistanceSpinBox::SettingDistanceSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                               fifojson json, QGridLayout* layout, int index)
    : SettingDoubleSpinBox(parent, sb, key, json, layout, index) {
    Distance cur;
    m_warn = false;
    if (sb->contains(key))
        cur = sb->setting<Distance>(key);
    else {
        cur = json[Constants::Settings::Master::kDefault].get<Distance>();
        sb->setSetting(key, cur);
    }

    Distance unit = PreferencesManager::getInstance()->getDistanceUnit();
    this->setMaximum(Constants::Limits::Maximums::kMaxDistance.to(unit));
    if (json[Constants::Settings::Master::kType] == "distance")
        this->setMinimum(Constants::Limits::Minimums::kMinDistance.to(unit));
    else
        this->setMinimum(Constants::Limits::Minimums::kMinLocation.to(unit));

    this->setDecimals(m_precision);
    this->setValue(cur.to(unit));

    // Set setting units
    m_unit_label->setText(PreferencesManager::getInstance()->getDistanceUnitText());
}

SettingRowBase* SettingDistanceSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                       fifojson json, QGridLayout* layout, int index) {
    return new SettingDistanceSpinBox(parent, sb, key, json, layout, index);
}

void SettingDistanceSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1); // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    Distance base_value;
    base_value.from(val.toDouble(), PreferencesManager::getInstance()->getDistanceUnit());
    SettingDoubleSpinBox::valueChanged(base_value());
}

void SettingDistanceSpinBox::reloadValue() {
    this->blockSignals(true);
    Distance unit = PreferencesManager::getInstance()->getDistanceUnit();
    this->setMaximum(Constants::Limits::Maximums::kMaxDistance.to(unit));
    if (m_json[Constants::Settings::Master::kType] == "distance")
        this->setMinimum(Constants::Limits::Minimums::kMinDistance.to(unit));
    else
        this->setMinimum(Constants::Limits::Minimums::kMinLocation.to(unit));
    m_unit_label->setText(PreferencesManager::getInstance()->getDistanceUnitText());

    bool consistent = true;
    Distance cur(reloadValueHelper<double>(consistent));
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
