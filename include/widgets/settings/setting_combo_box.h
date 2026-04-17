#pragma once

#include <QComboBox>
#include <qgridlayout.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvariant.h>
#include <qwidget.h>

#include "configs/settings_base.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
class SettingTab;

//! \brief Widget to provide custom combo box
//! Based on QComboBox with overridden wheelEvent functionality
//! Supports enumeration setting type
class SettingComboBox : public QComboBox, public SettingRowBase {
    Q_OBJECT

  public:
    //! \brief Default Constructor
    //! \param parent: parent settingtab to setup events
    //! \param sb: global setting base
    //! \param key: key of current row
    //! \param json: master json of current row
    //! \param layout: layout to add current row to
    //! \param index: index to insert the row into the layout
    SettingComboBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                    QGridLayout* layout, int index);

    //! \brief Static construction helper with the same parameters as default constructor
    //! Necessary to allow function pointer map in higher setting objects to construct
    //! widgets based on setting type listed in json (and avoid giant if-elseif trees)
    static SettingRowBase* createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                          fifojson json, QGridLayout* layout, int index);

    //! \brief Hides current row
    virtual void hide() override;

    //! \brief Shows current row
    virtual void show() override;

    //! \brief Enable/disable row
    //! \param enabled: enable/disable state
    void setEnabled(bool enabled) override;

  signals:
    //! \brief Signal emitted when setting is modified by user
    //! \param key: key of setting modified
    void modified(QString key);

    //! \brief Signal emitted to pass a warning, such as mismatched settings in the settings base, up to the next level
    //! \param count: Integer representing warning status, should be either 1, -1 or 0; 1 adds a warning, -1 removes a
    //! warning, 0 does nothing.
    void warnParent(int count);

  public slots:
    //! \brief Slot to handle when user manually changes value
    //! \param val: value cast to appropriate type within each class
    virtual void valueChanged(QVariant val) override;

    //! \brief Slot to handle setting reload when user changes units or
    //! selects new setting profile
    virtual void reloadValue() override;

  protected:
    //! \brief Sets error notification when dynamic dependency check fails
    //! \param msg: Message to display
    virtual void setNotification(QString msg) override;

    //! \brief Clears notifications when dynamic dependency check passes
    virtual void clearNotification() override;

    //! \brief Overridden behavior to prevent wheel event from changing the value when not focused
    //! \param event: captured wheel event
    void wheelEvent(QWheelEvent* event) override;

    //! \brief Keeps track of if a warning has been emitted or not.
    bool m_warn;

    //! \brief Index of the currently selected item
    int m_cur;

    //! \brief Index of the previously selected item
    int m_prev;
};
} // namespace ORNL
