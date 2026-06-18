#pragma once

#include <array>

#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QSharedPointer>
#include <QString>
#include <QVariant>
#include <QWidget>
#include <qobject.h>
#include <qtmetamacros.h>

#include "configs/settings_base.h"
#include "units/unit.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
class SettingTab;

//! \brief Composite row that edits two distance/location components on one settings line.
class Vector2InputWidget : public QWidget, public SettingRowBase {
    Q_OBJECT

  public:
    Vector2InputWidget(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString primary_key, QString secondary_key,
                       fifojson json, QGridLayout* layout, int index, Distance primary_default,
                       Distance secondary_default, QString primary_label = "X", QString secondary_label = "Y");

    //! \brief Hides current row.
    void hide() override;

    //! \brief Shows current row.
    void show() override;

    //! \brief Enable/disable row.
    void setEnabled(bool enabled) override;

  signals:
    //! \brief Signal emitted when a setting is modified by user.
    void modified(QString key);

    //! \brief Signal emitted to pass a warning up to the next level.
    void warnParent(int count);

  public slots:
    //! \brief Slot to satisfy SettingRowBase; updates the primary setting.
    void valueChanged(QVariant val) override;

    //! \brief Slot to handle setting reload when user changes units or selects another setting profile.
    void reloadValue() override;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void setNotification(QString msg) override;

    void clearNotification() override;

  private:
    struct Component {
        QString key;
        QDoubleSpinBox* spin_box;
        Distance default_value;
    };

    void configureSpinBox(QDoubleSpinBox* spin_box);
    void ensureSetting(const QString& key, Distance default_value);
    void updateSetting(const QString& key, double displayed_value);
    Distance reloadDistanceValue(const QString& key, Distance default_value, bool& consistent);
    bool areConsistent(const QString& key, QSharedPointer<SettingsBase> a, QSharedPointer<SettingsBase> b,
                       Distance default_value);
    Distance effectiveDistanceValue(const QString& key, QSharedPointer<SettingsBase> settings_base,
                                    Distance default_value);
    bool hasConsistentEffectiveValues(const QString& key, Distance default_value);
    bool hasAnyInconsistentValue();
    void updateWarningStateAfterEdit();
    void setSpinBoxValue(QDoubleSpinBox* spin_box, const QString& key, Distance default_value, bool only_if_consistent,
                         bool& consistent);

    std::array<Component, 2> m_components;
    bool m_warn;
    int m_precision = 4;
};
} // namespace ORNL
