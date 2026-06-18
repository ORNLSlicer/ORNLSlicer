#include "widgets/settings/vector3_input_widget.h"

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

#include "utilities/constants.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
namespace {
constexpr int kComponentLabelWidth = 18;
constexpr int kComponentMinimumHeight = 22;
constexpr int kComponentSpacing = 4;
constexpr int kSpinBoxWidth = 82;

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

Vector3InputWidget::Vector3InputWidget(SettingTab* parent, QSharedPointer<SettingsBase> sb, QString primary_key,
                                       QString secondary_key, QString tertiary_key, fifojson json, QGridLayout* layout,
                                       int index, double primary_default, double secondary_default,
                                       double tertiary_default, QString primary_label, QString secondary_label,
                                       QString tertiary_label)
    : QWidget(parent), SettingRowBase(parent, sb, primary_key, json, layout, index),
      m_components {{{primary_key, new QDoubleSpinBox(this), primary_default},
                     {secondary_key, new QDoubleSpinBox(this), secondary_default},
                     {tertiary_key, new QDoubleSpinBox(this), tertiary_default}}},
      m_warn(false) {
    QHBoxLayout* vector_layout = new QHBoxLayout(this);
    vector_layout->setContentsMargins(0, 0, 0, 0);
    vector_layout->setSpacing(kComponentSpacing);

    const std::array<QString, 3> labels {primary_label, secondary_label, tertiary_label};

    for (std::size_t i = 0; i < m_components.size(); ++i) {
        Component& component = m_components[i];
        ensureSetting(component.key, component.default_value);

        if (i != 0)
            vector_layout->addSpacing(kComponentSpacing);
        addComponentEditor(vector_layout, this, labels[i], component.spin_box);

        configureSpinBox(component.spin_box);
        component.spin_box->installEventFilter(this);
        component.spin_box->setValue(m_sb->setting<double>(component.key));

        const QString component_key = component.key;
        connect(component.spin_box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, component_key](double value) { updateSetting(component_key, value); });
    }

    connect(this, &Vector3InputWidget::modified, parent, &SettingTab::keyModified);
    connect(this, &Vector3InputWidget::warnParent, parent, &SettingTab::headerWarning);

    layout->addWidget(this, index, 1, Qt::AlignRight | Qt::AlignVCenter);

    m_unit_label.reset(new QLabel(""));
    layout->addWidget(m_unit_label.get(), index, 2, Qt::AlignLeft);
}

void Vector3InputWidget::hide() {
    QWidget::hide();
    SettingRowBase::hide();
}

void Vector3InputWidget::show() {
    QWidget::show();
    SettingRowBase::show();
}

void Vector3InputWidget::setEnabled(bool enabled) {
    QWidget::setEnabled(enabled);
    SettingRowBase::setEnabled(enabled);
}

void Vector3InputWidget::valueChanged(QVariant val) { updateSetting(m_components[0].key, val.toDouble()); }

void Vector3InputWidget::reloadValue() {
    for (Component& component : m_components)
        component.spin_box->blockSignals(true);

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

bool Vector3InputWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        QDoubleSpinBox* spin_box = qobject_cast<QDoubleSpinBox*>(watched);
        if (spin_box != nullptr && !spin_box->hasFocus()) {
            event->ignore();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void Vector3InputWidget::setNotification(QString msg) {
    for (Component& component : m_components) {
        setStyleFromFile(component.spin_box, m_theme_path + "setting_rows_warning.qss");
        component.spin_box->setToolTip(msg);
    }

    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), msg, nullptr, QRect(), 30000);
}

void Vector3InputWidget::clearNotification() {
    for (Component& component : m_components) {
        setStyleFromFile(component.spin_box, m_theme_path + "setting_rows_normal.qss");
        component.spin_box->setToolTip("");
    }
}

void Vector3InputWidget::configureSpinBox(QDoubleSpinBox* spin_box) {
    spin_box->setFocusPolicy(Qt::StrongFocus);
    spin_box->setAlignment(Qt::AlignRight);
    spin_box->setMinimum(Constants::Limits::Minimums::kMinUnitlessFloat);
    spin_box->setMaximum(Constants::Limits::Maximums::kMaxUnitlessFloat);
    spin_box->setDecimals(m_precision);
    spin_box->setFixedWidth(kSpinBoxWidth);
    spin_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Vector3InputWidget::ensureSetting(const QString& key, double default_value) {
    if (!m_sb->contains(key))
        m_sb->setSetting(key, default_value);
}

void Vector3InputWidget::updateSetting(const QString& key, double value) {
    if (m_settings_bases.size() != 0) {
        for (QSharedPointer<SettingsBase> range : m_settings_bases)
            range->setSetting(key, value);
    }
    else {
        m_sb->setSetting(key, value);
    }

    for (QSharedPointer<SettingRowBase> row : m_rows_to_notify)
        row->checkDependencies();

    checkDynamicDependencies();
    updateWarningStateAfterEdit();
    emit modified(key);
}

double Vector3InputWidget::reloadDoubleValue(const QString& key, double default_value, bool& consistent) {
    if (m_settings_bases.size() > 0) {
        bool all_bases_consistent = true;
        for (int i = 1, end = m_settings_bases.size(); i < end; ++i) {
            auto sb_1 = m_settings_bases[i - 1];
            auto sb_2 = m_settings_bases[i];
            all_bases_consistent = all_bases_consistent && areConsistent(key, sb_1, sb_2, default_value);
        }

        if (all_bases_consistent)
            return effectiveDoubleValue(key, m_settings_bases[0], default_value);

        consistent = false;
        return default_value;
    }

    return effectiveDoubleValue(key, m_sb, default_value);
}

bool Vector3InputWidget::areConsistent(const QString& key, QSharedPointer<SettingsBase> a,
                                       QSharedPointer<SettingsBase> b, double default_value) {
    const double base_value = m_sb->contains(key) ? m_sb->setting<double>(key) : default_value;

    if (a->contains(key) && base_value == a->setting<double>(key))
        a->remove(key);

    if (b->contains(key) && base_value == b->setting<double>(key))
        b->remove(key);

    bool containsA = a->contains(key);
    bool containsB = b->contains(key);

    return (containsA == containsB) &&
           ((containsA && (a->setting<double>(key) == b->setting<double>(key))) || !containsA);
}

double Vector3InputWidget::effectiveDoubleValue(const QString& key, QSharedPointer<SettingsBase> settings_base,
                                                double default_value) {
    if (settings_base->contains(key))
        return settings_base->setting<double>(key);

    if (m_sb->contains(key))
        return m_sb->setting<double>(key);

    return default_value;
}

bool Vector3InputWidget::hasConsistentEffectiveValues(const QString& key, double default_value) {
    if (m_settings_bases.size() <= 1)
        return true;

    double first_value = effectiveDoubleValue(key, m_settings_bases[0], default_value);
    for (int i = 1, end = m_settings_bases.size(); i < end; ++i) {
        if (effectiveDoubleValue(key, m_settings_bases[i], default_value) != first_value)
            return false;
    }

    return true;
}

bool Vector3InputWidget::hasAnyInconsistentValue() {
    for (const Component& component : m_components) {
        if (!hasConsistentEffectiveValues(component.key, component.default_value))
            return true;
    }

    return false;
}

void Vector3InputWidget::updateWarningStateAfterEdit() {
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

void Vector3InputWidget::setSpinBoxValue(QDoubleSpinBox* spin_box, const QString& key, double default_value,
                                         bool only_if_consistent, bool& consistent) {
    bool setting_consistent = true;
    double value = reloadDoubleValue(key, default_value, setting_consistent);
    consistent = setting_consistent;

    if (!only_if_consistent || setting_consistent)
        spin_box->setValue(value);
}
} // namespace ORNL
