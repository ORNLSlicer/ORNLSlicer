#pragma once

#include <qgridlayout.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include "configs/settings_base.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_double_spin_box.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
class SettingTab;

//! \brief Widget to provide custom double spin box
//! Based on QDoubleSpinBox with overridden wheelEvent functionality
//! Supports the speed setting type
class SettingSpeedSpinBox : public SettingDoubleSpinBox {
    Q_OBJECT

  public:
    //! \brief Default Constructor
    //! \param parent: parent settingtab to setup events
    //! \param sb: global setting base
    //! \param key: key of current row
    //! \param json: master json of current row
    //! \param layout: layout to add current row to
    //! \param index: index to insert the row into the layout
    SettingSpeedSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
                        QGridLayout* layout, int index);

    //! \brief Static construction helper with the same parameters as default constructor
    //! Necessary to allow function pointer map in higher setting objects to construct
    //! widgets based on setting type listed in json (and avoid giant if-elseif trees)
    static SettingRowBase* createInstance(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key,
                                          fifojson json, QGridLayout* layout, int index);

  public slots:
    //! \brief Slot to handle when user manually changes value
    //! \param val: value cast to appropriate type within each class
    virtual void valueChanged(QVariant val) override;

    //! \brief Slot to handle setting reload when user changes units or
    //! selects new setting profile
    virtual void reloadValue() override;

    //! \brief Checks speed-specific dynamic dependencies.
    void checkDynamicDependencies() override;

  private:
    //! \brief Returns this row's effective speed value in base units.
    Velocity effectiveSpeed() const;

    //! \brief Returns whether selected settings bases agree on this row's effective value.
    bool hasConsistentEffectiveSpeed() const;

    //! \brief Returns a warning message when this row violates the active XY speed range.
    QString speedLimitWarning() const;
};
} // namespace ORNL
