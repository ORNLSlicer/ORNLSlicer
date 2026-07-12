#include "widgets/settings/vector2_input_widget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QRect>
#include <QSizePolicy>
#include <QToolTip>
#include <qevent.h>
#include <qnamespace.h>
#include <qoverload.h>

#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
namespace {
constexpr int kComponentLabelWidth = 18;
constexpr int kComponentMinimumHeight = 22;
constexpr int kComponentSpacing = 4;
constexpr int kComponentGroupSpacing = 6;
constexpr int kSpinBoxWidth = 96;

QLabel* createComponentLabel(QWidget* parent, const QString& text) {
    QLabel* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedWidth(kComponentLabelWidth);
    label->setMinimumHeight(kComponentMinimumHeight);
    label->setStyleSheet("QLabel { image: none; margin: 0px; padding: 0px; }");

    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);

    return label;
}

void addComponentEditor(QHBoxLayout* layout, QWidget* parent, const QString& label_text, QDoubleSpinBox* spin_box) {
    layout->addWidget(createComponentLabel(parent, label_text));
    layout->addWidget(spin_box);
}
} // namespace

Vector2InputWidget::Vector2InputWidget(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString primary_key,
                                       QString secondary_key, fifojson json, QGridLayout* layout, int index,
                                       Distance primary_default, Distance secondary_default, QString primary_label,
                                       QString secondary_label)
    : QWidget(parent), SettingRowBase(parent, sb, primary_key, json, layout, index),
      m_components {{{primary_key, new QDoubleSpinBox(this), primary_default},
                     {secondary_key, new QDoubleSpinBox(this), secondary_default}}},
      m_warn(false) {
    setLocalOverrideKeys({primary_key, secondary_key});

    QHBoxLayout* vector_layout = new QHBoxLayout(this);
    vector_layout->setContentsMargins(0, 0, 0, 0);
    vector_layout->setSpacing(kComponentSpacing);

    const std::array<QString, 2> labels {primary_label, secondary_label};
    const Distance unit = PreferencesManager::getInstance()->getDistanceUnit();

    for (std::size_t i = 0; i < m_components.size(); ++i) {
        Component& component = m_components[i];
        ensureSetting(component.key, component.default_value);

        if (i != 0)
            vector_layout->addSpacing(kComponentGroupSpacing);
        addComponentEditor(vector_layout, this, labels[i], component.spin_box);

        configureSpinBox(component.spin_box);
        component.spin_box->installEventFilter(this);
        component.spin_box->setValue(m_sb->setting<Distance>(component.key).to(unit));

        const QString component_key = component.key;
        connect(component.spin_box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, component_key](double value) { updateSetting(component_key, value); });
    }

    connect(this, &Vector2InputWidget::modified, parent, &SettingTab::keyModified);
    connect(this, &Vector2InputWidget::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight | Qt::AlignVCenter);

    m_unit_label.reset(new QLabel(PreferencesManager::getInstance()->getDistanceUnitText()));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

void Vector2InputWidget::hide() {
    QWidget::hide();
    SettingRowBase::hide();
}

void Vector2InputWidget::show() {
    QWidget::show();
    SettingRowBase::show();
}

void Vector2InputWidget::setEnabled(bool enabled) {
    QWidget::setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void Vector2InputWidget::valueChanged(QVariant val) { updateSetting(m_components[0].key, val.toDouble()); }

void Vector2InputWidget::reloadValue() {
    for (Component& component : m_components)
        component.spin_box->blockSignals(true);

    m_unit_label->setText(PreferencesManager::getInstance()->getDistanceUnitText());

    bool all_consistent = true;
    for (Component& component : m_components) {
        configureSpinBox(component.spin_box);

        bool component_consistent = true;
        setSpinBoxValue(component.spin_box, component.key, component.default_value, true, component_consistent);
        all_consistent = all_consistent && component_consistent;
    }

    for (Component& component : m_components)
        component.spin_box->blockSignals(false);

    for (const Component& component : m_components)
        emit modified(component.key);

    if (!all_consistent) {
        setNotification("Multiple Values");
        styleLabel(false);
        m_warn = true;
        emit warnParent(1);
    }
    else {
        clearNotification();
        styleLabel(true);
        m_warn = false;
        emit warnParent(0);
    }
}

bool Vector2InputWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        QDoubleSpinBox* spin_box = qobject_cast<QDoubleSpinBox*>(watched);
        if (spin_box != nullptr && !spin_box->hasFocus()) {
            event->ignore();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void Vector2InputWidget::setNotification(QString msg) {
    for (Component& component : m_components) {
        setStyleFromFile(component.spin_box, m_theme_path + "setting_rows_warning.qss");
        component.spin_box->setToolTip(msg);
    }

    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void Vector2InputWidget::clearNotification() {
    for (Component& component : m_components) {
        setStyleFromFile(component.spin_box, m_theme_path + "setting_rows_normal.qss");
        component.spin_box->setToolTip("");
    }
}

void Vector2InputWidget::configureSpinBox(QDoubleSpinBox* spin_box) {
    Distance unit = PreferencesManager::getInstance()->getDistanceUnit();
    spin_box->setFocusPolicy(Qt::StrongFocus);
    spin_box->setAlignment(Qt::AlignRight);
    spin_box->setMaximum(Constants::Limits::Maximums::kMaxDistance.to(unit));
    if (m_json[Constants::Settings::Master::kType] == "distance")
        spin_box->setMinimum(Constants::Limits::Minimums::kMinDistance.to(unit));
    else
        spin_box->setMinimum(Constants::Limits::Minimums::kMinLocation.to(unit));
    spin_box->setDecimals(m_precision);
    spin_box->setFixedWidth(kSpinBoxWidth);
    spin_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Vector2InputWidget::ensureSetting(const QString& key, Distance default_value) {
    if (!m_sb->contains(key))
        m_sb->setSetting(key, default_value());
}

void Vector2InputWidget::updateSetting(const QString& key, double displayed_value) {
    Distance base_value;
    base_value.from(displayed_value, PreferencesManager::getInstance()->getDistanceUnit());

    notifyValueAboutToChange(key);

    if (m_settings_bases.size() != 0) {
        for (QSharedPointer<SettingsBase> range : m_settings_bases)
            range->setSetting(key, base_value());

        const Distance global_value = m_sb->contains(key) ? m_sb->setting<Distance>(key) : base_value;
        removeRedundantLocalOverrides<Distance>(key, global_value);
    }
    else {
        m_sb->setSetting(key, base_value());
    }

    for (QSharedPointer<SettingRowBase> row : m_rows_to_notify)
        row->checkDependencies();

    checkDynamicDependencies();
    updateWarningStateAfterEdit();
    emit modified(key);
}

Distance Vector2InputWidget::reloadDistanceValue(const QString& key, Distance default_value, bool& consistent) {
    if (m_settings_bases.size() > 0) {
        const Distance base_value = m_sb->contains(key) ? m_sb->setting<Distance>(key) : default_value;
        removeRedundantLocalOverrides<Distance>(key, base_value);

        bool all_bases_consistent = true;
        for (int i = 1, end = m_settings_bases.size(); i < end; ++i) {
            auto sb_1 = m_settings_bases[i - 1];
            auto sb_2 = m_settings_bases[i];
            all_bases_consistent = all_bases_consistent && areConsistent(key, sb_1, sb_2, default_value);
        }

        if (all_bases_consistent)
            return effectiveDistanceValue(key, m_settings_bases[0], default_value);

        consistent = false;
        return default_value;
    }

    return effectiveDistanceValue(key, m_sb, default_value);
}

bool Vector2InputWidget::areConsistent(const QString& key, QSharedPointer<SettingsBase> a,
                                       QSharedPointer<SettingsBase> b, Distance default_value) {
    const Distance base_value = m_sb->contains(key) ? m_sb->setting<Distance>(key) : default_value;

    if (a->contains(key) && base_value == a->setting<Distance>(key))
        a->remove(key);

    if (b->contains(key) && base_value == b->setting<Distance>(key))
        b->remove(key);

    bool containsA = a->contains(key);
    bool containsB = b->contains(key);

    return (containsA == containsB) &&
           ((containsA && (a->setting<Distance>(key) == b->setting<Distance>(key))) || !containsA);
}

Distance Vector2InputWidget::effectiveDistanceValue(const QString& key, QSharedPointer<SettingsBase> settings_base,
                                                    Distance default_value) {
    if (settings_base->contains(key))
        return settings_base->setting<Distance>(key);

    if (m_sb->contains(key))
        return m_sb->setting<Distance>(key);

    return default_value;
}

bool Vector2InputWidget::hasConsistentEffectiveValues(const QString& key, Distance default_value) {
    if (m_settings_bases.size() <= 1)
        return true;

    Distance first_value = effectiveDistanceValue(key, m_settings_bases[0], default_value);
    for (int i = 1, end = m_settings_bases.size(); i < end; ++i) {
        if (effectiveDistanceValue(key, m_settings_bases[i], default_value) != first_value)
            return false;
    }

    return true;
}

bool Vector2InputWidget::hasAnyInconsistentValue() {
    for (const Component& component : m_components) {
        if (!hasConsistentEffectiveValues(component.key, component.default_value))
            return true;
    }

    return false;
}

void Vector2InputWidget::updateWarningStateAfterEdit() {
    if (hasAnyInconsistentValue()) {
        setNotification("Multiple Values");
        styleLabel(false);
        if (!m_warn)
            emit warnParent(1);
        else
            emit warnParent(0);
        m_warn = true;
    }
    else {
        clearNotification();
        styleLabel(true);
        if (m_warn)
            emit warnParent(-1);
        else
            emit warnParent(0);
        m_warn = false;
    }
}

void Vector2InputWidget::setSpinBoxValue(QDoubleSpinBox* spin_box, const QString& key, Distance default_value,
                                         bool only_if_consistent, bool& consistent) {
    Distance unit = PreferencesManager::getInstance()->getDistanceUnit();
    bool setting_consistent = true;
    Distance value = reloadDistanceValue(key, default_value, setting_consistent);
    consistent = setting_consistent;

    if (!only_if_consistent || setting_consistent)
        spin_box->setValue(value.to(unit));
}
} // namespace ORNL
