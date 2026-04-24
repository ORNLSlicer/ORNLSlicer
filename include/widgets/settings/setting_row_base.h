#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QObject>
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

    //! \brief Sets dependency logic for this row
    //! \param root: DependencyNode object that contains all dependency information
    void setDependencyLogic(DependencyNode root);

    //! \brief Adds rows to dependency list
    //! \param row: row to add to depenendcy list
    void addRowToNotify(QSharedPointer<SettingRowBase> row);

    //! \brief Set setting bases
    //! \param bases: list of bases currently selected by the user
    void setBases(QList<QSharedPointer<SettingsBase>> settings_bases);

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

  protected:
    //! \brief Function to handle value changes for each widget type
    virtual void valueChanged(QVariant val) = 0;

    //! \brief Check dependencies enforced through dynamic feedback
    void checkDynamicDependencies();

    //! \brief Recursive check of dependencynode logic
    //! \param root: Dependency logic to check
    bool checkLogic(DependencyNode root);

    //! \brief Templated helper for all widget types when value is changed
    template <class T> void valueChangedHelper(T value) {
        if (m_settings_bases.size() != 0)
            for (QSharedPointer<SettingsBase> range : m_settings_bases)
                range->setSetting(m_key, value);
        else
            m_sb->setSetting(m_key, value);

        clearNotification();
        styleLabel(true);

        for (QSharedPointer<SettingRowBase> row : m_rows_to_notify)
            row->checkDependencies();

        checkDynamicDependencies();
    }

    //! \brief Templated comparator for checking range list consistency
    template <class T> bool areConsistent(QSharedPointer<SettingsBase> a, QSharedPointer<SettingsBase> b) {
        if (a->contains(m_key) && m_sb->setting<T>(m_key) == a->setting<T>(m_key))
            a->remove(m_key);

        if (b->contains(m_key) && m_sb->setting<T>(m_key) == b->setting<T>(m_key))
            b->remove(m_key);

        bool containsA = a->contains(m_key);
        bool containsB = b->contains(m_key);

        // returns true
        //     - if both settings bases do NOT contain the key
        //     - if both settings bases contain the SAME VALUE for the key
        // returns false otherwise

        return (containsA == containsB) &&
               ((containsA && (a->setting<T>(m_key) == b->setting<T>(m_key))) || !containsA);
    }

    //! \brief Templated helper for all widget types when settings must be reloaded
    template <class T> T reloadValueHelper(bool& consistent) {
        T cur;
        if (m_settings_bases.size() > 0) {
            bool all_bases_consistent = true;
            for (int i = 1, end = m_settings_bases.size(); i < end; ++i) {
                auto sb_1 = m_settings_bases[i - 1];
                auto sb_2 = m_settings_bases[i];
                all_bases_consistent = all_bases_consistent && areConsistent<T>(sb_1, sb_2);
            }

            if (all_bases_consistent) {
                // setting was consistent and either didn't exist or did exist and were equal
                // So, if it exists, use it, otherwise grab the global
                if (m_settings_bases[0]->contains(m_key))
                    cur = m_settings_bases[0]->setting<T>(m_key);
                else {
                    if (m_sb->contains(m_key))
                        cur = m_sb->setting<T>(m_key);
                    else
                        cur = m_json[Constants::Settings::Master::kDefault].get<T>();
                }
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

    //! \brief Master json that this row was constructed from
    fifojson m_json;

    //! \brief Nodes that hold the other settings this row is dependent on for enable/disable
    DependencyNode m_dependency_logic;

    //! \brief Holds pointers to dependents to notify when current value changes
    QList<QSharedPointer<SettingRowBase>> m_rows_to_notify;
};

} // Namespace ORNL
