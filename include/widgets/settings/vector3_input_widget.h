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
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
class SettingTab;

//! \brief Composite row that edits three unitless vector components on one settings line.
class Vector3InputWidget : public QWidget, public SettingRowBase {
    Q_OBJECT

  public:
    Vector3InputWidget(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString primary_key, QString secondary_key,
                       QString tertiary_key, fifojson json, QGridLayout* layout, int index, double primary_default,
                       double secondary_default, double tertiary_default, QString primary_label = "X",
                       QString secondary_label = "Y", QString tertiary_label = "Z");

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

    //! \brief Slot to handle setting reload when user selects another setting profile.
    void reloadValue() override;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void setNotification(QString msg) override;

    void clearNotification() override;

  private:
    struct Component {
        QString key;
        QDoubleSpinBox* spin_box;
        double default_value;
    };

    void configureSpinBox(QDoubleSpinBox* spin_box);
    void ensureSetting(const QString& key, double default_value);
    void updateSetting(const QString& key, double value);
    double reloadDoubleValue(const QString& key, double default_value, bool& consistent);
    bool hasConsistentEffectiveValues(const QString& key, double default_value);
    bool hasAnyInconsistentValue();
    void updateWarningStateAfterEdit();
    void setSpinBoxValue(QDoubleSpinBox* spin_box, const QString& key, double default_value, bool only_if_consistent,
                         bool& consistent);

    std::array<Component, 3> m_components;
    bool m_warn;
    int m_precision = 4;
};
} // namespace ORNL
