#include "widgets/settings/setting_tab.h"

#include <QGroupBox>
#include <cstddef>

#include <qboxlayout.h>
#include <qframe.h>
#include <qgridlayout.h>
#include <qicon.h>
#include <qlist.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"
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
#include "widgets/settings/setting_file_path.h"
#include "widgets/settings/setting_header.h"
#include "widgets/settings/setting_line_edit.h"
#include "widgets/settings/setting_numbered_list.h"
#include "widgets/settings/setting_plain_text_edit.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_spin_box.h"
#include "widgets/settings/vector2_input_widget.h"
#include "widgets/settings/vector3_input_widget.h"

namespace ORNL {
namespace {
QString jsonString(const fifojson& object, const std::string& key) {
    return QString::fromStdString(object.at(key).get<std::string>());
}

QString componentSetting(const fifojson& component) {
    return jsonString(component, Constants::Settings::Input::kSetting);
}

QString componentLabel(const fifojson& component) {
    return jsonString(component, Constants::Settings::Input::kLabel);
}

fifojson makeInputRowJson(const fifojson& setting_json, const fifojson& input) {
    fifojson row_json                               = setting_json;
    row_json[Constants::Settings::Master::kDisplay] = input.at(Constants::Settings::Input::kDisplay);
    row_json[Constants::Settings::Master::kToolTip] = input.at(Constants::Settings::Input::kToolTip);
    row_json[Constants::Settings::Master::kDepends] = input.at(Constants::Settings::Input::kDepends);
    row_json[Constants::Settings::Master::kMinor]   = input.at(Constants::Settings::Input::kMinor);
    row_json[Constants::Settings::Master::kMajor]   = input.at(Constants::Settings::Input::kMajor);
    row_json[Constants::Settings::Master::kLocal]   = input.at(Constants::Settings::Input::kLocal);
    if (input.contains(Constants::Settings::Input::kVisibleIf))
        row_json[Constants::Settings::Master::kVisibleIf] = input.at(Constants::Settings::Input::kVisibleIf);
    return row_json;
}
}  // namespace

SettingTab::SettingTab(QWidget* parent, QString name, QIcon icon, int index, bool isHidden,
                       QSharedPointer<SettingsBase> sb)
    : QWidget(parent), m_name(name), m_icon(icon), m_sb(sb), m_index(index) {
    this->setupWidget(isHidden);

    m_size          = 0;
    m_warning_count = 0;

    m_creation_mapping = {{"number", &SettingSpinBox::createInstance},
                          {"non_negative_int", &SettingSpinBox::createInstance},
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
                          {"file_path", &SettingFilePath::createInstance},
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
    for (QSharedPointer<SettingRowBase> curr_row : m_rows) { curr_row->setSettingsBase(m_sb); }
}

QList<QSharedPointer<SettingRowBase>> SettingTab::getRows() {
    return m_rows.values();
}

QSharedPointer<SettingRowBase> SettingTab::getRow(QString key) {
    if (m_rows.contains(key)) return m_rows[key];

    return m_row_aliases.value(key);
}

int SettingTab::getIndex() {
    return m_index;
}

QString SettingTab::getName() {
    return m_name;
}

bool SettingTab::hasShownRows() const {
    for (const QSharedPointer<SettingRowBase>& row : m_rows) {
        if (row->isShown()) return true;
    }

    return false;
}

void SettingTab::refreshGroupVisibility(bool searching) {
    m_group_searching = searching;
    for (auto it = m_group_cells.begin(); it != m_group_cells.end(); ++it)
        it.value()->setVisible(m_rows.value(it.key())->isShown());
    for (auto& group : m_groups) {
        bool visible = false;
        for (const auto& key : group.keys) visible |= m_rows.value(key)->isShown();
        group.frame->setVisible(visible);
        if (group.toggle != nullptr) {
            const bool expanded = searching || !group.collapsed;
            group.content->setVisible(expanded);
            group.toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            group.toggle->setEnabled(!searching);
            group.toggle->setToolTip(searching ? "Matching regions stay expanded while searching."
                                               : "Expand or collapse this region's weld schedule settings.");
        }
    }
}

std::pair<QGridLayout*, int> SettingTab::groupedRowLayout(const QString& key, const fifojson& metadata) {
    const QString region  = jsonString(metadata, Constants::Settings::Group::kRegion);
    const QString section = jsonString(metadata, Constants::Settings::Group::kSection);
    const QString path    = region + "/" + section;
    if (!m_groups.contains(region)) {
        m_container_layout->setAlignment(Qt::AlignTop);
        SettingGroup group;
        group.frame = new QFrame(m_container);
        group.frame->setObjectName("setting_group_" + region);
        auto* layout = new QVBoxLayout(group.frame);
        layout->setContentsMargins(8, 4, 0, 4);
        group.toggle = new QToolButton(group.frame);
        group.toggle->setObjectName("setting_group_toggle_" + region);
        group.toggle->setProperty("settingGroupToggle", true);
        group.toggle->setText(region);
        group.toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        group.toggle->setAutoRaise(true);
        group.content = new QFrame(group.frame);
        group.content->setObjectName("setting_group_content_" + region);
        group.layout = new QGridLayout(group.content);
        group.layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(group.toggle);
        layout->addWidget(group.content);
        connect(group.toggle, &QToolButton::clicked, this, [this, region]() {
            m_groups[region].collapsed = !m_groups[region].collapsed;
            refreshGroupVisibility(m_group_searching);
        });
        m_container_layout->addWidget(group.frame, m_size++, 0, 1, 5);
        m_groups.insert(region, group);
    }
    if (!m_groups.contains(path)) {
        SettingGroup group;
        group.frame = new QGroupBox(section, m_groups[region].content);
        group.frame->setObjectName("setting_group_" + path);
        group.content = group.frame;
        group.layout  = new QGridLayout(group.content);
        group.layout->setContentsMargins(8, 8, 8, 8);
        m_groups[region].layout->addWidget(group.frame, m_groups[region].layout->rowCount(), 0);
        m_groups.insert(path, group);
    }
    auto& group = m_groups[path];
    group.keys.append(key);
    m_groups[region].keys.append(key);
    const int index = group.size++;
    if (metadata.value(Constants::Settings::Group::kColumns, 1) == 2) {
        auto* cell   = new QWidget(group.content);
        auto* layout = new QGridLayout(cell);
        layout->setColumnStretch(1, 1);
        group.layout->addWidget(cell, index / 2, index % 2);
        group.layout->setColumnStretch(index % 2, 1);
        m_group_cells.insert(key, cell);
        return {layout, 0};
    }
    return {group.layout, index};
}

void SettingTab::addRow(QString key, const fifojson& json, const fifojson& input) {
    if (m_row_aliases.contains(key)) return;

    if (!input.is_null()) {
        const fifojson& components = input.at(Constants::Settings::Input::kComponents);
        const QString primary_key  = componentSetting(components.at(0));
        if (key != primary_key) return;

        const fifojson row_json  = makeInputRowJson(json, input);
        const std::string widget = input.at(Constants::Settings::Input::kWidget).get<std::string>();

        QSharedPointer<SettingRowBase> newRow;
        if (widget == Constants::Settings::Input::kVector2) {
            newRow = QSharedPointer<SettingRowBase>(new Vector2InputWidget(
                this, m_sb, primary_key, componentSetting(components.at(1)), row_json, m_container_layout, m_size,
                components.at(0).at(Constants::Settings::Input::kDefault).get<Distance>(),
                components.at(1).at(Constants::Settings::Input::kDefault).get<Distance>(),
                componentLabel(components.at(0)), componentLabel(components.at(1))));
        }
        else if (widget == Constants::Settings::Input::kVector3) {
            newRow = QSharedPointer<SettingRowBase>(new Vector3InputWidget(
                this, m_sb, primary_key, componentSetting(components.at(1)), componentSetting(components.at(2)),
                row_json, m_container_layout, m_size,
                components.at(0).at(Constants::Settings::Input::kDefault).get<double>(),
                components.at(1).at(Constants::Settings::Input::kDefault).get<double>(),
                components.at(2).at(Constants::Settings::Input::kDefault).get<double>(),
                componentLabel(components.at(0)), componentLabel(components.at(1)), componentLabel(components.at(2))));
        }

        if (!newRow.isNull()) {
            newRow->setValueChangeCallback(
                [this](const QString& changed_key, const QList<QSharedPointer<SettingsBase>>& settings_bases) {
                    emit settingAboutToChange(changed_key, settings_bases);
                });
            m_rows.insert(primary_key, newRow);
            for (std::size_t i = 1; i < components.size(); ++i)
                m_row_aliases.insert(componentSetting(components.at(i)), newRow);
            ++m_size;
        }

        return;
    }

    QGridLayout* row_layout = m_container_layout;
    int row_index           = m_size;
    const bool grouped      = json.contains(Constants::Settings::Master::kGroup);
    if (grouped) {
        const auto placement = groupedRowLayout(key, json.at(Constants::Settings::Master::kGroup));
        row_layout           = placement.first;
        row_index            = placement.second;
    }
    QSharedPointer<SettingRowBase> newRow = QSharedPointer<SettingRowBase>(
        m_creation_mapping[json.at(Constants::Settings::Master::kType)](this, m_sb, key, json, row_layout, row_index));
    if (grouped) {
        const int margin = m_group_cells.contains(key) ? 0 : 8;
        row_layout->setContentsMargins(margin, margin, margin, margin);
        if (!m_group_cells.contains(key)) row_layout->setColumnStretch(1, 1);
    }

    newRow->setValueChangeCallback(
        [this](const QString& changed_key, const QList<QSharedPointer<SettingsBase>>& settings_bases) {
            emit settingAboutToChange(changed_key, settings_bases);
        });
    m_rows.insert(key, newRow);
    if (!grouped) ++m_size;
}

void SettingTab::keyModified(QString key) {
    emit modified(key);
}

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

void SettingTab::showTab() {
    m_header->showHeader();
}

void SettingTab::reload() {
    for (QSharedPointer<SettingRowBase> curr_row : m_rows) { curr_row->reloadValue(); }
}

void SettingTab::settingsBasesSelected(QList<QSharedPointer<SettingsBase>> settings_bases,
                                       QList<QSharedPointer<SettingsBase>> inherited_bases) {
    if (m_settings_bases != settings_bases || m_inherited_settings_bases != inherited_bases) {
        m_warning_count            = 0;  // reset the count of warnings if a new settings base has been selected
        m_settings_bases           = settings_bases;
        m_inherited_settings_bases = inherited_bases;
        for (QSharedPointer<SettingRowBase> curr_row : m_rows) {
            curr_row->setBases(m_settings_bases, m_inherited_settings_bases);
            curr_row->reloadValue();
        }
    }
}

void SettingTab::headerWarning(int count) {
    emit warnPane(count);
    m_warning_count = m_warning_count + count;  // keep track of all warnings from children (setting_rows)
    // if there is more than 1 warning, change the header icon to show the warning
    if (m_warning_count > 0) { m_header->setIcon(QIcon(":/icons/warning.png")); }
    else { m_header->setIcon(QIcon(":/icons/ornlslicer_logo.png")); }
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
    if (isHidden) m_header->hide();

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

void SettingTab::setupStyle() {
    m_header->setupStyle();
}

void SettingTab::setupInsert() {
    m_layout->addWidget(m_header);
    m_layout->addWidget(m_container);
}

void SettingTab::setupEvents() {
    connect(m_header, &SettingHeader::expand, this, &SettingTab::expandTab);
    connect(m_header, &SettingHeader::shrink, this, &SettingTab::shrinkTab);
    connect(m_header, &SettingHeader::hideHeader, this, &SettingTab::hideTab);
}
}  // Namespace ORNL
