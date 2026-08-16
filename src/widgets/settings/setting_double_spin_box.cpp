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
#include <qstringlist.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "managers/preferences_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {
namespace {
constexpr int kModifyFeedrateLayerTimeMethod = 1;
constexpr int kFrictionStirMachineType = static_cast<int>(MachineType::kFrictionStir);
constexpr int kMeldSyntax = static_cast<int>(GcodeSyntax::kMeld);
constexpr double kMaximumDepositionRate = 10000.0;

const QString kInfillMaximumPathLength = "infill_maximum_path_length";

QString masterString(const fifojson& json, const std::string& key) {
    return QString::fromStdString(json.at(key).get<std::string>());
}

bool isOneOf(const QString& key, const QStringList& keys) {
    return keys.contains(key);
}

bool isConfiguredRange(double min_value, double max_value) {
    return min_value != 0 || max_value != 0;
}

bool isValidRange(double min_value, double max_value) {
    return isConfiguredRange(min_value, max_value) && min_value < max_value;
}

QString formatRpm(double value) {
    return QString::number(value, 'g', 6) + " rpm";
}

QString formatDistance(double value) {
    const Distance unit = PreferencesManager::getInstance()->getDistanceUnit();
    return QString::number(Distance(value).to(unit), 'g', 6) + " " +
           PreferencesManager::getInstance()->getDistanceUnitText();
}

QString formatTime(double value) {
    const Time unit = PreferencesManager::getInstance()->getTimeUnit();
    return QString::number(Time(value).to(unit), 'g', 6) + " " + PreferencesManager::getInstance()->getTimeUnitText();
}

QString formatTypedValue(double value, const QString& type) {
    if (type == "distance" || type == "location") return formatDistance(value);
    if (type == "time") return formatTime(value);
    if (type == "rpm") return formatRpm(value);
    if (type == "percentage" || type == "percentage100") return QString::number(value, 'g', 6) + "%";

    return QString::number(value, 'g', 6);
}

QString dimensionRangeWarning(const QString& key, const QString& min_key, const QString& max_key, double min_value,
                              double max_value, const QString& min_label, const QString& max_label) {
    if (!isConfiguredRange(min_value, max_value) || min_value < max_value) return QString();

    if (key == min_key || key == max_key) {
        return min_label + " (" + formatDistance(min_value) + ") must be less than " + max_label + " (" +
               formatDistance(max_value) + ").";
    }

    return QString();
}

QString boundsWarning(const QString& display, double value, double min_value, double max_value, const QString& axis) {
    if (!isValidRange(min_value, max_value) || (value >= min_value && value <= max_value)) return QString();

    return display + " (" + formatDistance(value) + ") is outside the " + axis + " build volume range (" +
           formatDistance(min_value) + " to " + formatDistance(max_value) + ").";
}

QString enabledSettingWarning(const QString& display, double value, const QString& required_description,
                              const QString& type) {
    if (value > 0) return QString();

    return display + " must be greater than zero " + required_description + " (currently " +
           formatTypedValue(value, type) + ").";
}
}  // namespace

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
    double displayed_cur = cur;
    if (type == "rpm") {
        this->setMaximum(9999.99);
        unitText = "rpm";
    }
    else if (type == "deposition_rate") {
        this->setMinimum(0);
        if (usesMeldVelocityDepositionRate()) {
            const Velocity unit = PreferencesManager::getInstance()->getVelocityUnit();
            this->setMaximum(Constants::Limits::Maximums::kMaxSpeed.to(unit));
            this->setDecimals(m_precision);
            displayed_cur = displayedDepositionRateValue(cur);
            unitText = PreferencesManager::getInstance()->getVelocityUnitText();
        }
        else if (usesIntegerDepositionRate()) {
            this->setMaximum(kMaximumDepositionRate);
            this->setDecimals(0);
        }
        else {
            this->setMaximum(kMaximumDepositionRate);
            unitText = "rpm";
        }
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
        this->setMinimum(Constants::Limits::Minimums::kMinUnitlessFloat);
        this->setMaximum(Constants::Limits::Maximums::kMaxUnitlessFloat);
        this->setDecimals(m_precision);
    }

    this->setValue(displayed_cur);

    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SettingDoubleSpinBox::valueChanged);
    connect(this, &SettingDoubleSpinBox::modified, parent, &SettingTab::keyModified);
    connect(this, &SettingDoubleSpinBox::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight);

    // Set setting units
    m_unit_label.reset(new QLabel(unitText));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
    registerRowWidget(this);
    updateDepositionRatePresentation();
}

SettingRowBase* SettingDoubleSpinBox::createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                                     fifojson json, QGridLayout* layout, int index) {
    return new SettingDoubleSpinBox(parent, sb, key, json, layout, index);
}

void SettingDoubleSpinBox::setEnabled(bool enabled) {
    SettingRowBase::setEnabled(enabled);
    applyWidgetState(static_cast<QDoubleSpinBox*>(this));
}

void SettingDoubleSpinBox::setNotification(QString msg) {
    applyNotification(msg, true);
}

void SettingDoubleSpinBox::applyNotification(QString msg, bool show_tooltip) {
    // apply checkbox stylesheet
    // set pop-up/tooltip
    this->setStyleFromFile(this, m_theme_path + "setting_rows_warning.qss");
    this->setToolTip(msg);
    if (show_tooltip) QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void SettingDoubleSpinBox::clearNotification() {
    // clear notification information
    this->setStyleFromFile(this, m_theme_path + "setting_rows_normal.qss");
    this->setToolTip("");
}

void SettingDoubleSpinBox::hide() {
    SettingRowBase::hide();
    applyWidgetState(static_cast<QDoubleSpinBox*>(this));
}

void SettingDoubleSpinBox::show() {
    SettingRowBase::show();
    applyWidgetState(static_cast<QDoubleSpinBox*>(this));
}

void SettingDoubleSpinBox::valueChanged(QVariant val) {
    if (m_warn)
        emit warnParent(-1);  // if a value is changed, it changes for all selected settings bases, so remove a warning.
    m_warn = false;
    valueChangedHelper<double>(storedDepositionRateValue(val.toDouble()));
    emit modified(m_key);
}

void SettingDoubleSpinBox::reloadValue() {
    this->blockSignals(true);
    updateDepositionRatePresentation();
    bool consistent = true;
    double cur = reloadValueHelper<double>(consistent);
    if (consistent)
        setValue(displayedDepositionRateValue(cur));

    this->blockSignals(false);
    emit modified(m_key);

    checkDynamicDependencies();
}

void SettingDoubleSpinBox::wheelEvent(QWheelEvent* event) {
    if (!hasFocus())
        event->ignore();
    else
        QDoubleSpinBox::wheelEvent(event);
}

void SettingDoubleSpinBox::checkDynamicDependencies() {
    updateDepositionRatePresentation();
    syncDisplayedDepositionRateValue();

    if (!m_dependency_enabled) {
        clearNotification();
        styleLabel(true);
        emit warnParent(warningCountDelta(false, m_warn));
        return;
    }

    if (!hasConsistentEffectiveDouble()) {
        applyNotification("Multiple Values", false);
        styleLabel(false);
        emit warnParent(warningCountDelta(true, m_warn));
        return;
    }

    const QString warning     = dynamicDependencyWarning();
    const bool warning_active = !warning.isEmpty();
    if (warning_active) {
        applyNotification(warning, false);
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

int SettingDoubleSpinBox::effectiveSettingsBaseCount() const {
    return m_settings_bases.isEmpty() ? 1 : m_settings_bases.size();
}

double SettingDoubleSpinBox::effectiveDouble() const {
    return effectiveDouble(0);
}

double SettingDoubleSpinBox::effectiveDouble(int settings_base_index) const {
    const double default_value = m_json[Constants::Settings::Master::kDefault].get<double>();
    const double global_value  = m_sb->contains(m_key) ? m_sb->setting<double>(m_key) : default_value;

    if (!m_settings_bases.isEmpty()) return effectiveValueHelper<double>(m_key, settings_base_index, global_value);

    return global_value;
}

double SettingDoubleSpinBox::effectiveDouble(const QString& key) const {
    return effectiveDouble(key, 0);
}

double SettingDoubleSpinBox::effectiveDouble(const QString& key, int settings_base_index) const {
    const double global_value = m_sb->contains(key) ? m_sb->setting<double>(key) : 0.0;

    if (!m_settings_bases.isEmpty()) return effectiveValueHelper<double>(key, settings_base_index, global_value);

    return global_value;
}

bool SettingDoubleSpinBox::effectiveBool(const QString& key) const {
    return effectiveBool(key, 0);
}

bool SettingDoubleSpinBox::effectiveBool(const QString& key, int settings_base_index) const {
    const bool global_value = m_sb->contains(key) ? m_sb->setting<bool>(key) : false;

    if (!m_settings_bases.isEmpty()) return effectiveValueHelper<bool>(key, settings_base_index, global_value);

    return global_value;
}

int SettingDoubleSpinBox::effectiveInt(const QString& key) const {
    return effectiveInt(key, 0);
}

int SettingDoubleSpinBox::effectiveInt(const QString& key, int settings_base_index) const {
    const int global_value = m_sb->contains(key) ? m_sb->setting<int>(key) : 0;

    if (!m_settings_bases.isEmpty()) return effectiveValueHelper<int>(key, settings_base_index, global_value);

    return global_value;
}

bool SettingDoubleSpinBox::usesIntegerDepositionRate() const { return usesIntegerDepositionRate(0); }

bool SettingDoubleSpinBox::usesIntegerDepositionRate(int settings_base_index) const {
    return effectiveInt(PRS::MachineSetup::kMachineType, settings_base_index) == kFrictionStirMachineType &&
           !usesMeldVelocityDepositionRate(settings_base_index);
}

bool SettingDoubleSpinBox::usesMeldVelocityDepositionRate() const { return usesMeldVelocityDepositionRate(0); }

bool SettingDoubleSpinBox::usesMeldVelocityDepositionRate(int settings_base_index) const {
    return effectiveInt(PRS::MachineSetup::kSyntax, settings_base_index) == kMeldSyntax &&
           effectiveInt(PRS::MachineSetup::kMachineType, settings_base_index) == kFrictionStirMachineType;
}

double SettingDoubleSpinBox::displayedDepositionRateValue(double stored_value) const {
    const QString type = masterString(m_json, Constants::Settings::Master::kType);
    if (type == "deposition_rate" && usesMeldVelocityDepositionRate())
        return Velocity(stored_value).to(PreferencesManager::getInstance()->getVelocityUnit());

    return stored_value;
}

double SettingDoubleSpinBox::storedDepositionRateValue(double displayed_value) const {
    const QString type = masterString(m_json, Constants::Settings::Master::kType);
    if (type == "deposition_rate" && usesMeldVelocityDepositionRate()) {
        Velocity stored_value;
        stored_value.from(displayed_value, PreferencesManager::getInstance()->getVelocityUnit());
        return stored_value();
    }

    return displayed_value;
}

void SettingDoubleSpinBox::syncDisplayedDepositionRateValue() {
    const QString type = masterString(m_json, Constants::Settings::Master::kType);
    if (type != "deposition_rate")
        return;

    bool consistent = true;
    const double cur = reloadValueHelper<double>(consistent);
    if (!consistent)
        return;

    const bool was_blocked = blockSignals(true);
    setValue(displayedDepositionRateValue(cur));
    blockSignals(was_blocked);
}

void SettingDoubleSpinBox::updateDepositionRatePresentation() {
    const QString type = masterString(m_json, Constants::Settings::Master::kType);
    if (type != "deposition_rate")
        return;

    if (usesMeldVelocityDepositionRate()) {
        const Velocity unit = PreferencesManager::getInstance()->getVelocityUnit();
        setDecimals(m_precision);
        setMinimum(0);
        setMaximum(Constants::Limits::Maximums::kMaxSpeed.to(unit));
        if (!m_key_label.isNull()) {
            QString display = masterString(m_json, Constants::Settings::Master::kDisplay);
            display.replace("Extruder Speed", "Deposition Rate");
            m_key_label->setText(display);
        }
        if (!m_unit_label.isNull())
            m_unit_label->setText(PreferencesManager::getInstance()->getVelocityUnitText());
    }
    else if (usesIntegerDepositionRate()) {
        setDecimals(0);
        setMaximum(kMaximumDepositionRate);
        if (!m_key_label.isNull()) {
            QString display = masterString(m_json, Constants::Settings::Master::kDisplay);
            display.replace("Extruder Speed", "Deposition Value");
            m_key_label->setText(display);
        }
        if (!m_unit_label.isNull())
            m_unit_label->setText("");
    }
    else {
        setDecimals(2);
        setMaximum(kMaximumDepositionRate);
        if (!m_key_label.isNull())
            m_key_label->setText(masterString(m_json, Constants::Settings::Master::kDisplay));
        if (!m_unit_label.isNull())
            m_unit_label->setText("rpm");
    }
}

bool SettingDoubleSpinBox::hasConsistentEffectiveDouble() const {
    if (m_settings_bases.size() <= 1) return true;

    const double default_value = m_json[Constants::Settings::Master::kDefault].get<double>();
    const double global_value  = m_sb->contains(m_key) ? m_sb->setting<double>(m_key) : default_value;
    const double first_value   = effectiveValueHelper<double>(m_key, 0, global_value);

    for (int index = 1, end = m_settings_bases.size(); index < end; ++index) {
        if (effectiveValueHelper<double>(m_key, index, global_value) != first_value) return false;
    }

    return true;
}

QString SettingDoubleSpinBox::dynamicDependencyWarning() const {
    for (int index = 0, end = effectiveSettingsBaseCount(); index < end; ++index) {
        const QString warning = dynamicDependencyWarning(index);
        if (!warning.isEmpty()) return warning;
    }

    return QString();
}

QString SettingDoubleSpinBox::dynamicDependencyWarning(int settings_base_index) const {
    if (m_sb.isNull()) return QString();

    const QString display = masterString(m_json, Constants::Settings::Master::kDisplay);
    const QString type = masterString(m_json, Constants::Settings::Master::kType);
    const double value = effectiveDouble(settings_base_index);
    const bool rpm_based_deposition =
        type == "rpm" || (type == "deposition_rate" && !usesIntegerDepositionRate(settings_base_index) &&
                          !usesMeldVelocityDepositionRate(settings_base_index));

    const double min_extruder_speed   = effectiveDouble(PRS::MachineSpeed::kMinExtruderSpeed, settings_base_index);
    const double max_extruder_speed   = effectiveDouble(PRS::MachineSpeed::kMaxExtruderSpeed, settings_base_index);
    const bool has_min_extruder_speed = min_extruder_speed > 0;
    const bool has_max_extruder_speed = max_extruder_speed > 0;
    const bool invalid_extruder_range =
        has_min_extruder_speed && has_max_extruder_speed && min_extruder_speed > max_extruder_speed;

    if (m_key == PRS::MachineSpeed::kMinExtruderSpeed || m_key == PRS::MachineSpeed::kMaxExtruderSpeed) {
        if (invalid_extruder_range)
            return "Minimum Extruder Speed (" + formatRpm(min_extruder_speed) +
                   ") is greater than Maximum Extruder Speed (" + formatRpm(max_extruder_speed) + ").";
    }
    else if (rpm_based_deposition && !invalid_extruder_range) {
        if (has_min_extruder_speed && value < min_extruder_speed)
            return display + " (" + formatRpm(value) + ") is below Minimum Extruder Speed (" +
                   formatRpm(min_extruder_speed) + ").";

        if (has_max_extruder_speed && value > max_extruder_speed)
            return display + " (" + formatRpm(value) + ") exceeds Maximum Extruder Speed (" +
                   formatRpm(max_extruder_speed) + ").";
    }

    const double x_min = effectiveDouble(PRS::Dimensions::kXMin, settings_base_index);
    const double x_max = effectiveDouble(PRS::Dimensions::kXMax, settings_base_index);
    const double y_min = effectiveDouble(PRS::Dimensions::kYMin, settings_base_index);
    const double y_max = effectiveDouble(PRS::Dimensions::kYMax, settings_base_index);
    const double z_min = effectiveDouble(PRS::Dimensions::kZMin, settings_base_index);
    const double z_max = effectiveDouble(PRS::Dimensions::kZMax, settings_base_index);
    const double w_min = effectiveDouble(PRS::Dimensions::kWMin, settings_base_index);
    const double w_max = effectiveDouble(PRS::Dimensions::kWMax, settings_base_index);

    QString warning = dimensionRangeWarning(m_key, PRS::Dimensions::kXMin, PRS::Dimensions::kXMax, x_min, x_max,
                                            "Minimum X", "Maximum X");
    if (!warning.isEmpty()) return warning;

    warning = dimensionRangeWarning(m_key, PRS::Dimensions::kYMin, PRS::Dimensions::kYMax, y_min, y_max, "Minimum Y",
                                    "Maximum Y");
    if (!warning.isEmpty()) return warning;

    warning = dimensionRangeWarning(m_key, PRS::Dimensions::kZMin, PRS::Dimensions::kZMax, z_min, z_max, "Minimum Z",
                                    "Maximum Z");
    if (!warning.isEmpty()) return warning;

    warning = dimensionRangeWarning(m_key, PRS::Dimensions::kWMin, PRS::Dimensions::kWMax, w_min, w_max, "Minimum W",
                                    "Maximum W");
    if (!warning.isEmpty()) return warning;

    if (m_key == PRS::Dimensions::kPurgeX || m_key == PS::Perimeter::kEnableLeadInX)
        return boundsWarning(display, value, x_min, x_max, "X");

    if (m_key == PRS::Dimensions::kPurgeY || m_key == PS::Perimeter::kEnableLeadInY)
        return boundsWarning(display, value, y_min, y_max, "Y");

    if (m_key == PRS::Dimensions::kPurgeZ) return boundsWarning(display, value, z_min, z_max, "Z");

    if (m_key == PRS::Dimensions::kDoffingHeight) return boundsWarning(display, value, w_min, w_max, "W");

    if (m_key == PS::Layer::kLayerHeight && value <= 0)
        return display + " must be greater than zero (currently " + formatDistance(value) + ").";

    const QStringList bead_width_keys {
        PS::Layer::kBeadWidth,
        PS::Perimeter::kBeadWidth,
        PS::Inset::kBeadWidth,
        PS::Skeleton::kBeadWidth,
        PS::Skin::kBeadWidth,
        PS::Infill::kBeadWidth,
        MS::PlatformAdhesion::kRaftBeadWidth,
        MS::PlatformAdhesion::kBrimBeadWidth,
        MS::PlatformAdhesion::kSkirtBeadWidth,
    };

    if (isOneOf(m_key, bead_width_keys)) {
        const double layer_height = effectiveDouble(PS::Layer::kLayerHeight, settings_base_index);
        if (value <= 0) return display + " must be greater than zero (currently " + formatDistance(value) + ").";

        if (layer_height > 0 && value < layer_height)
            return display + " (" + formatDistance(value) + ") is smaller than Layer Height (" +
                   formatDistance(layer_height) + ").";
    }

    if (m_key == PS::Infill::kMinPathLength || m_key == kInfillMaximumPathLength) {
        const double min_path_length = effectiveDouble(PS::Infill::kMinPathLength, settings_base_index);
        const double max_path_length = effectiveDouble(kInfillMaximumPathLength, settings_base_index);
        if (min_path_length > 0 && max_path_length > 0 && min_path_length > max_path_length)
            return "Minimum Infill Path Length (" + formatDistance(min_path_length) +
                   ") is greater than Maximum Infill Path Length (" + formatDistance(max_path_length) + ").";
    }

    const QStringList lift_distance_keys {
        PS::Travel::kLiftHeight,           PS::Travel::kFinalLiftDistance,
        MS::SpiralLift::kLiftHeight,       MS::Slowdown::kPerimeterLiftDistance,
        MS::Slowdown::kInsetLiftDistance,  MS::Slowdown::kSkinLiftDistance,
        MS::Slowdown::kInfillLiftDistance, MS::Slowdown::kSkeletonLiftDistance,
        MS::TipWipe::kPerimeterLiftHeight, MS::TipWipe::kInsetLiftHeight,
        MS::TipWipe::kSkinLiftHeight,      MS::TipWipe::kInfillLiftHeight,
        MS::TipWipe::kSkeletonLiftHeight,
    };

    if (isOneOf(m_key, lift_distance_keys) && value > 0) {
        const double z_speed = effectiveDouble(PRS::MachineSpeed::kZSpeed, settings_base_index);
        if (z_speed <= 0) return display + " requires Z Speed to be greater than zero.";

        if (isValidRange(z_min, z_max) && value > (z_max - z_min))
            return display + " (" + formatDistance(value) + ") exceeds the Z build volume range (" +
                   formatDistance(z_max - z_min) + ").";
    }

    if (m_key == PS::Travel::kMinTravelForLift && value > 0 &&
        effectiveDouble(PS::Travel::kLiftHeight, settings_base_index) <= 0)
        return display + " is set, but Travel Lift Height is zero.";

    const double fan_min_speed = effectiveDouble(MS::Cooling::kMinSpeed, settings_base_index);
    const double fan_max_speed = effectiveDouble(MS::Cooling::kMaxSpeed, settings_base_index);
    if ((m_key == MS::Cooling::kMinSpeed || m_key == MS::Cooling::kMaxSpeed) && fan_min_speed > fan_max_speed)
        return "Min Fan Speed (" + formatTypedValue(fan_min_speed, type) + ") is greater than Max Fan Speed (" +
               formatTypedValue(fan_max_speed, type) + ").";

    const bool force_layer_time = effectiveBool(MS::Cooling::kForceMinLayerTime, settings_base_index);
    const bool use_feedrate_layer_time =
        effectiveInt(MS::Cooling::kForceMinLayerTimeMethod, settings_base_index) == kModifyFeedrateLayerTimeMethod;
    const double min_layer_time = effectiveDouble(MS::Cooling::kMinLayerTime, settings_base_index);
    const double max_layer_time = effectiveDouble(MS::Cooling::kMaxLayerTime, settings_base_index);

    if (force_layer_time && m_key == MS::Cooling::kMinLayerTime) {
        if (value <= 0)
            return enabledSettingWarning(display, value, "when Force Min / Max Layer Time is enabled", type);

        if (use_feedrate_layer_time && max_layer_time > 0 && value > max_layer_time)
            return "Minimum Layer Time (" + formatTime(value) + ") is greater than Maximum Layer Time (" +
                   formatTime(max_layer_time) + ").";
    }

    if (force_layer_time && use_feedrate_layer_time && m_key == MS::Cooling::kMaxLayerTime) {
        if (value <= 0)
            return enabledSettingWarning(display, value, "when Modify Feedrate layer-time control is selected", type);

        if (min_layer_time > 0 && value < min_layer_time)
            return "Maximum Layer Time (" + formatTime(value) + ") is less than Minimum Layer Time (" +
                   formatTime(min_layer_time) + ").";
    }

    if (force_layer_time && use_feedrate_layer_time && m_key == MS::Cooling::kExtruderScaleFactor && value <= 0)
        return enabledSettingWarning(display, value, "when Modify Feedrate layer-time control is selected", type);

    return QString();
}
}  // namespace ORNL
