#pragma once

#include <functional>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QObject>
#include <QToolButton>
#include <QWidget>
#include <qlist.h>
#include <qscopedpointer.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "utilities/constants.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {

class SettingRowBase;
//! \brief Struct that holds dependency information
struct DependencyNode {
    //! \brief Key the represents name or operation
    QString key;

    //! \brief Value to compare against when name is a row and not an operation
    fifojson val;

    //! \brief Pointer to row to check against val
    QSharedPointer<SettingRowBase> dependentRow;

    //! \brief Children of current node when key is an operation
    //! Currently allows AND, OR
    QList<DependencyNode> children;
};

//! \brief Base class for widgets that holds additional information such as dependency
//! logic and json.  Used in combination with child widget types to create a row
class SettingRowBase {

  public:
    using ValueChangeCallback = std::function<void(const QString&, const QList<QSharedPointer<SettingsBase>>&)>;

    //! \brief Default Constructor
    //! \param sb: global setting base
    //! \param key: key of current row
    //! \param json: master json of current row
    //! \param layout: layout to add current row to
    //! \param index: index to insert the row into the layout
    SettingRowBase(QWidget* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json, QGridLayout* layout,
                   int index);

    //! \brief Destructor override
    virtual ~SettingRowBase();

    //! \brief Reloads values due to new
    virtual void reloadValue() = 0;

    //! \brief Returns text for label of row
    //! \return row label text
    QString getLabelText();

    //! \brief Styles row label
    //! \param isConsistent: whether or not settings are consistent
    void styleLabel(bool isConsistent);

    //! \brief applies a qss file to the target widget. Must specify a target that is not SettingRowBase.
    //! \param target: the subwidget of SettingRowBase that is being styled; usually will be "this".
    //! \param file: String representing the location of a qss file
    bool setStyleFromFile(QWidget* target, QString file);

    //! \brief Pointer to a qss file
    QSharedPointer<QFile> m_style_file;

    //! \brief Returns whether or not setting is valid for local setting assignment
    //! \return boolean to indicate whether or not setting can be assigned locally
    bool isLocal();

    //! \brief Returns dependency information
    //! \return master json information containing dependency information
    fifojson getDependencies();

    //! \brief Check dependencies and enable/disable as appropriate
    void checkDependencies();

    //! \brief Hide this row.
    virtual void hide();

    //! \brief Show this row.
    virtual void show();

    //! \brief Returns whether this row's widgets should currently be visible.
    bool isShown() const;

    //! \brief Sets dependency logic for this row
    //! \param root: DependencyNode object that contains all dependency information
    void setDependencyLogic(DependencyNode root);

    //! \brief Adds rows to dependency list
    //! \param row: row to add to depenendcy list
    void addRowToNotify(QSharedPointer<SettingRowBase> row);

    //! \brief Set selected setting bases and the parent settings each base inherits.
    //! \param settings_bases: list of bases currently selected by the user
    //! \param inherited_bases: corresponding parent bases; null entries inherit Global directly
    void setBases(QList<QSharedPointer<SettingsBase>> settings_bases,
                  QList<QSharedPointer<SettingsBase>> inherited_bases);

    //! \brief Get the setting bases of a row
    //! \return Returns the current settings bases of a row
    QList<QSharedPointer<SettingsBase>> getBases();

    //! \brief Clears dependency information (all qsharedpointers must be
    //! cleaned before destruction, otherwise parent->child relationships cause
    //! errors via multiple free's)
    void clearDependencyLogic();

    //! \brief Enable or disable this row.
    //! \param enabled: enable state
    virtual void setEnabled(bool enabled);

    //! \brief Set new global settingsbase
    //! \param sb: new settingsbase to set
    void setSettingsBase(QSharedPointer<SettingsBase> sb);

    //! \brief Set callback used to notify before this row writes a new value.
    void setValueChangeCallback(ValueChangeCallback callback);

  protected:
    //! \brief Function to handle value changes for each widget type
    virtual void valueChanged(QVariant val) = 0;

    //! \brief Check dependencies enforced through dynamic feedback
    void checkDynamicDependencies();

    //! \brief Notify before a setting key is written by this row.
    void notifyValueAboutToChange(const QString& key);

    //! \brief Returns the warning-count delta for a new row warning state.
    int warningCountDelta(bool warning_active, bool& previous_warning_active);

    //! \brief Sets keys that should be checked when deciding whether this row is locally overridden.
    void setLocalOverrideKeys(QList<QString> keys);

    //! \brief Returns the value inherited by one selected settings base.
    template <class T> T inheritedValueHelper(const QString& key, int index, const T& global_value) const {
        if (index >= 0 && index < m_inherited_settings_bases.size()) {
            const QSharedPointer<SettingsBase>& inherited_base = m_inherited_settings_bases[index];
            if (!inherited_base.isNull() && inherited_base->contains(key))
                return inherited_base->setting<T>(key);
        }

        return global_value;
    }

    //! \brief Returns the effective value for one selected settings base.
    template <class T> T effectiveValueHelper(const QString& key, int index, const T& global_value) const {
        if (index >= 0 && index < m_settings_bases.size() && m_settings_bases[index]->contains(key))
            return m_settings_bases[index]->setting<T>(key);

        return inheritedValueHelper<T>(key, index, global_value);
    }

    //! \brief Removes edited overrides matching the value inherited by each selected base.
    template <class T> void removeRedundantLocalOverrides(const QString& key, const T& global_value) {
        for (int index = 0, end = m_settings_bases.size(); index < end; ++index) {
            QSharedPointer<SettingsBase> settings_base = m_settings_bases[index];
            const T inherited_value = inheritedValueHelper<T>(key, index, global_value);
            if (settings_base->contains(key) && settings_base->setting<T>(key) == inherited_value)
                settings_base->remove(key);
        }
    }

    //! \brief Recursive check of dependencynode logic
    //! \param root: Dependency logic to check
    bool checkLogic(DependencyNode root);

    //! \brief Templated helper for all widget types when value is changed
    template <class T> void valueChangedHelper(T value) {
        notifyValueAboutToChange(m_key);

        if (m_settings_bases.size() != 0)
            for (QSharedPointer<SettingsBase> range : m_settings_bases)
                range->setSetting(m_key, value);
        else
            m_sb->setSetting(m_key, value);

        if (m_settings_bases.size() != 0) {
            const T global_value = m_sb->contains(m_key) ? m_sb->setting<T>(m_key)
                                                         : m_json[Constants::Settings::Master::kDefault].get<T>();
            removeRedundantLocalOverrides<T>(m_key, global_value);
        }

        clearNotification();
        styleLabel(true);

        for (QSharedPointer<SettingRowBase> row : m_rows_to_notify)
            row->checkDependencies();

        checkDynamicDependencies();
    }

    //! \brief Templated helper for all widget types when settings must be reloaded
    template <class T> T reloadValueHelper(bool& consistent) {
        T cur;
        if (m_settings_bases.size() > 0) {
            const T global_value = m_sb->contains(m_key) ? m_sb->setting<T>(m_key)
                                                         : m_json[Constants::Settings::Master::kDefault].get<T>();
            cur = effectiveValueHelper<T>(m_key, 0, global_value);

            bool all_bases_consistent = true;
            for (int index = 1, end = m_settings_bases.size(); index < end; ++index)
                all_bases_consistent =
                    all_bases_consistent && effectiveValueHelper<T>(m_key, index, global_value) == cur;

            if (all_bases_consistent) {
                clearNotification();
                styleLabel(true);
            }
            else {
                // set to default
                setNotification("Multiple Values");
                styleLabel(false);
                cur = m_json[Constants::Settings::Master::kDefault].get<T>();
                consistent = false;
            }
        }
        else if (m_sb->contains(m_key)) {
            cur = m_sb->setting<T>(m_key);
            clearNotification();
            styleLabel(true);
        }
        else {
            cur = m_json[Constants::Settings::Master::kDefault].get<T>();
            clearNotification();
            styleLabel(true);
        }

        return cur;
    }

    //! \brief Override for each child widget type for
    //! setting notifications when dependency checks fail
    //! \param msg: Message to display
    virtual void setNotification(QString msg) = 0;

    //! \brief Override for each child widget type for
    //! clearing notifications when dependency checks pass
    virtual void clearNotification() = 0;

    //! \brief Pointers to settings bases when a user selects them for local settings
    QList<QSharedPointer<SettingsBase>> m_settings_bases;

    //! \brief Parent settings inherited by the corresponding selected settings bases.
    QList<QSharedPointer<SettingsBase>> m_inherited_settings_bases;

    //! \brief Whether parent warning counters were reset for a newly selected set of bases.
    bool m_warning_state_reset_pending = false;

    //! \brief Index of row
    int m_index;

    //! \brief Pointer to parent layout to add widgets to
    QGridLayout* m_layout;

    //! \brief applies a qss file to the label of the row
    //! \param file: String representing the location of a qss file
    bool styleLabelFromFile(QString file);

    //! \brief Folder path of theme for qss sheets
    QString m_theme_path;

    //! \brief Label for key display
    QScopedPointer<QLabel> m_key_label;

    //! \brief Label for units (if applicable)
    QScopedPointer<QLabel> m_unit_label;

    //! \brief Key that this row corresponds to
    QString m_key;

    //! \brief Pointer to global setting base
    QSharedPointer<SettingsBase> m_sb;

  protected:
    //! \brief Returns the default tooltip from the row's master setting metadata.
    QString baseToolTip() const;

    //! \brief Returns whether any selected base differs from its inherited value for one of this row's keys.
    bool hasLocalOverride() const;

    //! \brief Returns whether one selected key matches the value inherited by that settings base.
    bool matchesInheritedValue(const QString& key, int index) const;

    //! \brief Applies visibility/enabled state to the local override reset button.
    void updateResetButton();

    //! \brief Remove this row's selected local overrides and reload the row.
    void resetLocalOverrides();

    //! \brief Returns whether this row should be visible after all visibility controls are applied.
    bool rowWidgetsVisible() const;

    //! \brief Returns whether this row should accept input after all enabled controls are applied.
    bool rowWidgetsEnabled() const;

    //! \brief Applies current row enabled and visibility state to a widget.
    void applyWidgetState(QWidget* widget) const;

    //! \brief Applies current row enabled and visibility state to base widgets.
    void applyBaseWidgetState();

    //! \brief Applies current row enabled and visibility state to a row editor widget.
    void registerRowWidget(QWidget* widget);

    //! \brief Sets dependency-driven enabled state for this row.
    void setDependencyEnabled(bool enabled);

    //! \brief Keys that can make this row locally overridden.
    QList<QString> m_local_override_keys;

    //! \brief Button used to remove selected local overrides.
    QScopedPointer<QToolButton> m_reset_button;

    //! \brief Editor widgets that must follow row visibility and enabled state.
    QList<QWidget*> m_row_widgets;

    //! \brief Whether this row is currently visible.
    bool m_row_visible;

    //! \brief Whether this row is currently enabled.
    bool m_row_enabled;

    //! \brief Whether this row is currently enabled by dependency logic.
    bool m_dependency_enabled;

    //! \brief Master json that this row was constructed from
    fifojson m_json;

    //! \brief Nodes that hold the other settings this row is dependent on for enable/disable
    DependencyNode m_dependency_logic;

    //! \brief Holds pointers to dependents to notify when current value changes
    QList<QSharedPointer<SettingRowBase>> m_rows_to_notify;

    //! \brief Callback for snapshotting undo state before a row mutates settings.
    ValueChangeCallback m_value_change_callback;
};

} // Namespace ORNL
