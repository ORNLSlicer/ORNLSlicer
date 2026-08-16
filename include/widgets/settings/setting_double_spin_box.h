#pragma once

#include <QDoubleSpinBox>

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

//! \brief Widget to provide custom double spin box
//! Based on QDoubleSpinBox with overridden wheelEvent functionality
//! Supports the following setting types: unitless_float, rpm, deposition_rate, density, percentage
class SettingDoubleSpinBox : public QDoubleSpinBox, public SettingRowBase {
    Q_OBJECT

   public:
    //! \brief Default Constructor
    //! \param parent: parent settingtab to setup events
    //! \param sb: global setting base
    //! \param key: key of current row
    //! \param json: master json of current row
    //! \param layout: layout to add current row to
    //! \param index: index to insert the row into the layout
    SettingDoubleSpinBox(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString key, fifojson json,
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

    //! \brief Checks double-based setting dependencies enforced through warnings.
    void checkDynamicDependencies() override;

   protected:
    //! \brief Sets error notification when dynamic dependency check fails
    //! \param msg: Message to display
    virtual void setNotification(QString msg) override;

    //! \brief Clears notifications when dynamic dependency check passes
    virtual void clearNotification() override;

    //! \brief Overridden behavior to prevent wheel event from changing the value when not focused
    //! \param event: captured wheel event
    void wheelEvent(QWheelEvent* event) override;

    //! \brief Returns this row's effective value in base units.
    double effectiveDouble() const;

    //! \brief Returns this row's effective value for one selected base in base units.
    double effectiveDouble(int settings_base_index) const;

    //! \brief Returns another setting's effective value in base units.
    double effectiveDouble(const QString& key) const;

    //! \brief Returns another setting's effective value for one selected base in base units.
    double effectiveDouble(const QString& key, int settings_base_index) const;

    //! \brief Returns another boolean setting's effective value.
    bool effectiveBool(const QString& key) const;

    //! \brief Returns another boolean setting's effective value for one selected base.
    bool effectiveBool(const QString& key, int settings_base_index) const;

    //! \brief Returns another integer setting's effective value.
    int effectiveInt(const QString& key) const;

    //! \brief Returns another integer setting's effective value for one selected base.
    int effectiveInt(const QString& key, int settings_base_index) const;

    //! \brief Returns whether this row should present deposition control as an integer value.
    bool usesIntegerDepositionRate() const;

    //! \brief Returns whether one selected base should present deposition control as an integer value.
    bool usesIntegerDepositionRate(int settings_base_index) const;

    //! \brief Returns whether this row should present Meld deposition control as a velocity.
    bool usesMeldVelocityDepositionRate() const;

    //! \brief Returns whether one selected base should present Meld deposition control as a velocity.
    bool usesMeldVelocityDepositionRate(int settings_base_index) const;

    //! \brief Converts a stored deposition-rate value to the currently displayed value.
    double displayedDepositionRateValue(double stored_value) const;

    //! \brief Converts a displayed deposition-rate value back to the stored value.
    double storedDepositionRateValue(double displayed_value) const;

    //! \brief Refreshes the visible spin-box value from the stored value using the current presentation.
    void syncDisplayedDepositionRateValue();

    //! \brief Updates deposition-rate decimals and unit text for the selected machine type.
    void updateDepositionRatePresentation();

    //! \brief Returns whether selected settings bases agree on this row's effective value.
    bool hasConsistentEffectiveDouble() const;

    //! \brief Returns the number of effective settings contexts to evaluate.
    int effectiveSettingsBaseCount() const;

    //! \brief Returns a warning message when this row violates a dynamic dependency.
    QString dynamicDependencyWarning() const;

    //! \brief Returns a warning message for one selected base when this row violates a dynamic dependency.
    QString dynamicDependencyWarning(int settings_base_index) const;

    //! \brief Applies warning styling and optionally shows the transient tooltip.
    void applyNotification(QString msg, bool show_tooltip);

    //! \brief Number of units of precision for child derived types with units
    //! Currently, all the same
    int m_precision = 4;

    //! \brief Keeps track of if a warning has been emitted or not.
    bool m_warn;
};
}  // namespace ORNL
