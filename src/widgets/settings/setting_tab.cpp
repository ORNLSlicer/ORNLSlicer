#include "widgets/settings/setting_tab.h"

#include "widgets/settings/double_spin_subtypes/setting_accel_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_ang_vel_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_angle_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_area_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_density_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_distance_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_speed_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_temperature_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_time_spin_box.h"
#include "widgets/settings/double_spin_subtypes/setting_voltage_spin_box.h"
#include "widgets/settings/setting_check_box.h"
#include "widgets/settings/setting_combo_box.h"
#include "widgets/settings/setting_double_spin_box.h"
#include "widgets/settings/setting_line_edit.h"
#include "widgets/settings/setting_numbered_list.h"
#include "widgets/settings/setting_plain_text_edit.h"
#include "widgets/settings/setting_spin_box.h"

namespace ORNL {

SettingTab::SettingTab(QWidget* parent, QString name, QIcon icon, int index, bool isHidden,
                       QSharedPointer<SettingsBase> sb)
    : QWidget(parent), m_name(name), m_icon(icon), m_sb(sb), m_index(index) {
    this->setupWidget(isHidden);

    m_size = 0;
    m_warning_count = 0;

    m_creation_mapping = {{"number", &SettingSpinBox::createInstance},
                          {"positive_int", &SettingSpinBox::createInstance},
                          {"location", &SettingDistanceSpinBox::createInstance},
                          {"distance", &SettingDistanceSpinBox::createInstance},
                          {"unitless_float", &SettingDoubleSpinBox::createInstance},
                          {"boolean", &SettingCheckBox::createInstance},
                          {"enumeration", &SettingComboBox::createInstance},
                          {"voltage", &SettingVoltageSpinBox::createInstance},
                          {"speed", &SettingSpeedSpinBox::createInstance},
                          {"rpm", &SettingDoubleSpinBox::createInstance},
                          {"accel", &SettingAccelSpinBox::createInstance},
                          {"string", &SettingLineEdit::createInstance},
                          {"multiline_text", &SettingPlainTextEdit::createInstance},
                          {"density", &SettingDoubleSpinBox::createInstance},
                          {"ang_vel", &SettingAngVelSpinBox::createInstance},
                          {"time", &SettingTimeSpinBox::createInstance},
                          {"percentage", &SettingDoubleSpinBox::createInstance},
                          {"percentage100", &SettingDoubleSpinBox::createInstance},
                          {"temperature", &SettingTemperatureSpinBox::createInstance},
                          {"angle", &SettingAngleSpinBox::createInstance},
                          {"area", &SettingAreaSpinBox::createInstance},
                          {"numbered_list", &SettingNumberedList::createInstance},
                          {"power", &SettingSpinBox::createInstance},
                          {"density", &SettingDensitySpinBox::createInstance}};
}

void SettingTab::setSettingBase(QSharedPointer<SettingsBase> sb) {
    m_sb = sb;
    for (QSharedPointer<SettingRowBase> curr_row : m_rows) {
        curr_row->setSettingsBase(m_sb);
    }
}

QList<QSharedPointer<SettingRowBase>> SettingTab::getRows() { return m_rows.values(); }

QSharedPointer<SettingRowBase> SettingTab::getRow(QString key) { return m_rows[key]; }

int SettingTab::getIndex() { return m_index; }

QString SettingTab::getName() { return m_name; }

void SettingTab::addRow(QString key, fifojson& json) {
    QSharedPointer<SettingRowBase> newRow =
        QSharedPointer<SettingRowBase>(m_creation_mapping[json.at(Constants::Settings::Master::kType)](
            this, m_sb, key, json, m_container_layout, m_size));

    m_rows.insert(key, newRow);
    ++m_size;
}

void SettingTab::keyModified(QString key) { emit modified(key); }

void SettingTab::expandTab() {
    m_container->show();
    m_header->setStatus(true);
}

void SettingTab::shrinkTab() {
    m_container->hide();
    m_header->setStatus(false);
}

void SettingTab::hideTab() {
    shrinkTab();
    emit removeTabFromList(m_name);
}

void SettingTab::showTab() { m_header->showHeader(); }

void SettingTab::reload() {
    for (QSharedPointer<SettingRowBase> curr_row : m_rows) {
        curr_row->reloadValue();
    }
}

void SettingTab::settingsBasesSelected(QList<QSharedPointer<SettingsBase>> settings_bases) {
    if (m_settings_bases != settings_bases) {
        m_warning_count = 0; // reset the count of warnings if a new settings base has been selected
        m_settings_bases = settings_bases;
        for (QSharedPointer<SettingRowBase> curr_row : m_rows) {
            curr_row->setBases(m_settings_bases);
            curr_row->reloadValue();
        }
    }
}

void SettingTab::headerWarning(int count) {
    emit warnPane(count);
    m_warning_count = m_warning_count + count; // keep track of all warnings from children (setting_rows)
    // if there is more than 1 warning, change the header icon to show the warning
    if (m_warning_count > 0) {
        m_header->setIcon(QIcon(":/icons/warning.png"));
    }
    else {
        m_header->setIcon(QIcon(":/icons/ornlslicer_logo.png"));
    }
}

void SettingTab::setupWidget(bool isHidden) {
    this->setupSubWidgets(isHidden);
    this->setupLayouts();
    this->setupInsert();
    this->setupEvents();
}

void SettingTab::setupSubWidgets(bool isHidden) {
    // Header
    m_header = new SettingHeader(this, m_name, m_icon);
    if (isHidden)
        m_header->hide();

    // Container
    m_container = new QFrame(this);
    m_container->hide();
}

void SettingTab::setupLayouts() {
    // Main Layout
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 10, 0);

    // Container
    m_container_layout = new QGridLayout(m_container);
    m_container_layout->setContentsMargins(0, 0, 0, 0);

    // Ensure middle column gets most space.
    m_container_layout->setColumnStretch(1, 1);
}

void SettingTab::setupStyle() { m_header->setupStyle(); }

void SettingTab::setupInsert() {
    m_layout->addWidget(m_header);
    m_layout->addWidget(m_container);
}

void SettingTab::setupEvents() {
    connect(m_header, &SettingHeader::expand, this, &SettingTab::expandTab);
    connect(m_header, &SettingHeader::shrink, this, &SettingTab::shrinkTab);
    connect(m_header, &SettingHeader::hideHeader, this, &SettingTab::hideTab);
}
} // Namespace ORNL
