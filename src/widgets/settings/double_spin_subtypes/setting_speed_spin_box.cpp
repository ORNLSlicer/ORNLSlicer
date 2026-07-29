#include "widgets/settings/double_spin_subtypes/setting_speed_spin_box.h"

#include <qgridlayout.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qstring.h>
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
namespace {
bool isActiveSpeedLimit(Velocity speed) { return speed > 0; }

QString masterString(const fifojson& json, const std::string& key) {
    return QString::fromStdString(json.at(key).get<std::string>());
}

bool isXYSpeedLimitKey(const QString& key) {
    return key == PRS::MachineSpeed::kMinXYSpeed || key == PRS::MachineSpeed::kMaxXYSpeed;
}

bool isPrinterXYMotionSpeed(const QString& key, const fifojson& json) {
    if (isXYSpeedLimitKey(key))
        return false;

    const QString major = masterString(json, Constants::Settings::Master::kMajor);
    const QString minor = masterString(json, Constants::Settings::Master::kMinor);

    if (major == Constants::Settings::SettingTab::kPrinter)
        return false;

    if (major == Constants::Settings::SettingTab::kMaterial &&
        (minor == "Extruder" || minor == "Purge" || minor == "Retraction"))
        return false;

    return true;
}

QString formatVelocity(Velocity speed) {
    const Velocity unit = PreferencesManager::getInstance()->getVelocityUnit();
    return QString::number(speed.to(unit), 'g', 6) + " " + PreferencesManager::getInstance()->getVelocityUnitText();
}
} // namespace

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

    checkDynamicDependencies();
}

void SettingSpeedSpinBox::checkDynamicDependencies() {
    if (!m_dependency_enabled) {
        clearNotification();
        styleLabel(true);
        emit warnParent(warningCountDelta(false, m_warn));
        return;
    }

    if (!hasConsistentEffectiveSpeed()) {
        setNotification("Multiple Values");
        styleLabel(false);
        emit warnParent(warningCountDelta(true, m_warn));
        return;
    }

    const QString warning = speedLimitWarning();
    const bool warning_active = !warning.isEmpty();
    if (warning_active) {
        setNotification(warning);
        styleLabel(false);
        m_key_label->setToolTip("<html><body><p><b>" + warning + "</b></p><p>" +
                                QString::fromStdString(m_json.at(Constants::Settings::Master::kToolTip)) +
                                "</p></body></html>");
    }
    else {
        clearNotification();
        styleLabel(true);
    }

    emit warnParent(warningCountDelta(warning_active, m_warn));
}

Velocity SettingSpeedSpinBox::effectiveSpeed() const {
    const double default_value = m_json[Constants::Settings::Master::kDefault].get<double>();
    const double global_value = m_sb->contains(m_key) ? m_sb->setting<double>(m_key) : default_value;

    if (!m_settings_bases.isEmpty())
        return Velocity(effectiveValueHelper<double>(m_key, 0, global_value));

    return Velocity(global_value);
}

bool SettingSpeedSpinBox::hasConsistentEffectiveSpeed() const {
    if (m_settings_bases.size() <= 1)
        return true;

    const double default_value = m_json[Constants::Settings::Master::kDefault].get<double>();
    const double global_value = m_sb->contains(m_key) ? m_sb->setting<double>(m_key) : default_value;
    const double first_value = effectiveValueHelper<double>(m_key, 0, global_value);

    for (int index = 1, end = m_settings_bases.size(); index < end; ++index) {
        if (effectiveValueHelper<double>(m_key, index, global_value) != first_value)
            return false;
    }

    return true;
}

QString SettingSpeedSpinBox::speedLimitWarning() const {
    if (m_sb.isNull())
        return QString();

    const Velocity min_xy_speed(effectiveDouble(PRS::MachineSpeed::kMinXYSpeed));
    const Velocity max_xy_speed(effectiveDouble(PRS::MachineSpeed::kMaxXYSpeed));
    const bool has_min = isActiveSpeedLimit(min_xy_speed);
    const bool has_max = isActiveSpeedLimit(max_xy_speed);
    const bool invalid_range = has_min && has_max && min_xy_speed > max_xy_speed;

    if (isXYSpeedLimitKey(m_key)) {
        if (invalid_range)
            return QString("Minimum XY Speed (") + formatVelocity(min_xy_speed) +
                   ") is greater than Maximum XY Speed (" + formatVelocity(max_xy_speed) + ").";

        return QString();
    }

    if (!isPrinterXYMotionSpeed(m_key, m_json) || invalid_range)
        return QString();

    const Velocity current_speed = effectiveSpeed();
    const QString display = masterString(m_json, Constants::Settings::Master::kDisplay);

    if (has_min && current_speed < min_xy_speed)
        return display + " (" + formatVelocity(current_speed) + ") is below Minimum XY Speed (" +
               formatVelocity(min_xy_speed) + ").";

    if (has_max && current_speed > max_xy_speed)
        return display + " (" + formatVelocity(current_speed) + ") exceeds Maximum XY Speed (" +
               formatVelocity(max_xy_speed) + ").";

    return QString();
}
} // namespace ORNL
