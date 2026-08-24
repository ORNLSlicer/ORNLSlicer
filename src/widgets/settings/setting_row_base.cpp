#include "widgets/settings/setting_row_base.h"

#include <QCheckBox>
#include <QComboBox>
#include <QIcon>
#include <QSpinBox>
#include <QToolButton>

#include <qgridlayout.h>
#include <qhashfunctions.h>
#include <qlabel.h>
#include <qlist.h>
#include <qlogging.h>
#include <qnamespace.h>
#include <qpicture.h>
#include <qsharedpointer.h>
#include <qwidget.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {

SettingRowBase::SettingRowBase(QWidget* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                               QGridLayout* layout, int index)
    : m_index(index),
      m_layout(layout),
      m_key(key),
      m_sb(sb),
      m_row_visible(true),
      m_row_enabled(true),
      m_dependency_enabled(true),
      m_hidden_by_process_dependency(false),
      m_json(json) {
    m_theme_path          = PreferencesManager::getInstance()->getTheme().getFolderPath();
    m_local_override_keys = {m_key};

    m_key_label.reset(new QLabel());
    m_key_label->setText(json.at(Constants::Settings::Master::kDisplay));
    m_key_label->setToolTip(baseToolTip());
    m_key_label->setCursor(Qt::WhatsThisCursor);
    m_key_label->setMinimumHeight(25);
    m_key_label->setIndent(25);
    this->styleLabelFromFile(m_theme_path + "setting_rows_normal.qss");
    layout->addWidget(m_key_label.get(), index, 0);

    m_reset_button.reset(new QToolButton(parent));
    m_reset_button->setIcon(QIcon(":/icons/revert.png"));
    m_reset_button->setAutoRaise(true);
    m_reset_button->setFixedSize(20, 20);
    m_reset_button->setToolTip("Use inherited value");
    m_reset_button->hide();
    QObject::connect(m_reset_button.get(), &QToolButton::clicked, m_reset_button.get(),
                     [this]() { resetLocalOverrides(); });
    layout->addWidget(m_reset_button.get(), index, 3);

    layout->setContentsMargins(0, 0, 20, 0);
}

SettingRowBase::~SettingRowBase() {
    // NOP - all widgets inherit from this class and a QObject-derived class
    // Skip destruction and allow QObject-derivation to handle it
}

void SettingRowBase::clearDependencyLogic() {
    m_rows_to_notify.clear();
    m_dependency_logic.dependentRow.reset();
    m_dependency_logic.children.clear();
}

QString SettingRowBase::getLabelText() {
    return m_key_label->text();
}

// This is to style inheritents of setting_row_base (i.e., all the different types of settings)
bool SettingRowBase::setStyleFromFile(QWidget* target, QString file) {
    m_style_file = QSharedPointer<QFile>(new QFile(file));

    if (!m_style_file->open(QIODevice::ReadOnly)) {
        qDebug("Could not open style resource file '%s'.\n", file.toStdString().c_str());
        m_style_file.clear();
        return false;
    }

    target->setStyleSheet(m_style_file->readAll());
    m_style_file->close();
    return true;
}

// Label has to be styled independently
bool SettingRowBase::styleLabelFromFile(QString file) {
    m_style_file = QSharedPointer<QFile>(new QFile(file));

    if (!m_style_file->open(QIODevice::ReadOnly)) {
        qDebug("Could not open style resource file '%s'.\n", file.toStdString().c_str());
        m_style_file.clear();
        return false;
    }

    m_key_label->setStyleSheet(m_style_file->readAll());
    m_style_file->close();
    return true;
}

void SettingRowBase::styleLabel(bool isConsistent) {
    const QString tooltip = baseToolTip();

    if (isConsistent) {
        this->styleLabelFromFile(m_theme_path + "setting_rows_normal.qss");
        if (hasLocalOverride()) {
            const QString accent_color = PreferencesManager::getInstance()->getTheme().getDotPairedColor().name();
            m_key_label->setStyleSheet(m_key_label->styleSheet() + "\nQLabel { color: " + accent_color +
                                       "; font-weight: 700; }");
            m_key_label->setToolTip(
                "<html><body><p><b>Locally overridden.</b> Click the reset button to use the inherited value.</p><p>" +
                QString::fromStdString(m_json.at(Constants::Settings::Master::kToolTip)) + "</p></body></html>");
        }
        else { m_key_label->setToolTip(tooltip); }
    }
    else {
        this->styleLabelFromFile(m_theme_path + "setting_rows_warning.qss");
        m_key_label->setToolTip("Inconsistent settings between selected items.<p>" + tooltip);
    }

    updateResetButton();
}

bool SettingRowBase::isLocal() {
    return m_json[Constants::Settings::Master::kLocal];
}

fifojson SettingRowBase::getDependencies() {
    return m_json[Constants::Settings::Master::kDepends];
}

void SettingRowBase::addRowToNotify(QSharedPointer<SettingRowBase> row) {
    if (!m_rows_to_notify.contains(row)) { m_rows_to_notify.push_back(row); }
}

void SettingRowBase::setBases(QList<QSharedPointer<SettingsBase>> settings_bases,
                              QList<QSharedPointer<SettingsBase>> inherited_bases) {
    m_warning_state_reset_pending = m_settings_bases != settings_bases || m_inherited_settings_bases != inherited_bases;
    m_settings_bases              = settings_bases;
    m_inherited_settings_bases    = inherited_bases;
    updateResetButton();
}

QList<QSharedPointer<SettingsBase>> SettingRowBase::getBases() {
    return m_settings_bases;
}

void SettingRowBase::checkDependencies() {
    m_hidden_by_process_dependency = false;
    setDependencyEnabled(checkLogic(m_dependency_logic));
    checkDynamicDependencies();
}

void SettingRowBase::hide() {
    m_row_visible = false;
    applyBaseWidgetState();
}

void SettingRowBase::show() {
    m_row_visible = true;
    applyBaseWidgetState();
}

bool SettingRowBase::isShown() const {
    return rowWidgetsVisible();
}

bool SettingRowBase::dependencyEnabled() const {
    return m_dependency_enabled;
}

bool SettingRowBase::hiddenByProcessDependency() const {
    return m_hidden_by_process_dependency && !m_dependency_enabled;
}

bool SettingRowBase::rowWidgetsVisible() const {
    if (hiddenByProcessDependency()) return false;

    const bool show_disabled =
        PreferencesManager::getInstance()->getDisabledSettingVisibilityPreference() == DisabledSettingVisibility::kGrey;
    return m_row_visible && (m_dependency_enabled || show_disabled);
}

bool SettingRowBase::rowWidgetsEnabled() const {
    return m_row_enabled && m_dependency_enabled;
}

void SettingRowBase::applyWidgetState(QWidget* widget) const {
    if (widget == nullptr) return;

    widget->setVisible(rowWidgetsVisible());
    widget->setEnabled(rowWidgetsEnabled());
}

void SettingRowBase::applyBaseWidgetState() {
    const bool visible = rowWidgetsVisible();
    const bool enabled = rowWidgetsEnabled();

    for (QWidget* widget : m_row_widgets) {
        if (widget == nullptr) continue;

        widget->setVisible(visible);
        widget->setEnabled(enabled);
    }

    if (!m_key_label.isNull()) {
        m_key_label->setVisible(visible);
        m_key_label->setEnabled(enabled);
    }

    if (!m_unit_label.isNull()) {
        m_unit_label->setVisible(visible);
        m_unit_label->setEnabled(enabled);
    }

    updateResetButton();
}

void SettingRowBase::registerRowWidget(QWidget* widget) {
    if (widget == nullptr || m_row_widgets.contains(widget)) return;

    m_row_widgets.push_back(widget);
    applyWidgetState(widget);
}

void SettingRowBase::setEnabled(bool enabled) {
    m_row_enabled = enabled;
    applyBaseWidgetState();
}

void SettingRowBase::setDependencyEnabled(bool enabled) {
    const bool changed   = m_dependency_enabled != enabled;
    m_dependency_enabled = enabled;
    applyBaseWidgetState();

    if (changed) {
        for (QSharedPointer<SettingRowBase> row : m_rows_to_notify) { row->checkDependencies(); }
    }
}

void SettingRowBase::setDependencyLogic(DependencyNode root) {
    m_dependency_logic = root;
}

void SettingRowBase::setSettingsBase(QSharedPointer<SettingsBase> sb) {
    m_sb = sb;
}

void SettingRowBase::setValueChangeCallback(ValueChangeCallback callback) {
    m_value_change_callback = callback;
}

void SettingRowBase::notifyValueAboutToChange(const QString& key) {
    if (m_value_change_callback) { m_value_change_callback(key, m_settings_bases); }
}

int SettingRowBase::warningCountDelta(bool warning_active, bool& previous_warning_active) {
    if (m_warning_state_reset_pending) {
        previous_warning_active       = false;
        m_warning_state_reset_pending = false;
    }

    const int delta         = static_cast<int>(warning_active) - static_cast<int>(previous_warning_active);
    previous_warning_active = warning_active;
    return delta;
}

void SettingRowBase::setLocalOverrideKeys(QList<QString> keys) {
    m_local_override_keys = keys;
}

QString SettingRowBase::baseToolTip() const {
    return "<html><body><p>" + QString::fromStdString(m_json.at(Constants::Settings::Master::kToolTip)) +
           "</p></body></html>";
}

bool SettingRowBase::hasLocalOverride() const {
    if (m_settings_bases.isEmpty()) return false;

    for (int index = 0, end = m_settings_bases.size(); index < end; ++index) {
        const QSharedPointer<SettingsBase>& settings_base = m_settings_bases[index];
        for (const QString& key : m_local_override_keys) {
            if (settings_base->contains(key) && !matchesInheritedValue(key, index)) return true;
        }
    }

    return false;
}

bool SettingRowBase::matchesInheritedValue(const QString& key, int index) const {
    if (index < 0 || index >= m_settings_bases.size() || !m_settings_bases[index]->contains(key)) return true;

    const fifojson local_value = m_settings_bases[index]->setting<fifojson>(key);
    if (index < m_inherited_settings_bases.size()) {
        const QSharedPointer<SettingsBase>& inherited_base = m_inherited_settings_bases[index];
        if (!inherited_base.isNull() && inherited_base->contains(key))
            return local_value == inherited_base->setting<fifojson>(key);
    }

    if (m_sb->contains(key)) return local_value == m_sb->setting<fifojson>(key);

    if (key == m_key && m_json.contains(Constants::Settings::Master::kDefault))
        return local_value == m_json[Constants::Settings::Master::kDefault];

    return false;
}

void SettingRowBase::updateResetButton() {
    if (m_reset_button.isNull()) return;

    const bool should_show = rowWidgetsVisible() && hasLocalOverride();
    m_reset_button->setVisible(should_show);
    m_reset_button->setEnabled(should_show && rowWidgetsEnabled());
}

void SettingRowBase::resetLocalOverrides() {
    if (m_settings_bases.isEmpty()) return;

    for (const QString& key : m_local_override_keys) {
        bool key_removed = false;
        for (QSharedPointer<SettingsBase> settings_base : m_settings_bases) {
            if (settings_base->contains(key)) {
                if (!key_removed) {
                    notifyValueAboutToChange(key);
                    key_removed = true;
                }
                settings_base->remove(key);
            }
        }
    }

    reloadValue();

    for (QSharedPointer<SettingRowBase> row : m_rows_to_notify) row->checkDependencies();

    checkDynamicDependencies();
    updateResetButton();
}

bool SettingRowBase::checkLogic(DependencyNode root) {
    if (root.key == "AND") {
        const bool left  = checkLogic(root.children[0]);
        const bool right = checkLogic(root.children[1]);
        return left && right;
    }
    else if (root.key == "OR") {
        const bool left  = checkLogic(root.children[0]);
        const bool right = checkLogic(root.children[1]);
        return left || right;
    }
    else if (root.key == "NOT") {
        const bool child_result = checkLogic(root.children[0]);
        if (child_result && isActiveProcessDependency(root.children[0])) m_hidden_by_process_dependency = true;

        return !child_result;
    }
    else {
        if (root.dependentRow.isNull()) return true;

        auto trackProcessMismatch = [this, &root](bool matched) {
            if (!matched && (isActiveProcessDependency(root) || root.dependentRow->hiddenByProcessDependency()))
                m_hidden_by_process_dependency = true;

            return matched;
        };

        if (QCheckBox* checkBox = dynamic_cast<QCheckBox*>(root.dependentRow.get())) {
            for (auto& el : root.val.items()) {
                bool checked = checkBox->isChecked();
                if (!root.dependentRow->dependencyEnabled() &&
                    root.dependentRow->m_json.contains(Constants::Settings::Master::kDefault)) {
                    checked = root.dependentRow->m_json[Constants::Settings::Master::kDefault].get<bool>();
                }

                return trackProcessMismatch(checked == static_cast<bool>(el.value()));
            }
        }
        else if (QComboBox* comboBox = dynamic_cast<QComboBox*>(root.dependentRow.get())) {
            for (auto& el : root.val.items()) {
                int current_index = comboBox->currentIndex();
                if (!root.dependentRow->dependencyEnabled() &&
                    root.dependentRow->m_json.contains(Constants::Settings::Master::kDefault)) {
                    current_index = root.dependentRow->m_json[Constants::Settings::Master::kDefault].get<int>();
                }

                return trackProcessMismatch(current_index == el.value());
            }
        }
        else if (QSpinBox* spinBox = dynamic_cast<QSpinBox*>(root.dependentRow.get())) {
            for (auto& el : root.val.items()) {
                int value = spinBox->value();
                if (!root.dependentRow->dependencyEnabled() &&
                    root.dependentRow->m_json.contains(Constants::Settings::Master::kDefault)) {
                    value = root.dependentRow->m_json[Constants::Settings::Master::kDefault].get<int>();
                }

                return trackProcessMismatch(value == el.value());
            }
        }
    }

    // Default return in case none of the conditions match
    return false;
}

bool SettingRowBase::isActiveProcessDependency(const DependencyNode& root) const {
    if (root.dependentRow.isNull()) return false;

    if (root.dependentRow->m_key == PRS::MachineSetup::kMachineType) {
        return m_sb->setting<int>(PRS::MachineSetup::kMachineType) == static_cast<int>(MachineType::kFrictionStir);
    }

    if (root.dependentRow->m_key == PRS::MachineSetup::kSyntax) {
        return m_sb->setting<int>(PRS::MachineSetup::kSyntax) == static_cast<int>(GcodeSyntax::kMeld);
    }

    return false;
}

void SettingRowBase::checkDynamicDependencies() {}
}  // Namespace ORNL
